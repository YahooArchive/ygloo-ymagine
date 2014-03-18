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

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <setjmp.h>

#include "yosal/yosal.h"
#include "ymagine_priv.h"

#include "graphics/bitmap.h"

static YINLINE int
roundScale(int insize, int reqsize, int scalemode)
{
  int i;
  
  if (reqsize <= 0 || reqsize >= insize) {
    /* Current size matches requirements, no scaling */
    return insize;
  }
  
  if (scalemode == YMAGINE_SCALE_FIT) {
    /* Try to have the scaling factor match n/8 */
    for (i = 8; i > 0; i--) {
      if ( (insize * i) / 8 <= reqsize) {
        break;
      }
    }
    if (i >= 1) {
      reqsize = (insize * i) / 8;
    }
  }
  
  /* Even 1/8 is too large for the requested dimension */
  return reqsize;
}

int
computeBounds(int srcwidth, int srcheight,
              int maxwidth, int maxheight, int scalemode,
              int *widthPtr, int *heightPtr)
{
  int reqwidth;
  int reqheight;
  
  if (maxwidth == 0 || maxheight == 0 || srcwidth <= 0 || srcheight <= 0) {
    reqwidth = 0;
    reqheight = 0;
  } else if ( (maxwidth < 0 || srcwidth <= maxwidth) && (maxheight < 0 || srcheight <= maxheight) ) {
    /* Source image already fits in constraints. Keep geometry unmodified */
    reqwidth = srcwidth;
    reqheight = srcheight;
  } else if (maxwidth < 0  || srcwidth <= maxwidth) {
    /* Image too tall */
    reqheight = roundScale(srcheight, maxheight, scalemode);
    reqwidth = (srcwidth * reqheight) / srcheight;
  } else if (maxheight < 0) {
    /* Image too large */
    reqwidth = roundScale(srcwidth, maxwidth, scalemode);
    reqheight = (srcheight * reqwidth) / srcwidth;
  } else {
    /* Image too big in both dimensions */
    if (srcwidth * maxheight > srcheight * maxwidth) {
	    reqwidth = roundScale(srcwidth, maxwidth, scalemode);
	    reqheight = (srcheight * reqwidth) / srcwidth;
    } else {
	    reqheight = roundScale(srcheight, maxheight, scalemode);
	    reqwidth = (srcwidth * reqheight) / srcheight;
    }
  }
  
  if (widthPtr != NULL) {
    *widthPtr = reqwidth;
  }
  if (heightPtr != NULL) {
    *heightPtr = reqheight;
  }
  
  return YMAGINE_OK;
}

/* Transform constraints on source and origin into origin and destination regions */
int
computeTransform(int srcwidth, int srcheight, int destwidth, int destheight,
                 int scalemode,
                 Rect *srcrect, Rect *destrect)
{
  if (srcwidth <= 0 || srcheight <= 0 || destwidth <= 0 || destheight <= 0) {
    destrect->x = 0;
    destrect->y = 0;
    destrect->width = 0;
    destrect->height = 0;
    srcrect->x = 0;
    srcrect->y = 0;
    srcrect->width = 0;
    srcrect->height = 0;
    
    return YMAGINE_OK;
  }
  
  if (destwidth == srcwidth && destheight == srcheight) {
    /* Image and target have exact same size */
    destrect->x = 0;
    destrect->y = 0;
    destrect->width = destwidth;
    destrect->height = destheight;
    srcrect->x = 0;
    srcrect->y = 0;
    srcrect->width = destwidth;
    srcrect->height = destheight;
  } else if (srcwidth * destheight == srcheight * destwidth) {
    /* Image and target have same aspect ratio but different sizes, scale it */
    destrect->x = 0;
    destrect->y = 0;
    destrect->width = destwidth;
    destrect->height = destheight;
    srcrect->x = 0;
    srcrect->y = 0;
    srcrect->width = srcwidth;
    srcrect->height = srcheight;
  } else if (srcwidth * destheight > srcheight * destwidth) {
    if (scalemode == YMAGINE_SCALE_CROP) {
      /* Crop left and right */
      destrect->x = 0;
      destrect->y = 0;
      destrect->width = destwidth;
      destrect->height = destheight;
      srcrect->width = (srcheight * destwidth) / destheight;
      srcrect->height = srcheight;
      srcrect->x = (srcwidth - srcrect->width) / 2;
      srcrect->y = 0;
    } else {
      /* Letter box */
      destrect->width = destwidth;
      destrect->height = (srcheight * destrect->width) / srcwidth;
      destrect->x = 0;
      destrect->y = (destheight - destrect->height) / 2;
      srcrect->width = srcwidth;;
      srcrect->height = srcheight;
      srcrect->x = 0;
      srcrect->y = 0;
    }
  } else {
    if (scalemode == YMAGINE_SCALE_CROP) {
      /* Crop top and bottom */
      destrect->x = 0;
      destrect->y = 0;
      destrect->width = destwidth;
      destrect->height = destheight;
      srcrect->width = srcwidth;
      srcrect->height = (srcwidth * destheight) / destwidth;
      srcrect->x = 0;
      srcrect->y = (srcheight - srcrect->height) / 2;
    } else {
      /* Letter box */
      destrect->height = destheight;
      destrect->width = (srcwidth * destrect->height) / srcheight;
      destrect->x = (destwidth - destrect->width) / 2;
      destrect->y = 0;
      srcrect->width = srcwidth;;
      srcrect->height = srcheight;
      srcrect->x = 0;
      srcrect->y = 0;
    }
  }
  
  return YMAGINE_OK;
}

