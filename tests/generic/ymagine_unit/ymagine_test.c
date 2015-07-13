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
#include "yosal/yosal.h"
#include "ymagine/ymagine.h"
#if HAVE_PLUGIN_VISION
#include "ymagine/plugins/vision.h"
#endif

#include "ymagine_priv.h"
#include "psnr_html.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* Create a directory, creating all parents as needed */
static int mkDir(const char *name)
{
  char fullpath[PATH_MAX];
  char currpath[PATH_MAX];
  char *pathtoken;
  struct stat st;
  int ndirs = 0;
  int ret;
  int l;

  if (name == NULL || name[0] == '\0') {
    return ndirs;
  }

  l = strlen(name);
  if (l >= PATH_MAX) {
    return -1;
  }

  // reset path
  strcpy(fullpath, name);
  strcpy(currpath, "");
  // create the pieces of the path along the way
  pathtoken = strtok(fullpath, "/");
  if (fullpath[0] == '/') {
    // prepend / if needed
    strcat(currpath, "/");
  }
  while (pathtoken != NULL) {
    if(strlen(currpath) + strlen(pathtoken) + 2/*NUL and slash*/ > PATH_MAX) {
      /* Path is too long */
      return -1;
    }

    strcat(currpath, pathtoken);
    strcat(currpath, "/");
    if (stat(currpath, &st) != 0) {
      ret = mkdir(currpath, 0777);
      if(ret < 0) {
        /* Fail to create directory currpath */
        // fprintf(stderr, "mkdir failed for %s\n", currpath);
        return -1;
      }
      ndirs++;
    }
    pathtoken = strtok(NULL, "/");
  }

  return ndirs;
}

typedef struct {
  int srcwidth;
  int srcheight;
  int destwidth;
  int destheight;
  int scalemode;
  int usecrop;
  Vrect croprect;
  Vrect resultsrc;
  Vrect resultdest;
} ctinfo;

static int isRectEqual(const Vrect* r1, const Vrect* r2) {
  if ((r1->x != r2->x) || (r1->y != r2->y) ||
      (r1->width != r2->width) || (r1->height != r2->height)) {
    return 0;
  } else {
    return 1;
  }
}

static void printRect(const Vrect* r) {
  printf("%dx%d@%d,%d", r->width, r->height, r->x, r->y);
}

typedef struct {
  int linecount;
} twriterdata;

static int
transformerWriter(Transformer *transformer, void *writedata, void *line)
{
  twriterdata* data;
  YTEST_ASSERT_TRUE(transformer != NULL);
  YTEST_ASSERT_TRUE(writedata != NULL);
  YTEST_ASSERT_TRUE(line != NULL);

  data = (twriterdata*)writedata;
  data->linecount++;

  return YMAGINE_OK;
}

static void testTransformer() {
  /* test transformer outputs expected number of lines */
  const int srch = 300;
  const int srcw = 100;
  int bpp;
  int pitch;
  unsigned char *src;
  const int srcx = 10;
  const int srcy = 20;
  const int regionw = srcw - srcx;
  const int regionh = srch - srcy;
  const int destwidth = 1000;
  const int destheightcap = 1000;
  int j;
  int k;
  twriterdata writedata;
  Transformer* transformer;

  return;

  /* Initialize src to avoid warning about uninitialized value */
  bpp = 4;
  pitch = srcw * bpp;
  src = Ymem_malloc(pitch);
  if (src == NULL) {
    printf("error: failed to allocate %d bytes for testTransformer\n", pitch);
    exit(1);
  }
  memset(src, 0, pitch);

  for (k = 1; k <= destheightcap; k++) {
    int destheight = k;
    transformer = TransformerCreate();
    /* Guarantee of TransformerSetScale is that if srch lines are pushed in,
       exactly destheight lines will be pushed out */
    TransformerSetScale(transformer, srcw, srch, destwidth, destheight);
    TransformerSetRegion(transformer, srcx, srcy, regionw, regionh);
    TransformerSetMode(transformer, VBITMAP_COLOR_RGBA, VBITMAP_COLOR_RGBA);
    writedata.linecount = 0;
    TransformerSetWriter(transformer, transformerWriter, &writedata);

    for (j = 0; j < srch; j++) {
      /* push in srch lines */
      YTEST_ASSERT_EQ(TransformerPush(transformer, (const char*) src), YMAGINE_OK);
    }

    /* verify push out destheight lines */
    if (writedata.linecount != destheight) {
      printf("error: expected line count: %d, got line count: %d\n", destheight, writedata.linecount);
      exit(1);
    }
    TransformerRelease(transformer);
    transformer = NULL;
  }

  Ymem_free(src);
}

static void testMergeLine() {
  const int width = 100;
  const int cmode = VBITMAP_COLOR_RGB;
  const int bpp = 3;
  const int destvalue = 100;
  const int srcvalue = 50;
  const int destweight = 2;
  const int srcweight = 1;
  int i;
  int j;
  unsigned char dest[width * bpp];
  unsigned char src[width * bpp];

  memset(dest, destvalue, width * bpp);
  memset(src, srcvalue, width * bpp);

  YTEST_ASSERT_EQ(YmagineMergeLine(dest, cmode, destweight,
                                   src, cmode, srcweight,
                                   width), YMAGINE_OK);
  for (i = 0; i < width; i++) {
    for (j = 0; j < bpp; j++) {
      YTEST_ASSERT_EQ(dest[i*bpp + j], (unsigned char)((destvalue * destweight + srcvalue * srcweight) / (destweight + srcweight)));
    }
  }
}

