/*
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
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

#define LOG_TAG "ymagine::transformer"
#include "yosal/yosal.h"
#include "ymagine_priv.h"

#define TRANSFORMER_CONVOLUTION_NONE      0
#define TRANSFORMER_CONVOLUTION_SHARPEN   1
#define TRANSFORMER_CONVOLUTION_GENERAL   2

YOSAL_OBJECT_DECLARE(Transformer)
YOSAL_OBJECT_BEGIN
  /* Total dimension of input. srch lines of srcw pixels must be pushed in Transformer */
  int srcw;
  int srch;

  /* Total dimension of output. desth lines of destw pixels will be sent to writer */
  int destw;
  int desth;

  int cury;
  int curyf;

  int nexty;
  int nextyf;

  int desty;
  int stashedweight;

  /* Input region */
  Vrect srcrect;

  /* Output region */
  Vrect destrect;

  /* transformer state */
  int srcline;
  int srcmode;
  int srcbpp;

  int destmode;
  int destbpp;
  int destpitch;

  /* 3x3 convolution */
  int kernel[9];
  int convmode;
  int convlinecount;
  unsigned char *convcprev;
  unsigned char *convcur;
  unsigned char *convnext;

  /* Statistics collector */
  int statsmode;
  int statscount;
  int *statsbuf;
  int *histr;
  int *histg;
  int *histb;
  int *histlum;

  unsigned char *destbuf;
  unsigned char *destaligned;

  unsigned char *scaledbuf;
  unsigned char *curbuf;
  unsigned char *tmpbuf;

  int *bltmap;

  PixelShader *shader;
  float sharpen;

  /* Default writer to Vbitmap */
  Vbitmap *obitmap;
  unsigned char *obuffer;
  int offsetx;
  int offsety;
  int owidth;
  int oheight;
  int opitch;
  int omode;
  int obpp;
  int opremultiplied;

  /* Transformer writer callback */
  TransformerWriterFunc writer;
  void *writerdata;
YOSAL_OBJECT_END


/*
 * Scale number from [0..inmax[ range to [0..outmax[
 */
static YINLINE YOPTIMIZE_SPEED int
scaleFixedPoint(int n, int inmax, int outmax)
{
  return (int) ((((uint64_t) n) * YFIXED_ONE * outmax) / inmax);
}

/* Scale scanline. Pixels must be in RGB or RGBX format */
static int
bltLinePrepare(int *map, int owidth, int iwidth)
{
  int i;

  for (i = 0; i < owidth; i++) {
    map[i] = scaleFixedPoint(i + 1, owidth, iwidth);
  }

  return YMAGINE_OK;
}

static YINLINE YOPTIMIZE_SPEED int
bltLineExt(unsigned char *opixels, int owidth, int obpp,
           const unsigned char *ipixels, int iwidth, int ibpp,
           int *map)
{
  const unsigned char *srcptr;
  unsigned char *destptr;
  int i, j;
  int f0, f1;
  int j0, j1;
  int rgba[4];
  int w, wtotal;

  if (ibpp != obpp && (ibpp < 3 || obpp < 3)) {
    /* Color space conversion not yet implemented */
    return YMAGINE_ERROR;
  }

  if (owidth <= 0) {
    return YMAGINE_OK;
  }
  if (iwidth <= 0) {
    return YMAGINE_ERROR;
  }

  if (owidth == iwidth && ibpp == obpp) {
    memcpy(opixels, ipixels, owidth * obpp);
  } else {
    destptr = opixels;
    srcptr = ipixels;

    f1 = YFIXED_ZERO;
    j1 = 0;

    rgba[0] = 0;
    rgba[1] = 0;
    rgba[2] = 0;
    rgba[3] = 0xff;

    for (i = 0; i < owidth; i++) {
      f0 = f1;
      j0 = j1;

      if (map != NULL) {
        f1 = map[i];
      } else {
        f1 = scaleFixedPoint(i + 1, owidth, iwidth);
      }
      j1 = Y_INT(f1);

      wtotal = 0;

      w = YFIXED_ONE - Y_FRAC(f0);
      rgba[0] = srcptr[0] * w;
      if (obpp == 3 || obpp == 4) {
        rgba[1] = srcptr[1] * w;
        rgba[2] = srcptr[2] * w;
      }
      wtotal += w;

      if (j1 > j0) {
        w = YFIXED_ONE;
        for (j = j0 + 1; j < j1; j++) {
          srcptr += ibpp;
          rgba[0] += srcptr[0] * w;
          if (obpp == 3 || obpp == 4) {
            rgba[1] += srcptr[1] * w;
            rgba[2] += srcptr[2] * w;
          }
          wtotal += w;
        }

        srcptr += ibpp;

        w = Y_FRAC(f1);
        if (w > 0) {
          rgba[0] += srcptr[0] * w;
          if (obpp == 3 || obpp == 4) {
            rgba[1] += srcptr[1] * w;
            rgba[2] += srcptr[2] * w;
          }
          wtotal += w;
        }
      }

      destptr[0] = rgba[0] / wtotal;
      if (obpp == 3 || obpp == 4) {
        destptr[1] = rgba[1] / wtotal;
        destptr[2] = rgba[2] / wtotal;
      }
      if (obpp == 4) {
        destptr[3] = 0xff;
      }
      destptr += obpp;
    }
  }

  return YMAGINE_OK;
}

