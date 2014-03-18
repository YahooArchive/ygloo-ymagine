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

#define LOG_TAG "ymagine::color"

#include "ymagine_config.h"
#include "ymagine_priv.h"

#include <math.h>
#include <float.h>
#include <pthread.h>

#define ASHIFT 24
#define RSHIFT 16
#define GSHIFT 8
#define BSHIFT 0

#define HSHIFT 16
#define SSHIFT 8
#define VSHIFT 0

#define RGBA(r,g,b,a) \
( (((uint32_t) (r)) << RSHIFT) | \
(((uint32_t) (g)) << GSHIFT) | \
(((uint32_t) (b)) << BSHIFT) | \
(((uint32_t) (a)) << ASHIFT) )

#define HSVA(r,g,b,a) \
( (((uint32_t) (r)) << HSHIFT) | \
(((uint32_t) (g)) << SSHIFT) | \
(((uint32_t) (b)) << VSHIFT) | \
(((uint32_t) (a)) << ASHIFT) )


/* general resources about YUV formats:
 * https://wiki.videolan.org/YUV
 * http://linuxtv.org/downloads/v4l-dvb-apis/re29.html
 */


typedef int yuvcoeff;
typedef struct YUVTableStruct YUVTable;

struct YUVTableStruct {
  yuvcoeff cy[256];
  yuvcoeff crv[256];
  yuvcoeff cgu[256];
  yuvcoeff cgv[256];
  yuvcoeff cbu[256];
};



static YUVTable *converttbl = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


/**
 * initialize coefficient lookup table
 *
 * see http://www.fourcc.org/fccyvrgb.php#BOURGEOIS
 * for an explanation on why different coefficients and
 * a scaled Y might be used
 *
 * @return YMAGINE_OK on success
 */
int
ycolor_yuv_prepare()
{
  YUVTable *tbl;
  int i;

  pthread_mutex_lock(&mutex);

  if (converttbl == NULL) {
    tbl = Ymem_malloc(sizeof(YUVTable));

#if 0
    for (i=0;i<256;i++) {
      tbl->cy[i] =(yuvcoeff) i;
      tbl->crv[i]=(yuvcoeff) ( 1.402f   * (i-128));
      tbl->cgu[i]=(yuvcoeff) (-0.34414f * (i-128));
      tbl->cgv[i]=(yuvcoeff) (-0.71414f * (i-128));
      tbl->cbu[i]=(yuvcoeff) ( 1.772f   * (i-128));
    }
#else
    for (i=0;i<256;i++) {
      tbl->cy[i] =(yuvcoeff) ( 1.164  * (i-16) );
      tbl->crv[i]=(yuvcoeff) ( 1.596f * (i-128));
      tbl->cgu[i]=(yuvcoeff) (-0.391f * (i-128));
      tbl->cgv[i]=(yuvcoeff) (-0.813f * (i-128));
      tbl->cbu[i]=(yuvcoeff) ( 2.018f * (i-128));
    }
#endif

    converttbl = tbl;
  }

  pthread_mutex_unlock(&mutex);

  return YMAGINE_OK;
}


static YINLINE YOPTIMIZE_SPEED
int CLIP_TO_8(int x)
{
  return (x<=0 ? 0 : (x>=0xff ? 0xff : x));
}

static YINLINE YOPTIMIZE_SPEED void
yuv2rgb_write_scanline(const unsigned char* srcy, int ystride, int* drgba,
                       unsigned char* outp, int colormode, int downscale)
{
  int yvalue;

  if (downscale) {
    yvalue=converttbl->cy[ (srcy[0]+srcy[1]+srcy[ystride]+srcy[ystride+1])/4 ];
  } else {
    yvalue=converttbl->cy[srcy[0]];
  }

  outp[0] = CLIP_TO_8(drgba[0]+yvalue);
  outp[1] = CLIP_TO_8(drgba[1]+yvalue);
  outp[2] = CLIP_TO_8(drgba[2]+yvalue);

  if (colormode == VBITMAP_COLOR_RGBA) {
    outp[3] = drgba[3];
  }
}

