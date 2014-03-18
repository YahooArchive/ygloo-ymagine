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

#define LOG_TAG "ymagine::seam"

/* Some alternative implementation:
   https://github.com/KirillLykov/cvision-algorithms/blob/master/src/seamCarving.m
*/

#define DEBUG 1

#include "ymagine/ymagine.h"
#include "ymagine_priv.h"

#include <math.h>

static YINLINE YOPTIMIZE_SPEED
int CLIP_TO_8(int x)
{
  return (x<=0 ? 0 : (x>=0xff ? 0xff : x));
}

#define isqrt(x) ((int) sqrt(x))
#define iabs(x) (((x) >= 0) ? (x) : (-(x)))

 
// Computes the x component of the gradient vector
// at a given point in a image.
// returns gradient in the x direction

#define GETPIX(p, bpp, pitch, dx, dy) ((int) p[dx * bpp + dy * pitch])

static YINLINE YOPTIMIZE_SPEED
int gradientXBase(unsigned char *p, int bpp, int pitch,
                  int px, int nx, int py, int ny)
{
  return \
    +     GETPIX(p, bpp, pitch, px, py)
    + 2 * GETPIX(p, bpp, pitch, px,  0)
    +     GETPIX(p, bpp, pitch, px, ny)
    -     GETPIX(p, bpp, pitch, nx, py)
    - 2 * GETPIX(p, bpp, pitch, nx,  0)
    -     GETPIX(p, bpp, pitch, nx, ny);
}

static YINLINE YOPTIMIZE_SPEED
int gradientYBase(unsigned char *p, int bpp, int pitch,
                  int px, int nx, int py, int ny)
{
  return \
    +     GETPIX(p, bpp, pitch, px, py)
    + 2 * GETPIX(p, bpp, pitch,  0, py)
    +     GETPIX(p, bpp, pitch, nx, py)
    -     GETPIX(p, bpp, pitch, px, ny)
    - 2 * GETPIX(p, bpp, pitch,  0, ny)
    -     GETPIX(p, bpp, pitch, nx, ny);
}

static YINLINE YOPTIMIZE_SPEED
int gradientX(unsigned char *p, int bpp, int pitch)
{
  int px = -1;
  int nx = 1;
  int py = -1;
  int ny = 1;

  return gradientXBase(p, bpp, pitch, px, nx, py, ny);
}

static YINLINE YOPTIMIZE_SPEED
int gradientY(unsigned char *p, int bpp, int pitch)
{
  int px = -1;
  int nx = 1;
  int py = -1;
  int ny = 1;

  return gradientYBase(p, bpp, pitch, px, nx, py, ny);
}

static YINLINE YOPTIMIZE_SPEED
int gradientXCheck(unsigned char *p, int bpp, int pitch,
                   int x, int y, int width, int height)
{
  int px = -1;
  int nx = 1;
  int py = -1;
  int ny = 1;

  if (x <= 0) {
    px = 0;
  } else if (x >= width - 1) {
    nx = 0;
  }
  if (y == 0) {
    py = 0;
  } else if (y >= height - 1) {
    ny = 0;
  }

  return gradientXBase(p, bpp, pitch, px, nx, py, ny);
}

static YINLINE YOPTIMIZE_SPEED
int gradientYCheck(unsigned char *p, int bpp, int pitch,
                   int x, int y, int width, int height)
{
  int px = -1;
  int nx = 1;
  int py = -1;
  int ny = 1;

  if (x <= 0) {
    px = 0;
  }
  if (x >= width - 1) {
    nx = 0;
  }
  if (y == 0) {
    py = 0;
  }
  if (y >= height - 1) {
    ny = 0;
  }

  return gradientYBase(p, bpp, pitch, px, nx, py, ny);
}

YINLINE YOPTIMIZE_SPEED
int
EnergySobelFast(unsigned char *inp, int bpp, int pitch)
{
  int dx0, dx1, dx2;
  int dy0, dy1, dy2;
  int dx = 0;
  int dy = 0;
  int s2 = 0;

  if (1) {
    dx0 = gradientX(inp, bpp, pitch);
    dx1 = gradientX(inp + 1, bpp, pitch);
    dx2 = gradientX(inp + 2, bpp, pitch);
    dx = (dx0 + 2 * dx1 + dx2) / 4;
    s2 += dx * dx;
  }
  if (1) {
    dy0 = gradientY(inp, bpp, pitch);
    dy1 = gradientY(inp + 1, bpp, pitch);
    dy2 = gradientY(inp + 2, bpp, pitch);
    dy = (dy0 + 2 * dy1 + dy2) / 4;
    s2 += dy * dy;
  }

  return CLIP_TO_8(isqrt(s2));
}