int
bltLine(unsigned char *opixels, int owidth, int obpp,
        const unsigned char *ipixels, int iwidth, int ibpp)
{
  return bltLineExt(opixels, owidth, obpp, ipixels, iwidth, ibpp, NULL);
}

static YINLINE YOPTIMIZE_SPEED int
mergeLine(unsigned char *destpixels, int destweight,
          const unsigned char *srcpixels, int srcweight,
          int bpp, int width) {
  int i;
  int j;
  for (i = 0; i < width; i++) {
    for (j = 0; j < bpp; j++) {
      int pixel;
      int idx = i*bpp + j;
      pixel = (((int)srcpixels[idx])*srcweight + ((int)destpixels[idx])*destweight) / (srcweight + destweight);
      destpixels[idx] = (unsigned char)(pixel);
    }
  }

  return YMAGINE_OK;
}

YOPTIMIZE_SPEED int
YmagineMergeLine(unsigned char *destpixels, int destbpp, int destweight,
                 const unsigned char *srcpixels, int srcbpp, int srcweight,
                 int width)
{
  int bpp;
  int rc = YMAGINE_ERROR;

  if (destweight < 0 || srcweight < 0 ||
      destbpp <= 0 || srcbpp <= 0 ||
      destpixels == NULL || srcpixels == NULL) {
    return rc;
  }

  if (destbpp != srcbpp) {
    /* now only supports merge lines with same bpp */
    return rc;
  }

  bpp = destbpp;

  if (srcweight == 0) {
    /* Nothing to do */
    rc = YMAGINE_OK;
  } else if (destweight == 0) {
    memcpy(destpixels, srcpixels, width * bpp * sizeof(unsigned char));
    rc = YMAGINE_OK;
  } else {
    switch (bpp) {
      case 1:
        rc = mergeLine(destpixels, destweight, srcpixels, srcweight, 1, width);
        break;
      case 3:
        rc = mergeLine(destpixels, destweight, srcpixels, srcweight, 3, width);
        break;
      case 4:
        rc = mergeLine(destpixels, destweight, srcpixels, srcweight, 4, width);
        break;
      default:
        rc = mergeLine(destpixels, destweight, srcpixels, srcweight, bpp, width);
        break;
    }
  }

  return rc;
}

/*
 * API to manage shader effects
 */
static void
TransformerReleaseCallback(void *ptr)
{
  Transformer *transformer = (Transformer*)ptr;

  if (transformer == NULL) {
    return;
  }

  if (transformer->destbuf != NULL) {
    Ymem_free(transformer->destbuf);
    transformer->destbuf = NULL;
    transformer->destaligned = NULL;
  }

  if (transformer->bltmap != NULL) {
    Ymem_free(transformer->bltmap);
    transformer->bltmap = NULL;
  }

  if (transformer->statsbuf != NULL) {
    Ymem_free(transformer->statsbuf);
    transformer->statsbuf = NULL;
  }

  Ymem_free(transformer);
}