/**
 * @brief Convert a given YUV pixel buffer u,v,y into an RGB(A) buffer odata.
 *
 * Function to walk a YUV encoded image line by line, collect the Y, U and V for
 * each pixel, compute the RGB values and then write the RGB pixel into the output buffer.
 * Different YUV formats (one plane after the other vs interleaved planes, Y plane
 * twice the size of U/V planes) can be handled by giving the correct y, u and v pointer
 * as well as different steppings (e.g. move forward on odd, back on even to get a zig zag pattern).
 *
 * @param u beginning of U plane
 * @param ustride length (in bytes) of a scanline in the U plane
 * @param ubpp number of bytes to move to find next U value
 * @param v beginning of V plane
 * @param vstride length (in bytes) of a scanline in the V plane
 * @param vbpp number of bytes to move to find next V value
 * @param y beginning of Y plane
 * @param ystride length (in bytes) of a scanline in the Y plane
 * @param ybppodd number of bytes to move to find next Y value,
 *        if current pixel has an odd width coordinate
 * @param ybppeven number of bytes to move to find next Y value
 *        if current pixel has an even width coordinate
 *
 * @param odata output buffer (must be allocated by the caller)
 * @param ostride length (in bytes) of a scanline in the output buffer
 * @param obppodd number of bytes to move to find next pixel,
 *        if current pixel has an odd width coordinate
 * @param obppeven number of bytes to move to find next pixel,
 *        if current pixel has an even width coordinate
 * @param colormode of output buffer, e.g. VBITMAP_COLOR_RGB
 *
 * @param blen ratio between Y and U/V sampling
 * @param width of the output image
 * @param height of the output image
 *
 * @return YMAGINE_OK on success
 */
static YINLINE YOPTIMIZE_SPEED int
yuv2rgb(const unsigned char *u, int ustride, int ubpp,
        const unsigned char *v, int vstride, int vbpp,
        const unsigned char *y, int ystride, int ybppodd, int ybppeven,
        unsigned char *odata, int ostride, int obppodd, int obppeven, int colormode,
        int blen, YUVTable *converttbl,
        int width, int height, int downscale)
{
  int i,j;
  int ilen;
  const unsigned char *srcu, *srcv, *srcy;
  unsigned char *outp;
  int drgba[4];

  for (j=0;j<height;j++) {
    srcy = y + ystride * j;
    srcu = u + ustride * j;
    srcv = v + vstride * j;

    outp = odata + ostride * j;

    drgba[0] = converttbl->crv[*srcv];
    drgba[1] = converttbl->cgu[*srcu] + converttbl->cgv[*srcv];
    drgba[2] = converttbl->cbu[*srcu];
    drgba[3] = 0xff;

    for (i=0, ilen=0;i<width;i++,ilen++) {
      if (ilen==blen) {
        srcu+=ubpp;
        srcv+=vbpp;
        ilen=0;

        drgba[0] = converttbl->crv[*srcv];
        drgba[1] = converttbl->cgu[*srcu] + converttbl->cgv[*srcv];
        drgba[2] = converttbl->cbu[*srcu];
      }

      yuv2rgb_write_scanline(srcy, ystride, drgba, outp, colormode, downscale);

      outp += obppeven;
      srcy += ybppeven;
      i++;
      ilen++;

      if (ilen==blen) {
        srcu+=ubpp;
        srcv+=vbpp;
        ilen=0;

        drgba[0] = converttbl->crv[*srcv];
        drgba[1] = converttbl->cgu[*srcu] + converttbl->cgv[*srcv];
        drgba[2] = converttbl->cbu[*srcu];
      }

      yuv2rgb_write_scanline(srcy, ystride, drgba, outp, colormode, downscale);

      outp += obppodd;
      srcy += ybppodd;

    }
  }

  return YMAGINE_OK;
}



