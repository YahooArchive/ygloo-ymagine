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

#include "ymagine/ymagine.h"
#include "ymagine_priv.h"

#ifdef MAX
#undef MAX
#endif
#define MAX(a,b) ((a)>(b)?(a):(b))

#ifdef MIN
#undef MIN
#endif
#define MIN(a,b) ((a)<(b)?(a):(b))

#ifdef ABS
#undef ABS
#endif
#define ABS(a) ((a) < 0 ? (- (a)) : (a))

#define MAX_VAL 255

#define ASHIFT 24
#define RSHIFT 16
#define GSHIFT 8
#define BSHIFT 0

#define CHANNEL_MASK 255 //8 bit mask

#define LOG_TAG "ymagine::compose"


static YINLINE void
composeReplace(unsigned char *output,
        unsigned char *source, unsigned char *color)
{
    // Replace the all rgb values with the provided color
    output[0] = color[0];
    output[1] = color[1];
    output[2] = color[2];
    output[3] = color[3];
}

// Compose over function taken from
// http://en.wikipedia.org/wiki/Alpha_compositing
static YINLINE void
composeOver(unsigned char *output,
        unsigned char *source, unsigned char *color)
{
  int alpha = color[3];

  // a_o = 1 - (1-a_t)(1-a_b)
  int alphaOut = alpha + (((255-alpha) * source[3])/255);
  int alphaMult = alphaOut - alpha;

  // c_o = (a_t*c_t + (1-a_t)*a_b*c_b)/a_o
  output[0] = MIN(MAX_VAL, ((alpha*color[0]) + (alphaMult*source[0]))/alphaOut);
  output[1] = MIN(MAX_VAL, ((alpha*color[1]) + (alphaMult*source[1]))/alphaOut);
  output[2] = MIN(MAX_VAL, ((alpha*color[2]) + (alphaMult*source[2]))/alphaOut);
  output[3] = MIN(MAX_VAL, alphaOut);
}

#define composeUnder(out, source, color) composeOver(out, color, source)

#define COMPOSE_PLUS(a,b) MIN(((int) (a)) + (b), MAX_VAL)

static YINLINE void
composePlus(unsigned char *output,
        unsigned char *source, unsigned char *color)
{
    // Integer conversion is done to prevent overflow errors
    output[0] = COMPOSE_PLUS(source[0], color[0]);
    output[1] = COMPOSE_PLUS(source[1], color[1]);
    output[2] = COMPOSE_PLUS(source[2], color[2]);
    output[3] = COMPOSE_PLUS(source[3], color[3]);
}

#define COMPOSE_MINUS(a,b) MAX(((int) (a)) - (b), 0)

static YINLINE void
composeMinus(unsigned char *output,
        unsigned char *source, unsigned char *color)
{
    // Integer conversion is done to prevent overflow errors
    output[0] = COMPOSE_MINUS(source[0], color[0]);
    output[1] = COMPOSE_MINUS(source[1], color[1]);
    output[2] = COMPOSE_MINUS(source[2], color[2]);
    output[3] = COMPOSE_MINUS(source[3], color[3]);
}

static YINLINE void
composeAdd(unsigned char *output,
        unsigned char *source, unsigned char *color)
{
    // Integer conversion is done to prevent overflow errors
    output[0] = COMPOSE_PLUS(source[0], color[0]);
    output[1] = COMPOSE_PLUS(source[1], color[1]);
    output[2] = COMPOSE_PLUS(source[2], color[2]);
    /* Since both are unsigned chars and unsigned char overflow is 
       defined to be modulo 0x100, the following line of code doesn't 
        require modulo */
    output[3] = source[3] + color[3];
}

static YINLINE void
composeSubtract(unsigned char *output,
        unsigned char *source, unsigned char *color)
{
    // Integer conversion is done to prevent overflow errors
    output[0] = COMPOSE_MINUS(source[0], color[0]);
    output[1] = COMPOSE_MINUS(source[1], color[1]);
    output[2] = COMPOSE_MINUS(source[2], color[2]);
    /* Since both are unsigned chars and unsigned char overflow is 
        defined to be modulo 0x100, the following line of code doesn't 
        require modulo */
    output[3] = source[3] - color[3];
}

// Compute the difference by casting to integer
#define COMPOSE_DIFF(a, b) ABS(((int) (a)) - (b))
// Another implementation, which avoids casting into signed integer, but
// less efficient on armv7-a
// #define COMPOSE_DIFF(a,b) ((a) >= (b) ? (a) - (b) : (b) - (a))

