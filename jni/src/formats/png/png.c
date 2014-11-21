/*
 * A PNG decoder only, on top of libpng
 *
 * Copyright 2013 Yahoo! Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may
 * obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License. See accompanying LICENSE file.
 */
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <setjmp.h>

#define LOG_TAG "ymagine::png"
#include "yosal/yosal.h"
#include "ymagine/ymagine.h"
#include "ymagine_priv.h"

#include "graphics/bitmap.h"

/* 
 * PNG 1.2 specs
 *  http://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html
 */

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
} PNGDec;

/* Assume integer are at least 32 bits */
static unsigned int getInt32(const void *in) {
  unsigned int result;
  unsigned char *buffer = (unsigned char*) in;
  
  if (buffer == NULL) {
    return 0;
  }

  result = buffer[0];
  result = (result<<8) + buffer[1];
  result = (result<<8) + buffer[2];
  result = (result<<8) + buffer[3];

  return result;
}

/* Parse headers of RIFF container */
#define PNG_SIGNATURE_SIZE 8
#define PNG_IHDR_SIZE 21

#define PNG_HEADER_SIZE (PNG_SIGNATURE_SIZE+PNG_IHDR_SIZE)

static int PNGCheckHeader(const char *header, int len, int *pWidth, int *pHeight)
{
  const unsigned char *buffer;
  unsigned int width = 0;
  unsigned int height = 0;
  
  if (header == NULL || len < PNG_HEADER_SIZE) {
    return 0;
  }

  buffer = (const unsigned char*) header;

  if (buffer[0] != 0x89 || buffer[1] != 'P' ||
      buffer[2] != 'N' || buffer[3] != 'G' ||
      buffer[4] != 0x0d || buffer[5] != 0x0a ||
      buffer[6] != 0x1a || buffer[7] != 0x0a) {
    return 0;
  }

  /* According to specs, IHDR chunk MUST appear first */
  buffer = (const unsigned char*) (header + 8);
  if (buffer[4] != 'I' || buffer[5] != 'H' ||
      buffer[6] != 'D' || buffer[7] != 'R') {
    return 0;
  }

  width = getInt32(buffer + 8);
  height = getInt32(buffer + 12);

  if (width <= 0 || height <= 0) {
    return 0;
  }

  if (pWidth != NULL) {
    *pWidth = width;
  }
  if (pHeight != NULL) {
    *pHeight = height;
  }

  return 1;
}

#if HAVE_PNG

#include "png.h"

typedef struct cleanup_info {
  char **data;
} cleanup_info;

static void
ymagine_png_error(png_structp png_ptr, png_const_charp error_msg)
{
  cleanup_info *info;
  
  info = (cleanup_info *) png_get_error_ptr(png_ptr);
  if (info->data) {
    Ymem_free((char *) info->data);
  }
  longjmp(*(jmp_buf *) png_ptr,1);
}

static void
ymagine_png_warning(png_structp png_ptr, png_const_charp error_msg)
{
  return;
}

static void
ymagine_png_read(png_structp png_ptr, png_bytep data, png_size_t length)
{
  int check = 0;
  PNGDec *dec;

  dec = (PNGDec*) png_get_progressive_ptr(png_ptr);
  if (dec != NULL) {
    check = YchannelRead(dec->channel, data, (int) length);
  }
  if (check != (int) length) {
    png_error(png_ptr, "Read Error");
  }
}

static YINLINE void
ymagine_png_write(png_structp png_ptr, png_bytep data, png_size_t length)
{
  Ychannel *channel = (Ychannel*) png_get_progressive_ptr(png_ptr);

  if (YchannelWrite(channel, (const char *) data, (int) length) != length) {
    png_error(png_ptr, "Write Error");
  }
}

static int
PNGInit(PNGDec* pSrc, Ychannel *f, Vbitmap *bitmap)
{
  /* Reset all data in structure */
  memset(pSrc, 0, sizeof(PNGDec));
  
  pSrc->bitmap = bitmap;
  pSrc->channel = f;

  return YMAGINE_OK;
}

static void
PNGFini(PNGDec* pPNG)
{
  if (pPNG==NULL) {
    return;
  }
}

static int
PngWriter(Transformer *transformer, void *writerdata, void *line)
{
  return YMAGINE_OK;
}