Transformer*
TransformerCreate()
{
  Transformer* transformer;

  transformer = (Transformer*) yobject_create(sizeof(Transformer),
                                            TransformerReleaseCallback);
  if (transformer == NULL) {
    return NULL;
  }

  transformer->srcw = 0;
  transformer->srch = 0;

  transformer->destw = 0;
  transformer->desth = 0;

  transformer->nexty = 0;
  transformer->nextyf = YFIXED_ZERO;

  transformer->cury = transformer->nexty;
  transformer->curyf = transformer->nextyf;

  transformer->desty = -1;
  transformer->stashedweight = YFIXED_ZERO;

  transformer->srcrect.x = 0;
  transformer->srcrect.y = 0;
  transformer->srcrect.width = 0;
  transformer->srcrect.height = 0;

  transformer->destrect.x = 0;
  transformer->destrect.y = 0;
  transformer->destrect.width = 0;
  transformer->destrect.height = 0;

  /* 3x3 convolution */
  transformer->convmode = TRANSFORMER_CONVOLUTION_NONE;
  transformer->convlinecount = 0;
  transformer->convcprev = NULL;
  transformer->convcur = NULL;
  transformer->convnext = NULL;

  /* transformer state */
  transformer->srcline = -1;
  transformer->srcmode = VBITMAP_COLOR_RGBA;
  transformer->srcbpp = colorBpp(transformer->srcmode);

  transformer->destmode = VBITMAP_COLOR_RGBA;
  transformer->destbpp = colorBpp(transformer->destmode);
  transformer->destpitch = 0;

  transformer->destbuf = NULL;
  transformer->destaligned = NULL;

  transformer->statsmode = 0;
  transformer->statscount = 0;
  transformer->statsbuf = NULL;
  transformer->histr = NULL;
  transformer->histg = NULL;
  transformer->histb = NULL;
  transformer->histlum = NULL;

  transformer->scaledbuf = NULL;
  transformer->curbuf = NULL;
  transformer->tmpbuf = NULL;

  transformer->bltmap = NULL;

  transformer->obitmap = NULL;
  TransformerSetBitmap(transformer, NULL, 0, 0);

  transformer->shader = NULL;
  transformer->sharpen = 0.0f;

  /* Transformer writer callback */
  transformer->writer = NULL;
  transformer->writerdata = NULL;

  return transformer;
}

Transformer*
TransformerRetain(Transformer *transformer)
{
  return (Transformer*) yobject_retain((yobject*) transformer);
}

int
TransformerRelease(Transformer *transformer)
{
  TransformerSetBitmap(transformer, NULL, 0, 0);

  if (yobject_release((yobject*) transformer) != YOSAL_OK) {
    return YMAGINE_ERROR;
  }

  return YMAGINE_OK;
}

int
TransformerSetWriter(Transformer *transformer, TransformerWriterFunc writer, void *writerdata)
{
  if (transformer == NULL) {
    return YMAGINE_OK;
  }

  transformer->writer = writer;
  transformer->writerdata = writerdata;

  return YMAGINE_OK;
}

int
TransformerSetBitmap(Transformer *transformer, Vbitmap *vbitmap,
                     int offsetx, int offsety)
{
  int rc = YMAGINE_OK;

  if (transformer == NULL) {
    return YMAGINE_ERROR;
  }

  if (transformer->obitmap != NULL) {
    VbitmapUnlock(transformer->obitmap);
  }

  if (vbitmap != NULL) {
    rc = VbitmapLock(vbitmap);
    if (rc != YMAGINE_OK) {
      vbitmap = NULL;
    }
  }

  transformer->obitmap = vbitmap;

  if (vbitmap == NULL) {
    transformer->obitmap = NULL;
    transformer->obuffer = NULL;
    transformer->owidth = 0;
    transformer->oheight = 0;
    transformer->opitch = 0;
    transformer->omode = VBITMAP_COLOR_RGBA;
    transformer->obpp = 0;
    transformer->opremultiplied = 0;
    transformer->offsetx = 0;
    transformer->offsety = 0;
  } else {
    transformer->obitmap = vbitmap;
    transformer->obuffer = VbitmapBuffer(vbitmap);
    transformer->owidth = VbitmapWidth(vbitmap);
    transformer->oheight = VbitmapHeight(vbitmap);
    transformer->opitch = VbitmapPitch(vbitmap);
    transformer->omode = VbitmapColormode(vbitmap);
    transformer->obpp = colorBpp(transformer->omode);
    transformer->opremultiplied = 0;
    if (transformer->omode == VBITMAP_COLOR_RGBA && transformer->obuffer != NULL) {
      transformer->opremultiplied = 1;
    }
    transformer->offsetx = offsetx;
    transformer->offsety = offsety;
  }

  return rc;
}

/*
 * Guarantee is that if srch lines are pushed in, exactly dsth lines will be pushed out
 */