static YINLINE void
composeDifference(unsigned char *output,
        unsigned char *source, unsigned char *color)
{
    // Integer conversion is done to prevent overflow errors
    output[0] = COMPOSE_DIFF(source[0], color[0]);
    output[1] = COMPOSE_DIFF(source[1], color[1]);
    output[2] = COMPOSE_DIFF(source[2], color[2]);
    output[3] = COMPOSE_DIFF(source[3], color[3]);
}

static YINLINE void
composeBump(unsigned char *output,
        unsigned char *source, unsigned char *color)
{
    output[0] = color[0];
    output[1] = color[1];
    output[2] = color[2];
    output[3] = source[3];

}

static YINLINE void
composeMap(unsigned char *output,
        unsigned char *source, unsigned char *color)
{
    // Integer conversion is done to prevent overflow errors
    output[0] = color[0];
    output[1] = color[1];
    output[2] = color[2];
    output[3] = MIN(MAX(((((int)source[3]) * color[3]) / MAX_VAL), 0), MAX_VAL);
}

#define COMPOSE_MIX(a,b) (((int) (a)) + (b)) / 2

static YINLINE void
composeMix(unsigned char *output,
        unsigned char *source, unsigned char *color)
{
    // Integer conversion is done to prevent overflow errors
    output[0] = COMPOSE_MIX(source[0], color[0]);
    output[1] = COMPOSE_MIX(source[1], color[1]);
    output[2] = COMPOSE_MIX(source[2], color[2]);
    output[3] = COMPOSE_MIX(source[3], color[3]);
}

#if MAX_VAL == 255
/* Approximate division by 255 with 8 bit shift and accumulation of rounding error */
#define COMPOSE_MULT(a,b) (((int)(a) * (b) + 128) >> 8)
#else
#define COMPOSE_MULT(a,b) MIN(MAX(((((int)(a)) * (b)) / MAX_VAL), 0), MAX_VAL)
#endif
static YINLINE void
composeMult(unsigned char *output,
        unsigned char *source, unsigned char *color)
{
    // Integer conversion is done to prevent overflow errors
    output[0] = COMPOSE_MULT(source[0], color[0]);
    output[1] = COMPOSE_MULT(source[1], color[1]);
    output[2] = COMPOSE_MULT(source[2], color[2]);
    output[3] = COMPOSE_MULT(source[3], color[3]);
}

// Same as composeOver, but use luminance as alpha value
static YINLINE void
composeLuminance(unsigned char *output,
                 unsigned char *source, unsigned char *color)
{
  int alpha = (color[0] + color[1] + color[2]) / 3;

  // a_o = 1 - (1-a_t)(1-a_b)
  int alphaOut = alpha + (((255-alpha) * source[3])/255);
  int alphaMult = alphaOut - alpha;

  // c_o = (a_t*c_t + (1-a_t)*a_b*c_b)/a_o
  output[0] = MIN(MAX_VAL, ((alpha*color[0]) + (alphaMult*source[0]))/alphaOut);
  output[1] = MIN(MAX_VAL, ((alpha*color[1]) + (alphaMult*source[1]))/alphaOut);
  output[2] = MIN(MAX_VAL, ((alpha*color[2]) + (alphaMult*source[2]))/alphaOut);
  output[3] = MIN(MAX_VAL, alphaOut);
}

// Same as composeOver, but use luminance as alpha value
static YINLINE void
composeLuminanceInv(unsigned char *output,
                    unsigned char *source, unsigned char *color)
{
  int alpha = 255 - (color[0] + color[1] + color[2]) / 3;

  // a_o = 1 - (1-a_t)(1-a_b)
  int alphaOut = alpha + (((255-alpha) * source[3])/255);
  int alphaMult = alphaOut - alpha;

  // c_o = (a_t*c_t + (1-a_t)*a_b*c_b)/a_o
  output[0] = MIN(MAX_VAL, ((alpha*color[0]) + (alphaMult*source[0]))/alphaOut);
  output[1] = MIN(MAX_VAL, ((alpha*color[1]) + (alphaMult*source[1]))/alphaOut);
  output[2] = MIN(MAX_VAL, ((alpha*color[2]) + (alphaMult*source[2]))/alphaOut);
  output[3] = MIN(MAX_VAL, alphaOut);
}

