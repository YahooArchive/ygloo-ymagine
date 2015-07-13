/**
 * Copyright 2013-2015 Yahoo! Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may
 * obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License. See accompanying LICENSE file.
 */

#define LOG_TAG "ymagine::orb"

#include "ymagine/ymagine.h"
#include "ymagine_priv.h"

#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* Define to 1 to enable profiling, even on release build */
#define YMAGINE_PROFILE 0

#ifdef YMAGINE_PROFILE
#define HAVE_TIMER
#ifdef HAVE_TIMER
/* #define NSTIME() Ytime(SYSTEM_TIME_THREAD) */
#define NSTYPE nsecs_t
#define NSTIME() ((NSTYPE) Ytime(YTIME_CLOCK_REALTIME))
#else
#define NSTYPE uint64_t
#define NSTIME() ((NSTYPE) 0)
#endif
#endif

/* Orb definition as Bezier cubic closed path */

#include "orb.h"

static YINLINE int
cell(int sz, int num, int total)
{
  return ((sz * num) / total);
}

int VbitmapOrbLoad(Vbitmap *canvas, int sz)
{
  int rc = YMAGINE_ERROR;

  Ychannel *channelin;

  if (canvas != NULL) {
    channelin = YchannelInitByteArray((const char*) ORB_png, ORB_png_len);
    if (channelin != NULL) {
      YmagineFormatOptions *options = YmagineFormatOptions_Create();
      if (options != NULL) {
        if (sz > 0) {
          int maxWidth = sz;
          int maxHeight = sz;
          int scaleMode = YMAGINE_SCALE_CROP;

          YmagineFormatOptions_setResize(options, maxWidth, maxHeight, scaleMode);
        }
        if (YmagineDecode(canvas, channelin, options) == YMAGINE_OK) {
          rc = YMAGINE_OK;
        }
        YmagineFormatOptions_Release(options);
        options = NULL;
      }
    }

    YchannelResetBuffer(channelin);
    YchannelRelease(channelin);
    channelin = NULL;
  }

  return rc;
}