int
TransformerSetScale(Transformer *transformer,
                    int srcw, int srch,
                    int dstw, int dsth)
{
  transformer->srcw = srcw;
  transformer->srch = srch;
  transformer->destw = dstw;
  transformer->desth = dsth;

  return YMAGINE_OK;
}

int
TransformerSetRegion(Transformer *transformer,
                     int srcx, int srcy,
                     int srcw, int srch)
{
  transformer->srcrect.x = srcx;
  transformer->srcrect.y = srcy;
  transformer->srcrect.width = srcw;
  transformer->srcrect.height = srch;

  return YMAGINE_OK;
}

int
TransformerSetMode(Transformer *transformer, int srcmode, int destmode)
{
  if (transformer == NULL) {
    return YMAGINE_ERROR;
  }
  
  transformer->srcmode = srcmode;
  transformer->destmode = destmode;

  return YMAGINE_OK;
}

int
TransformerSetStats(Transformer *transformer, int statsmode)
{
  if (transformer == NULL) {
    return YMAGINE_ERROR;
  }

  transformer->statsmode = statsmode;

  return YMAGINE_OK;
}

int
TransformerSetKernel(Transformer *transformer, int *kernel)
{
  if (transformer == NULL) {
    return YMAGINE_ERROR;
  }

  memcpy(transformer->kernel, kernel, 9 * sizeof(int));
  transformer->convmode = TRANSFORMER_CONVOLUTION_GENERAL;

  return YMAGINE_OK;
}

static void
calculateSharpenKernel(int *destkernel, float sigma, YBOOL fast)
{
  int kedge;
  int kcorner;
  int kcenter;

  if (sigma > 0.0f) {
    /* kernel matrix corner and edge value is calculated based on equation
     -exp(-(u*u + v*v) / (2.0 * sigma * sigma)) / (2.0 * sigma * sigma),
     which will have symmetric values.
     */
    kcorner = (int)(YFIXED_ONE * -exp(-1 / (2.0f * sigma * sigma)) / (2.0f * sigma * sigma));
    kedge = (int)(YFIXED_ONE * -exp(-2 / (2.0f * sigma * sigma)) / (2.0f * sigma * sigma));

    if (fast) {
      /* fast mode set corner to 0, distribute weight of corner to edge */
      kedge += kcorner;
      kcorner = 0;
    }

    kcenter = YFIXED_ONE - 4 * kedge - 4 * kcorner;
  } else {
    kcorner = 0;
    kedge = 0;
    kcenter = YFIXED_ONE;
  }

  destkernel[1] = kedge;
  destkernel[3] = kedge;
  destkernel[5] = kedge;
  destkernel[7] = kedge;

  destkernel[0] = kcorner;
  destkernel[2] = kcorner;
  destkernel[6] = kcorner;
  destkernel[8] = kcorner;

  destkernel[4] = kcenter;

  ALOGD("calculated sharpen kernel:\n%d %d %d\n%d %d %d\n%d %d %d\n",
        destkernel[0], destkernel[1], destkernel[2],
        destkernel[3], destkernel[4], destkernel[5],
        destkernel[6], destkernel[7], destkernel[8]);
}

int
TransformerSetSharpen(Transformer *transformer, float sigma)
{
  const YBOOL fast = YTRUE;

  if (transformer == NULL) {
    return YMAGINE_ERROR;
  }

  transformer->sharpen = sigma;

  if (transformer->sharpen > 0.0f) {
    int kernel[9];
    calculateSharpenKernel(kernel, sigma, fast);

    TransformerSetKernel(transformer, kernel);

    if (fast) {
      transformer->convmode = TRANSFORMER_CONVOLUTION_SHARPEN;
    } else {
      transformer->convmode = TRANSFORMER_CONVOLUTION_GENERAL;
    }
  } else {
    /* invalidate sharpen/convolution */
    if (transformer->convmode != TRANSFORMER_CONVOLUTION_NONE) {
      transformer->convmode = TRANSFORMER_CONVOLUTION_NONE;
    }
  }

  return YMAGINE_OK;
}

/*
 * Set pixel shader to apply
 */
int
TransformerSetShader(Transformer *transformer, PixelShader *shader)
{
  if (transformer == NULL) {
    return YMAGINE_ERROR;
  }

  transformer->shader = shader;

  return YMAGINE_OK;
}

