/**
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

#define LOG_TAG "ymagine::bitmap"
#include "ymagine_priv.h"

#include "graphics/bitmap.h"

static const char* scaleModes[] = {
  "letterbox",
  "crop",
  "fit",
  "none",
  "halfquick",
  "halfaverage",
  "?"
};

const char*
Ymagine_scaleModeStr(int scalemode)
{
  switch (scalemode) {
  case YMAGINE_SCALE_LETTERBOX:
    return scaleModes[0];
  case YMAGINE_SCALE_CROP:
    return scaleModes[1];
  case YMAGINE_SCALE_FIT:
    return scaleModes[2];
  case YMAGINE_SCALE_NONE:
    return scaleModes[3];
  case YMAGINE_SCALE_HALF_QUICK:
    return scaleModes[4];
  case YMAGINE_SCALE_HALF_AVERAGE:
    return scaleModes[5];
  }

  return scaleModes[6];
}

YmagineFormatOptions*
YmagineFormatOptions_Create()
{
  YmagineFormatOptions *options;
  
  options = Ymem_malloc(sizeof(YmagineFormatOptions));
  if (options == NULL) {
    return NULL;
  }

  YmagineFormatOptions_Reset(options);

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_Duplicate(YmagineFormatOptions* refopts)
{
  YmagineFormatOptions *options;
  
  options = Ymem_malloc(sizeof(YmagineFormatOptions));
  if (options == NULL) {
    return NULL;
  }

  YmagineFormatOptions_Reset(options);
  if (refopts != NULL) {
    memcpy(options, refopts, sizeof(YmagineFormatOptions));
  }

  return options;
}

int
YmagineFormatOptions_Release(YmagineFormatOptions *options)
{
  if (options != NULL) {
    Ymem_free(options);
  }
  
  return YMAGINE_OK;
}

YmagineFormatOptions*
YmagineFormatOptions_Reset(YmagineFormatOptions *options)
{
  if (options == NULL) {
    return NULL;
  }

  options->cropoffsetmode = CROP_MODE_NONE;
  options->cropx = 0;
  options->cropy = 0;
  options->cropxp = 0.0f;
  options->cropyp = 0.0f;

  options->cropsizemode = CROP_MODE_NONE;
  options->cropwidth = 0;
  options->cropheight = 0;
  options->cropwidthp = 0.0f;
  options->cropheightp = 0.0f;
  options->maxwidth = -1;
  options->maxheight = -1;
  options->scalemode = YMAGINE_SCALE_CROP;
  options->resizable = 1;
  options->quality = -1;
  options->accuracy = -1;
  options->subsampling = -1;
  options->sharpen = 0.0f;
  options->blur = 0.0f;
  options->rotate = 0.0f;
  options->format = YMAGINE_IMAGEFORMAT_UNKNOWN;
  options->metamode = YMAGINE_METAMODE_DEFAULT;
  options->pixelshader = NULL;
  options->backgroundcolor = YcolorRGBA(0, 0, 0, 0);
  options->metadata = NULL;
  options->progresscb = NULL;

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setQuality(YmagineFormatOptions *options,
                                int quality)
{
  if (options == NULL) {
    return NULL;
  }

  options->quality = quality;

  return options;
}

int
YmagineFormatOptions_normalizeQuality(YmagineFormatOptions *options)
{
  if (options == NULL) {
    return YMAGINE_DEFAULT_QUALITY;
  }

  if (options->quality < 0) {
    return YMAGINE_DEFAULT_QUALITY;
  } else if (options->quality > 100) {
    return 100;
  } else {
    return options->quality;
  }
}

YmagineFormatOptions*
YmagineFormatOptions_setAccuracy(YmagineFormatOptions *options,
                                 int accuracy)
{
  if (options == NULL) {
    return NULL;
  }

  options->accuracy = accuracy;

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setSubsampling(YmagineFormatOptions *options,
                                    int subsampling)
{
  if (options == NULL) {
    return NULL;
  }

  options->subsampling = subsampling;

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setSharpen(YmagineFormatOptions *options,
                                float sigma)
{
  if (options == NULL) {
    return NULL;
  }

  options->sharpen = sigma;

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setBlur(YmagineFormatOptions *options,
                             float radius)
{
  if (options == NULL) {
    return NULL;
  }

  options->blur = radius;

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setRotate(YmagineFormatOptions *options,
                               float angle)
{
  if (options == NULL) {
    return NULL;
  }

  options->rotate = angle;

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setResize(YmagineFormatOptions *options,
                               int maxWidth, int maxHeight, int scaleMode)
{
  if (options == NULL) {
    return NULL;
  }

  options->maxwidth = maxWidth;
  options->maxheight = maxHeight;
  options->scalemode = scaleMode;

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setResizable(YmagineFormatOptions *options,
                                  int resizable)
{
  if (options == NULL) {
    return NULL;
  }

  options->resizable = resizable;

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setShader(YmagineFormatOptions *options,
                               PixelShader *shader)
{
  if (options == NULL) {
    return NULL;
  }

  options->pixelshader = shader;

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setFormat(YmagineFormatOptions *options, int format)
{
  if (options == NULL) {
    return NULL;
  }

  switch(format) {
  case YMAGINE_IMAGEFORMAT_JPEG:
  case YMAGINE_IMAGEFORMAT_WEBP:
  case YMAGINE_IMAGEFORMAT_GIF:
  case YMAGINE_IMAGEFORMAT_PNG:
    options->format = format;
    break;
  case YMAGINE_IMAGEFORMAT_UNKNOWN:
  default:
    options->format = YMAGINE_IMAGEFORMAT_UNKNOWN;
    break;
  }

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setCropOffset(YmagineFormatOptions *options,
                                   int x, int y)
{
  options->cropx = x;
  options->cropy = y;
  options->cropoffsetmode = CROP_MODE_ABSOLUTE;

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setCropSize(YmagineFormatOptions *options,
                                 int width, int height)
{
  options->cropwidth = width;
  options->cropheight = height;
  options->cropsizemode = CROP_MODE_ABSOLUTE;

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setCrop(YmagineFormatOptions *options,
                             int x, int y, int width, int height)
{
  if (options == NULL) {
    return NULL;
  }
  
  YmagineFormatOptions_setCropOffset(options, x, y);
  YmagineFormatOptions_setCropSize(options, width, height);

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setCropOffsetRelative(YmagineFormatOptions *options,
                                           float x, float y)
{
  options->cropxp = x;
  options->cropyp = y;
  options->cropoffsetmode = CROP_MODE_RELATIVE;

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setCropSizeRelative(YmagineFormatOptions *options,
                                         float width, float height)
{
  options->cropwidthp = width;
  options->cropheightp = height;
  options->cropsizemode = CROP_MODE_RELATIVE;

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setCropRelative(YmagineFormatOptions *options,
                                     float x, float y,
                                     float width, float height)
{
  if (options == NULL) {
    return NULL;
  }
  
  YmagineFormatOptions_setCropOffsetRelative(options, x, y);
  YmagineFormatOptions_setCropSizeRelative(options, width, height);

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setMetaMode(YmagineFormatOptions *options,
                                 int metamode)
{
  if (options == NULL) {
    return NULL;
  }

  switch(metamode) {
  case YMAGINE_METAMODE_NONE:
  case YMAGINE_METAMODE_COMMENTS:
  case YMAGINE_METAMODE_ALL:
    options->metamode = metamode;
    break;
  case YMAGINE_METAMODE_DEFAULT:
  default:
    options->metamode = YMAGINE_METAMODE_DEFAULT;
    break;
  }

  return options;
}

YmagineFormatOptions*
YmagineFormatOptions_setData(YmagineFormatOptions *options,
                             void *data)
{
  if (options == NULL) {
    return NULL;
  }

  options->metadata = data;

  return options;
}

void*
YmagineFormatOptions_getData(YmagineFormatOptions *options)
{
  if (options == NULL) {
    return NULL;
  }

  return options->metadata;
}

YmagineFormatOptions*
YmagineFormatOptions_setCallback(YmagineFormatOptions *options,
                                 YmagineFormatOptions_ProgressCB progresscb)
{
  if (options == NULL) {
    return NULL;
  }

  options->progresscb = progresscb;

  return options;
}

int
YmagineFormatOptions_invokeCallback(YmagineFormatOptions *options,
                                    int width, int height, int format)
{
  int rc = YMAGINE_OK;

  if (options != NULL) {
    if (options->progresscb != NULL) {
      rc = options->progresscb(options, width, height, format);
    }
  }

  return rc;
}

YmagineFormatOptions*
YmagineFormatOptions_setBackgroundColor(YmagineFormatOptions *options,
                                        int color)
{
  if (options == NULL) {
    return NULL;
  }

  options->backgroundcolor = (uint32_t) color;

  return options;
}

int
YmagineFormat(Ychannel *channel)
{
  /* Find format of input */
  if (matchJPEG(channel)) {
    /* JPEG */
    return YMAGINE_IMAGEFORMAT_JPEG;
  } else if (matchWEBP(channel)) {
    /* WEBP */
    return YMAGINE_IMAGEFORMAT_WEBP;
  } else if (matchGIF(channel)) {
    /* GIF */
    return YMAGINE_IMAGEFORMAT_GIF;
  } else if (matchPNG(channel)) {
    /* PNG */
    return YMAGINE_IMAGEFORMAT_PNG;
  }

  /* Failed to detect a supported image format */
  return YMAGINE_IMAGEFORMAT_UNKNOWN;
}

