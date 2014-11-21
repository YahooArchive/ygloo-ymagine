/*
  A WebP decoder only, on top of libwebp

  http://code.google.com/speed/webp/
  http://www.webmproject.org/code/#libwebp_webp_image_decoder_library
  http://review.webmproject.org/gitweb?p=libwebp.git
*/

// Based on src/dec/webp.c from libwebp

// Copyright 2010 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Main decoding functions for WEBP images.
//
// Author: Skal (pascal.massimino@gmail.com)

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <setjmp.h>

#define LOG_TAG "ymagine::webp"
#include "yosal/yosal.h"
#include "ymagine/ymagine.h"
#include "ymagine_priv.h"

#include "graphics/bitmap.h"


typedef struct
{
  Ychannel *channel; /* Input channel */
  Vbitmap *bitmap;

  int isdirect;

  int inwidth;
  int inheight;

  int outwidth;
  int outheight;
  int outbpp;
  int outstride;
  int outformat;
  unsigned char *outbuffer;
} WEBPDec;

#if HAVE_WEBP

#include "webp/decode.h"
#include "dec/decode_vp8.h"

#include "webp/encode.h"

/* Assume integer are at least 32 bits */
static unsigned int getInt32L(const void *in) {
  unsigned int result;
  unsigned char *buffer = (unsigned char*) in;
  
  if (buffer == NULL) {
    return 0;
  }

  result = buffer[3];
  result = (result<<8) + buffer[2];
  result = (result<<8) + buffer[1];
  result = (result<<8) + buffer[0];

  return result;
}

/* Parse headers of RIFF container */
#define WEBP_HEADER_SIZE 16
#define VP8_HEADER_SIZE 10

static int WebpCheckHeader(const char *header, int len)
{
  const unsigned char *buffer;
  unsigned int totalSize;
  
  if (header == NULL || len < WEBP_HEADER_SIZE) {
    return 0;
  }

  buffer = (const unsigned char*) header;

  // RIFF container for WEBP image should always have:
  //   0ffset	Description
  //   0	"RIFF" 4-byte tag
  //   4	size of image data (including metadata) starting at offset 8
  //   8	"WEBP" the form-type signature
  //   12	"VP8 " 4-bytes tags, describing the raw video format used
  //   16	size of the raw VP8 image data, starting at offset 20
  //   20	the VP8 bytes
  // First check for RIFF top chunk, consuming only 4 bytes
  if (buffer[0] != 'R' || buffer[1] != 'I' ||
      buffer[2] != 'F' || buffer[3] != 'F') {
    return 0;
  }
  
  totalSize = getInt32L(buffer+4);  
  if (totalSize <= 0 || totalSize > 0x7ffffff7) {
    return 0;
  }
  if (totalSize < (int) (WEBP_HEADER_SIZE-8)) {
    return 0;
  }

  /* If RIFF header found, check for RIFF content to start with 
     WEBP/VP8 chunk */
  if (buffer[8] != 'W' || buffer[9] != 'E' ||
      buffer[10] != 'B' || buffer[11] != 'P') {
    return 0;
  }
  if (buffer[12] != 'V' || buffer[13] != 'P' || buffer[14] != '8') {
    return 0;
  }

  /* 'VP8 ' for default profile, 'VP8L' for lossless, 'VP8X' for extented */
  if (buffer[15] != ' ' && buffer[15] != 'L' && buffer[15] != 'X') {
    return 0;
  }

  return (totalSize + 8);
}

/*
 *----------------------------------------------------------------------
 *
 * WEBPInit --
 *
 *		Initialize a WEBP decoding context
 *
 * Results:
 *		YMAGINE_OK, or YMAGINE_ERROR if initialization failed.
 *
 *----------------------------------------------------------------------
 */