static int
TransformerPrepare(Transformer *transformer)
{
  int pitch;
  void *tmpptr;
  unsigned char* alignedptr = NULL;
  int nlines;
  int alignment = 8;
  int i;
  YBOOL convolution;

  if (transformer == NULL) {
    return YMAGINE_ERROR;
  }

  convolution = transformer->convmode != TRANSFORMER_CONVOLUTION_NONE;

  if (transformer->srcrect.width <= 0 || transformer->srcrect.height <= 0) {
    /* No region set by caller, default to full input */
    transformer->srcrect.x = 0;
    transformer->srcrect.y = 0;
    transformer->srcrect.width = transformer->srcw;
    transformer->srcrect.height = transformer->srch;
  }

  transformer->destrect.x = 0;
  transformer->destrect.y = 0;
  transformer->destrect.width = transformer->destw;
  transformer->destrect.height = transformer->desth;

  /* Intersect source region with actual window */
  if (transformer->srcrect.width < 0) {
    transformer->srcrect.width = 0;
  }
  if (transformer->srcrect.height < 0) {
    transformer->srcrect.height = 0;
  }
  if (transformer->srcrect.x < 0) {
    transformer->srcrect.width += transformer->srcrect.x;
    transformer->srcrect.x = 0;
  }
  if (transformer->srcrect.y < 0) {
    transformer->srcrect.height += transformer->srcrect.y;
    transformer->srcrect.y = 0;
  }
  if (transformer->srcrect.x + transformer->srcrect.width > transformer->srcw) {
    transformer->srcrect.width = transformer->srcw - transformer->srcrect.x;
  }
  if (transformer->srcrect.y + transformer->srcrect.height > transformer->srch) {
    transformer->srcrect.height = transformer->srch - transformer->srcrect.y;
  }

  if (transformer->srcrect.x >= transformer->srcw ||
      transformer->srcrect.y >= transformer->srch ||
      transformer->srcrect.width <= 0 ||
      transformer->srcrect.height <= 0 ||
      transformer->srcrect.x + transformer->srcrect.width < 0 ||
      transformer->srcrect.y + transformer->srcrect.height < 0) {
    /* Empty region */
    transformer->srcrect.x = 0;
    transformer->srcrect.y = 0;
    transformer->srcrect.width = 0;
    transformer->srcrect.height = 0;
  }

  transformer->srcbpp = colorBpp(transformer->srcmode);
  transformer->destbpp = colorBpp(transformer->destmode);

  /* Allocate a working buffer large enough to contain at least 2 full lines
   at the output resolution, plus one working line */
  nlines = 3;

  if (convolution) {
    /* allocates 3 more line convolution buffer for 3x3 convolution.
       we may get better cache performance when convolution buffer is allocated
       together with working buffer (closer to working buffer in memory). */
    nlines += 3;
  }

  pitch = transformer->destw * transformer->destbpp;
  if (pitch % alignment) {
    pitch += alignment - (pitch % alignment);
  }

  if (pitch > 0) {
    tmpptr = Ymem_malloc_aligned(alignment, pitch * nlines, (void**) &alignedptr);
    if (tmpptr == NULL) {
      return YMAGINE_ERROR;
    }

    transformer->destbuf = tmpptr;
    transformer->destaligned = alignedptr;
    transformer->destpitch = pitch;

    /* Lines */
    transformer->scaledbuf = alignedptr + transformer->destpitch * 0;
    transformer->curbuf = alignedptr + transformer->destpitch * 1;
    transformer->tmpbuf = alignedptr + transformer->destpitch * 2;

    if (convolution) {
      transformer->convcprev = alignedptr + transformer->destpitch * 3;
      transformer->convcur = alignedptr + transformer->destpitch * 4;
      transformer->convnext = alignedptr + transformer->destpitch * 5;
    }
  }

  /* Pre-compute offset table for line scaling */
  if (transformer->destrect.width != transformer->srcrect.width && transformer->destrect.width > 0) {
    transformer->bltmap = (int*) Ymem_malloc(transformer->destrect.width * sizeof(int));
    if (transformer->bltmap != NULL) {
      bltLinePrepare(transformer->bltmap, transformer->destrect.width, transformer->srcrect.width);
    }
  }

  if (transformer->statsmode >= 0) {
    if (transformer->srcrect.width > 0 && transformer->srcrect.height > 0) {
      int nchannels;

      if (transformer->srcbpp == 1) {
        nchannels = 1;
      } else {
        nchannels = 4;
      }

      transformer->statsbuf = Ymem_malloc(256 * nchannels * sizeof(int));
      if (transformer->statsbuf != NULL) {
        transformer->histlum = transformer->statsbuf;
        for (i = 0; i < 256; i++) {
          transformer->histlum[i] = 0;
        }

        if (nchannels >= 4) {
          transformer->histr = transformer->statsbuf + 256 * 1;
          transformer->histg = transformer->statsbuf + 256 * 2;
          transformer->histb = transformer->statsbuf + 256 * 3;

          for (i = 0; i < 256; i++) {
            transformer->histr[i] = 0;
            transformer->histg[i] = 0;
            transformer->histb[i] = 0;
          }
        }
      }
      transformer->statscount = 0;
    }
  }

  return YMAGINE_OK;
}