int
YmagineDecode(Vbitmap *bitmap, Ychannel *channel,
              YmagineFormatOptions *options)
{
  int rc = YMAGINE_ERROR;
  int nlines;
  int default_options = 0;
  Vbitmap *decodebitmap;
  YmagineFormatOptions *decodeoptions;

  if (!YchannelReadable(channel)) {
    return YMAGINE_ERROR;
  }

  if (options == NULL) {
    options = YmagineFormatOptions_Create();
    if (options == NULL) {
      return YMAGINE_ERROR;
    }
    default_options = 1;
  }

  if (options->rotate != 0.0f) {
    decodeoptions = YmagineFormatOptions_Duplicate(options);
    decodebitmap = VbitmapInitMemory(VbitmapColormode(bitmap));

    if (decodeoptions == NULL || decodebitmap == NULL) {
      if (decodeoptions != NULL) {
        YmagineFormatOptions_Release(decodeoptions);
        decodeoptions = NULL;
      }

      if (decodebitmap != NULL) {
        VbitmapRelease(decodebitmap);
        decodebitmap = NULL;
      }

      if (default_options) {
        YmagineFormatOptions_Release(options);
        options = NULL;
      }

      return YMAGINE_ERROR;
    }

    decodeoptions->cropoffsetmode = CROP_MODE_NONE;
    decodeoptions->cropsizemode = CROP_MODE_NONE;
  } else {
    decodeoptions = options;
    decodebitmap = bitmap;
  }

  /* Find format of input */
  if (matchJPEG(channel)) {
    /* JPEG */
    nlines = decodeJPEG(channel, decodebitmap, decodeoptions);
  } else if (matchWEBP(channel)) {
    /* WEBP */
    nlines = decodeWEBP(channel, decodebitmap, decodeoptions);
  } else if (matchGIF(channel)) {
    /* GIF */
    nlines = decodeGIF(channel, decodebitmap, decodeoptions);
  } else if (matchPNG(channel)) {
    /* PNG */
    nlines = decodePNG(channel, decodebitmap, decodeoptions);
  } else {
    nlines = -1;
  }

  if (decodeoptions != options) {
    YmagineFormatOptions_Release(decodeoptions);
    decodeoptions = NULL;
  }

  if (nlines > 0) {
    if (options->rotate != 0.0f) {
      int width;
      int height;

      width = VbitmapWidth(decodebitmap);
      height = VbitmapHeight(decodebitmap);

      if (width > 0 && height > 0) {
        int centerx;
        int centery;
        Vrect croprect;

        computeCropRect(&croprect, options, width, height);
        centerx = croprect.x + (croprect.width / 2);
        centery = croprect.y + (croprect.height / 2);

        VbitmapResize(bitmap, croprect.width, croprect.height);
        rc = Ymagine_rotate(decodebitmap, bitmap, centerx, centery, options->rotate);
      }
    } else {
      rc = YMAGINE_OK;
    }
  }

  if (decodebitmap != bitmap) {
    VbitmapRelease(decodebitmap);
    decodebitmap = NULL;
  }

  if (rc == YMAGINE_OK && options->blur > 1.0) {
    Ymagine_blur(bitmap, (int) options->blur);
  }

  if (default_options) {
    YmagineFormatOptions_Release(options);
    options = NULL;
  }

  return rc;
}

