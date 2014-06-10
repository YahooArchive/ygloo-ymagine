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

static YINLINE YOPTIMIZE_SPEED
int CLIP_TO_8(int x)
{
  return (x<=0 ? 0 : (x>=0xff ? 0xff : x));
}

static unsigned char*
init_lookup_table(uint32_t refcolor, int brightness, int contrast)
{
  unsigned char *lookup;
  unsigned char *nextp;
  int red, green, blue, alpha;
  int i;
  int l;

  lookup = Ymem_malloc(256 * 4);
  if (lookup == NULL) {
    return NULL;
  }

  red = YcolorRGBtoRed(refcolor);
  green = YcolorRGBtoGreen(refcolor);
  blue = YcolorRGBtoBlue(refcolor);
  alpha = YcolorRGBtoAlpha(refcolor);

  nextp = lookup;
  for (i = 0; i < 256; i++) {
    l = CLIP_TO_8((i * brightness) / YFIXED_ONE + (contrast * 255) / YFIXED_ONE);

    nextp[0] = (l * red) / 255;
    nextp[1] = (l * green) / 255;
    nextp[2] = (l * blue) / 255;
    nextp[3] = alpha;

    nextp += 4;
  }

  return lookup;
}

/* Linear interpolation */
static YINLINE int
interpolate_linear(int vmin, int vmax, int cmin, int cmax, int c)
{
  if (c <= cmin) {
    return vmin;
  }
  if (c >= cmax) {
    return vmax;
  }
  
  return vmin + ((c - cmin) * (vmax - vmin)) / (cmax - cmin);
}

#define LUMINANCE_MAX 255

static int
map_luminance(int y, int height, int bright, int dark)
{
  int ymin;
  int ymax;
    
  /* Linear vertical gradient in top third of image */
  ymin = 0;
  ymax = height / 3;
  if (y >= ymin && y <= ymax) {
    return interpolate_linear(dark, bright, ymin, ymax, y);
  }

  ymin = (height * 2) / 3;
  ymax = height;
  if (y >= ymin && y < ymax) {
    return interpolate_linear(bright, dark, ymin, ymax, y);
  }

  return bright;
}

int YOPTIMIZE_SPEED
Ymagine_colorizeBuffer(unsigned char *pix,
                       int w, int h, int pitch, int bpp,
                       int color)
{
  int x, y;
  int r, g, b;
  uint32_t refcolor;
  uint32_t hsv;
  int hue;
  int rowlum;
  int lum;
  unsigned char *lookup;
  unsigned char *lcolor;
  unsigned char *nextp;
  int brightness = (YFIXED_ONE * 96) / 100;
  int contrast = (YFIXED_ONE * 12) / 100;

  if (bpp < 3 || bpp > 4) {
    return YMAGINE_ERROR;
  }

  if (w <= 0 || h <= 0) {
    return YMAGINE_OK;
  }

  hsv = YcolorRGBtoHSV(color);
  hue = YcolorHSVtoHue(hsv);
  refcolor = YcolorHSVtoRGB(YcolorHSV(hue, 180, 255));

  /* Prepare lookup table */
  lookup = init_lookup_table(refcolor, brightness, contrast);
  if (lookup == NULL) {
    return YMAGINE_ERROR;
  }

  for (y = 0; y < h; y++) {
    /* Luminance factor for this row, from 0 (x0.0, i.e. black) to LUMINANCE_MAX (x1.0, i.e. unchanged) */
    rowlum = map_luminance(y, h, YFIXED_ONE, (YFIXED_ONE * 28) / 100);
      
    nextp = pix + y * pitch;
    if (bpp == 1) {
      for (x = 0; x < w; x++) {
        nextp++;
      }
    } else if (bpp == 3 || bpp == 4) {
      for (x = 0; x < w; x++) {
        r = nextp[0];
        g = nextp[1];
        b = nextp[2];

        /* Luminance of current pixel */
        /* Y = 0.2126 R + 0.7152 G + 0.0722 B */
        lum = (54 * r + 183 * g + 19 * b) / 256;
        /* Or use fast approximation */
        // lum = (r + r + r + g + g + g + g + b) >> 3;
        /* Apply gradient */
        lum = (lum * rowlum) / YFIXED_ONE;
        lum = CLIP_TO_8(lum);

        /* TODO: if nextp is word aligned, can assign word directly */
        lcolor = lookup + lum * 4;
        if (bpp == 4 && nextp[3] != 0xff) {
          int alpha = nextp[3];

          nextp[0] = (lcolor[0] * alpha) / 255;
          nextp[1] = (lcolor[1] * alpha) / 255;
          nextp[2] = (lcolor[2] * alpha) / 255;
        } else {
          nextp[0] = lcolor[0];
          nextp[1] = lcolor[1];
          nextp[2] = lcolor[2];
        }

        nextp += bpp;        
      }
    }
  }

  Ymem_free(lookup);

  return YMAGINE_OK;
}

int
Ymagine_colorize(Vbitmap *vbitmap, int color)
{
  int rc = YMAGINE_ERROR;

  if (VbitmapLock(vbitmap) >= 0) {
    unsigned char *pixels = VbitmapBuffer(vbitmap);
    int width = VbitmapWidth(vbitmap);
    int height = VbitmapHeight(vbitmap);
    int pitch = VbitmapPitch(vbitmap);
    int bpp = colorBpp(VbitmapColormode(vbitmap));

    if (Ymagine_colorizeBuffer(pixels,
                               width, height, pitch, bpp,
                               color) == YMAGINE_OK) {
      rc = YMAGINE_OK;
    }

    VbitmapUnlock(vbitmap);
  }

  return rc;
}