/* Default writer for Vbitmap output */
static YINLINE YOPTIMIZE_SPEED int
WriterVbitmap(Transformer *transformer, const unsigned char *destptr)
{
  int destx = transformer->offsetx;
  int desty = transformer->desty + transformer->offsety;

  if (desty >= 0 && desty < transformer->oheight) {
    /* Line intersects valid region of output bitmap vertically */
    int srcx = 0;
    int destw = transformer->destrect.width;

    if (destx < 0) {
      destw += destx;
      srcx = -destx;
      destx = 0;
    }
    if (destx >= 0 && destx < transformer->owidth && destw > 0) {
      unsigned char *destc = (unsigned char*) 
        (transformer->obuffer + transformer->opitch * desty + destx * transformer->obpp);
      unsigned char *srcc = (unsigned char*)
        (destptr + srcx * transformer->srcbpp);

      /* If input is non-premultiplied alpha, premultiply if necessary */
      if (transformer->srcmode == VBITMAP_COLOR_RGBA &&
          transformer->omode == VBITMAP_COLOR_RGBA && transformer->opremultiplied) {
        int alpha;
        int xpos;
        for (xpos = 0; xpos < destw; xpos++) {
          alpha = (int) srcc[3];
          destc[0] = (srcc[0] * alpha) / 255;
          destc[1] = (srcc[1] * alpha) / 255;
          destc[2] = (srcc[2] * alpha) / 255;
          destc[3] = alpha;
          destc += 4;
          srcc += 4;
        }
      } else {
        memcpy(destc, srcc, destw * transformer->destbpp);
      }
    }
  }

  return YMAGINE_OK;
}

static void
TransformerOutput(Transformer *transformer, unsigned char * destptr)
{
  /* Default writers (currently to attached Vbitmap) */
  if (transformer->obuffer != NULL) {
    WriterVbitmap(transformer, destptr);
  }

  /* Custom writer */
  if (transformer->writer != NULL) {
    transformer->writer(transformer, transformer->writerdata, destptr);
  }
}

static YINLINE YOPTIMIZE_SPEED unsigned char
sharpenPixel(int kcenter, int kedge,
             unsigned char left, unsigned char right, unsigned char top, unsigned char bottom,
             unsigned char center)
{
  int n = (kcenter * center + kedge * (top + bottom + left + right)) >> 10;

  return (n < 0 ? 0 : (n > 255 ? 255 : n));
}

static YINLINE YOPTIMIZE_SPEED void
ApplySharpen(unsigned char *out, const int *kernel,
             const unsigned char *line0, const unsigned char *line1,
             const unsigned char *line2,
             const int bpp, const int width)
{
  int x;
  int i;
  const int kcenter = kernel[4];
  const int kedge = kernel[3];

  const unsigned char *l0 = line0;
  const unsigned char *l1 = line1;
  const unsigned char *l2 = line2;

  if (width > 0) {
    for (i = 0; i < bpp; i++) {
      out[i] = sharpenPixel(kcenter, kedge, line1[0], line1[bpp], line0[0], line2[0], line1[0]);
      line0++;
      line1++;
      line2++;
    }
    line0 = l0;
    line1 = l1;
    line2 = l2;
    out += bpp;
  }

  for (x = 1; x < width - 1; x++) {
    for (i = 0; i < bpp; i++) {
      out[i] = sharpenPixel(kcenter, kedge, line1[0], line1[2*bpp], line0[bpp], line2[bpp], line1[bpp]);

      line0++;
      line1++;
      line2++;
    }

    out += bpp;
  }

  if (width > 1) {
    for (i = 0; i < bpp; i++) {
      out[i] = sharpenPixel(kcenter, kedge, line1[0], line1[bpp], line0[bpp], line2[bpp], line1[bpp]);

      line0++;
      line1++;
      line2++;
    }
    out += bpp;
  }
}