static int
PNGDecode(PNGDec* pSrc, Vbitmap *vbitmap,
          YmagineFormatOptions *options)
{
  int origWidth = 0;
  int origHeight = 0;
  unsigned char header[PNG_HEADER_SIZE];
  int headerlen;
  unsigned char *input = NULL;
  int rc;
  Vrect srcrect;
  Vrect destrect;
  Transformer *transformer = NULL;
  png_infop info_ptr;
  png_infop end_info;
  unsigned char **png_data = NULL;
  png_uint_32 info_width, info_height;
  int bit_depth, color_type, interlace_type;
  int pitch;
  int passes;
  png_structp png_ptr;
  cleanup_info cleanup;
  unsigned char *data = NULL;
  float sharpen = 0.0f;

  if (options == NULL) {
    /* Options argument is mandatory */
    return 0;
  }

  headerlen = YchannelRead(pSrc->channel, (char *) header, sizeof(header));
  if (headerlen > 0) {
    YchannelPush(pSrc->channel, (const char*) header, headerlen);
  }

  /* Check PNG header */
  if (headerlen < PNG_HEADER_SIZE) {
    return 0;
  }
  if (!PNGCheckHeader((const char*) header, headerlen, &origWidth, &origHeight)) {
    return 0;
  }
  if (origWidth <= 0 || origHeight <= 0) {
    return 0;
  }

  YmagineFormatOptions_invokeCallback(options, YMAGINE_IMAGEFORMAT_PNG,
                                      origWidth, origHeight);

  ALOGV("Input is %dx%d bytes", origWidth, origHeight);

  if (YmaginePrepareTransform(vbitmap, options,
                              origWidth, origHeight,
                              &srcrect, &destrect) != YMAGINE_OK) {
    return 0;
  }

  sharpen = options->sharpen;

  /* Resize target bitmap */
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

  pSrc->inwidth = origWidth;
  pSrc->inheight = origHeight;
  pSrc->outwidth = destrect.width;
  pSrc->outheight = destrect.height;

  /* Default to error, set return code to OK only if decoding completes */
  rc = YMAGINE_ERROR;

  cleanup.data = NULL;
  png_ptr= png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                  (png_voidp) &cleanup,
                                  ymagine_png_error,ymagine_png_warning);
  if (png_ptr != NULL) {
    png_set_read_fn(png_ptr, pSrc, ymagine_png_read);
  }
      
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    png_destroy_read_struct(&png_ptr,NULL,NULL);
    png_ptr = NULL;
  } else {
    end_info = png_create_info_struct(png_ptr);
    if (end_info == NULL) {
      png_destroy_read_struct(&png_ptr,&info_ptr,NULL);
      png_ptr = NULL;
    }
  }
      
  if (png_ptr == NULL || setjmp(*(jmp_buf *) png_ptr)) {
    /* Come back here if failure */
    rc = YMAGINE_ERROR;
  } else {
    /* png_set_sib_bytes(png_ptr,8); */
    png_read_info(png_ptr,info_ptr);
    png_get_IHDR(png_ptr, info_ptr,
                 &info_width, &info_height,
                 &bit_depth,&color_type, &interlace_type,
                 (int *) NULL, (int *) NULL);
    passes = 1;
      
    /* if less than 8 bits /channels => 8 bits / channels */
    if (bit_depth < 8) {
      png_set_packing(png_ptr);
    }
      
    /* convert from 16 bits per channels to 8 bits */
    if (bit_depth == 16) {
      png_set_strip_16(png_ptr);
    }

    /* Grayscale => RGB or RGBA */
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
      png_set_gray_to_rgb(png_ptr);
    }

    /* Palette indexed colors to RGB */
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
      png_set_palette_to_rgb(png_ptr);
    }

    /* 8 bits / channel is needed */
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
      png_set_expand_gray_1_2_4_to_8(png_ptr);
    }

    /* all transparency type : 1 color, indexed => alpha channel */
    if (png_get_valid(png_ptr, info_ptr,PNG_INFO_tRNS)) {
      png_set_tRNS_to_alpha(png_ptr);
    }

    /* RGB => RGBA */
    if (color_type != PNG_COLOR_TYPE_RGBA) {
      png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
    }

