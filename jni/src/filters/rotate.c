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

#include <math.h>

#define R_PI 3.14159265f

static int
Ymagine_rotateRaw(const unsigned char *idata,
                  int w, int h, int pitch, int bpp,
                  unsigned char *odata,
                  int ow, int oh, int opitch, int obpp,
                  int centerx, int centery, float angle,
                  uint32_t bgcolor)
{
  int x, y;
  int k;
  int xdif, ydif, xpm, ypm, xp, yp, xf, yf;
  const unsigned char *pixel00;
  const unsigned char *pixel01;
  const unsigned char *pixel10;
  const unsigned char *pixel11;
  const unsigned char *origin;
  unsigned char *dest;
  float sina, cosa;
  unsigned char background[4];
  int fast = 0;
  float anglerad;
  int ocenterx, ocentery;

  anglerad = (angle * R_PI) / 180.0f;

  sina = (float) (16.0f * sin(anglerad));
  cosa = (float) (16.0f * cos(anglerad));

  background[0] = YcolorRGBtoRed(bgcolor);
  background[1] = YcolorRGBtoGreen(bgcolor);
  background[2] = YcolorRGBtoBlue(bgcolor);
  background[3] = YcolorRGBtoAlpha(bgcolor);

  ocenterx = ow / 2;
  ocentery = oh / 2;

  for (y = 0; y < oh; y++) {
    ydif = ocentery - y;
    dest = odata + y * opitch;
    for (x = 0; x < ow; x++) {
      xdif = ocenterx - x;
      xpm = (int) (-xdif * cosa - ydif * sina);
      ypm = (int) (-ydif * cosa + xdif * sina);
      xp = centerx + (xpm >> 4);
      yp = centery + (ypm >> 4);

      /* Fractional part of the source coordinates */
      xf = xpm & 0x0f;
      yf = ypm & 0x0f;

      origin = idata + yp * pitch + xp * bpp;

      if (xp < 0 || yp < 0 || xp >= w || yp >= h) {
        pixel00 = background;
      } else {
        pixel00 = origin;
      }

      if (fast) {
        /* Fast (but inaccurate) nearest neighbor */
        for (k = 0; k < bpp; k++) {
          dest[k] = pixel00[k];
        }
      } else {
        /* Area weighting */
        if (xp + 1 < 0 || yp < 0 || xp + 1 >= w || yp >= h) {
          pixel10 = background;
        } else {
          pixel10 = origin + bpp;
        }
        if (xp < 0 || yp + 1 < 0 || xp >= w || yp + 1 >= h) {
          pixel01 = background;
        } else {
          pixel01 = origin + pitch;
        }
        if (xp + 1 < 0 || yp + 1 < 0 || xp + 1 >= w || yp + 1 >= h) {
          pixel11 = background;
        } else {
          pixel11 = origin + pitch + bpp;
        }

        for (k = 0; k < bpp; k++) {
          dest[k] = ( ( (16 - xf) * (16 - yf) * pixel00[k] ) +
                       ( xf * (16 - yf) * pixel10[k] ) +
                       ( (16 - xf) * yf * pixel01[k] ) +
                       ( xf * yf * pixel11[k] ) + 128) / 256;
        }
      }
      dest += bpp;
    }
  }

  return YMAGINE_OK;
}

int
Ymagine_rotate(Vbitmap *outbitmap, Vbitmap *vbitmap,
               int centerx, int centery, float angle)
{
  int width;
  int height;
  int pitch;
  int bpp;
  unsigned char *pixels;
  int owidth;
  int oheight;
  int opitch;
  int obpp;
  unsigned char *opixels;
  uint32_t bgcolor;

  if (vbitmap == NULL) {
    return YMAGINE_ERROR;
  }

  /* Default to transparent black background */
  bgcolor = YcolorRGBA(0x00, 0x00, 0x00, 0x00);

  if (VbitmapLock(vbitmap) == YMAGINE_OK) {
    pixels = VbitmapBuffer(vbitmap);
    width = VbitmapWidth(vbitmap);
    height = VbitmapHeight(vbitmap);
    pitch = VbitmapPitch(vbitmap);
    bpp = colorBpp(VbitmapColormode(vbitmap));

    if (VbitmapLock(outbitmap) == YMAGINE_OK) {
      opixels = VbitmapBuffer(outbitmap);
      owidth = VbitmapWidth(outbitmap);
      oheight = VbitmapHeight(outbitmap);
      opitch = VbitmapPitch(outbitmap);
      obpp = colorBpp(VbitmapColormode(outbitmap));
      
      if (bpp == obpp) {
        Ymagine_rotateRaw(pixels, width, height, pitch, bpp,
                          opixels, owidth, oheight, opitch, obpp,
                          centerx, centery, angle, bgcolor);
      }

      VbitmapUnlock(outbitmap);
    }

    VbitmapUnlock(vbitmap);
  }

  return YMAGINE_OK;
}