// Extract luminance of source and multiply it by color
static YINLINE void
composeColorize(unsigned char *output,
                unsigned char *source, unsigned char *color)
{
  int brightness;

  /* Brightness of current pixel, in the range 0..1024 */
  /* Y = 0.2126 R + 0.7152 G + 0.0722 B */
  brightness = (218 * source[0] + 732 * source[1] + 74 * source[2]);

  output[0] = (brightness * color[0]) >> 10;
  output[1] = (brightness * color[1]) >> 10;
  output[2] = (brightness * color[2]) >> 10;
  output[3] = source[3];
}

#define COMPOSE_LOOP(pixels, width, bpp,                                 \
                     overlay, overlaybpp,                                \
                     composemethod)                                      \
{                                                                        \
    int i;                                                               \
    for (i = 0; i < width; i++) {                                        \
        composemethod(pixels, pixels, overlay);                          \
        pixels += bpp;                                                   \
        overlay += overlaybpp;                                           \
    }                                                                    \
}

#define COMPOSE_LOOP_SCALE(pixels, width, bpp,                           \
                           overlay, overlaywidth, overlaybpp,            \
                           composemethod)                                \
{                                                                        \
    int i;                                                               \
    int overlayi;                                                        \
    unsigned char *current;                                              \
    for (i = 0; i < width; i++) {                                        \
        overlayi = (i * (overlaywidth - 1)) / (width - 1);               \
        current = overlay + overlaybpp * overlayi;                       \
        composemethod(pixels, pixels, current);                          \
        pixels += bpp;                                                   \
    }                                                                    \
}

static YINLINE int
composeLine(unsigned char *source, unsigned char *overlay, 
                int bpp, int overlaybpp, int width, 
                int composeMode)
{
  int rc = YMAGINE_ERROR;

  if (bpp != 4) return rc;
  if (overlaybpp != 0 && overlaybpp != 4) return rc;
  if (width <= 0) return rc;
  if (source == NULL || overlay == NULL) return rc;

  rc = YMAGINE_OK;

  switch (composeMode) {
  case YMAGINE_COMPOSE_REPLACE:
    COMPOSE_LOOP(source, width, bpp, overlay, overlaybpp, composeReplace);
    break;
  case YMAGINE_COMPOSE_OVER:
    COMPOSE_LOOP(source, width, bpp, overlay, overlaybpp, composeOver);
    break;
  case YMAGINE_COMPOSE_UNDER:
    COMPOSE_LOOP(source, width, bpp, overlay, overlaybpp, composeUnder);
    break;
  case YMAGINE_COMPOSE_PLUS:
    COMPOSE_LOOP(source, width, bpp, overlay, overlaybpp, composePlus);
    break;
  case YMAGINE_COMPOSE_MINUS:
    COMPOSE_LOOP(source, width, bpp, overlay, overlaybpp, composeMinus);
    break;
  case YMAGINE_COMPOSE_ADD:
    COMPOSE_LOOP(source, width, bpp, overlay, overlaybpp, composeAdd);
    break;
  case YMAGINE_COMPOSE_SUBTRACT:
    COMPOSE_LOOP(source, width, bpp, overlay, overlaybpp, composeSubtract);
    break;
  case YMAGINE_COMPOSE_DIFFERENCE:
    COMPOSE_LOOP(source, width, bpp, overlay, overlaybpp, composeDifference);
    break;
  case YMAGINE_COMPOSE_BUMP:
    COMPOSE_LOOP(source, width, bpp, overlay, overlaybpp, composeBump);
    break;
  case YMAGINE_COMPOSE_MAP:
    COMPOSE_LOOP(source, width, bpp, overlay, overlaybpp, composeMap);
    break;
  case YMAGINE_COMPOSE_MIX:
    COMPOSE_LOOP(source, width, bpp, overlay, overlaybpp, composeMix);
    break;
  case YMAGINE_COMPOSE_MULT:
    COMPOSE_LOOP(source, width, bpp, overlay, overlaybpp, composeMult);
    break;

  case YMAGINE_COMPOSE_LUMINANCE:
    COMPOSE_LOOP(source, width, bpp, overlay, overlaybpp, composeLuminance);
    break;
  case YMAGINE_COMPOSE_LUMINANCEINV:
    COMPOSE_LOOP(source, width, bpp, overlay, overlaybpp, composeLuminanceInv);
    break;
  case YMAGINE_COMPOSE_COLORIZE:
    COMPOSE_LOOP(source, width, bpp, overlay, overlaybpp, composeColorize);
    break;
  default:
    /* Wrong composition mode */
    ALOGE("Specified composition mode doesn't exist");
    rc = YMAGINE_ERROR;
    break;
  }

  return rc;
}