static YBOOL
ConvolutionApply(Transformer *transformer,
                 unsigned char *destptr, const unsigned char* srcptr)
{
  if (transformer->convlinecount <= 0) {
    transformer->convlinecount = 1;
    memcpy(transformer->convcprev, srcptr, transformer->destpitch);
    return YFALSE;
  } else if (transformer->convlinecount == 1) {
    transformer->convlinecount = 2;
    memcpy(transformer->convcur, srcptr, transformer->destpitch);
    return YFALSE;
  } else {
    unsigned char *temp;

    memcpy(transformer->convnext, srcptr, transformer->destpitch);

    switch (transformer->destbpp) {
      case 1:
        ApplySharpen(destptr, transformer->kernel,
                         transformer->convcprev, transformer->convcur,
                         transformer->convnext, 1,
                         transformer->destw);
        break;

      case 3:
        ApplySharpen(destptr, transformer->kernel,
                         transformer->convcprev, transformer->convcur,
                         transformer->convnext, 3,
                         transformer->destw);
        break;

      case 4:
        ApplySharpen(destptr, transformer->kernel,
                         transformer->convcprev, transformer->convcur,
                         transformer->convnext, 4,
                         transformer->destw);
        break;

      default:
        ApplySharpen(destptr, transformer->kernel,
                         transformer->convcprev, transformer->convcur,
                         transformer->convnext, transformer->destbpp,
                         transformer->destw);
        break;
    }

    temp = transformer->convcprev;
    transformer->convcprev = transformer->convcur;
    transformer->convcur = transformer->convnext;
    transformer->convnext = temp;
    return YTRUE;
  }
}

static void
TransformerFlush(Transformer *transformer, unsigned char* destptr)
{
  if (transformer->convmode != TRANSFORMER_CONVOLUTION_NONE) {
    /* boundary condition: at row h - 1, replicate convcur as row h */
    if (ConvolutionApply(transformer, destptr, transformer->convcur)) {
      TransformerOutput(transformer, destptr);
    } else {
      ALOGE("convolution should have output when transformer flushes.");
    }
  }
}

/**
 * prepares transformer output
 * @return YTRUE if transformer output is ready, otherwise YFALSE
 */
static YBOOL
TransformerPrepareOutput(Transformer *transformer, unsigned char *destptr)
{
  if (transformer->convmode != TRANSFORMER_CONVOLUTION_NONE) {
    if (transformer->desty == 0) {
      /* boundary condition: at row 0, replicate destptr as row -1 */
      ConvolutionApply(transformer, destptr, destptr);
    }

    return ConvolutionApply(transformer, destptr, destptr);
  } else {
    return YTRUE;
  }
}