static int
tileRender(Vbitmap *canvas, int ntiles, int tileid, Ychannel* channelin, Vbitmap *srcbitmap)
{
  int rc;
  int maxWidth = -1;
  int maxHeight = -1;
  /* scaleMode can be YMAGINE_SCALE_CROP or YMAGINE_SCALE_LETTERBOX */
  int scaleMode = YMAGINE_SCALE_CROP;
  /* Fit mode */
  int adjustMode = YMAGINE_ADJUST_NONE;
  /* metaMode can be one of YMAGINE_METAMODE_ALL, YMAGINE_METAMODE_COMMENTS,
     YMAGINE_METAMODE_NONE or YMAGINE_METAMODE_DEFAULT */
  int metaMode = YMAGINE_METAMODE_DEFAULT;
  int quality = -1;
  int accuracy = -1;
  int subsampling = -1;
  int progressive = -1;
  float sharpen = 0.0f;
  float blur = 0.0f;
  float rotate = 0.0f;
#if YMAGINE_PROFILE
  int profile = 0;
  NSTYPE start = 0;
  NSTYPE end = 0;
  NSTYPE start_decode = 0;
  NSTYPE end_decode = 0;
#endif
  YmagineFormatOptions *options = NULL;
  Vbitmap *vbitmap = NULL;
  int canvasw;
  int canvash;
  int tilex;
  int tiley;
  int tilew;
  int tileh;

#if YMAGINE_PROFILE
  if (profile) {
    start = NSTIME();
  }
#endif

  canvasw = VbitmapWidth(canvas);
  canvash = VbitmapHeight(canvas);

  if (ntiles > 4) {
    ntiles = 4;
  }

  if (tileid < 0 || tileid >= ntiles) {
    return YMAGINE_OK;
  }

  if (ntiles == 1) {
    tilex = 0;
    tiley = 0;
    tilew = canvasw;
    tileh = canvash;
  } else if (ntiles == 2) {
    tilex = cell(canvasw, tileid % 2, 2);
    tiley = cell(canvash, tileid / 2, 1);
    tilew = cell(canvasw, (tileid % 2) + 1, 2) - tilex;
    tileh = cell(canvash, (tileid / 2) + 1, 1) - tiley;
  } else if (ntiles == 3) {
    tilex = cell(canvasw, tileid % 2, 2);
    tiley = cell(canvash, tileid / 2, 2);
    tilew = cell(canvasw, (tileid % 2) + 1, 2) - tilex;
    if (tileid == 0) {
      tilex = 0;
      tiley = 0;
      tilew = cell(canvasw, 1, 2) - tilex;
      tileh = canvash;
    } else {
      tilex = cell(canvasw, 1, 2);
      tiley = cell(canvash, (tileid - 1) % 2, 2);
      tilew = cell(canvasw, 2, 2) - tilex;
      tileh = cell(canvash, ((tileid - 1) % 2) + 1, 2) - tiley;
    }
  } else {
    tilex = cell(canvasw, tileid % 2, 2);
    tiley = cell(canvash, tileid / 2, 2);
    tilew = cell(canvasw, (tileid % 2) + 1, 2) - tilex;
    tileh = cell(canvash, (tileid / 2) + 1, 2) - tiley;
  }

  maxWidth = tilew;
  maxHeight = tileh;

  rc = YMAGINE_ERROR;

  options = YmagineFormatOptions_Create();
  if (options != NULL) {
    YmagineFormatOptions_setResize(options, maxWidth, maxHeight, scaleMode);
    YmagineFormatOptions_setQuality(options, quality);
    YmagineFormatOptions_setAccuracy(options, accuracy);
    YmagineFormatOptions_setMetaMode(options, metaMode);
    if (subsampling >= 0) {
      YmagineFormatOptions_setSubsampling(options, subsampling);
    }
    if (progressive >= 0) {
      YmagineFormatOptions_setProgressive(options, progressive);
    }
    if (sharpen > 0.0f) {
      YmagineFormatOptions_setSharpen(options, sharpen);
    }
    if (blur > 0.0f) {
      YmagineFormatOptions_setBlur(options, blur);
    }
    if (rotate != 0.0f) {
      YmagineFormatOptions_setRotate(options, rotate);
    }
    YmagineFormatOptions_setAdjust(options, adjustMode);

    vbitmap = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
    if (vbitmap != NULL) {
#if YMAGINE_PROFILE
      start_decode = NSTIME();
#endif
      if (channelin != NULL) {
        rc = YmagineDecode(vbitmap, channelin, options);
      } else if (srcbitmap != NULL) {
        rc = YmagineDecodeCopy(vbitmap, srcbitmap, options);
      } else {
        rc = YMAGINE_ERROR;
      }
#if YMAGINE_PROFILE
      end_decode = NSTIME();
      if (profile) {
        ALOGI("Decoded image %dx%d in %.2f ms\n",
              VbitmapWidth(vbitmap), VbitmapHeight(vbitmap),
              ((double) (end_decode - start_decode)) / 1000000.0);
      }
 #endif

      if (rc == YMAGINE_OK) {
        VbitmapRegionSelect(canvas, tilex, tiley, tilew, tileh);
        Ymagine_composeImage(canvas, vbitmap, 0, 0, YMAGINE_COMPOSE_BUMP);
        VbitmapRegionReset(canvas);

#if YMAGINE_PROFILE
        if (profile) {
          end = NSTIME();
          ALOGI("Rendered orb tile %d/%d in %.2f ms\n",
                tileid, ntiles,
          ((double) (end - start)) / 1000000.0);
        }
#endif
      }
      VbitmapRelease(vbitmap);
    }

    YmagineFormatOptions_Release(options);
    options = NULL;
  }

  return rc;
}

int
VbitmapOrbRenderTile(Vbitmap *canvas, int ntiles, int tileid, Ychannel* channelin)
{
  return tileRender(canvas, ntiles, tileid, channelin, NULL);
}

int
VbitmapOrbRenderTileBitmap(Vbitmap *canvas, int ntiles, int tileid, Vbitmap *srcbitmap)
{
  return tileRender(canvas, ntiles, tileid, NULL, srcbitmap);
}
