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
 *		This function is invoked by each of the Tk image handler
 *		procs (MatchStringProc, etc.) to initialize state information
 *		used during the course of decoding a WEBP image.
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
 * WEBPCleanup --
 *
 *		This function is invoked by each of the Tk image handler
 *		procs (MatchStringProc, etc.) prior to returning to Tcl
 *		in order to clean up any allocated memory and call other
 *		cleanup handlers such as zlib's inflateEnd/deflateEnd.
 *
 * Results:
 *		None.
 *
 * Side effects:
 *		The reference count of the -data Tcl_Obj*, if any, is
 *		decremented.  Buffers are freed, zstreams are closed.
 *      The WEBPDec should not be used for any purpose without being
 *      reinitialized post-cleanup.
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
           int maxWidth, int maxHeight, int scaleMode, int quality)
{
  int contentSize;
  int inwidth, inheight;
  int origWidth = 0;
  int origHeight = 0;
  int width, height;
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
  Rect srcrect;
  Rect destrect;

  if (quality < 0) {
    quality = 85;
  }
  if (quality > 100) {
    quality = 100;
  }

  headerlen = YchannelRead(pSrc->channel, (char *) header, sizeof(header));
  if (headerlen < WEBP_HEADER_SIZE) {
    return 0;
  }

  /* Check WEBP header */
  contentSize = WebpCheckHeader((const char*) header, headerlen);
  if (contentSize <= 0) {
    return YMAGINE_ERROR;
  }
  ALOGV("Input is %d bytes", contentSize);

  if (WebPGetInfo(header, headerlen, &origWidth, &origHeight) == 0) {
    ALOGD("invalid VP8 header");
    return YMAGINE_ERROR;
  }

  if (origWidth <= 0 || origHeight <= 0) {
    return YMAGINE_ERROR;
  }

  inwidth = VbitmapWidth(vbitmap);
  inheight = VbitmapHeight(vbitmap);
  
  srcrect.x = 0;
  srcrect.y = 0;
  srcrect.width = origWidth;
  srcrect.height = origHeight;

  if (inwidth > 0 && inheight > 0) {
    width = inwidth;
    height = inheight;

    computeTransform(origWidth, origHeight,
                     maxWidth, maxHeight, scaleMode,
                     &srcrect, &destrect);
  } else {
    int reqwidth, reqheight;

    computeBounds(origWidth, origHeight,
                  maxWidth, maxHeight, scaleMode,
                  &reqwidth, &reqheight);
    if (VbitmapResize(vbitmap, reqwidth, reqheight) != YMAGINE_OK) {
      return YMAGINE_ERROR;
    }
    if (VbitmapType(vbitmap) == VBITMAP_NONE) {
      return YMAGINE_OK;
    }

    width = VbitmapWidth(vbitmap);
    height = VbitmapHeight(vbitmap);

    destrect.x = 0;
    destrect.y = 0;
    destrect.width = width;
    destrect.height = height;
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
    pSrc->outwidth = width;
    pSrc->outheight = height;

    if (odata == NULL) {
      ALOGD("failed to get reference to pixel buffer");
      rc = YMAGINE_ERROR;
    } else {
      WebPDecoderConfig config;

      pSrc->isdirect = 1;
      pSrc->outformat = oformat;
      pSrc->outbpp = VbitmapBpp(vbitmap);
      pSrc->outstride = opitch;
      pSrc->outbuffer = odata + destrect.x * pSrc->outbpp + destrect.y * pSrc->outstride;

      WebPInitDecoderConfig(&config);

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
      if (width != origWidth || height != origHeight) {
        config.options.use_scaling = 1;
        config.options.scaled_width = width;
        config.options.scaled_height = height;
      }

      rc = YMAGINE_ERROR;
      if (WebPGetFeatures(input, inputlen, &config.input) == VP8_STATUS_OK) {
        WebPIDecoder* idec;

        // Specify the desired output colorspace:
        if (premultiplied) {
          /* Premultiplied */
          config.output.colorspace = MODE_rgbA;
        } else {
          config.output.colorspace = MODE_RGBA;
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

  return -1;
}

#endif /* HAVE_WEBP */

int
decodeWEBP(Ychannel *channel, Vbitmap *vbitmap,
           int maxWidth, int maxHeight,
           int scaleMode, int quality,
           PixelShader* pixelShader)
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
    nlines = WEBPDecode(&webp, vbitmap, maxWidth, maxHeight, scaleMode, quality);
    WEBPFini(&webp);
  }
#endif

  return nlines;
}

int
encodeWEBP(Vbitmap *vbitmap, Ychannel *channelout, int quality)
{
  int rc = YMAGINE_ERROR;

  return rc;
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