static void testComputeTransform() {
  ctinfo infos[] = {
    /* invalid values */
    {0, 0, 0, 0, YMAGINE_SCALE_LETTERBOX, 0, {-1, -1, -1, -1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {100, 200, 0, 0, YMAGINE_SCALE_CROP, 0, {-1, -1, -1, -1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {100, 200, 0, 100, YMAGINE_SCALE_FIT, 0, {-1, -1, -1, -1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {100, 200, 200, 100, YMAGINE_SCALE_LETTERBOX, 1, {-1, -1, -1, -1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {100, 200, 200, 100, YMAGINE_SCALE_CROP, 1, {10, 300, 100, 300}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {100, 200, 200, 100, YMAGINE_SCALE_FIT, 1, {300, 10, 100, 300}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {100, 200, 200, 100, YMAGINE_SCALE_LETTERBOX, 1, {0, 0, 0, 300}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {100, 200, 200, 100, YMAGINE_SCALE_CROP, 1, {0, 0, 300, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {100, 200, 200, 100, YMAGINE_SCALE_FIT, 1, {300, 10, -1, 300}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {100, 200, 200, 100, YMAGINE_SCALE_LETTERBOX, 1, {100, 0, 100, 200}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {100, 200, 200, 100, YMAGINE_SCALE_LETTERBOX, 1, {0, 200, 100, 200}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {100, 200, 200, 100, YMAGINE_SCALE_CROP, 1, {0, 0, 300, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {100, 200, 200, 100, YMAGINE_SCALE_FIT, 1, {300, 10, -1, 300}, {0, 0, 0, 0}, {0, 0, 0, 0}},

    /* edge cases */
    {100, 200, 200, 100, YMAGINE_SCALE_LETTERBOX, 1, {99, 0, 10, 20}, {99, 0, 1, 20}, {97, 0, 5, 100}},
    {100, 200, 200, 100, YMAGINE_SCALE_FIT, 1, {99, 0, 10, 20}, {99, 0, 1, 20}, {97, 0, 5, 100}},
    {100, 200, 200, 100, YMAGINE_SCALE_CROP, 1, {99, 0, 10, 20}, {99, 9, 1, 1}, {0, 0, 200, 100}},

    {100, 200, 200, 100, YMAGINE_SCALE_LETTERBOX, 1, {0, 199, 10, 20}, {0, 199, 10, 1}, {0, 40, 200, 20}},
    {100, 200, 200, 100, YMAGINE_SCALE_FIT, 1, {0, 199, 10, 20}, {0, 199, 10, 1}, {0, 40, 200, 20}},
    {100, 200, 200, 100, YMAGINE_SCALE_CROP, 1, {0, 199, 10, 20}, {4, 199, 2, 1}, {0, 0, 200, 100}},

    /* normal cases */
    {100, 200, 200, 100, YMAGINE_SCALE_LETTERBOX, 1, {0, 0, 10, 20}, {0, 0, 10, 20}, {75, 0, 50, 100}},
    {100, 200, 200, 100, YMAGINE_SCALE_FIT, 1, {0, 0, 10, 20}, {0, 0, 10, 20}, {75, 0, 50, 100}},
    {100, 200, 200, 100, YMAGINE_SCALE_CROP, 1, {0, 0, 10, 20}, {0, 7, 10, 5}, {0, 0, 200, 100}},

    {100, 200, 200, 100, YMAGINE_SCALE_LETTERBOX, 1, {0, 100, 100, 1}, {0, 100, 100, 1}, {0, 49, 200, 2}},
    {100, 200, 200, 100, YMAGINE_SCALE_FIT, 1, {0, 100, 100, 1}, {0, 100, 100, 1}, {0, 49, 200, 2}},
    {100, 200, 200, 100, YMAGINE_SCALE_CROP, 1, {0, 100, 100, 1}, {49, 100, 2, 1}, {0, 0, 200, 100}},

    {100, 200, 200, 100, YMAGINE_SCALE_LETTERBOX, 1, {50, 0, 1, 200}, {50, 0, 1, 200}, {99, 0, 1, 100}},
    {100, 200, 200, 100, YMAGINE_SCALE_FIT, 1, {50, 0, 1, 200}, {50, 0, 1, 200}, {99, 0, 1, 100}},
    {100, 200, 200, 100, YMAGINE_SCALE_CROP, 1, {50, 0, 1, 200}, {50, 99, 1, 1}, {0, 0, 200, 100}},

    /* cases that make sure interface definition is precise */
    /* ask 0-th column will result in 0-th column */
    {100, 200, 100, 200, YMAGINE_SCALE_LETTERBOX, 1, {0, 0, 1, 200}, {0, 0, 1, 200}, {49, 0, 1, 200}},
    /* ask 99-th column will result in 99-th column */
    {100, 200, 100, 200, YMAGINE_SCALE_LETTERBOX, 1, {99, 0, 1, 200}, {99, 0, 1, 200}, {49, 0, 1, 200}},
    /* ask first 99 column out of 100 column should get first 99 column */
    {100, 200, 100, 200, YMAGINE_SCALE_LETTERBOX, 1, {0, 0, 99, 200}, {0, 0, 99, 200}, {0, 0, 99, 200}},
    /* ask last 99 column out of 100 column should get last 99 column */
    {100, 200, 100, 200, YMAGINE_SCALE_LETTERBOX, 1, {1, 0, 99, 200}, {1, 0, 99, 200}, {0, 0, 99, 200}},
  };
  int length = sizeof(infos) / sizeof(infos[0]) * 2;
  int j;

  for (j=0; j < length; j++) {
    /* when j%2==1, the transposed input gets tested */
    int i = j/2;
    int tmp;
    Vrect srcrect;
    Vrect destrect;

    if (infos[i].usecrop) {
      computeTransform(infos[i].srcwidth, infos[i].srcheight,
                       &infos[i].croprect,
                       infos[i].destwidth, infos[i].destheight,
                       infos[i].scalemode, &srcrect, &destrect);
    } else {
      computeTransform(infos[i].srcwidth, infos[i].srcheight,
                       NULL,
                       infos[i].destwidth, infos[i].destheight,
                       infos[i].scalemode, &srcrect, &destrect);
    }

    if (!isRectEqual(&srcrect, &infos[i].resultsrc) || !isRectEqual(&destrect, &infos[i].resultdest)) {
      printf("error: %dth testComputeTransform failed.\n", i);
      printf("expected: ");
      printRect(&infos[i].resultsrc);
      printf(" ");
      printRect(&infos[i].resultdest);
      printf("\n");
      printf("result: ");
      printRect(&srcrect);
      printf(" ");
      printRect(&destrect);
      printf("\n");
      exit(1);
      return;
    }

    /* transposed input should get transposed output */
    tmp = infos[i].srcwidth;
    infos[i].srcwidth = infos[i].srcheight;
    infos[i].srcheight = tmp;
    tmp = infos[i].destwidth;
    infos[i].destwidth = infos[i].destheight;
    infos[i].destheight = tmp;

    tmp = infos[i].croprect.x;
    infos[i].croprect.x = infos[i].croprect.y;
    infos[i].croprect.y = tmp;
    tmp = infos[i].croprect.width;
    infos[i].croprect.width = infos[i].croprect.height;
    infos[i].croprect.height = tmp;

    tmp = infos[i].resultsrc.x;
    infos[i].resultsrc.x = infos[i].resultsrc.y;
    infos[i].resultsrc.y = tmp;
    tmp = infos[i].resultsrc.width;
    infos[i].resultsrc.width = infos[i].resultsrc.height;
    infos[i].resultsrc.height = tmp;

    tmp = infos[i].resultdest.x;
    infos[i].resultdest.x = infos[i].resultdest.y;
    infos[i].resultdest.y = tmp;
    tmp = infos[i].resultdest.width;
    infos[i].resultdest.width = infos[i].resultdest.height;
    infos[i].resultdest.height = tmp;
  }
}

typedef struct {
  const char* name;
  const char* inname;
  int oformat;
  int maxwidth;
  int maxheight;
  int scalemode;
  int cropmode;
  Vrect croprect;
} tinfo;

// TODO: expose these helpers
static const char* Ymagine_formatStr(int format) {
  static const char* formats[] = {
    "unknown",
    "jpg",
    "webp",
    "png",
    "gif",
  };

  if (format >= sizeof(formats) / sizeof(formats[0])) {
    format = 0;
  }

  return formats[format];
}

#if 0
static const char* Ymagine_cropModeStr(int cropmode) {
  static const char* cropmodes[] = {
    "none",
    "absolute",
    "relative",
  };

  if (cropmode >= sizeof(cropmodes) / sizeof(cropmodes[0])) {
    return "unknown";
  }

  return cropmodes[cropmode];
}
#endif

static const char* REF_FOLDER = "ref";
static const char* RUN_FOLDER = "run";
static const char* BASEDIR = "./test/ymagine-data/transcode";

static const char* TEMP_FOLDER = "./out/test/ymagine";
static const char* REF_RELATIVE_PATH = "../../..";

#define TR_regionpngname   "base_region.png"
#define TR_regionjpgname   "base_region.jpg"
#define TR_regionwebpname  "base_region.webp"
#define TR_cropjpgname     "base_crop.jpg"        /* 400x400 */
#define TR_jpgname         "base.jpg"             /* 1350x900 */
#define TR_pngname         "base.png"             /* 1473x1854 */
#define TR_webpname        "base.webp"            /* 1024x772 */
#define TR_name            "ref-base"

static const tinfo transform_infos[] = {
  /* crop contract validation for scalemode fit */

  /* crop region wider and shorter */
  {TR_regionpngname, TR_regionpngname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, 50, 250, 50}},
  {TR_regionpngname, TR_regionpngname, YMAGINE_IMAGEFORMAT_PNG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, 50, 250, 50}},
  {TR_regionpngname, TR_regionpngname, YMAGINE_IMAGEFORMAT_WEBP, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, 50, 250, 50}},
  {TR_regionjpgname, TR_regionjpgname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, 50, 250, 50}},
  {TR_regionjpgname, TR_regionjpgname, YMAGINE_IMAGEFORMAT_PNG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, 50, 250, 50}},
  {TR_regionjpgname, TR_regionjpgname, YMAGINE_IMAGEFORMAT_WEBP, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, 50, 250, 50}},
  {TR_regionwebpname, TR_regionwebpname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, 50, 250, 50}},
  {TR_regionwebpname, TR_regionwebpname, YMAGINE_IMAGEFORMAT_PNG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, 50, 250, 50}},
  {TR_regionwebpname, TR_regionwebpname, YMAGINE_IMAGEFORMAT_WEBP, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, 50, 250, 50}},

  /* crop region taller and narrower */
  {TR_regionpngname, TR_regionpngname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, -50, 50, 250}},
  {TR_regionpngname, TR_regionpngname, YMAGINE_IMAGEFORMAT_PNG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, -50, 50, 250}},
  {TR_regionpngname, TR_regionpngname, YMAGINE_IMAGEFORMAT_WEBP, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, -50, 50, 250}},
  {TR_regionjpgname, TR_regionjpgname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, -50, 50, 250}},
  {TR_regionjpgname, TR_regionjpgname, YMAGINE_IMAGEFORMAT_PNG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, -50, 50, 250}},
  {TR_regionjpgname, TR_regionjpgname, YMAGINE_IMAGEFORMAT_WEBP, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, -50, 50, 250}},
  {TR_regionwebpname, TR_regionwebpname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, -50, 50, 250}},
  {TR_regionwebpname, TR_regionwebpname, YMAGINE_IMAGEFORMAT_PNG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, -50, 50, 250}},
  {TR_regionwebpname, TR_regionwebpname, YMAGINE_IMAGEFORMAT_WEBP, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, -50, 50, 250}},

  /* crop region taller and wider */
  {TR_regionpngname, TR_regionpngname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, -50, 250, 250}},
  {TR_regionpngname, TR_regionpngname, YMAGINE_IMAGEFORMAT_PNG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, -50, 250, 250}},
  {TR_regionpngname, TR_regionpngname, YMAGINE_IMAGEFORMAT_WEBP, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, -50, 250, 250}},
  {TR_regionjpgname, TR_regionjpgname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, -50, 250, 250}},
  {TR_regionjpgname, TR_regionjpgname, YMAGINE_IMAGEFORMAT_PNG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, -50, 250, 250}},
  {TR_regionjpgname, TR_regionjpgname, YMAGINE_IMAGEFORMAT_WEBP, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, -50, 250, 250}},
  {TR_regionwebpname, TR_regionwebpname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, -50, 250, 250}},
  {TR_regionwebpname, TR_regionwebpname, YMAGINE_IMAGEFORMAT_PNG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, -50, 250, 250}},
  {TR_regionwebpname, TR_regionwebpname, YMAGINE_IMAGEFORMAT_WEBP, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {-50, -50, 250, 250}},

  /* crop region shorter and narrower */
  {TR_regionpngname, TR_regionpngname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, 50, 50, 50}},
  {TR_regionpngname, TR_regionpngname, YMAGINE_IMAGEFORMAT_PNG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, 50, 50, 50}},
  {TR_regionpngname, TR_regionpngname, YMAGINE_IMAGEFORMAT_WEBP, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, 50, 50, 50}},
  {TR_regionjpgname, TR_regionjpgname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, 50, 50, 50}},
  {TR_regionjpgname, TR_regionjpgname, YMAGINE_IMAGEFORMAT_PNG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, 50, 50, 50}},
  {TR_regionjpgname, TR_regionjpgname, YMAGINE_IMAGEFORMAT_WEBP, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, 50, 50, 50}},
  {TR_regionwebpname, TR_regionwebpname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, 50, 50, 50}},
  {TR_regionwebpname, TR_regionwebpname, YMAGINE_IMAGEFORMAT_PNG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, 50, 50, 50}},
  {TR_regionwebpname, TR_regionwebpname, YMAGINE_IMAGEFORMAT_WEBP, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {50, 50, 50, 50}},

  /* crop, no width height limit */
  {TR_name, TR_cropjpgname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_LETTERBOX, CROP_MODE_ABSOLUTE, {100, 100, 200, 200}},
  {TR_name, TR_cropjpgname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_CROP, CROP_MODE_ABSOLUTE, {100, 100, 200, 200}},
  {TR_name, TR_cropjpgname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {100, 100, 200, 200}},

  /* crop, scale down */
  {TR_name, TR_cropjpgname, YMAGINE_IMAGEFORMAT_JPEG, 100, 100, YMAGINE_SCALE_LETTERBOX, CROP_MODE_ABSOLUTE, {100, 100, 200, 200}},
  {TR_name, TR_cropjpgname, YMAGINE_IMAGEFORMAT_JPEG, 100, 100, YMAGINE_SCALE_CROP, CROP_MODE_ABSOLUTE, {100, 100, 200, 200}},
  {TR_name, TR_cropjpgname, YMAGINE_IMAGEFORMAT_JPEG, 100, 100, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {100, 100, 200, 200}},

  /* crop, no scale */
  {TR_name, TR_cropjpgname, YMAGINE_IMAGEFORMAT_JPEG, 200, 200, YMAGINE_SCALE_LETTERBOX, CROP_MODE_ABSOLUTE, {100, 100, 200, 200}},
  {TR_name, TR_cropjpgname, YMAGINE_IMAGEFORMAT_JPEG, 200, 200, YMAGINE_SCALE_CROP, CROP_MODE_ABSOLUTE, {100, 100, 200, 200}},
  {TR_name, TR_cropjpgname, YMAGINE_IMAGEFORMAT_JPEG, 200, 200, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {100, 100, 200, 200}},

  /* crop, scale up */
  {TR_name, TR_cropjpgname, YMAGINE_IMAGEFORMAT_JPEG, 300, 300, YMAGINE_SCALE_LETTERBOX, CROP_MODE_ABSOLUTE, {100, 100, 200, 200}},
  {TR_name, TR_cropjpgname, YMAGINE_IMAGEFORMAT_JPEG, 300, 300, YMAGINE_SCALE_CROP, CROP_MODE_ABSOLUTE, {100, 100, 200, 200}},
  {TR_name, TR_cropjpgname, YMAGINE_IMAGEFORMAT_JPEG, 300, 300, YMAGINE_SCALE_FIT, CROP_MODE_ABSOLUTE, {100, 100, 200, 200}},

  /* crop, relative no scale */
  {TR_name, TR_cropjpgname, YMAGINE_IMAGEFORMAT_JPEG, 200, 200, YMAGINE_SCALE_LETTERBOX, CROP_MODE_RELATIVE, {25, 25, 50, 50}},
  {TR_name, TR_cropjpgname, YMAGINE_IMAGEFORMAT_JPEG, 200, 200, YMAGINE_SCALE_CROP, CROP_MODE_RELATIVE, {25, 25, 50, 50}},
  {TR_name, TR_cropjpgname, YMAGINE_IMAGEFORMAT_JPEG, 200, 200, YMAGINE_SCALE_FIT, CROP_MODE_RELATIVE, {25, 25, 50, 50}},

  /* trascode, scale down */
  {TR_name, TR_jpgname, YMAGINE_IMAGEFORMAT_JPEG, 600, 600, YMAGINE_SCALE_LETTERBOX, CROP_MODE_NONE, {0, 0, 0, 0}},
  {TR_name, TR_jpgname, YMAGINE_IMAGEFORMAT_JPEG, 600, 600, YMAGINE_SCALE_CROP, CROP_MODE_NONE, {0, 0, 0, 0}},
  {TR_name, TR_jpgname, YMAGINE_IMAGEFORMAT_JPEG, 600, 600, YMAGINE_SCALE_FIT, CROP_MODE_NONE, {0, 0, 0, 0}},

  /* trascode, no scale */
  {TR_name, TR_jpgname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_LETTERBOX, CROP_MODE_NONE, {0, 0, 0, 0}},
  {TR_name, TR_jpgname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_CROP, CROP_MODE_NONE, {0, 0, 0, 0}},
  {TR_name, TR_jpgname, YMAGINE_IMAGEFORMAT_JPEG, -1, -1, YMAGINE_SCALE_FIT, CROP_MODE_NONE, {0, 0, 0, 0}},

  /* trascode, specify width, use default height */
  {TR_name, TR_jpgname, YMAGINE_IMAGEFORMAT_JPEG, 300, -1, YMAGINE_SCALE_LETTERBOX, CROP_MODE_NONE, {0, 0, 0, 0}},
  {TR_name, TR_jpgname, YMAGINE_IMAGEFORMAT_JPEG, 300, -1, YMAGINE_SCALE_CROP, CROP_MODE_NONE, {0, 0, 0, 0}},
  {TR_name, TR_jpgname, YMAGINE_IMAGEFORMAT_JPEG, 300, -1, YMAGINE_SCALE_FIT, CROP_MODE_NONE, {0, 0, 0, 0}},

  /* trascode, scale up */
  {TR_name, TR_jpgname, YMAGINE_IMAGEFORMAT_JPEG, 2000, 2000, YMAGINE_SCALE_LETTERBOX, CROP_MODE_NONE, {0, 0, 0, 0}},
  {TR_name, TR_jpgname, YMAGINE_IMAGEFORMAT_JPEG, 2000, 2000, YMAGINE_SCALE_CROP, CROP_MODE_NONE, {0, 0, 0, 0}},
  {TR_name, TR_jpgname, YMAGINE_IMAGEFORMAT_JPEG, 2000, 2000, YMAGINE_SCALE_FIT, CROP_MODE_NONE, {0, 0, 0, 0}},

  /* decode from jpeg, encode to png, webp (already have many jpeg to jpeg case above) */
  {TR_name, TR_jpgname, YMAGINE_IMAGEFORMAT_PNG, 600, 600, YMAGINE_SCALE_LETTERBOX, CROP_MODE_ABSOLUTE, {100, 150, 600, 600}},
  {TR_name, TR_jpgname, YMAGINE_IMAGEFORMAT_WEBP, 600, 600, YMAGINE_SCALE_LETTERBOX, CROP_MODE_ABSOLUTE, {100, 150, 600, 600}},

  /* decode from png, encode to png, webp, jpeg */
  {TR_name, TR_pngname, YMAGINE_IMAGEFORMAT_PNG, 600, 600, YMAGINE_SCALE_LETTERBOX, CROP_MODE_ABSOLUTE, {100, 150, 600, 600}},
  {TR_name, TR_pngname, YMAGINE_IMAGEFORMAT_WEBP, 600, 600, YMAGINE_SCALE_LETTERBOX, CROP_MODE_ABSOLUTE, {100, 150, 600, 600}},
  {TR_name, TR_pngname, YMAGINE_IMAGEFORMAT_JPEG, 600, 600, YMAGINE_SCALE_LETTERBOX, CROP_MODE_ABSOLUTE, {100, 150, 600, 600}},

  /* decode from webp, encode to png, webp, jpeg */
  {TR_name, TR_webpname, YMAGINE_IMAGEFORMAT_PNG, 600, 600, YMAGINE_SCALE_LETTERBOX, CROP_MODE_ABSOLUTE, {100, 150, 600, 600}},
  {TR_name, TR_webpname, YMAGINE_IMAGEFORMAT_WEBP, 600, 600, YMAGINE_SCALE_LETTERBOX, CROP_MODE_ABSOLUTE, {100, 150, 600, 600}},
  {TR_name, TR_webpname, YMAGINE_IMAGEFORMAT_JPEG, 600, 600, YMAGINE_SCALE_LETTERBOX, CROP_MODE_ABSOLUTE, {100, 150, 600, 600}},
};

