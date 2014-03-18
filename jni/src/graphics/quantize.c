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

#define LOG_TAG "ymagine::quantize"

#include "ymagine_config.h"
#include "ymagine_priv.h"

#include <math.h>
#include <float.h>

typedef struct {
  /* Accumulator for each channel */
  uint32_t accum_red;
  uint32_t accum_green;
  uint32_t accum_blue;
  uint32_t accum_alpha;
  /* Number of elements */
  uint32_t count;
  /* Centroid color */
  Vcolor color;
} ColorArea;

#define SQ(x) (((uint32_t) (x))*((uint32_t) (x)))
#define DIST(a,b) ( ((a) > (b)) ? ((a) - (b)) : ((b) - (a)) )

/* Function to compare ColorArea, used for sorting */
int ColorAreaCompare(const void *v1, const void *v2)
{
  ColorArea *c1 = (ColorArea*) v1;
  ColorArea *c2 = (ColorArea*) v2;

  /* Return centroids with higher count first */
  if (c1->count != c2->count) {
    return ((c1->count < c2->count) ? 1 : -1);
  }

  /* If same score, define order based on color components */
  if (c1->color.red != c2->color.red) {
    return ((c1->color.red < c2->color.red) ? -1 : 1);
  }
  if (c1->color.green != c2->color.green) {
    return ((c1->color.green < c2->color.green) ? -1 : 1);
  }
  if (c1->color.blue != c2->color.blue) {
    return ((c1->color.blue < c2->color.blue) ? -1 : 1);
  }
  if (c1->color.alpha != c2->color.alpha) {
    return ((c1->color.alpha < c2->color.alpha) ? -1 : 1);
  }

  return 0;
}

/* Initialization palette */
static const Vcolor palette[] =
{
  { .red = 0x00, .green = 0x00, .blue = 0x00, .alpha = 0xff },
  { .red = 0x00, .green = 0x00, .blue = 0xaa, .alpha = 0xff },
  { .red = 0x00, .green = 0xaa, .blue = 0x00, .alpha = 0xff },
  { .red = 0x00, .green = 0xaa, .blue = 0xaa, .alpha = 0xff },
  { .red = 0xaa, .green = 0x00, .blue = 0x00, .alpha = 0xff },
  { .red = 0xaa, .green = 0x00, .blue = 0xaa, .alpha = 0xff },
  { .red = 0xaa, .green = 0x55, .blue = 0x00, .alpha = 0xff },
  { .red = 0xaa, .green = 0xaa, .blue = 0xaa, .alpha = 0xff },
  { .red = 0x55, .green = 0x55, .blue = 0x55, .alpha = 0xff },
  { .red = 0x55, .green = 0x55, .blue = 0xff, .alpha = 0xff },
  { .red = 0x55, .green = 0xff, .blue = 0x55, .alpha = 0xff },
  { .red = 0x55, .green = 0xff, .blue = 0xff, .alpha = 0xff },
  { .red = 0xff, .green = 0x55, .blue = 0x55, .alpha = 0xff },
  { .red = 0xff, .green = 0x55, .blue = 0xff, .alpha = 0xff },
  { .red = 0xff, .green = 0x55, .blue = 0xff, .alpha = 0xff },
  { .red = 0xff, .green = 0xff, .blue = 0x55, .alpha = 0xff },
  { .red = 0xff, .green = 0xff, .blue = 0xff, .alpha = 0xff }
};

#define PALETTESIZE (sizeof(palette)/sizeof(palette[0]))

#define RED_OFFSET   0
#define GREEN_OFFSET 1
#define BLUE_OFFSET  2
#define ALPHA_OFFSET 3

static
uint32_t norm2(const Vcolor *c1, const Vcolor *c2)
{
  return 7*SQ(DIST(c1->red, c2->red)) +
	28*SQ(DIST(c1->green, c2->green)) +
	1*SQ(DIST(c1->blue, c2->blue));
}

static int
hasMultiplyOverflow(uint32_t m, uint32_t i1, uint32_t i2)
{
  if (i1 == 0 || i2 == 0) {
    return (m == 0);
  }
  if (i1 >= m || i2 >= m || (m / i1) <= i2) {
    return 1;
  }

  return 0;
}