int
TransformerPush(Transformer *transformer, const char *line)
{
  const unsigned char *srcptr;
  unsigned char *destptr;
  int i;
  int weight;
  int nextyfrac;

  if (line == NULL) {
    return YMAGINE_ERROR;
  }

  if (transformer->srcline < 0) {
    /* When first line is pushed in, prepare transformer state and working buffers */
    if (TransformerPrepare(transformer) != YMAGINE_OK) {
      ALOGD("fail to prepare transformer");
      return YMAGINE_ERROR;
    }
    transformer->srcline = 0;
    ALOGD("Transformer in=%dx%d region=%dx%d@%d,%d out=%dx%d",
          transformer->srcw, transformer->srch,
          transformer->srcrect.width, transformer->srcrect.height,
          transformer->srcrect.x, transformer->srcrect.y,
          transformer->destw, transformer->desth);
  } else {
    transformer->srcline++;
  }

  if (transformer->srcline < transformer->srcrect.y) {
    /* Scan line ignored at the top of the image */
    return YMAGINE_OK;
  }
  if (transformer->srcline >= transformer->srcrect.y + transformer->srcrect.height) {
    /* Scan line ignored at the end of the image */
    return YMAGINE_OK;
  }

  /* Scale new line to match horizontal resolution, convert to correct color space and save */
  ALOGV("scale line %d (%d -> %d)",
        transformer->srcline,
        transformer->srcrect.width, transformer->destrect.width);

  srcptr = ((const unsigned char*) line) + transformer->srcrect.x * transformer->srcbpp;
  destptr = transformer->curbuf;

  transformer->curyf = transformer->nextyf;
  transformer->cury = transformer->nexty;

  transformer->nextyf = scaleFixedPoint(transformer->srcline + 1 - transformer->srcrect.y,
                                        transformer->srcrect.height, transformer->destrect.height);
  transformer->nexty = Y_INT(transformer->nextyf);

  if (transformer->statsmode >= 0) {
    if (transformer->srcrect.width > 0) {
      if (transformer->statsbuf != NULL) {
        /* Collect statistics for the segment of the line intersecting the active region */
        const unsigned char *nextc = srcptr;

        if (transformer->srcbpp == 1) {
          /* Statistic on luminance only */
          for (i = 0; i < transformer->srcrect.width; i++) {
            transformer->histlum[nextc[0]]++;
            nextc++;
          }
        } else if (transformer->srcbpp >= 3) {
          /* Statistic on red, green and blue channels, and on total luminance */
          int brightness;

          for (i = 0; i < transformer->srcrect.width; i++) {
            brightness = (218 * nextc[0] + 732 * nextc[1] + 74 * nextc[2]) >> 10;

            transformer->histr[nextc[0]]++;
            transformer->histg[nextc[1]]++;
            transformer->histb[nextc[2]]++;
            transformer->histlum[brightness]++;

            nextc += transformer->srcbpp;
          }
        }
      }

      transformer->statscount += transformer->srcrect.width;
    }
  }

  if (transformer->destrect.width <= 0 || transformer->destrect.height <= 0) {
    return YMAGINE_OK;
  }

  /* Compute weight of this line */
  nextyfrac = Y_FRAC(transformer->nextyf);
  if (transformer->nexty > transformer->cury) {
    /* Last input line for this output line */
    weight = ((transformer->nextyf - nextyfrac) - transformer->curyf);
  } else {
    weight = transformer->nextyf - transformer->curyf;
  }
  if (weight == 0) {
    weight = YFIXED_ONE;
  }

  bltLineExt(transformer->scaledbuf, transformer->destrect.width, transformer->destbpp,
             srcptr, transformer->srcrect.width, transformer->srcbpp, transformer->bltmap);

  if (YmagineMergeLine(transformer->curbuf, transformer->destbpp, transformer->stashedweight,
                       transformer->scaledbuf, transformer->destbpp, weight,
                       transformer->destw) != YMAGINE_OK) {
    ALOGE("merge line %d failed, number of stashed line: %d",
          transformer->srcline, transformer->stashedweight);
    return YMAGINE_ERROR;
  }
  transformer->stashedweight += weight;

  if (transformer->nexty > transformer->cury) {
    /* Last input line for this output line */
    for (i = transformer->cury; i < transformer->nexty; i++) {
      transformer->desty++;

      if (transformer->shader != NULL) {
        if (Yshader_hasVignette(transformer->shader)) {
          /* Has vignette shader (y-dependent), save original line,
             and apply shader according to transformer->desty */
          if (i == transformer->cury) {
            memcpy(transformer->tmpbuf, destptr, transformer->destpitch);
          } else {
            memcpy(destptr, transformer->tmpbuf, transformer->destpitch);
          }
          Yshader_apply(transformer->shader,
                        destptr, transformer->destrect.width, transformer->destbpp,
                        transformer->destrect.width, transformer->destrect.height,
                        0, transformer->desty);
        } else {
          /* Has color shader (y-independent),
             just need to apply color shader once */
          if (i == transformer->cury) {
            Yshader_apply(transformer->shader,
                          destptr, transformer->destrect.width, transformer->destbpp,
                          transformer->destrect.width, transformer->destrect.height,
                          0, transformer->desty);
          }
        }
      }

      if (TransformerPrepareOutput(transformer, destptr)) {
        TransformerOutput(transformer, destptr);
      }

      if (transformer->desty == transformer->desth - 1) {
        TransformerFlush(transformer, destptr);
      }
    }

    transformer->stashedweight = YFIXED_ZERO;
    if (nextyfrac > 0) {
      /* stashed weight is zero, so merging current line is just a copy */
      memcpy(transformer->curbuf, transformer->scaledbuf, transformer->destbpp * transformer->destw);
      transformer->stashedweight += nextyfrac;
    }
  }

  return YMAGINE_OK;
}