static int
WEBPInit(WEBPDec* pSrc, Ychannel *f, Vbitmap *bitmap)
{
  /* Reset all data in structure */
  memset(pSrc, 0, sizeof(WEBPDec));
  
  pSrc->bitmap = bitmap;
  pSrc->channel = f;

  return YMAGINE_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * WEBPFini --
 *
 *		This function cleans up a WEBP decoding context
 *
 * Results:
 *		None.
 *
 *----------------------------------------------------------------------
 */

static void
WEBPFini(WEBPDec* pWEBP)
{
  if (pWEBP==NULL) {
    return;
  }  
}


/*
 *----------------------------------------------------------------------
 *
 * WEBPDecode --
 *
 *		This function handles the entirety of reading a WEBP file (or
 *		data) from the first byte to the last.
 *
 * Results:
 *		YMAGINE_OK, or YMAGINE_ERROR if an I/O error occurs or any problems
 *		are detected in the WEBP file.
 *
 * Side effects:
 *		The access position in f advances.  Memory may be allocated
 *		and image dimensions and contents may change.
 *
 *----------------------------------------------------------------------
 */

static int
WEBPDecode(WEBPDec* pSrc, Vbitmap *vbitmap,
           YmagineFormatOptions *options)
{
  int contentSize;
  int origWidth = 0;
  int origHeight = 0;
  int quality;
  unsigned char header[WEBP_HEADER_SIZE + 32];
  int headerlen;
  int toRead;
  unsigned char *input = NULL;
  int inputlen;
  int oformat;
  int opitch;
  unsigned char *odata;
  int rc;
  int premultiplied = 1;
  Vrect srcrect;
  Vrect destrect;
  WebPIDecoder* idec;

  if (options == NULL) {
    /* Options argument is mandatory */
    return 0;
  }

  headerlen = YchannelRead(pSrc->channel, (char *) header, sizeof(header));
  if (headerlen < WEBP_HEADER_SIZE) {
    return 0;
  }

  /* Check WEBP header */
  contentSize = WebpCheckHeader((const char*) header, headerlen);
  if (contentSize <= 0) {
    return 0;
  }

  if (WebPGetInfo(header, headerlen, &origWidth, &origHeight) == 0) {
    ALOGD("invalid VP8 header");
    return 0;
  }

  if (origWidth <= 0 || origHeight <= 0) {
    return 0;
  }

  YmagineFormatOptions_invokeCallback(options, YMAGINE_IMAGEFORMAT_WEBP,
                                      origWidth, origHeight);

  if (YmaginePrepareTransform(vbitmap, options,
                              origWidth, origHeight,
                              &srcrect, &destrect) != YMAGINE_OK) {
    return 0;
  }

#if YMAGINE_DEBUG_WEBP
  ALOGD("size: %dx%d req: %dx%d %s -> output: %dx%d",
        origWidth, origHeight,
        destrect.width, destrect.height,
        (options->scalemode == YMAGINE_SCALE_CROP) ? "crop" :
        (options->scalemode == YMAGINE_SCALE_FIT ? "fit" : "letterbox"),
        destrect.width, destrect.height);
#endif

  if (vbitmap != NULL) {
    if (options->resizable) {
      destrect.x = 0;
      destrect.y = 0;
      if (VbitmapResize(vbitmap, destrect.width, destrect.height) != YMAGINE_OK) {
        return 0;
      }
    }
    if (VbitmapType(vbitmap) == VBITMAP_NONE) {
      /* Decode bounds only, return positive number (number of lines) on success */
      return VbitmapHeight(vbitmap);
    }
  }

  pSrc->bitmap = vbitmap;

  inputlen = contentSize;
  toRead = inputlen - headerlen;

  rc = VbitmapLock(vbitmap);
  if (rc != YMAGINE_OK) {
    ALOGE("VbitmapLock() failed (code %d)", rc);
    rc = YMAGINE_ERROR;
  } else {
    odata = VbitmapBuffer(vbitmap);
    opitch = VbitmapPitch(vbitmap);
    oformat = VbitmapColormode(vbitmap);

    pSrc->inwidth = origWidth;
    pSrc->inheight = origHeight;
    pSrc->outwidth = destrect.width;
    pSrc->outheight = destrect.height;

    if (odata == NULL) {
      ALOGD("failed to get reference to pixel buffer");
      rc = YMAGINE_ERROR;
    } else if (oformat != VBITMAP_COLOR_RGBA && oformat != VBITMAP_COLOR_RGB) {
      ALOGD("currently only support RGB, RGBA webp decoding");
      rc = YMAGINE_ERROR;
    } else {
      WebPDecoderConfig config;

      pSrc->isdirect = 1;
      pSrc->outformat = oformat;
      pSrc->outbpp = VbitmapBpp(vbitmap);
      pSrc->outstride = opitch;
      pSrc->outbuffer = odata + destrect.x * pSrc->outbpp + destrect.y * pSrc->outstride;

      WebPInitDecoderConfig(&config);

      quality = YmagineFormatOptions_normalizeQuality(options);
      if (quality < 90) {
        config.options.no_fancy_upsampling = 1;
      }
      if (quality < 60) {
        config.options.bypass_filtering = 1;
      }
      config.options.use_threads = 1;

      if (srcrect.x != 0 || srcrect.y != 0 || srcrect.width != origWidth || srcrect.height != origHeight) {
        /* Crop on source */
        config.options.use_cropping = 1;
        config.options.crop_left = srcrect.x;
        config.options.crop_top = srcrect.y;
        config.options.crop_width = srcrect.width;
        config.options.crop_height = srcrect.height;
      }
      if (pSrc->outwidth != pSrc->inwidth || pSrc->outheight != pSrc->inheight) {
        config.options.use_scaling = 1;
        config.options.scaled_width = pSrc->outwidth;
        config.options.scaled_height = pSrc->outheight;
      }

      rc = YMAGINE_ERROR;

      // Specify the desired output colorspace:
      if (oformat == VBITMAP_COLOR_RGBA) {
        if (premultiplied) {
          /* Premultiplied */
          config.output.colorspace = MODE_rgbA;
        } else {
          config.output.colorspace = MODE_RGBA;
        }
      } else {
        config.output.colorspace = MODE_RGB;
      }

      // Have config.output point to an external buffer:
      config.output.u.RGBA.rgba = (uint8_t*) pSrc->outbuffer;
      config.output.u.RGBA.stride = pSrc->outstride;
      config.output.u.RGBA.size = pSrc->outstride * pSrc->outheight;
      config.output.is_external_memory = 1;

      idec = WebPIDecode(NULL, 0, &config);
      if (idec != NULL) {
        VP8StatusCode status;

        status = WebPIAppend(idec, header, headerlen);
        if (status == VP8_STATUS_OK || status == VP8_STATUS_SUSPENDED) {
          int bytes_remaining = toRead;
          int bytes_read;
          int bytes_req;
          unsigned char rbuf[8192];

          // See WebPIUpdate(idec, buffer, size_of_transmitted_buffer);
          bytes_req = sizeof(rbuf);
          while (bytes_remaining > 0) {
            if (bytes_req > bytes_remaining) {
              bytes_req = bytes_remaining;
            }
            bytes_read = YchannelRead(pSrc->channel, rbuf, bytes_req);
            if (bytes_read <= 0) {
              break;
            }
            status = WebPIAppend(idec, (uint8_t*) rbuf, bytes_read);
            if (status == VP8_STATUS_OK) {
              rc = YMAGINE_OK;
              break;
            } else if (status == VP8_STATUS_SUSPENDED) {
              if (bytes_remaining > 0) {
                bytes_remaining -= bytes_read;
              }
            } else {
              /* error */
              break;
            }
            // The above call decodes the current available buffer.
            // Part of the image can now be refreshed by calling
            // WebPIDecGetRGB()/WebPIDecGetYUVA() etc.
          }
        }
      }

      // the object doesn't own the image memory, so it can now be deleted.
      WebPIDelete(idec);
      WebPFreeDecBuffer(&config.output);
    }

    VbitmapUnlock(vbitmap);
  }

  if (input) {
    Ymem_free((char*) input);
  }
  if (!pSrc->isdirect) {
    Ymem_free(pSrc->outbuffer);
  }

  if (rc == YMAGINE_OK) {
    return origHeight;
  }

  return 0;
}

#endif /* HAVE_WEBP */

int
decodeWEBP(Ychannel *channel, Vbitmap *vbitmap,
           YmagineFormatOptions *options)
{
  int nlines = -1;
#if HAVE_WEBP
  WEBPDec  webp;
#endif
  
  if (!YchannelReadable(channel)) {
#if YMAGINE_DEBUG_WEBP
    ALOGD("input channel not readable");
#endif
    return nlines;
  }

#if HAVE_WEBP
  if (WEBPInit(&webp, channel, vbitmap) == YMAGINE_OK) {
    nlines = WEBPDecode(&webp, vbitmap, options);
    WEBPFini(&webp);
  }
#endif

  return nlines;
}

static int
WebPYchannelWrite(const uint8_t* data, size_t data_size,
                  const WebPPicture* picture)
{
  Ychannel *channel = (Ychannel*) picture->custom_ptr;

  if (channel != NULL) {
    if (YchannelWrite(channel, data, data_size) != data_size) {
      return 0;
    }
  }
  return 1;
}

int
encodeWEBP(Vbitmap *vbitmap, Ychannel *channelout, YmagineFormatOptions *options)
{
  int rc = YMAGINE_ERROR;
  WebPConfig config;
  WebPPicture picture;
  WebPPreset preset = WEBP_PRESET_PHOTO; // One of DEFAULT, PICTURE, PHOTO, DRAWING, ICON or TEXT
  int width;
  int height;
  int pitch;
  int quality;
  const unsigned char *pixels;
  int colormode;

  /*
   * List of encoding options for WebP config
   *   int lossless;           // Lossless encoding (0=lossy(default), 1=lossless).
   *   float quality;          // between 0 (smallest file) and 100 (biggest)
   *   int method;             // quality/speed trade-off (0=fast, 6=slower-better)
   *
   *   WebPImageHint image_hint;  // Hint for image type (lossless only for now).
   *
   *   // Parameters related to lossy compression only:
   *   int target_size;        // if non-zero, set the desired target size in bytes.
   *                           // Takes precedence over the 'compression' parameter.
   *   float target_PSNR;      // if non-zero, specifies the minimal distortion to
   *                           // try to achieve. Takes precedence over target_size.
   *   int segments;           // maximum number of segments to use, in [1..4]
   *   int sns_strength;       // Spatial Noise Shaping. 0=off, 100=maximum.
   *   int filter_strength;    // range: [0 = off .. 100 = strongest]
   *   int filter_sharpness;   // range: [0 = off .. 7 = least sharp]
   *   int filter_type;        // filtering type: 0 = simple, 1 = strong (only used
   *                           // if filter_strength > 0 or autofilter > 0)
   *   int autofilter;         // Auto adjust filter's strength [0 = off, 1 = on]
   *   int alpha_compression;  // Algorithm for encoding the alpha plane (0 = none,
   *                           // 1 = compressed with WebP lossless). Default is 1.
   *   int alpha_filtering;    // Predictive filtering method for alpha plane.
   *                           //  0: none, 1: fast, 2: best. Default if 1.
   *   int alpha_quality;      // Between 0 (smallest size) and 100 (lossless).
   *                           // Default is 100.
   *   int pass;               // number of entropy-analysis passes (in [1..10]).
   *
   *   int show_compressed;    // if true, export the compressed picture back.
   *                           // In-loop filtering is not applied.
   *   int preprocessing;      // preprocessing filter (0=none, 1=segment-smooth)
   *   int partitions;         // log2(number of token partitions) in [0..3]
   *                           // Default is set to 0 for easier progressive decoding.
   *   int partition_limit;    // quality degradation allowed to fit the 512k limit on
   *                           // prediction modes coding (0: no degradation,
   *                           // 100: maximum possible degradation).
   */

  colormode = VbitmapColormode(vbitmap);

  if (colormode != VBITMAP_COLOR_RGBA && colormode != VBITMAP_COLOR_RGB) {
    ALOGD("currently only support RGB, RGBA webp encoding");
    return rc;
  }

  quality = YmagineFormatOptions_normalizeQuality(options);

  if (!WebPConfigPreset(&config, preset, (float) quality)) {
    return YMAGINE_ERROR;   // version error
  }

  if (options && options->accuracy >= 0) {
    int method = options->accuracy / 15;
    if (method > 6) {
      method = 6;
    }
    config.method = method;
  }

  if (!WebPValidateConfig(&config)) {
    // parameter ranges verification failed
    return YMAGINE_ERROR;
  }

  rc = VbitmapLock(vbitmap);
  if (rc < 0) {
    ALOGE("AndroidBitmap_lockPixels() failed");
    return YMAGINE_ERROR;
  }

  width = VbitmapWidth(vbitmap);
  height = VbitmapHeight(vbitmap);
  pitch = VbitmapPitch(vbitmap);
  pixels = VbitmapBuffer(vbitmap);

  if (WebPPictureInit(&picture)) {
    picture.use_argb = 1;
    picture.width = width;
    picture.height = height;
    picture.writer = WebPYchannelWrite;
    picture.custom_ptr = channelout;

    if (colormode == VBITMAP_COLOR_RGBA) {
      WebPPictureImportRGBA(&picture, pixels, pitch);
    } else {
      WebPPictureImportRGB(&picture, pixels, pitch);
    }

    WebPEncode(&config, &picture);

    WebPPictureFree(&picture);
  }

  VbitmapUnlock(vbitmap);

  return YMAGINE_OK;
}

int
matchWEBP(Ychannel *channel)
{
  char header[WEBP_HEADER_SIZE];
  int hlen;

  if (!YchannelReadable(channel)) {
    return YFALSE;
  }

  hlen = YchannelRead(channel, header, sizeof(header));
  if (hlen > 0) {
    YchannelPush(channel, header, hlen);
  }

  if (WebpCheckHeader(header, hlen) <= 0) {
    return YFALSE;
  }

  return YTRUE;
}