static void sprintInfoToCommand(char* buffer, int buffersize, tinfo* info) {
  if (info->cropmode == CROP_MODE_ABSOLUTE) {
    snprintf(buffer, buffersize,
             "transcode -width %d -height %d -format %s -scale %s -crop %dx%d@%d,%d -force",
             info->maxwidth, info->maxheight, Ymagine_formatStr(info->oformat),
             Ymagine_scaleModeStr(info->scalemode),
             info->croprect.width, info->croprect.height,
             info->croprect.x, info->croprect.y);
  } else if (info->cropmode == CROP_MODE_RELATIVE) {
    snprintf(buffer, buffersize,
             "transcode -width %d -height %d -format %s -scale %s -cropr %fx%f@%f,%f -force",
             info->maxwidth, info->maxheight, Ymagine_formatStr(info->oformat),
             Ymagine_scaleModeStr(info->scalemode),
             ((float)info->croprect.width) / 100, ((float)info->croprect.height) / 100,
             ((float)info->croprect.x) / 100, ((float)info->croprect.y) / 100);
  } else {
    snprintf(buffer, buffersize,
             "transcode -width %d -height %d -format %s -scale %s -force",
             info->maxwidth, info->maxheight, Ymagine_formatStr(info->oformat),
             Ymagine_scaleModeStr(info->scalemode));
  }
}

