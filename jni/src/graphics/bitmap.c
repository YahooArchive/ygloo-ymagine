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

#define LOG_TAG "ymagine::bitmap"

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

Vrect*
computeCropRect(Vrect *croprect, YmagineFormatOptions *options,
                int width, int height)
{
  Vrect full, current;

  if (croprect == NULL) {
    return NULL;
  }

  if (width < 0) {
    width = 0;
  }
  if (height < 0) {
    height = 0;
  }

  full.x = 0;
  full.y = 0;
  full.width = width;
  full.height = height;

  if (options == NULL || width <= 0 || height <= 0) {
    current.x = full.x;
    current.y = full.y;
    current.width = full.width;
    current.height = full.height;
  } else {
    if (options->cropoffsetmode == CROP_MODE_ABSOLUTE) {
      current.x = options->cropx;
      current.y = options->cropy;
    } else if (options->cropoffsetmode == CROP_MODE_RELATIVE) {
      current.x = (int)(options->cropxp * width);
      current.y = (int)(options->cropyp * height);
    } else {
      current.x = 0;
      current.y = 0;
    }

    if (options->cropsizemode == CROP_MODE_ABSOLUTE) {
      current.width = options->cropwidth;
      current.height = options->cropheight;

      if (options->cropwidth > 0 && current.width == 0) {
        current.width = 1;
      }
      if (options->cropheight > 0 && current.height == 0) {
        current.height = 1;
      }
    } else if (options->cropsizemode == CROP_MODE_RELATIVE) {
      current.width = (int)(options->cropwidthp * width);
      current.height = (int)(options->cropheightp * height);
    } else {
      current.width = width;
      current.height = height;
    }
  }

  /* Normalize region to be stritly inside input image area */
  VrectComputeIntersection(&full, &current, croprect);

  return croprect;
}

/* Transform constraints on source and origin into origin and destination regions */
int
computeTransform(int srcwidth, int srcheight, const Vrect *croprect,
                 int destwidth, int destheight,
                 int scalemode,
                 Vrect *srcrect, Vrect *destrect)
{
  YBOOL invalid = (srcwidth <= 0) || (srcheight <= 0) ||
      (destwidth <= 0) || (destheight <= 0);

#if YMAGINE_DEBUG
  ALOGD("compute transform: src=%dx%d dst=%dx%d scale=%s",
        srcwidth, srcheight, destwidth, destheight,
        Ymagine_scaleModeStr(scalemode));

  if (croprect != NULL) {
    ALOGD("compute transform: croprect=%dx%d@%d,%d",
          croprect->width, croprect->height, croprect->x, croprect->y);
  }
#endif

  if (!invalid) {
    if (croprect != NULL) {
      /* Intersect source region with crop region */
      if (croprect->width <= 0 || croprect->height <= 0) {
        invalid = YTRUE;
      } else {
        int cropwidth = croprect->width;
        int cropheight = croprect->height;
        int cropx = croprect->x;
        int cropy = croprect->y;

        if (cropx < 0) {
          cropwidth += cropx;
          cropx = 0;
        }
        if (cropy < 0) {
          cropheight += cropy;
          cropy = 0;
        }
        if (cropx + cropwidth > srcwidth) {
          cropwidth = srcwidth - cropx;
        }
        if (cropy + cropheight > srcheight) {
          cropheight = srcheight - cropy;
        }

        if (cropx >= srcwidth ||
            cropy >= srcheight ||
            cropwidth <= 0 ||
            cropheight <= 0) {
          invalid = YTRUE;
        } else {
          srcwidth = cropwidth;
          srcheight = cropheight;
          srcrect->x = cropx;
          srcrect->y = cropy;
        }
      }
    } else {
      srcrect->x = 0;
      srcrect->y = 0;
    }
  }

  if (invalid) {
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
    srcrect->width = destwidth;
    srcrect->height = destheight;
  } else if (srcwidth * destheight == srcheight * destwidth) {
    /* Image and target have same aspect ratio but different sizes, scale it */
    destrect->x = 0;
    destrect->y = 0;
    destrect->width = destwidth;
    destrect->height = destheight;
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

      if (srcrect->width == 0) {
        srcrect->width = 1;
      }

      srcrect->height = srcheight;
      /* when croprect != null, srcrect->x = cropx, srcwidth = cropwidth,
       and cropx + cropwidh <= original srcwidth, thus
       srcrect->x + (srcwidth - srcrect->width) / 2 <= original srcwidth */
      srcrect->x += (srcwidth - srcrect->width) / 2;
    } else {
      /* Letter box */
      destrect->width = destwidth;
      destrect->height = (srcheight * destrect->width) / srcwidth;

      if (destrect->height == 0) {
        destrect->height = 1;
      }

      destrect->x = 0;
      destrect->y = (destheight - destrect->height) / 2;
      srcrect->width = srcwidth;
      srcrect->height = srcheight;
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

      if (srcrect->height == 0) {
        srcrect->height = 1;
      }

      /* when croprect != null, srcrect->y = cropy, srcheight = cropheight,
       and cropy + cropheight <= original srcheight, thus
       srcrect->y + (srcheight - srcrect->height) / 2 <= original srcheight */
      srcrect->y += (srcheight - srcrect->height) / 2;
    } else {
      /* Letter box */
      destrect->height = destheight;
      destrect->width = (srcwidth * destrect->height) / srcheight;

      if (destrect->width == 0) {
        destrect->width = 1;
      }

      destrect->x = (destwidth - destrect->width) / 2;
      destrect->y = 0;
      srcrect->width = srcwidth;
      srcrect->height = srcheight;
    }
  }