static YINLINE int
composeLineScale(unsigned char *source, unsigned char *overlay,
                 int bpp, int overlaybpp,
                 int width, int overlaywidth,
                 int composeMode)
{
  int rc = YMAGINE_ERROR;

  if (bpp != 4) return rc;
  if (overlaybpp != 0 && overlaybpp != 4) return rc;
  if (width <= 0) return rc;
  if (source == NULL || overlay == NULL) return rc;

  rc = YMAGINE_OK;

  switch (composeMode) {
  case YMAGINE_COMPOSE_REPLACE:
    COMPOSE_LOOP_SCALE(source, width, bpp, overlay, overlaywidth, overlaybpp, composeReplace);
    break;
  case YMAGINE_COMPOSE_OVER:
    COMPOSE_LOOP_SCALE(source, width, bpp, overlay, overlaywidth, overlaybpp, composeOver);
    break;
  case YMAGINE_COMPOSE_UNDER:
    COMPOSE_LOOP_SCALE(source, width, bpp, overlay, overlaywidth, overlaybpp, composeUnder);
    break;
  case YMAGINE_COMPOSE_PLUS:
    COMPOSE_LOOP_SCALE(source, width, bpp, overlay, overlaywidth, overlaybpp, composePlus);
    break;
  case YMAGINE_COMPOSE_MINUS:
    COMPOSE_LOOP_SCALE(source, width, bpp, overlay, overlaywidth, overlaybpp, composeMinus);
    break;
  case YMAGINE_COMPOSE_ADD:
    COMPOSE_LOOP_SCALE(source, width, bpp, overlay, overlaywidth, overlaybpp, composeAdd);
    break;
  case YMAGINE_COMPOSE_SUBTRACT:
    COMPOSE_LOOP_SCALE(source, width, bpp, overlay, overlaywidth, overlaybpp, composeSubtract);
    break;
  case YMAGINE_COMPOSE_DIFFERENCE:
    COMPOSE_LOOP_SCALE(source, width, bpp, overlay, overlaywidth, overlaybpp, composeDifference);
    break;
  case YMAGINE_COMPOSE_BUMP:
    COMPOSE_LOOP_SCALE(source, width, bpp, overlay, overlaywidth, overlaybpp, composeBump);
    break;
  case YMAGINE_COMPOSE_MAP:
    COMPOSE_LOOP_SCALE(source, width, bpp, overlay, overlaywidth, overlaybpp, composeMap);
    break;
  case YMAGINE_COMPOSE_MIX:
    COMPOSE_LOOP_SCALE(source, width, bpp, overlay, overlaywidth, overlaybpp, composeMix);
    break;
  case YMAGINE_COMPOSE_MULT:
    COMPOSE_LOOP_SCALE(source, width, bpp, overlay, overlaywidth, overlaybpp, composeMult);
    break;
  case YMAGINE_COMPOSE_LUMINANCE:
    COMPOSE_LOOP_SCALE(source, width, bpp, overlay, overlaywidth, overlaybpp, composeLuminance);
    break;
  case YMAGINE_COMPOSE_LUMINANCEINV:
    COMPOSE_LOOP_SCALE(source, width, bpp, overlay, overlaywidth, overlaybpp, composeLuminanceInv);
    break;
  case YMAGINE_COMPOSE_COLORIZE:
    COMPOSE_LOOP_SCALE(source, width, bpp, overlay, overlaywidth, overlaybpp, composeColorize);
    break;
  default:
    /* Wrong composition mode */
    ALOGE("Specified composition mode doesn't exist");
    rc = YMAGINE_ERROR;
    break;
  }

  return rc;
}

int
Ymagine_composeLine(unsigned char *srcdata, int srcbpp, int srcwidth,
                    unsigned char *maskdata, int maskbpp, int maskwidth,
                    int composeMode)
{
  int rc = YMAGINE_ERROR;

  if (srcwidth == maskwidth) {
    rc = composeLine(srcdata, maskdata,
                     srcbpp, maskbpp,
                     srcwidth, composeMode);
  } else {
    rc = composeLineScale(srcdata, maskdata,
                          srcbpp, maskbpp,
                          srcwidth, maskwidth, composeMode);
  }

  return rc;
}


