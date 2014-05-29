/*
 Derived from superFast blur from Mario Klingemann:

 Copyright (c) 2011 Mario Klingemann

 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:

 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 Accurate, nearly gaussian stack blur:
 http://www.quasimondo.com/StackBlurForCanvas/StackBlurDemo.html
 Slightly faster, less accurante options:
 - superFast blur (radius 20, iterations 2)
 http://www.quasimondo.com/BoxBlurForCanvas/FastBlurDemo.html
 - stack box blur (radius 20, iterations 2)
 http://www.quasimondo.com/BoxBlurForCanvas/FastBlur2Demo.html
 - integral image blur
 http://www.quasimondo.com/IntegralImageForCanvas/IntegralImageBlurDemo.html
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

/*
 A fast, yet not gaussian blurring algorithm.

 To approximate gaussian blur accurately, one can call blurSuperfast with
 a small radius and higher number of iterations
 */
int
Ymagine_blurSuperfast(unsigned char *pix,
                      int w, int h, int pitch, int bpp,
                      int radius, int niter)
{
  unsigned char *dv = NULL;
  int *vbuf = NULL;
  unsigned char *rgba = NULL;
  int wm, hm;
  int wh;
  int div;
  unsigned char *r, *g, *b;
  int rsum, gsum, bsum;
  int x, y;
  int i, n;
  int p, p1, p2;
  int yi, zi, yw;
  int maxwh;
  int *vmin;
  int *vmax;
  int rc = YMAGINE_ERROR;

  if (radius <= 0 || niter <= 0) {
    return YMAGINE_OK;
  }
  if (w <= 0 || h <= 0) {
    return YMAGINE_OK;
  }
  if (bpp < 3) {
    return YMAGINE_ERROR;
  }

  maxwh = MAX(w, h);
  wm = w - 1;
  hm = h - 1;
  wh = w * h;
  div = radius + radius + 1;

  /* TODO: dont recalculate if calling multiple time with same radius */
  dv = (unsigned char*) Ymem_malloc(256*div*sizeof(dv[0]));
  if (dv == NULL) {
    goto cleanup;
  }
  vbuf = (int*) Ymem_malloc(maxwh * sizeof(vbuf[0]) * 2);
  if (vbuf == NULL) {
    goto cleanup;
  }
  rgba = Ymem_malloc(wh * 4);
  if (rgba == NULL) {
    goto cleanup;
  }

  for (i = 0 ; i < 256 * div; i++) {
    dv[i] = (i / div);
  }

  r = rgba;
  g = r + wh;
  b = g + wh;

  vmin = vbuf;
  vmax = vbuf + maxwh;

  for (n = 0; n < niter; n++) {
    yw = 0;

    for (y = 0; y < h; y++) {
      rsum = 0;
      gsum = 0;
      bsum = 0;

      yi = y * pitch;
      zi = y * w;

      for (i = -radius; i <= radius; i++) {
        p = yi + (MIN(wm, MAX(i,0)) * bpp);
        rsum += pix[p];
        gsum += pix[p+1];
        bsum += pix[p+2];
      }

      for (x = 0; x < w; x++) {
        r[zi] = dv[rsum];
        g[zi] = dv[gsum];
        b[zi] = dv[bsum];

        if (y == 0) {
          vmin[x] = MIN(x+radius+1, wm);
          vmax[x] = MAX(x-radius, 0);
        }
        p1 = yw+(vmin[x]*bpp);
        p2 = yw+(vmax[x]*bpp);

        rsum += pix[p1] - pix[p2];
        gsum += pix[p1+1] - pix[p2+1];
        bsum += pix[p1+2] - pix[p2+2];

        zi++;
      }

      yw += pitch;
    }

    for (x = 0; x < w; x++) {
      rsum = 0;
      gsum = 0;
      bsum = 0;

      for (i = -radius; i <= radius; i++) {
        yi = MIN(MAX(0,i),hm) * w + x;
        rsum += r[yi];
        gsum += g[yi];
        bsum += b[yi];
      }

      yi = x * bpp;
      for (y = 0; y < h; y++) {
        pix[yi] = dv[rsum];
        pix[yi + 1] = dv[gsum];
        pix[yi + 2] = dv[bsum];

        if (x == 0) {
          vmin[y] = MIN(y+radius+1, hm)*w;
          vmax[y] = MAX(y-radius, 0)*w;
        }
        p1 = x + vmin[y];
        p2 = x + vmax[y];

        rsum += r[p1]-r[p2];
        gsum += g[p1]-g[p2];
        bsum += b[p1]-b[p2];

        yi += pitch;
      }
    }
  }

  rc = YMAGINE_OK;

cleanup:
  if (rgba != NULL) {
    Ymem_free(rgba);
    rgba = NULL;
  }
  if (vbuf != NULL) {
    Ymem_free(vbuf);
    vbuf = NULL;
  }
  if (dv != NULL) {
    Ymem_free(dv);
    dv = NULL;
  }

  return rc;
}