static void testTranscode(YBOOL htmlmode, const char* basefolder,
                          int startindex, int endindex) {
  char basedir[PATH_MAX];
  char outdir[PATH_MAX];
  char rundir[PATH_MAX];
  char refdir[PATH_MAX];
  int i;
  int fdin;
  int fdout;
  int fdpsnr0;
  int fdpsnr1;
  int fhtml = -1;
  Ychannel *channelin;
  Ychannel *channelout;
  Ychannel *channelpsnr0;
  Ychannel *channelpsnr1;
  Ychannel *channelhtml = NULL;
  int failedcount = 0;
  int warningcount = 0;
  int oformat;
  int iformat;
  const char *outformat;
  const char *informat;
  YmagineFormatOptions *options = NULL;
  Vbitmap *vbitmap;
  Vbitmap *vbitmapref;
  tinfo info;
  int rc;
  int ret = 0;
  const char* decodecolormodestr = "RGBA";
  char outpath[200];
  char outname[200];
  char psnr0path[200];
  char psnr1path[200];
  char htmlpath[200];
  char errorstring[200];
  char basepath[200];

  if (basefolder == NULL) {
    snprintf(basedir, sizeof(basedir), "%s", BASEDIR);
    snprintf(refdir, sizeof(refdir), "%s/%s", BASEDIR, REF_FOLDER);
  } else {
    snprintf(basedir, sizeof(basedir), "%s", basefolder);
    snprintf(refdir, sizeof(refdir), "%s/%s", basefolder, REF_FOLDER);
  }
  snprintf(outdir, sizeof(outdir), "%s", TEMP_FOLDER);
  snprintf(rundir, sizeof(rundir), "%s/%s", TEMP_FOLDER, RUN_FOLDER);

  if (mkDir(outdir) < 0) {
    fprintf(stdout, "error: failed to create output directory %s\n", outdir);
    fflush(stdout);
    exit(1);
  }
  if (mkDir(rundir) < 0) {
    fprintf(stdout, "error: failed to create output directory %s\n", rundir);
    fflush(stdout);
    exit(1);
  }

  if (htmlmode) {
    snprintf(htmlpath, sizeof(htmlpath), "%s/%s", outdir, "report.html");
    fhtml = open(htmlpath, O_WRONLY | O_CREAT | O_BINARY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fhtml >= 0) {
      channelhtml = YchannelInitFd(fhtml, 1);
      if (channelhtml != NULL) {
        YmaginePsnrAppendHtmlHead(channelhtml);
      } else {
        fprintf(stdout, "error: failed to open report file %s\n", htmlpath);
        fflush(stdout);
        exit(1);
      }
    }
  }

  for (i = startindex; i < endindex; i++) {
    rc = YMAGINE_ERROR;
    info = transform_infos[i];
    snprintf(basepath, sizeof(basepath), "%s/%s", basedir, info.inname);
    fdin = open(basepath, O_RDONLY | O_BINARY);
    if (fdin >= 0) {
      channelin = YchannelInitFd(fdin, 0);
      if (channelin != NULL) {
        iformat = YmagineFormat(channelin);
        oformat = info.oformat;

        if (oformat == YMAGINE_IMAGEFORMAT_UNKNOWN) {
          oformat = YMAGINE_IMAGEFORMAT_JPEG;
        }

        outformat = Ymagine_formatStr(oformat);
        informat = Ymagine_formatStr(iformat);

        if (info.cropmode != CROP_MODE_NONE) {
          char* format;
          if (info.cropmode == CROP_MODE_ABSOLUTE) {
            format = "%s-%s-%s-%s-%d-%d-absolute_crop-%d-%d-%d-%d.%s";
          } else {
            format = "%s-%s-%s-%s-%d-%d-relative_crop-%d-%d-%d-%d.%s";
          }
          snprintf(outname, 200, format,
                   info.name, decodecolormodestr, informat,
                   Ymagine_scaleModeStr(info.scalemode), info.maxwidth, info.maxheight,
                   info.croprect.x, info.croprect.y, info.croprect.width,
                   info.croprect.height, outformat);
          snprintf(outpath, 200, "%s/%s", rundir, outname);
        } else {
          snprintf(outname, 200, "%s-%s-%s-%s-%d-%d.%s",
                   info.name, decodecolormodestr, informat,
                   Ymagine_scaleModeStr(info.scalemode), info.maxwidth, info.maxheight,
                   outformat);
          snprintf(outpath, 200, "%s/%s", rundir, outname);
        }

        fdout = open(outpath, O_WRONLY | O_CREAT | O_BINARY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (fdout >= 0) {
          channelout = YchannelInitFd(fdout, 1);
          if (channelout != NULL) {
            options = YmagineFormatOptions_Create();
            if (options != NULL) {
              YmagineFormatOptions_setResize(options, info.maxwidth,
                                             info.maxheight, info.scalemode);
              YmagineFormatOptions_setQuality(options, 95);
              YmagineFormatOptions_setFormat(options, oformat);

              if (info.cropmode == CROP_MODE_ABSOLUTE) {
                YmagineFormatOptions_setCrop(options,
                                             info.croprect.x, info.croprect.y,
                                             info.croprect.width, info.croprect.height);
              } else if (info.cropmode == CROP_MODE_RELATIVE) {
                YmagineFormatOptions_setCropRelative(options,
                                                     info.croprect.x / 100.0f, info.croprect.y / 100.0f,
                                                     info.croprect.width / 100.0f, info.croprect.height / 100.0f);
              }

              rc = YmagineTranscode(channelin, channelout, options);

              if (rc != YMAGINE_OK) {
                snprintf(errorstring, sizeof(errorstring), "error: failed to transcode \"%s\" -> \"%s\"\n", basepath, outpath);

                if (htmlmode) {
                  YmaginePsnrAppendError(channelhtml, errorstring);
                  failedcount++;
                } else {
                  fprintf(stdout, "%s", errorstring);
                  fflush(stdout);
                  exit(1);
                }
              }

              YmagineFormatOptions_Release(options);
              options = NULL;
            }

            YchannelRelease(channelout);
          } else {
            snprintf(errorstring, sizeof(errorstring), "error: failed to create output stream %s\n", outpath);

            if (htmlmode) {
              YmaginePsnrAppendError(channelhtml, errorstring);
              failedcount++;
            } else {
              fprintf(stdout, "%s", errorstring);
              fflush(stdout);
              exit(1);
            }
          }
          close(fdout);
        } else {
          snprintf(errorstring, sizeof(errorstring), "error: failed to open output file \"%s\"\n", outpath);

          if (htmlmode) {
            YmaginePsnrAppendError(channelhtml, errorstring);
            failedcount++;
          } else {
            fprintf(stdout, "%s", errorstring);
            fflush(stdout);
            exit(1);
          }
        }
        YchannelRelease(channelin);
      } else {
        snprintf(errorstring, sizeof(errorstring), "error: failed to create input stream %s\n", basepath);

        if (htmlmode) {
          YmaginePsnrAppendError(channelhtml, errorstring);
          failedcount++;
        } else {
          fprintf(stdout, "%s", errorstring);
          fflush(stdout);
          exit(1);
        }
      }
      close(fdin);
    } else {
      snprintf(errorstring, sizeof(errorstring), "error: failed to open input file \"%s\"\n", basepath);

      if (htmlmode) {
        YmaginePsnrAppendError(channelhtml, errorstring);
        failedcount++;
      } else {
        fprintf(stdout, "%s", errorstring);
        fflush(stdout);
        exit(1);
      }
    }

    if (rc == YMAGINE_OK) {
      snprintf(psnr0path, 200, "%s/%s", rundir, outname);
      fdpsnr0 = open(psnr0path, O_RDONLY | O_BINARY);
      if (fdpsnr0 >= 0) {
        channelpsnr0 = YchannelInitFd(fdpsnr0, 0);
        if (channelpsnr0 != NULL) {
          snprintf(psnr1path, 200, "%s/%s", refdir, outname);
          fdpsnr1 = open(psnr1path, O_RDONLY | O_BINARY);
          if (fdpsnr1 >= 0) {
            channelpsnr1 = YchannelInitFd(fdpsnr1, 0);
            if (channelpsnr1 != NULL) {

              vbitmap = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
              if (vbitmap != NULL) {
                rc = YmagineDecode(vbitmap, channelpsnr0, NULL);
                if (rc == YMAGINE_OK) {
                  vbitmapref = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
                  if (vbitmapref != NULL) {
                    rc = YmagineDecode(vbitmapref, channelpsnr1, NULL);
                    if (rc == YMAGINE_OK) {
                      double psnr;
                      YBOOL warning = YFALSE;
                      YBOOL success = YFALSE;
                      psnr = VbitmapComputePSNR(vbitmap, vbitmapref);

                      if (psnr > 99.9) {
                        success = YTRUE;
                        rc = YMAGINE_OK;
                      } else if (psnr > 50.0) {
                        warningcount++;
                        success = YTRUE;
                        warning = YTRUE;
                        rc = YMAGINE_OK;
                      } else {
                        failedcount++;
                        rc = YMAGINE_ERROR;
                      }

                      if (htmlmode) {
                        char srcsize[20] = {0};
                        char srcrelative[200];
                        char srcpath[200];
                        char psnrstr[20];
                        char refsize[20];
                        char refpath[200];
                        char outsize[20];
                        char outpath[200];
                        char command[200];

                        sprintInfoToCommand(command, sizeof(command), &info);
                        snprintf(srcpath, sizeof(srcpath), "%s/%s", basedir, info.inname);
                        snprintf(srcrelative, sizeof(srcrelative), "../%s", info.inname);
                        snprintf(psnrstr, sizeof(psnrstr), "%f", psnr);
                        snprintf(refsize, sizeof(refsize), "%dx%d", VbitmapWidth(vbitmapref), VbitmapHeight(vbitmapref));
                        snprintf(refpath, sizeof(refpath), "%s/%s/%s/%s", REF_RELATIVE_PATH, BASEDIR, REF_FOLDER, outname);
                        snprintf(outsize, sizeof(outsize), "%dx%d", VbitmapWidth(vbitmap), VbitmapHeight(vbitmap));
                        snprintf(outpath, sizeof(outpath), "%s/%s", RUN_FOLDER, outname);

                        fdin = open(basepath, O_RDONLY | O_BINARY);
                        if (fdin >= 0) {
                          channelin = YchannelInitFd(fdin, 0);
                          if (channelin != NULL) {
                            Vbitmap* srcvbitmap = VbitmapInitNone();
                            /* decode bounds only*/
                            rc = YmagineDecode(srcvbitmap, channelin, NULL);
                            if (rc == YMAGINE_OK) {
                              snprintf(srcsize, sizeof(srcsize), "%dx%d", VbitmapWidth(srcvbitmap), VbitmapHeight(srcvbitmap));
                            }

                            VbitmapRelease(srcvbitmap);
                            YchannelRelease(channelin);
                          }
                          close(fdin);
                        }

                        YmaginePsnrAppendRow(channelhtml, success, warning,
                                             command,
                                             srcpath, srcrelative,
                                             srcsize, psnrstr,
                                             outsize, outpath,
                                             refsize, refpath,
                                             Ymagine_formatStr(info.oformat));
                      } else if (rc != YMAGINE_OK) {
                        char command[200];
                        fprintf(stdout, "error: psnr: %f went below threashold.\n"
                                "output file: %s\n"
                                "reference file: %s\n"
                                "base path: %s/%s\n",
                                psnr, psnr0path, psnr1path, basedir, info.inname);
                        sprintInfoToCommand(command, sizeof(command), &info);
                        fprintf(stdout, "command: %s %s/%s %s\n", command, basedir, info.inname, psnr0path);
                        fflush(stdout);
                        exit(1);
                      }

                    } else {
                      snprintf(errorstring, sizeof(errorstring), "error: failed to decode psnr1 \"%s\"\n", psnr1path);

                      if (htmlmode) {
                        YmaginePsnrAppendError(channelhtml, errorstring);
                        failedcount++;
                      } else {
                        fprintf(stdout, "%s", errorstring);
                        fflush(stdout);
                        exit(1);
                      }
                    }
                    VbitmapRelease(vbitmapref);
                  }
                } else {
                  snprintf(errorstring, sizeof(errorstring), "error: failed to decode psnr0\n \"%s\"\n", psnr0path);

                  if (htmlmode) {
                    YmaginePsnrAppendError(channelhtml, errorstring);
                    failedcount++;
                  } else {
                    fprintf(stdout, "%s", errorstring);
                    fflush(stdout);
                    exit(1);
                  }
                }
                VbitmapRelease(vbitmap);
              }

              YchannelRelease(channelpsnr1);
            } else {
              snprintf(errorstring, sizeof(errorstring), "error: failed to create psnr1 stream \"%s\"\n", psnr1path);

              if (htmlmode) {
                YmaginePsnrAppendError(channelhtml, errorstring);
                failedcount++;
              } else {
                fprintf(stdout, "%s", errorstring);
                fflush(stdout);
                exit(1);
              }
            }
            close(fdpsnr1);
          } else {
            snprintf(errorstring, sizeof(errorstring), "error: failed to open psnr1 file \"%s\"\n", psnr1path);

            if (htmlmode) {
              YmaginePsnrAppendError(channelhtml, errorstring);
              failedcount++;
            } else {
              fprintf(stdout, "%s", errorstring);
              fflush(stdout);
              exit(1);
            }
          }
          YchannelRelease(channelpsnr0);
        } else {
          snprintf(errorstring, sizeof(errorstring), "error: failed to create psnr0 stream \"%s\"\n", psnr0path);

          if (htmlmode) {
            YmaginePsnrAppendError(channelhtml, errorstring);
            failedcount++;
          } else {
            fprintf(stdout, "%s", errorstring);
            fflush(stdout);
            exit(1);
          }
        }
        close(fdpsnr0);
      } else {
        snprintf(errorstring, sizeof(errorstring), "error: failed to open psnr0 file \"%s\"\n", psnr0path);

        if (htmlmode) {
          YmaginePsnrAppendError(channelhtml, errorstring);
          failedcount++;
        } else {
          fprintf(stdout, "%s", errorstring);
          fflush(stdout);
          exit(1);
        }
      }
    }

    if (rc == YMAGINE_ERROR && ret == 0) {
      ret = -1;
    }
  }

  if (htmlmode) {
    if (channelhtml != NULL) {
      YmaginePsnrAppendHtmlTail(channelhtml, failedcount, warningcount);
      YchannelRelease(channelhtml);
    }
    close(fhtml);

    fprintf(stdout, "report for transcode resize: %s\n", htmlpath);
    fflush(stdout);

    if (failedcount <= 0) {
      fprintf(stdout, "transcodeResize: passed with %d warning\n", warningcount);
    } else {
      fprintf(stdout, "error: transcodeResize: %d failed with %d warning\n",
              failedcount, warningcount);
      fflush(stdout);
      exit(1);
    }
  }
}

void help() {
  int ntests;

  ntests = sizeof(transform_infos)/sizeof(transform_infos[0]);
  printf("unit test for ymagine\n"
         "\n"
         "command:\n"
         "help: print out help\n"
         "transformer: run transformer test\n"
         "transcode: run transcode test\n"
         "compute_transform: run compute transform test\n"
         "merge_line: run merge line test\n"
         "* if no command specified, all tests will be run\n"
         "\n"
         "argument:\n"
         "-transcode_dir: optional base directory for transcode test. E.g., -transcode_dir ./test/ymagine-data/transcode\n"
         "-no-report: transcode test will fail immediately instead of generating report\n"
         "-range: index range that transcode test case will run, valid range is 0:%d. E.g., -range 5:50\n", ntests);
}

int main(int argc, const char* argv[]) {
  enum command {
    COMMAND_DEFAULT,
    COMMAND_HELP,
    COMMAND_TRANSFORMER,
    COMMAND_COMPUTE_TRANSFORM,
    COMMAND_MERGE_LINE,
    COMMAND_TRANSCODE,
  };
  enum argument {
    ARGUMENT_NONE,
    ARGUMENT_TRANSCODE_DIR,
    ARGUMENT_TEST_INDEX,
  };
  enum command mode = COMMAND_DEFAULT;
  YBOOL htmlreport = YTRUE;
  const char* transcodedir = NULL;
  enum argument arg = ARGUMENT_NONE;
  int ntests;
  int startindex = 0;
  int endindex;
  int i;

  ntests = sizeof(transform_infos)/sizeof(transform_infos[0]);

  startindex = 0;
  endindex = ntests;

  if (argc >= 2) {
    if (strcmp(argv[1], "help") == 0) {
      mode = COMMAND_HELP;
    } else if (strcmp(argv[1], "transformer") == 0) {
      mode = COMMAND_TRANSFORMER;
    } else if (strcmp(argv[1], "transcode") == 0) {
      mode = COMMAND_TRANSCODE;
    } else if (strcmp(argv[1], "compute_transform") == 0) {
      mode = COMMAND_COMPUTE_TRANSFORM;
    } else if (strcmp(argv[1], "merge_line") == 0) {
      mode = COMMAND_MERGE_LINE;
    }

    for (i = 1; i < argc; i++) {
      switch (arg) {
        case ARGUMENT_TRANSCODE_DIR:
          transcodedir = argv[i];
          arg = ARGUMENT_NONE;
          break;

        case ARGUMENT_TEST_INDEX:
          if (sscanf(argv[i], "%d:%d", &startindex, &endindex) != 2) {
            help();
            return 1;
          }

          if (startindex <= 0 || startindex >= ntests || endindex > ntests) {
            printf("transcode test case index out of accepted range 0:%d\n", ntests);
            return 1;
          }

          arg = ARGUMENT_NONE;
          break;

        default:
          if (strcmp(argv[i], "-transcode_dir") == 0) {
            arg = ARGUMENT_TRANSCODE_DIR;
          } else if (strcmp(argv[i], "-no_report") == 0) {
            htmlreport = YFALSE;
          } else if (strcmp(argv[i], "-range") == 0) {
            arg = ARGUMENT_TEST_INDEX;
          }
          break;
      }
    }
  }

  switch (mode) {
    case COMMAND_HELP:
      help();
      break;

    case COMMAND_TRANSFORMER:
      testTransformer();
      break;

    case COMMAND_COMPUTE_TRANSFORM:
      testComputeTransform();
      break;

    case COMMAND_TRANSCODE:
      testTranscode(htmlreport, transcodedir, startindex, endindex);
      break;

    case COMMAND_MERGE_LINE:
      testMergeLine();
      break;

    default:
      testTransformer();
      testComputeTransform();
      testMergeLine();
      testTranscode(htmlreport, transcodedir, startindex, endindex);
      break;
  }

  return 0;
}