YOPTIMIZE_SPEED int
ycolor_nv21torgb(int width, int height, const unsigned char* indata, unsigned char* outdata, int colormode, int downscale)
{
  int outbpp;

  const unsigned char* Y = indata;
  const unsigned char* U = indata + (width*height) + 1;
  const unsigned char* V = indata + (width*height);
  unsigned char* O = outdata;

  int Ystride, Ustride, Vstride, Ostride;

  int Ybppodd, Ybppeve;
  int Ubpp;
  int Vbpp;
  int Obppodd, Obppeve;

  int blen;
  int adjustedWidth, adjustedHeight;

  int rc;

  if (colormode == VBITMAP_COLOR_RGB) {
    outbpp = 3;
  } else if (colormode == VBITMAP_COLOR_RGBA) {
    outbpp = 4;
  } else {
    ALOGE("unsupported pixel format");
    return YMAGINE_ERROR;
  }

  if (downscale == YMAGINE_SCALE_HALF_AVERAGE || downscale == YMAGINE_SCALE_HALF_QUICK) {
    Ystride = width*2;
    Ustride = width;
    Vstride = width;
    Ostride = (width/2)*outbpp;

    Ybppodd = 2;
    Ybppeve = 2;
    Ubpp = 2;
    Vbpp = 2;
    Obppodd = outbpp;
    Obppeve = outbpp;

    blen = 1;

    adjustedWidth = width/2;
    adjustedHeight = height/2;
    // walk a zig-zag pattern (scanning 2 Y lines at once)
    if (downscale == YMAGINE_SCALE_HALF_AVERAGE) {
      rc = yuv2rgb(U, Ustride, Ubpp,
                     V, Vstride, Vbpp,
                     Y, Ystride, Ybppodd, Ybppeve,
                     O, Ostride, Obppodd, Obppeve,
                     colormode, blen, converttbl, adjustedWidth, adjustedHeight, 1);
    } else {
      rc = yuv2rgb(U, Ustride, Ubpp,
                   V, Vstride, Vbpp,
                   Y, Ystride, Ybppodd, Ybppeve,
                   O, Ostride, Obppodd, Obppeve,
                   colormode, blen, converttbl, adjustedWidth, adjustedHeight, 0);
    }
  } else {
    Ystride = width*2;
    Ustride = width;
    Vstride = width;
    Ostride = width*outbpp*2;

    Ybppodd = -width+1;
    Ybppeve = width;
    Ubpp = 2;
    Vbpp = 2;
    Obppodd = -(width*outbpp)+outbpp;
    Obppeve = width*outbpp;

    blen = 4;

    adjustedWidth = width*2;
    adjustedHeight = height/2;
    // walk a zig-zag pattern (scanning 2 Y lines at once)
    rc = yuv2rgb(U, Ustride, Ubpp,
                   V, Vstride, Vbpp,
                   Y, Ystride, Ybppodd, Ybppeve,
                   O, Ostride, Obppodd, Obppeve,
                   colormode, blen, converttbl, adjustedWidth, adjustedHeight, 0);
  }

  return rc;
}

#define S_ONE 255

#define H_360 ((int) 256)
#define H_0   ((int) (0))
#define H_60  ((int) ((H_360 + 3) / 6))
#define H_120 ((int) ((H_360 + 1) / 3))
#define H_180 ((int) ((H_360 + 1) / 2))
#define H_240 ((int) (H_360 - H_120))
#define H_300 ((int) (H_360 - H_60))

#ifdef MIN
#  undef MIN
#endif
#define MIN(a,b) ((a) < (b) ? (a) : (b))

#ifdef MAX
#  undef MAX
#endif
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#ifdef ABS
#  undef ABS
#endif
#define ABS(a) ((a) > 0 ? (-a) : (a))

uint32_t
YcolorRGBA(int r, int g, int b, int a)
{
  return RGBA(r, g, b, a);
}

uint32_t
YcolorRGB(int r, int g, int b)
{
  return RGBA(r, g, b, 0xff);
}

uint32_t
YcolorHSVA(int h, int s, int v, int a)
{
  return HSVA(h, s, v, a);
}

uint32_t
YcolorHSV(int h, int s, int v)
{
  return HSVA(h, s, v, 0xff);
}

int
YcolorHSVtoHue(uint32_t hsv)
{
  return (int) ((hsv >> HSHIFT) & 0xff);
}

int
YcolorHSVtoSaturation(uint32_t hsv)
{
  return (int) ((hsv >> SSHIFT) & 0xff);
}

int
YcolorHSVtoBrightness(uint32_t hsv)
{
  return (int) ((hsv >> VSHIFT) & 0xff);
}

int
YcolorHSVtoAlpha(uint32_t hsv)
{
  return (int) ((hsv >> ASHIFT) & 0xff);
}

int
YcolorRGBtoRed(uint32_t rgb)
{
  return (int) ((rgb >> RSHIFT) & 0xff);
}

int
YcolorRGBtoGreen(uint32_t rgb)
{
  return (int) ((rgb >> GSHIFT) & 0xff);
}

int
YcolorRGBtoBlue(uint32_t rgb)
{
  return (int) ((rgb >> BSHIFT) & 0xff);
}

int
YcolorRGBtoAlpha(uint32_t rgb)
{
  return (int) ((rgb >> ASHIFT) & 0xff);
}