int
YmagineDecodeResize(Vbitmap *bitmap, Ychannel *channel,
                    int maxWidth, int maxHeight, int scaleMode)
{
  YmagineFormatOptions *options;
  int rc = YMAGINE_ERROR;
  
  options = YmagineFormatOptions_Create();
  if (options != NULL) {
    YmagineFormatOptions_setResize(options, maxWidth, maxHeight, scaleMode);
    YmagineFormatOptions_setResizable(options, 1);
    rc = YmagineDecode(bitmap, channel, options);
    YmagineFormatOptions_Release(options);
  }

  return rc;
}

int
YmagineDecodeInPlace(Vbitmap *bitmap, Ychannel *channel,
                     int maxWidth, int maxHeight, int scaleMode)
{
  YmagineFormatOptions *options;
  int rc = YMAGINE_ERROR;

  options = YmagineFormatOptions_Create();
  if (options != NULL) {
    YmagineFormatOptions_setResize(options, maxWidth, maxHeight, scaleMode);
    YmagineFormatOptions_setResizable(options, 0);
    rc = YmagineDecode(bitmap, channel, options);
    YmagineFormatOptions_Release(options);
  }

  return rc;
}

int
YmagineTranscode(Ychannel *channelin, Ychannel *channelout,
                 YmagineFormatOptions *options)
{
  int rc = YMAGINE_ERROR;
  int iformat;
  Vbitmap* vbitmap;
  float rotate = 0.0f;
  float blur = 0.0f;

  if (channelin == NULL || channelout == NULL) {
    return rc;
  }

  iformat = YmagineFormat(channelin);

  if (iformat == YMAGINE_IMAGEFORMAT_UNKNOWN) {
    return rc;
  }

  if (options != NULL) {
    rotate = options->rotate;
    blur = options->blur;
  }

  if ( ( rotate == 0.0f ) &&
       ( blur == 0.0f ) &&
       ( iformat == YMAGINE_IMAGEFORMAT_JPEG ) &&
       ( options->format == YMAGINE_IMAGEFORMAT_JPEG ||
         options->format == YMAGINE_IMAGEFORMAT_UNKNOWN ) ) {
    /* Transcode JPEG into JPEG using optimized code path */
    rc = transcodeJPEG(channelin, channelout, options);
  } else {
    /* Decode from any supported format */
    vbitmap = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
    rc = YmagineDecode(vbitmap, channelin, options);

    if (rc == YMAGINE_OK) {
      rc = YmagineEncode(vbitmap, channelout, options);
    }

    VbitmapRelease(vbitmap);
    vbitmap = NULL;
  }

  return rc;
}

int
YmagineEncode(Vbitmap *bitmap, Ychannel *channel,
              YmagineFormatOptions *options)
{
  int format = YMAGINE_IMAGEFORMAT_UNKNOWN;
  int rc = YMAGINE_ERROR;
  YBOOL defaultoptions = YFALSE;

  if (options == NULL) {
    options = YmagineFormatOptions_Create();

    if (options == NULL) {
      return rc;
    } else {
      defaultoptions = YTRUE;
    }
  }

  format = options->format;

  if (format == YMAGINE_IMAGEFORMAT_UNKNOWN) {
    format = YMAGINE_IMAGEFORMAT_JPEG;
  }

  switch(format) {
  case YMAGINE_IMAGEFORMAT_JPEG:
    rc = encodeJPEG(bitmap, channel, options);
    break;
  case YMAGINE_IMAGEFORMAT_WEBP:
    rc = encodeWEBP(bitmap, channel, options);
    break;
  case YMAGINE_IMAGEFORMAT_PNG:
    rc = encodePNG(bitmap, channel, options);
    break;
  default:
    rc = YMAGINE_ERROR;
    break;
  }

  if (defaultoptions) {
    YmagineFormatOptions_Release(options);
    options = NULL;
  }

  return rc;
}