int
Ymagine_composeColor(Vbitmap *vbitmap, int color,
             ymagineCompose composeMode)
{
    int rc = YMAGINE_ERROR;
    unsigned char *pixels = NULL;
    int width;
    int height;
    int pitch;
    int bpp;
    int i;
    int overlaybpp = 0;

    bpp = VbitmapBpp(vbitmap);
    if (bpp != 4) {
        // Not configured for bpp other than 4 (need alpha!)
        ALOGE("Failed compose (bitmap must be RGBA)");
        return rc;
    }

    if (VbitmapLock(vbitmap) != YMAGINE_OK) {
        return rc;
    }

    pixels = VbitmapRegionBuffer(vbitmap);
    width = VbitmapRegionWidth(vbitmap);
    height = VbitmapRegionHeight(vbitmap);
    pitch = VbitmapPitch(vbitmap);

    if (pixels != NULL) {
        unsigned char colorArray[4] = {
            (color >> RSHIFT) & CHANNEL_MASK,
            (color >> GSHIFT) & CHANNEL_MASK,
            (color >> BSHIFT) & CHANNEL_MASK,
            (color >> ASHIFT) & CHANNEL_MASK
        };
        for (i = 0; i < height; i++) {
            rc = composeLine(pixels + (i * pitch), colorArray, bpp,
                                overlaybpp, width, composeMode);
            if (rc == YMAGINE_ERROR) break;
        }
    }
    VbitmapUnlock(vbitmap);
    return rc;
}

int
Ymagine_composeImage(Vbitmap *vbitmap, Vbitmap *overlay,
             int x, int y, ymagineCompose composeMode)
{
    int rc = YMAGINE_ERROR;
    // o prefix means that property belongs to the overlay
    unsigned char *pixels = NULL;
    unsigned char *opixels = NULL;
    int width, owidth;
    int height, oheight;
    int pitch, opitch;
    int bpp, obpp;
    int j;
    int xstart, ystart, newwidth, newheight;

    bpp = VbitmapBpp(vbitmap);
    obpp = VbitmapBpp(overlay);
    if (bpp != 4 || obpp != 4) {
        // Not configured for bpp other than 4 (need alpha!)
        ALOGE("Failed compose (both bitmaps must be RGBA)");
        return rc;
    }

    if (VbitmapLock(vbitmap) != YMAGINE_OK) {
        return rc;
    }
    if (VbitmapLock(overlay) != YMAGINE_OK) {
        VbitmapUnlock(vbitmap);
        return rc;
    }

    pixels = VbitmapRegionBuffer(vbitmap);
    width = VbitmapRegionWidth(vbitmap);
    height = VbitmapRegionHeight(vbitmap);
    pitch = VbitmapPitch(vbitmap);

    opixels = VbitmapRegionBuffer(overlay);
    owidth = VbitmapRegionWidth(overlay);
    oheight = VbitmapRegionHeight(overlay);
    opitch = VbitmapPitch(overlay);

    rc = YMAGINE_OK;
    xstart = 0;
    ystart = 0;
    newwidth = 0;
    newheight = 0;
    // Calculate the horizontal part of the bounding box
    if (x < 0) {
        if (x + owidth < 0) {
            rc = YMAGINE_ERROR;
        } else {
            if (x + owidth > width) {
                xstart = 0;
                newwidth = width;
            } else {
                xstart = 0;
                newwidth = x + owidth;
            }
        }
    } else {
        if (x > width) {
            rc = YMAGINE_ERROR;
        } else {
            if (x + owidth > width) {
                xstart = x;
                newwidth = width - x; 
            } else {
                xstart = x;
                newwidth = owidth;
            }
        }
    }

    // Calculate the vertical part of the bounding box
    if (y < 0) {
        if (y + oheight < 0) {
            rc = YMAGINE_ERROR;
        } else {
            if (y + oheight > height) {
                ystart = 0;
                newheight = height;
            } else {
                ystart = 0;
                newheight = y + oheight;
            }
        }
    } else {
        if (y > height) {
            rc = YMAGINE_ERROR;
        } else {
            if (y + oheight > height) {
                ystart = y;
                newheight = height - y; 
            } else {
                ystart = y;
                newheight = oheight;
            }
        }
    }

    // if rc == YMAGINE_ERROR this means bounding box has a size of 0.
    if (pixels != NULL && opixels != NULL && rc != YMAGINE_ERROR) {
      unsigned char *iline = pixels + xstart * bpp + ystart * pitch;
      unsigned char *oline = opixels + ((xstart - x) * obpp) + ystart * opitch;

      for (j = 0; j < newheight; j++) {
        rc = composeLine(iline, oline, bpp, obpp, newwidth, composeMode);
        if (rc == YMAGINE_ERROR) break;

        iline += pitch;
        oline += opitch;
      }
    }

    VbitmapUnlock(vbitmap);
    VbitmapUnlock(overlay);

    return rc;
}