uint32_t YcolorRGBtoHSV(uint32_t rgb)
{
  int r, b, g, a;
  int h, s, v;
  int min, max, chroma;

  a = (rgb >> ASHIFT) & 0xff;
  r = (rgb >> RSHIFT) & 0xff;
  g = (rgb >> GSHIFT) & 0xff;
  b = (rgb >> BSHIFT) & 0xff;

  min = MIN(MIN(r, g), b);
  max = MAX(MAX(r, g), b);

  if (max == min) {
    /* Grayscale */
    h = 0;
    s = 0;
    v = max;
  } else {
    chroma = (max - min);

    v = max;
    s = (chroma * S_ONE) / max;

    if (r == max) {
      h = H_0 + ((g - b) * H_60) / chroma;
    }
    else if (g == max) {
      h = H_120 + ((b - r) * H_60) / chroma;
    }
    else {
      h = H_240 + ((r - g) * H_60) / chroma;
    }

    if (h < 0) {
      h += H_360;
    }
  }

  /* Return color as an HSV uint32 */
  return HSVA(h, s, v, a);
}

uint32_t YcolorHSVtoRGB(uint32_t hsv)
{
  int r, b, g, a;
  int h, s, v;
  int region;
  int fpart, p, q, t;

  a = (hsv >> ASHIFT) & 0xff;
  h = (hsv >> HSHIFT) & 0xff;
  s = (hsv >> SSHIFT) & 0xff;
  v = (hsv >> VSHIFT) & 0xff;

  if(s == 0) {
    /* Grayscale */
    r = v;
    g = v;
    b = v;
  } else {
    /* Compute region of hue (from 0 to 5) */
    if (h >= H_240) {
      region = 4;
      fpart = h - H_240;
    } else if (h >= H_120) {
      region = 2;
      fpart = h - H_120;
    } else {
      region = 0;
      fpart = h;
    }

    if (fpart >= H_60) {
      region ++;
      fpart -= H_60;
    }
    if (fpart >= H_60) {
      fpart = H_60;
    }

    p = (v * (S_ONE - s)) / S_ONE;
    q = (v * (S_ONE - ((s * fpart) / H_60))) / S_ONE;
    t = (v * (S_ONE - ((s * (H_60 - fpart) / H_60)))) / S_ONE;
        
    /* Set RGB depending on the cone region */
    switch(region) {
    case 0:
      r = v;
      g = t;
      b = p;
      break;
    case 1:
      r = q;
      g = v;
      b = p;
      break;
    case 2:
      r = p;
      g = v;
      b = t;
      break;
    case 3:
      r = p;
      g = q;
      b = v;
      break;
    case 4:
      r = t;
      g = p;
      b = v;
      break;
    case 5:
    default:
      r = v;
      g = p;
      b = q;
      break;
    }
  }

  /* Return color as an RGB uint32 */
  return RGBA(r, g, b, a);
}

/*
  Compute the equivalent color (in ARGB format) of a temperature in Kelvin
*/
static int
getTemperatureColor(int k, float *rgb)
{
  float r, g, b;

  /* Approximation works only for temperature from 1000 to 40000 */
  if (k < 1000) {
    k = 1000;
  } else if (k > 40000) {
    k = 40000;
  }

  k = (k + 50) / 100;

  /* Red component */
  if (k <= 66) {
    r = 255.0;
  } else {
    /* Note: the R-squared value for this approximation is .988 */
    r = 329.698727446 * powf(k - 60, -0.1332047592);
  }

  /* Green */
  if (k <= 66) {
    /* Note: the R-squared value for this approximation is .996 */
    g = 99.4708025861 * logf(k) - 161.1195681661;
  } else {
    /* Note: the R-squared value for this approximation is .987 */
    g = 288.1221695283 * powf(k - 60, -0.0755148492);
  }

  /* Blue */
  if (k <= 19) {
    b = 0.0;
  } else if (k >= 66) {
    b = 255.0;
  } else {
    /* Note: the R-squared value for this approximation is .998 */
    b = 138.5177312231 * logf(k - 10) - 305.0447927307;
  }

  if (r < 0) {
    r = 0;
  } else if (r > 255.0f) {
    r = 255;
  }
  if (g < 0) {
    g = 0;
  } else if (g > 255.0f) {
    g = 255;
  }
  if (b < 0) {
    b = 0;
  } else if (b > 255.0f) {
    b = 255.0;
  }
  
  if (rgb != NULL) {
    rgb[0] = r;
    rgb[1] = g;
    rgb[2] = b;
  }

  return YMAGINE_OK;
}

/* Get RGB color from temperature in Kelvin */
uint32_t YcolorKtoRGB(int k)
{
  float rgb[3];
  int r, g, b;

  getTemperatureColor(k, rgb);

  r = (int) rgb[0];
  g = (int) rgb[1];
  b = (int) rgb[2];

  return YcolorRGBA(r, g, b, 0xff);
}