#if YMAGINE_DEBUG
  if (srcrect != NULL && destrect != NULL) {
    ALOGD("compute transform: srcrect=%dx%d@%d,%d dstrect=%dx%d@%d,%d",
          srcrect->width, srcrect->height, srcrect->x, srcrect->y,
          destrect->width, destrect->height, destrect->x, destrect->y);
  }
#endif
  
  return YMAGINE_OK;
}


int
YmaginePrepareTransform(Vbitmap* vbitmap, YmagineFormatOptions *options,
                        int imagewidth, int imageheight,
                        Vrect* srcrect, Vrect* destrect)
{
  Vrect croprect;
  int owidth;
  int oheight;

  if (srcrect == NULL || destrect == NULL) {
    return YMAGINE_ERROR;
  }

  if (computeCropRect(&croprect, options, imagewidth, imageheight) == NULL) {
    return YMAGINE_ERROR;
  }

  /* Never return an image larger than the input */
  if (vbitmap != NULL && !options->resizable) {
    owidth = VbitmapWidth(vbitmap);
    oheight = VbitmapHeight(vbitmap);
  } else {
    owidth = options->maxwidth;
    oheight = options->maxheight;

    if (owidth < 0) {
      owidth = imagewidth;
    }
    if (oheight < 0) {
      oheight = imageheight;
    }

    if (owidth > croprect.width) {
      owidth = croprect.width;
    }
    if (oheight > croprect.height) {
      oheight = croprect.height;
    }
  }

  computeTransform(croprect.width, croprect.height, NULL,
                   owidth, oheight, options->scalemode,
                   srcrect, destrect);
  srcrect->x += croprect.x;
  srcrect->y += croprect.y;

  return YMAGINE_OK;
}

YOPTIMIZE_SPEED int
imageFillOut(unsigned char *opixels, int owidth, int oheight, int opitch,
             int ocolormode, Vrect *rect)
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
  Vrect srcrect;
  Vrect destrect;
  
  if (iwidth <= 0 || iheight <= 0 || ipixels == NULL) {
    return YMAGINE_OK;
  }
  if (owidth <= 0 || oheight <= 0 || opixels == NULL) {
    return YMAGINE_OK;
  }
  
  computeTransform(iwidth, iheight, NULL, owidth, oheight, scalemode, &srcrect, &destrect);
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