#define ASHIFT 24
#define RSHIFT 16
#define GSHIFT 8
#define BSHIFT 0

#define RGBA(r,g,b,a) \
( (((uint32_t) (r)) << RSHIFT) | \
(((uint32_t) (g)) << GSHIFT) | \
(((uint32_t) (b)) << BSHIFT) | \
(((uint32_t) (a)) << ASHIFT) )

static const Vcolor black = {
  .red = 0x00,
  .green = 0x00,
  .blue = 0x00,
  .alpha = 0xff
};

static const Vcolor white = {
  .red = 0xff,
  .green = 0xff,
  .blue = 0xff,
  .alpha = 0xff
};

static int
quantizeWithOptions(Vbitmap *vbitmap, int maxcolors,
                    Vcolor *colors, int *scores, int processing)
{
  int i;
  int x, y, c;
  unsigned char *pixels;
  unsigned char *line;
  ColorArea centroid[PALETTESIZE];
  Vcolor current;
  int width, height, pitch;
  uint64_t dist;
  uint64_t refdist;
  int refid;
  int ncolors;
  int maxiters = 100;

  if (maxcolors <= 0 || colors == NULL) {
    return 0;
  }
  if (vbitmap == NULL) {
    return 0;
  }

  if (processing == YMAGINE_THEME_DEFAULT) {
    processing = YMAGINE_THEME_SATURATION;
  }

  if (processing != YMAGINE_THEME_NONE && processing != YMAGINE_THEME_SATURATION) {
    return 0;
  }

  if (VbitmapLock(vbitmap) < 0) {
    return 0;
  }

  pixels = VbitmapBuffer(vbitmap);
  if (pixels == NULL) {
    VbitmapUnlock(vbitmap);
    return 0;
  }

  width = VbitmapWidth(vbitmap);
  height = VbitmapHeight(vbitmap);
  pitch = VbitmapPitch(vbitmap);
  if (width <= 0 || height <= 0 || pitch <= 0) {
    VbitmapUnlock(vbitmap);
    return 0;
  }

  /* Verify that width*height < 1<<24. This quarantee that will be
   no overflow in accumulators */
  if (hasMultiplyOverflow((((uint32_t) 1)<<24), width, height)) {
    VbitmapUnlock(vbitmap);
    return 0;
  }

  /* Initial result, using default palette */
  if (maxcolors > PALETTESIZE) {
    maxcolors = PALETTESIZE;
  }

  for (c = 0; c < maxcolors; c++) {
    centroid[c].color = palette[c];
    centroid[c].count = 0;
  }
#if 0
  for (c = 0; c < maxcolors; c++) {
    fprintf(stderr,
            "Init[%d] = #%02x%02x%02x%02x (%d)\n",
            c,
            centroid[c].color.red,
            centroid[c].color.green,
            centroid[c].color.blue,
            centroid[c].color.alpha,
            centroid[c].count);
  }
#endif

  for (i = 0; i < maxiters; i++) {
    /* Reset accumulators */
    for (c = 0; c < maxcolors; c++) {
      centroid[c].accum_red = 0;
      centroid[c].accum_green = 0;
      centroid[c].accum_blue = 0;
      centroid[c].accum_alpha = 0;
      centroid[c].count = 0;
    }

    /* Assign each pixel to its nearest centroid */
    current.alpha = 0xff;
    for (y = 0; y < height; y++){
      line = pixels + y * pitch;

      for (x = 0; x < width; x++) {
	      /* Current pixel as a Color record */
	      current.red=line[RED_OFFSET];
	      current.green=line[GREEN_OFFSET];
	      current.blue=line[BLUE_OFFSET];
	      current.alpha=line[ALPHA_OFFSET];
	      line+=4;

	      /* Assign to first centroid by default */
	      refdist = norm2(&current, &(centroid[0].color));
	      refid = 0;

	      /* Then try to find a closer one */
	      for (c = 1; c < maxcolors; c++) {
          dist = norm2(&current, &(centroid[c].color));
          if (dist < refdist) {
            refdist = dist;
            refid = c;
          }
	      }

	      /* Assign this pixel to closest centroid */
	      centroid[refid].accum_red += current.red;
	      centroid[refid].accum_green += current.green;
	      centroid[refid].accum_blue += current.blue;
	      centroid[refid].accum_alpha += current.alpha;
	      centroid[refid].count++;
      }
    }

    refdist = 0;
    for (c = 0; c < maxcolors; c++) {
      if (centroid[c].count == 0) {
	      current.red = 0xff;
	      current.green = 0xff;
	      current.blue = 0xff;
	      current.alpha = 0xff;
      } else {
	      current.red = centroid[c].accum_red/centroid[c].count;
	      current.green = centroid[c].accum_green/centroid[c].count;
	      current.blue = centroid[c].accum_blue/centroid[c].count;
	      current.alpha = 0xff;
      }

      dist = norm2(&current, &(centroid[c].color));
      if (dist > refdist) {
	      refdist = dist;
      }
      centroid[c].color = current;
    }

    if (refdist < 1) {
      break;
    }
  }

  VbitmapUnlock(vbitmap);

  /* Saturation */
  if (processing == YMAGINE_THEME_SATURATION) {
    int i;
    uint32_t dwhite;
    uint32_t dblack;
    uint32_t dcrit, dmin;
    Vcolor col;
    col.red = 0x20;
    col.green = 0x20;
    col.blue = 0x20;
    col.alpha = 0xff;
    dcrit = norm2(&col, &black);


    /* Update scores to prefer saturated colors */
    for (i = 0; i < maxcolors; i++) {
      uint32_t coeff;
      col.red = centroid[i].color.red;
      col.green = centroid[i].color.green;
      col.blue = centroid[i].color.blue;
      col.alpha = centroid[i].color.alpha;

      dwhite = norm2(&white, &col);
      dblack = norm2(&black, &col);

      dmin = (dwhite < dblack) ? dwhite : dblack;

      coeff = 256;
      if (dmin < dcrit) {
        coeff = (256 * (dcrit + (dcrit - dmin))) / dcrit;
      }

      centroid[i].count = (centroid[i].count * 256) / coeff;
    }
  }

  /* Sort colors, most dominant one first */
  qsort(centroid, maxcolors, sizeof(centroid[0]), ColorAreaCompare);

  /* Put colors into ARGB format */
  ncolors = 0;
  for (c = 0; c < maxcolors; c++) {
    if (centroid[c].count <= 0) {
      continue;
    }
    colors[c] = centroid[c].color;
    if (scores != NULL) {
      scores[c] = (int) centroid[c].count;
    }
#if 0
    ALOGD("color[%d] = #%02x%02x%02x%02x (%d)",
          c,
          centroid[c].color.red,
          centroid[c].color.green,
          centroid[c].color.blue,
          centroid[c].color.alpha,
          centroid[c].count);
#endif
    ncolors++;
  }

  return ncolors;
}