YOPTIMIZE_SPEED int
imageFillOut(unsigned char *opixels, int owidth, int oheight, int opitch,
             int ocolormode, Rect *rect)
{
  if (rect == NULL) {
    imageFill(opixels, owidth, oheight, opitch, ocolormode, 0, 0, owidth, oheight);
    return YMAGINE_OK;
  }
  
  /* Reset empty pixels now, if target region is smaller than output buffer */
  if (rect->y > 0) {
    /* Fill top side */
    imageFill(opixels, owidth, oheight, opitch, ocolormode, 0, 0, owidth, rect->y);
  }
  if (rect->y + rect->height < oheight) {
    /* Fill bottom side */
    imageFill(opixels, owidth, oheight, opitch, ocolormode,
              0, rect->y + rect->height, owidth,
              oheight - (rect->y + rect->height));
  }
  if (rect->x > 0) {
    /* Fill left */
    imageFill(opixels, owidth, oheight, opitch, ocolormode,
              0, rect->y, rect->x,
              rect->height);
  }
  if (rect->x + rect->width < owidth) {
    /* Fill right */
    imageFill(opixels, owidth, oheight, opitch, ocolormode,
              rect->x + rect->width,
              rect->y, owidth - (rect->x + rect->width), rect->height);
  }
  
  return YMAGINE_OK;
}

YOPTIMIZE_SPEED int
copyBitmap(unsigned char *ipixels, int iwidth, int iheight, int ipitch,
           unsigned char *opixels, int owidth, int oheight, int opitch,
           int scalemode)
{
  int i, j;
  int ii;
  int srcbpp, destbpp;
  unsigned char* destptr;
  unsigned char* srcptr;
  unsigned char* prevptr;
  int srcy, prevsrcy;
  int hdenom, vdenom;
  Rect srcrect;
  Rect destrect;
  
  if (iwidth <= 0 || iheight <= 0 || ipixels == NULL) {
    return YMAGINE_OK;
  }
  if (owidth <= 0 || oheight <= 0 || opixels == NULL) {
    return YMAGINE_OK;
  }
  
  computeTransform(iwidth, iheight, owidth, oheight, scalemode, &srcrect, &destrect);
  imageFillOut(opixels, owidth, oheight, VBITMAP_COLOR_RGBA, opitch, &destrect);
  
  /* Assume RGBA format for input and output */
  srcbpp = 4;
  destbpp = 4;
  
  /* Check for edge case, when target image is 1 pixel large */
  if (destrect.width <= 1) {
    hdenom = 1;
  } else {
    hdenom = destrect.width - 1;
  }
  if (destrect.height <= 1) {
    vdenom = 1;
  } else {
    vdenom = (destrect.height - 1);
  }
  
  prevsrcy = -1;
  prevptr = NULL;

  for (j = 0; j < destrect.height; j++) {
    srcy = srcrect.y + (j * (srcrect.height - 1)) / vdenom;
    
    srcptr = ipixels + srcy * ipitch + srcrect.x * srcbpp;
    destptr = opixels + (j + destrect.y) * opitch + destrect.x * destbpp;
    
    if (prevsrcy == srcy) {
      /* Duplicate previous scan line */
      if (prevptr != NULL) {
        memcpy(destptr, prevptr, destrect.width * destbpp);
      }
    } else {
      if (destrect.width == srcrect.width) {
        /* Fast copy, no transformation except for colorspace */
        memcpy(destptr, srcptr, destrect.width * destbpp);
      } else {
        /* Need to resize pixels */
        for (i = 0; i < destrect.width; i++) {
          ii = (i * (srcrect.width - 1)) / hdenom;
          
          destptr[4 * i] = srcptr[4 * ii];
          destptr[4 * i + 1] = srcptr[4 * ii + 1];
          destptr[4 * i + 2] = srcptr[4 * ii + 2];
          destptr[4 * i + 3] = srcptr[4 * ii + 3];
        }
      }
      
      prevsrcy = srcy;
      prevptr = destptr;
    }
  }
  
  return destrect.height;
}