#if 0
    /* Get ARGB instead of RGBA */
    if (color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
      png_set_swap_alpha(png_ptr);
    }
#endif


    /* Get number of passes (1 if not interlaced, 7 for Adam7 */
    passes = png_set_interlace_handling (png_ptr);
#ifdef YMAGINE_PNG_GAMMA
    if (png_get_sRGB && png_setsRGB) {
      if (png_get_sRGB(png_ptr, info_ptr, &intent)) {
        png_set_sRGB(png_ptr, info_ptr, intent);
      }
    } else if (png_get_gAMA && png_set_gamma) {
      /* Gamma settings
         2.2 => A good guess for a  PC monitor in a bright office or a dim room
         2.0 => A good guess for a PC monitor in a dark room
         1.7 => A good guess for Mac systems
      */
      double gamma;
      if (!png_get_gAMA(png_ptr, info_ptr, &gamma)) {
        gamma = 0.45455;
      }
      png_set_gamma(png_ptr, 2.2, gamma);
    }
#endif

    /* Update infos */
    png_read_update_info (png_ptr, info_ptr);
    png_get_IHDR (png_ptr, info_ptr,
                  &info_width, &info_height,
                  &bit_depth, &color_type, &interlace_type,
                  (int *) NULL, (int *) NULL);

    // bpp = png_get_channels (png_ptr, info_ptr);
    pitch = png_get_rowbytes (png_ptr, info_ptr);

    /* TODO: check info_width == origWidth && info_height == origHeight */
#if YMAGINE_DEBUG_PNG
    ALOGD("info: %dx%d, orig: %dx%d",
          info_width, info_height, origWidth, origHeight);
#endif

    /* Create transformer */
    transformer = TransformerCreate();
    if (transformer != NULL) {
      TransformerSetWriter(transformer, PngWriter, NULL);
      TransformerSetMode(transformer, VBITMAP_COLOR_RGBA, VbitmapColormode(vbitmap));
      TransformerSetScale(transformer, info_width, info_height, destrect.width, destrect.height);
      TransformerSetRegion(transformer,
                           srcrect.x, srcrect.y, srcrect.width, srcrect.height);

      TransformerSetBitmap(transformer, vbitmap, destrect.x, destrect.y);
      TransformerSetSharpen(transformer, sharpen);

      if (passes == 1) {
        int ypos;
        /* If image isn't interlaced, can read it without pre-allocating
           buffer to contain whole image */
        data = (unsigned char*) Ymem_malloc(pitch);
        if (data != NULL) {
          for (ypos = 0; ypos < info_height; ypos++) {
            png_read_row(png_ptr, data, NULL);
            if (TransformerPush(transformer, (const char*) data) != YMAGINE_OK) {
              ymagine_png_error(png_ptr, "push failed");
            }
          }
          png_read_end(png_ptr, NULL);
          rc = YMAGINE_OK;
        }
      } else {
        int pass;
        int ypos;

        /* Else, create a temporary buffer to handle image content */
        data=(unsigned char*) Ymem_malloc(info_height*pitch);
        if (data != NULL) {
          png_data = (unsigned char **) Ymem_malloc(info_height*sizeof(char*));
          if (png_data != NULL) {
            for (ypos=0;ypos<info_height;ypos++) {
              png_data[ypos] = (unsigned char *) (data+ypos*pitch);
            }

            for (pass = 0; pass < passes; pass++) {
              for (ypos = 0; ypos < info_height; ypos++) {
                png_read_row(png_ptr, png_data[ypos], NULL);
                if (pass == passes - 1) {
                  /* Last pass, row is completed, push content */
                  if (TransformerPush(transformer, (const char*) (data + ypos * pitch)) != YMAGINE_OK) {
                    ymagine_png_error(png_ptr, "push failed");
                  }
                }
              }
            }
            png_read_end(png_ptr, NULL);
            rc = YMAGINE_OK;
          }
        }
      }
    }
  }

  /* Clean up */
  if (png_data != NULL) {
    Ymem_free(png_data);
    png_data = NULL;
  }
  if (data != NULL) {
    Ymem_free(data);
    data = NULL;
  }
  if (transformer != NULL) {
    TransformerRelease(transformer);
  }
  if (png_ptr != NULL) {
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
  }
  if (input) {
    Ymem_free((char*) input);
  }
  if (!pSrc->isdirect) {
    Ymem_free(pSrc->outbuffer);
  }

  /* Return number of decoded lines */
  if (rc == YMAGINE_OK) {
    return origHeight;
  }

  return 0;
}