int
quantize(Vbitmap *vbitmap, int maxcolors,
         Vcolor *colors, int *scores)
{
  return quantizeWithOptions(vbitmap, maxcolors, colors, scores, YMAGINE_THEME_NONE);
}

int
getThemeColors(Vbitmap *vbitmap, int ncol, int *colors, int *scores)
{
  int ncolors = 0;
  int i;

  if (vbitmap == NULL || ncol <= 0) {
    return 0;
  }

  Vcolor *vcolors = Ymem_malloc(ncol * sizeof(Vcolor));

  if (vcolors != NULL) {
    ncolors = quantizeWithOptions(vbitmap, ncol, vcolors, scores,
                                  YMAGINE_THEME_SATURATION);
    for (i = 0; i < ncolors; i++) {
	    colors[i] = RGBA(vcolors[i].red, vcolors[i].green,
                       vcolors[i].blue, vcolors[i].alpha);
    }
    Ymem_free(vcolors);
  }

  return ncolors;
}

int
getThemeColor(Vbitmap *vbitmap)
{
  int colors[8];
  int scores[8];
  int ncolors;

  ncolors = getThemeColors(vbitmap, sizeof(colors) / sizeof(colors[0]), colors, scores);

  if (ncolors <= 0 || colors == NULL) {
    return RGBA(0, 0, 0, 0);
  }

  return colors[0];
}