YOPTIMIZE_SPEED int
imageFill(unsigned char *opixels, int owidth, int oheight,
          int opitch, int ocolormode,
          int bx, int by, int bw, int bh)
{
  int i, j;
  int linelength;
  unsigned char *baseline;
  unsigned char *nextp;
  unsigned char c[4];
  int bpp;
  
  if (bw <= 0 || bh <= 0) {
    return YMAGINE_OK;
  }
  
  bpp = colorBpp(ocolormode);
  if (bpp <= 0) {
    return YMAGINE_OK;
  }
  
  baseline = opixels + by * opitch + bx * 4;
  linelength = bw * 4;
  
  c[0] = 0x00;
  c[1] = 0x00;
  c[2] = 0x00;
  c[3] = 0x00;
  
  for (i = 0; i < bpp; i++) {
    baseline[i] = c[i];
  }
  
  if (bw > 1) {
    if (bpp == 4 && Ymem_isaligned(baseline, 4)) {
      /* Pointer is word-aligned, copy using uint32 */
      uint32_t *p = (uint32_t*) baseline;
      uint32_t pixel = p[0];
      for (i = 1; i < bw; i++) {
        p[i] = pixel;
      }
    } else {
      nextp = baseline + bpp;
      for (j = 1; j < bpp; j++) {
        for (i = 0; i < bpp; i++) {
          *nextp++ = c[i];
        }
      }
    }
  }
  
  if (bh > 1) {
    nextp = baseline;
    if (linelength == opitch) {
      nextp += opitch;
      memcpy(nextp, baseline, linelength);
    } else {
      for (i = 1; i < bh; i++) {
        nextp += opitch;
        memcpy(nextp, baseline, linelength);
      }
    }
  }
  
  return YMAGINE_OK;
}

/* For other fast averaging, see:
 * http://www.compuphase.com/graphic/scale3.htm
 */
static YINLINE YOPTIMIZE_SPEED void
PixelAverage(unsigned char *opixels, unsigned char *ipixels, int n, int ibpp)
{
#if 1
  if (ibpp == 1) {
    if (n == 1) {
      opixels[0] = ipixels[0];
    } else if (n == 2) {
      opixels[0] = ((int) ipixels[0] + (int) ipixels[ibpp]) >> 1;
    } else {
      int i;
      int gs = (int) ipixels[0];
      
      ipixels += ibpp;
      
      for (i = 1; i < n; i++) {
        gs += ipixels[0];
        ipixels += ibpp;
      }
      
      opixels[0] = (unsigned char) (gs / n);
    }
  } else {
    if (n == 1) {
      opixels[0] = ipixels[0];
      opixels[1] = ipixels[1];
      opixels[2] = ipixels[2];
    } else if (n == 2) {
      opixels[0] = ((int) ipixels[0] + (int) ipixels[ibpp]) >> 1;
      opixels[1] = ((int) ipixels[1] + (int) ipixels[ibpp + 1]) >> 1;
      opixels[2] = ((int) ipixels[2] + (int) ipixels[ibpp + 2]) >> 1;
    } else {
      int i;
      int r = (int) ipixels[0];
      int g = (int) ipixels[1];
      int b = (int) ipixels[2];
      
      ipixels += ibpp;
      
      for (i = 1; i < n; i++) {
        r += ipixels[0];
        g += ipixels[1];
        b += ipixels[2];
        ipixels += ibpp;
      }
      
      opixels[0] = (unsigned char) (r / n);
      opixels[1] = (unsigned char) (g / n);
      opixels[2] = (unsigned char) (b / n);
    }
  }
#else
  opixels[0] = ipixels[0];
  opixels[1] = ipixels[1];
  opixels[2] = ipixels[2];
  ipixels += bpp;
  
  while (--n > 1) {
#if 0
    opixels[0] = (opixels[0] >> 1) + (ipixels[0] >> 1);
    opixels[1] = (opixels[1] >> 1) + (ipixels[1] >> 1);
    opixels[2] = (opixels[2] >> 1) + (ipixels[2] >> 1);
#else
    opixels[0] = ((int) opixels[0] + (int) ipixels[0]) >> 1;
    opixels[1] = ((int) opixels[1] + (int) ipixels[1]) >> 1;
    opixels[2] = ((int) opixels[2] + (int) ipixels[2]) >> 1;
#endif
    ipixels += bpp;
  }
#endif
}

/* Scale scanline. Pixels must be in RGB or RGBX format */
YOPTIMIZE_SPEED int
bltLine(unsigned char *opixels, int owidth, int obpp,
        unsigned char *ipixels, int iwidth, int ibpp)
{
  unsigned char *srcptr;
  unsigned char *destptr;
  int i;
  int j0, j1;
  
  if (ibpp != obpp && (ibpp < 3 || obpp < 3)) {
    /* Color space conversion not yet implemented */
    return YMAGINE_ERROR;
  }
  
  if (owidth == iwidth && ibpp == obpp) {
    memcpy(opixels, ipixels, owidth * obpp);
  } else {
    destptr = opixels;
    srcptr = ipixels;
    
    j1 = 0;
    for (i = 0; i < owidth; i++) {
      j0 = j1;
      j1 = ((i + 1) * iwidth) / owidth;
      
      if (j0 == j1) {
        destptr[0] = srcptr[0];
        if (obpp == 3 || obpp == 4) {
          destptr[1] = srcptr[1];
          destptr[2] = srcptr[2];
        }
      } else {
        PixelAverage(destptr, srcptr, j1 - j0, obpp);
        srcptr += (j1 - j0) * ibpp;
      }
      if (obpp == 4) {
        destptr[3] = 0xff;
      }
      destptr += obpp;
    }
  }
  
  return YMAGINE_OK;
}
