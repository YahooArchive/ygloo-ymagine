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

  options->maxwidth = -1;
  options->maxheight = -1;
  options->scalemode = YMAGINE_SCALE_CROP;
  options->quality = -1;
  options->pixelshader = NULL;

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
YmagineFormatOptions_setShader(YmagineFormatOptions *options,
                               PixelShader *shader)
{
  if (options == NULL) {
    return NULL;
  }

  options->pixelshader = shader;

  return options;
}

int
YmagineDecode(Vbitmap *bitmap, Ychannel *channel,
              YmagineFormatOptions *options)
{
  int nlines;
  int maxWidth = -1;
  int maxHeight = -1;
  int scaleMode = YMAGINE_SCALE_CROP;
  int quality = -1;
  PixelShader *shader = NULL;

  if (options != NULL) {
    maxWidth = options->maxwidth;
    maxHeight = options->maxheight;
    scaleMode = options->scalemode;
    quality = options->quality;
    shader = options->pixelshader;
  }

  /* Normalize quality parameter */
  if (quality < 0) {
    quality = 85;
  }
  if (quality > 100) {
    quality = 100;
  }

  if (!YchannelReadable(channel)) {
    return YMAGINE_ERROR;
  }

  /* Find format of input */
  if (matchJPEG(channel)) {
    /* JPEG */
    nlines = decodeJPEG(channel, bitmap,
                        maxWidth, maxHeight, scaleMode,
                        quality, shader);
  } else if (matchWEBP(channel)) {
    /* WEBP */
    nlines = decodeWEBP(channel, bitmap,
                        maxWidth, maxHeight, scaleMode,
                        quality, shader);    
  } else {
    nlines = -1;
  }

  if (nlines <= 0) {
    return YMAGINE_ERROR;
  }

  return YMAGINE_OK;
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
    rc = YmagineDecode(bitmap, channel, options);
    YmagineFormatOptions_Release(options);
  }

  return rc;
}

int
YmagineEncode(Vbitmap *bitmap, Ychannel *channel,
              YmagineFormatOptions *options)
{
  int quality = -1;

  if (options != NULL) {
    quality = options->quality;
  }

  return encodeJPEG(bitmap, channel, quality);
}
