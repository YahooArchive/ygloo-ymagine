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

#include "ymagine_main.h"

static void
usage_convolution()
{
  printf("usage: ymagine conv_profile -repeat N1 -bpp N2 -width N3 -height N4 -kernel a,b,c,d,e,f,g,h,i:denominator\n");
}

static int
dummyTransformerWriter(Transformer *transformer, void *writedata, void *line)
{
  return YMAGINE_OK;
}

int
main_convolution_profile(int argc, const char* argv[])
{
  int niters = 1;
  int bpp = 3;
  int width = 1000;
  int height = 1000;
  int kernel[9] = {0, -1, 0, -1, 5, -1, 0, -1, 0};
  int denominator;
  int i;
  int j;
  YBOOL sharpen = YTRUE;
  float sharpenvalue = 0.0f;
  NSTYPE starttime;
  NSTYPE endtime;
  NSTYPE convstarttime;
  NSTYPE convendtime;
  Transformer* transformer;
  unsigned char* bitmap;
  enum argument {
    ARGUMENT_NONE,
    ARGUMENT_REPEAT,
    ARGUMENT_BPP,
    ARGUMENT_WIDTH,
    ARGUMENT_HEIGHT,
    ARGUMENT_KERNEL,
    ARGUMENT_SHARPEN,
  };
  enum argument arg = ARGUMENT_NONE;

  for (i = 0; i < argc; i++) {
    switch (arg) {
      case ARGUMENT_REPEAT:
        niters = atoi(argv[i]);
        arg = ARGUMENT_NONE;
        break;

      case ARGUMENT_BPP:
        bpp = atoi(argv[i]);
        arg = ARGUMENT_NONE;
        break;

      case ARGUMENT_WIDTH:
        width = atoi(argv[i]);
        arg = ARGUMENT_NONE;
        break;

      case ARGUMENT_HEIGHT:
        height = atoi(argv[i]);
        arg = ARGUMENT_NONE;
        break;

      case ARGUMENT_SHARPEN:
        sharpenvalue = (float)atof(argv[i]);
        arg = ARGUMENT_NONE;
        break;

      case ARGUMENT_KERNEL:
        if (sscanf(argv[i], "%d,%d,%d,%d,%d,%d,%d,%d,%d:%d",
                   &kernel[0], &kernel[1], &kernel[2],
                   &kernel[3], &kernel[4], &kernel[5],
                   &kernel[6], &kernel[7], &kernel[8], &denominator) != 10) {
          usage_convolution();
          return 1;
        }
        sharpen = YFALSE;
        arg = ARGUMENT_NONE;
        break;

      default:
        if (strcmp(argv[i], "-repeat") == 0) {
          arg = ARGUMENT_REPEAT;
        } else if (strcmp(argv[i], "-bpp") == 0) {
          arg = ARGUMENT_BPP;
        } else if (strcmp(argv[i], "-width") == 0) {
          arg = ARGUMENT_WIDTH;
        } else if (strcmp(argv[i], "-height") == 0) {
          arg = ARGUMENT_HEIGHT;
        } else if (strcmp(argv[i], "-kernel") == 0) {
          arg = ARGUMENT_KERNEL;
        } else if (strcmp(argv[i], "-sharpen") == 0) {
          arg = ARGUMENT_KERNEL;
        }
        break;
    }
  }

  bitmap = Ymem_malloc(width * height * bpp);

  starttime = NSTIME();

  for (i = 0; i < niters; i++) {
    transformer = TransformerCreate();
    TransformerSetScale(transformer, width, height, width, height);
    TransformerSetRegion(transformer, 0, 0, width, height);

    if (bpp == 1) {
      TransformerSetMode(transformer, VBITMAP_COLOR_GRAYSCALE, VBITMAP_COLOR_GRAYSCALE);
    } else if (bpp == 3) {
      TransformerSetMode(transformer, VBITMAP_COLOR_RGB, VBITMAP_COLOR_RGB);
    } else {
      TransformerSetMode(transformer, VBITMAP_COLOR_RGBA, VBITMAP_COLOR_RGBA);
    }

    TransformerSetWriter(transformer, dummyTransformerWriter, NULL);

    for (j = 0; j < height; j++) {
      TransformerPush(transformer, (const char*)(&bitmap[j * width * bpp]));
    }

    TransformerRelease(transformer);
    transformer = NULL;
  }

  endtime = NSTIME();

  convstarttime = NSTIME();

  for (i = 0; i < niters; i++) {
    transformer = TransformerCreate();
    TransformerSetScale(transformer, width, height, width, height);
    TransformerSetRegion(transformer, 0, 0, width, height);

    if (bpp == 1) {
      TransformerSetMode(transformer, VBITMAP_COLOR_GRAYSCALE, VBITMAP_COLOR_GRAYSCALE);
    } else if (bpp == 3) {
      TransformerSetMode(transformer, VBITMAP_COLOR_RGB, VBITMAP_COLOR_RGB);
    } else {
      TransformerSetMode(transformer, VBITMAP_COLOR_RGBA, VBITMAP_COLOR_RGBA);
    }

    if (sharpen) {
      TransformerSetSharpen(transformer, sharpenvalue);
    } else {
      TransformerSetKernel(transformer, kernel);
    }

    TransformerSetWriter(transformer, dummyTransformerWriter, NULL);

    for (j = 0; j < height; j++) {
      TransformerPush(transformer, (const char*)(&bitmap[j * width * bpp]));
    }

    TransformerRelease(transformer);
    transformer = NULL;
  }

  convendtime = NSTIME();

  printf("apply convolution on %dx%d bpp: %d image %d iterations, ",
         width, height, bpp, niters);

  if (sharpen) {
    printf("sharpen: %f\n", sharpenvalue);
  } else {
    printf("using kernel\n %d %d %d\n%d %d %d\n%d %d %d\n",
            kernel[0], kernel[1], kernel[2],
            kernel[3], kernel[4], kernel[5],
            kernel[6], kernel[7], kernel[8]);
  }

  printf("without convolution: took %f(ms), %f(ms) per iteration\n",
         ((double) (endtime - starttime)) / 1000000.0,
         ((double) (endtime - starttime)) / (niters * 1000000.0));
  printf("with convolution: took %f(ms), %f(ms) per iteration\n",
         ((double) (convendtime - convstarttime)) / 1000000.0,
         ((double) (convendtime - convstarttime)) / (niters * 1000000.0));
  printf("delta: took %f(ms), %f(ms) per iteration\n",
         ((double) (convendtime - convstarttime) - (endtime - starttime)) / 1000000.0,
         ((double) (convendtime - convstarttime) - (endtime - starttime)) / (niters * 1000000.0));

  Ymem_free(bitmap);
  bitmap = NULL;

  return 0;
}