#endif /* HAVE_PNG */

int
decodePNG(Ychannel *channel, Vbitmap *vbitmap,
          YmagineFormatOptions *options)
{
  int nlines = -1;
#if HAVE_PNG
  PNGDec  pngdec;
#endif
  
  if (!YchannelReadable(channel)) {
#if YMAGINE_DEBUG_PNG
    ALOGD("input channel not readable");
#endif
    return nlines;
  }

#if HAVE_PNG
  if (PNGInit(&pngdec, channel, vbitmap) == YMAGINE_OK) {
    nlines = PNGDecode(&pngdec, vbitmap, options);
    PNGFini(&pngdec);
  }
#endif

  return nlines;
}

int
encodePNG(Vbitmap *vbitmap, Ychannel *channelout, YmagineFormatOptions *options)
{
  int rc = YMAGINE_ERROR;
  char **tags = NULL;
  int pass, number_passes, color_type;  
  int ypos;
  const unsigned char *line;
  const unsigned char *pixels;
  int width;
  int height;
  int pitch;
  int bpp;
  png_textp text = NULL;
  cleanup_info cleanup;
  png_structp png_ptr;
  png_infop info_ptr;
  
  cleanup.data = (char **) NULL;

  png_ptr= png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                   (png_voidp) &cleanup,
                                   ymagine_png_error,ymagine_png_warning);
  if (!png_ptr) {
    return YMAGINE_ERROR;
  }

  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr,NULL);
    return YMAGINE_ERROR;
  }

  /* Define custom */
  png_set_write_fn(png_ptr,
                   (png_voidp) channelout,
                   ymagine_png_write, (png_voidp) NULL);


  rc = VbitmapLock(vbitmap);
  if (rc != YMAGINE_OK) {
    ALOGE("AndroidBitmap_lockPixels() failed");
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return YMAGINE_ERROR;
  }

  if (setjmp(*(jmp_buf *)png_ptr)) {
    VbitmapUnlock(vbitmap);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return YMAGINE_ERROR;
  }

  rc = YMAGINE_ERROR;

  width = VbitmapWidth(vbitmap);
  height = VbitmapHeight(vbitmap);
  pitch = VbitmapPitch(vbitmap);
  bpp = VbitmapBpp(vbitmap);
  pixels = VbitmapBuffer(vbitmap);

  if (bpp == 1) {
    color_type = PNG_COLOR_TYPE_GRAY;
    bpp = 1;
  }
  else {
    color_type = PNG_COLOR_TYPE_RGB;
    if (bpp == 4) {
      color_type |= PNG_COLOR_MASK_ALPHA;
    }
  }

  png_set_IHDR(png_ptr, info_ptr, width, height, 8, color_type, 
               PNG_INTERLACE_ADAM7, PNG_COMPRESSION_TYPE_BASE,
               PNG_FILTER_TYPE_BASE);
  
  png_write_info(png_ptr, info_ptr);

  number_passes = png_set_interlace_handling(png_ptr);
  for (pass = 0; pass < number_passes; pass++) {
    line = pixels;
    for (ypos = 0; ypos < height; ypos++) {
      png_write_row(png_ptr, line);
      line += pitch;
    }
  }
  png_write_end(png_ptr,NULL);
  rc = YMAGINE_OK;

  VbitmapUnlock(vbitmap);
  png_destroy_write_struct(&png_ptr,&info_ptr);
    
  if (text) {
    Ymem_free((char *) text);
  }
  if (tags) {
    Ymem_free((char *) tags);
  }
  
  return rc;
}

int
matchPNG(Ychannel *channel)
{
  char header[PNG_HEADER_SIZE];
  int hlen;
  int width = 0;
  int height = 0;

  if (!YchannelReadable(channel)) {
    return YFALSE;
  }

  hlen = YchannelRead(channel, header, sizeof(header));
  if (hlen > 0) {
    YchannelPush(channel, header, hlen);
  }

  if (PNGCheckHeader(header, hlen, &width, &height) <= 0) {
    return YFALSE;
  }

  return YTRUE;
}