YINLINE YOPTIMIZE_SPEED
int
EnergySobel(unsigned char *inp, int bpp, int pitch,
            int x, int y, int width, int height)
{
  int dx0, dx1, dx2;
  int dy0, dy1, dy2;
  int dx = 0;
  int dy = 0;
  int s2 = 0;

  if (1) {
    dx0 = gradientXCheck(inp, bpp, pitch, x, y, width, height);
    dx1 = gradientXCheck(inp + 1, bpp, pitch, x, y, width, height);
    dx2 = gradientXCheck(inp + 2, bpp, pitch, x, y, width, height);
    dx = (dx0 + 2 * dx1 + dx2) / 4;
    s2 += dx * dx;
  }
  if (1) {
    dy0 = gradientYCheck(inp, bpp, pitch, x, y, width, height);
    dy1 = gradientYCheck(inp + 1, bpp, pitch, x, y, width, height);
    dy2 = gradientYCheck(inp + 2, bpp, pitch, x, y, width, height);
    dy = (dy0 + 2 * dy1 + dy2) / 4;
    s2 += dy * dy;
  }

  return CLIP_TO_8(isqrt(s2));
}

int
Vbitmap_sobel(Vbitmap *outbitmap, Vbitmap *vbitmap)
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

  int i, j;

  unsigned char *inp;
  unsigned char *outp;

  if (vbitmap == NULL) {
    return YMAGINE_ERROR;
  }

  if (VbitmapLock(vbitmap) >= 0) {
    pixels = VbitmapBuffer(vbitmap);
    width = VbitmapWidth(vbitmap);
    height = VbitmapHeight(vbitmap);
    pitch = VbitmapPitch(vbitmap);
    bpp = colorBpp(VbitmapColormode(vbitmap));

    if (VbitmapLock(outbitmap) >= 0) {
      opixels = VbitmapBuffer(outbitmap);
      owidth = VbitmapWidth(outbitmap);
      oheight = VbitmapHeight(outbitmap);
      opitch = VbitmapPitch(outbitmap);
      obpp = colorBpp(VbitmapColormode(outbitmap));

      if (width != owidth || height != oheight) {
        VbitmapUnlock(outbitmap);
        if (VbitmapResize(outbitmap, width, height) == YMAGINE_OK) {
          if (VbitmapLock(outbitmap) < 0) {
            VbitmapUnlock(vbitmap);
            return YMAGINE_ERROR;
          }

          opixels = VbitmapBuffer(outbitmap);
          owidth = VbitmapWidth(outbitmap);
          oheight = VbitmapHeight(outbitmap);
          opitch = VbitmapPitch(outbitmap);
          obpp = colorBpp(VbitmapColormode(outbitmap));
        }
      }

      if (width == owidth && height == oheight && bpp >= 3) {
        for (j = 0; j < height; j++) {
          inp = pixels + pitch * j;
          outp = opixels + opitch * j;

          outp[0] = EnergySobel(inp, bpp, pitch, 0, j, width, height);
          outp += obpp;
          inp += bpp;

          if (j != 0 && j != height-1) {
            for (i = 1; i < width - 1; i++) {
              outp[0] = EnergySobelFast(inp, bpp, pitch);
              outp += obpp;
              inp += bpp;
            }
          } else {
            for (i = 1; i < width - 1; i++) {
              outp[0] = EnergySobel(inp, bpp, pitch, i, j, width, height);
              outp += obpp;
              inp += bpp;
            }
          }

          outp[0] = EnergySobel(inp, bpp, pitch, width - 1, j, width, height);
        }

        if (obpp >= 3) {
          for (j = 0; j < height; j++) {
            outp = opixels + opitch * j;
            for (i = 0; i < width; i++) {              
              outp[1] = outp[0];
              outp[2] = outp[0];
              if (obpp == 4) {                
                outp[0] = 0xff;
              }

              outp += obpp;
            }
          }
        }
      }

      VbitmapUnlock(outbitmap);
    }

    VbitmapUnlock(vbitmap);
  }

  return YMAGINE_OK;
}

