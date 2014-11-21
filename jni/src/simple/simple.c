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

#define LOG_TAG "ymagine::simple"

#include "ymagine/ymagine.h"

#include "ymagine_priv.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

int
YmagineSNI_Transcode(const char *infile,
                     const char *outfile, int oformat,
                     int maxwidth, int maxheight, int scalemode,
                     int quality, int isharpen, int subsample,
                     int irotate, int meta)
{
  int force = 1;
  int fmode;
  Ychannel *channelin;
  Ychannel *channelout;
  int fdin;
  int fdout;
  int closein = 0;
  float sharpen = 0.0f;
  int accuracy = -1;
  int iformat = YMAGINE_IMAGEFORMAT_UNKNOWN;
  YmagineFormatOptions *options = NULL;
  /* metamode can be one of YMAGINE_METAMODE_ALL, YMAGINE_METAMODE_COMMENTS,
     YMAGINE_METAMODE_NONE or YMAGINE_METAMODE_DEFAULT */
  int metamode = YMAGINE_METAMODE_DEFAULT;
  PixelShader *shader = NULL;
  int rc = YMAGINE_ERROR;
  
  if (isharpen <= 0) {
    sharpen = 0.0f;
  } else if (isharpen >= 100) {
    sharpen = 1.0f;
  } else {
    sharpen = 0.01f * ((float) isharpen);
  }

  ALOGV("in=%s out=%s format=%d w=%d h=%d scalemode=%d q=%d sharpen=%d subsample=%d\n",
        infile, outfile, oformat, maxwidth, maxheight, scalemode,
        quality, isharpen, subsample);

  fdin = open(infile, O_RDONLY | O_BINARY);
  if (fdin < 0) {
    ALOGE("failed to open input file \"%s\"\n", infile);
    return rc;
  }
  closein = 1;

  fmode = O_WRONLY | O_CREAT | O_BINARY;
  if (force) {
    /* Truncate file if it already exisst */
    fmode |= O_TRUNC;
  } else {
    /* Fail if file already exists */
    fmode |= O_EXCL;
  }

  fdout = open(outfile, fmode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fdout < 0) {
    if (closein) {
      close(fdin);
    }
    ALOGE("failed to open output file \"%s\"\n", infile);
    return rc;
  }

  channelin = YchannelInitFd(fdin, 0);

  if (channelin != NULL) {
    iformat = YmagineFormat(channelin);
    if (oformat == YMAGINE_IMAGEFORMAT_UNKNOWN) {
      oformat = iformat;
    }

    channelout = YchannelInitFd(fdout, 1);
    if (channelout != NULL) {
      options = YmagineFormatOptions_Create();
      if (options != NULL) {
        YmagineFormatOptions_setFormat(options, oformat);
        YmagineFormatOptions_setResize(options, maxwidth, maxheight, scalemode);
        YmagineFormatOptions_setShader(options, shader);
        YmagineFormatOptions_setQuality(options, quality);
        YmagineFormatOptions_setAccuracy(options, accuracy);
        YmagineFormatOptions_setMetaMode(options, metamode);
        if (subsample >= 0) {
          YmagineFormatOptions_setSubsampling(options, subsample);
        }
        if (quality >= 0) {
          YmagineFormatOptions_setQuality(options, quality);
        }
        if (sharpen > 0.0f) {
          YmagineFormatOptions_setSharpen(options, sharpen);
        }
        if (irotate != 0) {
          YmagineFormatOptions_setRotate(options, (float) irotate);
        }
        if (meta == 0) {
          YmagineFormatOptions_setMetaMode(options, YMAGINE_METAMODE_NONE);
        } else if (meta == 1) {
          YmagineFormatOptions_setMetaMode(options, YMAGINE_METAMODE_COMMENTS);
        } else if (meta >= 2) {
          YmagineFormatOptions_setMetaMode(options, YMAGINE_METAMODE_ALL);
        } else {
          YmagineFormatOptions_setMetaMode(options, YMAGINE_METAMODE_DEFAULT);
        }

        rc = YmagineTranscode(channelin, channelout, options);

        YmagineFormatOptions_Release(options);
      }
      YchannelRelease(channelout);
    }
    YchannelRelease(channelin);
  }

  return rc;
}

int
YmagineSNI_Decode(Vbitmap *bitmap, const char *data, int datalen,
                  int maxwidth, int maxheight, int scalemode)
{
  int rc = YMAGINE_ERROR;
  if ((data != NULL) && (datalen > 0)) {
    Ychannel *channel = YchannelInitByteArray(data, datalen);
    if (channel != NULL) {
      rc = YmagineDecodeResize(bitmap, channel, maxwidth, maxheight, scalemode);
      YchannelResetBuffer(channel); // Don't let Ychannel free the buffer.
      YchannelRelease(channel);
    }
  }
  return rc;
}
