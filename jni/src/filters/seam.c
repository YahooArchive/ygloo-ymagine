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

#include "ymagine/ymagine.h"
#include "ymagine_priv.h"

typedef uint16_t seamid_t;
#define SEAM_NONE ((seamid_t) 0)
#define SEAM_FIRST ((seamid_t) 1)

typedef uint32_t cost_t;
#define COST_MAX ((cost_t) 0xffffffff)

#if 0
static YINLINE YOPTIMIZE_SPEED
int COST_ADD(cost_t c1, cost_t c2)
{
  if (c2 > COST_MAX - c1) {
    return COST_MAX;
  }
  return c1 + c2;
}

static YINLINE YOPTIMIZE_SPEED
int COST_SUB(cost_t c1, cost_t c2)
{
  if (c2 > c1) {
    return 0;
  }
  return c1 - c2;
}
#endif

typedef int8_t direction_t;

struct VbitmapSeamMapStruct {
  uint16_t width;
  uint16_t height;
  seamid_t *map;
};

/*
 * TODO:
 * - implement faster seam method based on:
 *    http://vmcl.xjtu.edu.cn/Real-Time%20Content-Aware%20Image%20Resizing.files/real_time_content_aware_image_resizing.pdf
 */

typedef enum {
  EnergyBackward = 0,
  EnergyBackwardDx = 1,
  EnergyForward = 2
} EnergyType;

#if 0
static YINLINE YOPTIMIZE_SPEED
int CLIP_TO_8(int x)
{
  return (x<=0 ? 0 : (x>=0xff ? 0xff : x));
}

static YINLINE YOPTIMIZE_SPEED
int CLIP_TO_16(int x)
{
  return (x<=0 ? 0 : (x>=0xffff ? 0xffff : x));
}
#endif

static YINLINE YOPTIMIZE_SPEED
int
seamcarving(unsigned char *pix, 
            int w, int h, int bpp, int pitch,
            Vbitmap *weightmap,
            EnergyType eType, double *progress,
            VbitmapSeamMap *seamMap)
{					
  int trimmedHeight = h;
  int trimmedWidth = w;
	
  direction_t *directMap;

  uint16_t *energyX;
  uint32_t *costMap;
  uint32_t *costCurrent;
  uint32_t *costAbove;

  Vbitmap *vimage;
  unsigned char *image;
  int imagepitch;
  int imagebpp;

  Vbitmap *venergy;
  unsigned char *energy;
  int energypitch;
  int energybpp;

  unsigned char *psrc;
  unsigned char *pdest;

  uint16_t seam;
  uint16_t numSeams;

  int i, j;
  int mode;

  int minCostIndex;
  int currentCostIndex;
  uint32_t minCost;


  if (seamMap == NULL || seamMap->width != w || seamMap->height != h) {
    return YMAGINE_ERROR;
  }

  /* Create a working bitmap */
  switch(bpp) {
  case 1:
    mode = VBITMAP_COLOR_GRAYSCALE;
    break;
  case 3:
    mode = VBITMAP_COLOR_RGB;
    break;
  case 4:
    mode = VBITMAP_COLOR_RGBA;
    break;
  default:
    return YMAGINE_ERROR; 
  }

  vimage = VbitmapInitMemory(mode);
  VbitmapResize(vimage, w, h);

  VbitmapLock(vimage);
  imagepitch = VbitmapPitch(vimage);
  imagebpp = VbitmapBpp(vimage);
  image = VbitmapBuffer(vimage);

  /* Working copy of the input image, to be modified during processing */
  psrc = pix;
  pdest = image;
  for (i = 0; i < h; i++) {
    memcpy(pdest, psrc, w*bpp);
    psrc += pitch;
    pdest += imagepitch;
  }

  /* Compute initial energy map */
  venergy = VbitmapInitMemory(VBITMAP_COLOR_GRAYSCALE);
  VbitmapResize(venergy, w, h);
  Vbitmap_sobel(venergy, vimage);

  VbitmapLock(venergy);
  energypitch = VbitmapPitch(venergy);
  energybpp = VbitmapBpp(venergy);
  energy = VbitmapBuffer(venergy);

  energyX = Ymem_malloc(w * h * sizeof(energyX[0]));
  costMap = Ymem_malloc(w * h * sizeof(costMap[0]));
  directMap = Ymem_malloc(w * h * sizeof(directMap[0]));

  /* Reset seam map */
  for (i = 0; i < h; i++) {
    for (j=0; j < w; j++){
      seamMap->map[i * w + j] = SEAM_NONE;
    }
  }

  /* Init coordinate mapping table */
  for (i = 0; i < h; i++) {
    for (j = 0; j < w; j++) {
      energyX[i * w + j] = j;
    }
  }

  numSeams = (w + 1) / 2;
  for (seam = 0; seam < numSeams; seam++) {
    /* Set cumulative cost for first row to energy */
    for (j = 0; j < trimmedWidth; j++) {
      costMap[j] = (cost_t) energy[j];
    }

    if (trimmedWidth == 1) {
      /* If one one column left, seam can only be vertical one */
      cost_t cost = costMap[0];

      for (i = 1; i < trimmedHeight; i++) {
        cost = energy[i * energypitch] + cost;
        costMap[i * w] = cost;
        directMap[i * w] = 0;
      }
    } else {
      costCurrent = costMap;
      for (i = 1; i < trimmedHeight; i++) {
        direction_t dir;
        cost_t cost;
        cost_t lCost, uCost, rCost;

        costAbove = costCurrent;
        costCurrent += w;


        /* Manage edge case for first column out of loop for performance */
        uCost = costAbove[0];
        rCost = costAbove[1];

        cost = uCost;
        dir = 0;
        if (rCost < cost) {
          cost = rCost;
          dir = 1;
        }
        costCurrent[0] = energy[i * energypitch] + cost;
        directMap[i * w] = dir;

        for (j = 1; j < trimmedWidth - 1; j++) {
          lCost = uCost;
          uCost = rCost;

          rCost = costAbove[j+1];

          /* Default to pixel above current one */
          cost = uCost;
          dir = 0;

          /* Check if up-left one exists and has lower energy */
          if (lCost < cost) {
            cost = lCost;
            dir = -1;             
          }
          /* Check if up-right one exists and has lower energy */
          if (rCost < cost) {
            cost = rCost;
            dir = 1;
          }

          costCurrent[j] = energy[i * energypitch + j] + cost;
          directMap[i * w + j] = dir;
        }

        /* Manage edge case for first column out of loop for performance */
        lCost = uCost;
        uCost = rCost;

        cost = uCost;
        dir = 0;
        if (lCost < cost) {
          cost = lCost;
          dir = -1;
        }
        costCurrent[j] = energy[i * energypitch + j] + cost;
        directMap[i * w + j] = dir;
      }
    }
		
    /* Find the minimum seam cost */
    costCurrent = costMap + (h - 1) * w;

    minCostIndex = 0;
    minCost = costCurrent[minCostIndex];

    for (j = 1; j < trimmedWidth; j++) {
      if (costCurrent[j] < minCost) {
        minCostIndex = j;
        minCost = costCurrent[minCostIndex];
      }
    }

    /* Follow the seam up to the top of the image, removing it from the
     * working copy of the image, marking it on the seam index map, and
     * shifting the coordinate values in energyMap left accordingly */
    currentCostIndex = minCostIndex;
    for (i = h - 1; i >= 0; i--) {
      int tomove = trimmedWidth - currentCostIndex - 1;
      unsigned int seamXCoord = energyX[i * w + currentCostIndex];

      seamMap->map[i * w + seamXCoord] = seam + SEAM_FIRST;
      if (tomove > 0) {
        psrc = image + i * imagepitch + currentCostIndex * imagebpp;
        memmove(psrc, psrc+bpp, tomove*bpp);
        
        psrc = (unsigned char*) (energyX + i * w + currentCostIndex);
        memmove(psrc, psrc+sizeof(energyX[0]), tomove*sizeof(energyX[0]));
      }

      psrc = energy + i * energypitch + currentCostIndex * energybpp;
      if (tomove > 0) {
        memmove(psrc, psrc+energybpp, tomove*energybpp);
      }

      currentCostIndex += directMap[i * w + currentCostIndex];
    }

    currentCostIndex = minCostIndex;
    for (i = h - 1; i >= 0; i--) {
      energy[i * energypitch + currentCostIndex] = 
        EnergySobel(image + i * imagepitch + currentCostIndex * bpp,
                    imagebpp, imagepitch,
                    currentCostIndex, i, trimmedWidth, h);

      currentCostIndex += directMap[i * w + currentCostIndex];
    }

    /* The seam has been removed from the working copy of the image,
     * so reduce its width by one */
    trimmedWidth--;
  }

  Ymem_free(energyX);
  Ymem_free(costMap);
  Ymem_free(directMap);

  VbitmapUnlock(vimage);
  VbitmapRelease(vimage);

  VbitmapUnlock(venergy);
  VbitmapRelease(venergy);

  return YMAGINE_OK;
}

static YINLINE YOPTIMIZE_SPEED
int
drawImageWithWidth(unsigned char *pix,
                   int w, int h, int bpp, int pitch,
                   unsigned char *opixels,
                   int owidth, int oheight, int obpp, int opitch,
                   VbitmapSeamMap *seamMap)
{
  int pixelsToRemove;
  unsigned char *targetBufferPtr;
  int i, j;
  int c;
  seamid_t *s;
  int copied;
  unsigned char *pixPointer;
  int mbpp;

  if (obpp != bpp) {
    /* Support only RGB<->RGBA conversion */
    if ((obpp != 3 && obpp != 4) || (bpp != 3 && bpp != 4)) {
      ALOGD("unsupported colormode conversion from %d bpp to %d bpp", bpp, obpp);
      return YMAGINE_ERROR;
    }
  }
  if (oheight != h) {
    ALOGD("unsupported vertical seam carving");
    return YMAGINE_ERROR;
  }

  if (seamMap == NULL || seamMap->width != w || seamMap->height != h) {
    ALOGD("invalid seam map");
    return YMAGINE_ERROR;
  }

  mbpp = bpp;
  if (obpp < bpp) {
    mbpp = obpp;
  }

  pixelsToRemove =  w - owidth;

  /* Seam removal */
  for (i = 0; i < h; i++) {
    pixPointer = pix + pitch * i;
    targetBufferPtr = opixels + i * opitch;
    s = seamMap->map + i * w;

    copied = 0;
    if (pixelsToRemove >= 0) {
      /* Seam removal */
      for (j = 0; j < w; j++) {
        if (s[0] == SEAM_NONE || s[0] >= SEAM_FIRST + pixelsToRemove) {
          /* Copy pixels that should be retained */
          for (c = 0; c < mbpp; c++) {
            targetBufferPtr[c] = pixPointer[c];
          }
          /* Force alpha to opaque if input doesn't have alpha channel */
          if (mbpp < obpp) {
            targetBufferPtr[mbpp] = 0xff;
          }
          targetBufferPtr += obpp;
          copied++;

          if (copied == owidth) {
            break;
          }
        }

        pixPointer += bpp;
        s++;
      }
    } else {
      /* Seam insertion */
      int pixelsToInsert = -pixelsToRemove;

      for (j = 0; j < w; j++) {
        int tocopy = 1;
        int k;

        if (s[0] != SEAM_NONE && s[0] < SEAM_FIRST + pixelsToInsert) {
          tocopy++;
        }

        for (k = 0; k < tocopy; k++) {
          /* Copy pixels that should be retained */
          for (c = 0; c < mbpp; c++) {
            targetBufferPtr[c] = pixPointer[c];
          }
          /* Force alpha to opaque if input doesn't have alpha channel */
          if (mbpp < obpp) {
            targetBufferPtr[mbpp] = 0xff;
          }
          targetBufferPtr += obpp;
          copied++;
          if (copied == owidth) {
            break;
          }
        }

        pixPointer += bpp;
        s++;
      }
    }
  }

  return YMAGINE_OK;
}

/* Debug helper to render the seams */
int Vbitmap_seamRender(Vbitmap *vbitmap, VbitmapSeamMap *seamMap, int toremove)
{
  int width;
  int height;
  int pitch;
  int bpp;
  unsigned char *pixels;
  int i, j;

  if (seamMap == NULL || vbitmap == NULL || toremove == 0) {
    return YMAGINE_ERROR;
  }

  if (toremove < 0) {
    toremove = -toremove;
  }

  if (VbitmapLock(vbitmap) >= 0) {
    pixels = VbitmapBuffer(vbitmap);
    width = VbitmapWidth(vbitmap);
    height = VbitmapHeight(vbitmap);
    pitch = VbitmapPitch(vbitmap);
    bpp = colorBpp(VbitmapColormode(vbitmap));

    if (seamMap == NULL || seamMap->width != width || seamMap->height != height) {
      ALOGD("invalid seam map %p %d %d", seamMap, seamMap->width, seamMap->height);
    } else {
      for (i = 0; i < height; i++)    {
        unsigned char *bitmapPointer = pixels + pitch * i;
        int removeCount = 0;
        uint16_t *map = seamMap->map + i * seamMap->width;
        int seamid;

        for (j = 0; j < width; j++) {
          seamid = map[j];
          if (seamid != SEAM_NONE && seamid < SEAM_FIRST + toremove) {
            bitmapPointer[0] = 0xff - (0x7f  * seamid) / toremove;
            bitmapPointer[1] = 0;
            bitmapPointer[2] = 0;
            if (bpp == 4) {
              bitmapPointer[3] = 0xff;
            }
            removeCount++;
          }
          bitmapPointer += bpp;
        }
      }
    }

    VbitmapUnlock(vbitmap);
  }

  return YMAGINE_OK;
}

VbitmapSeamMap*
VbitmapSeamMap_create(int width, int height)
{
  VbitmapSeamMap *seammap;

  if (width <= 0 || height <= 0) {
    return NULL;
  }

  seammap = Ymem_malloc(sizeof(VbitmapSeamMap));
  if (seammap == NULL) {
    return NULL;
  }

  seammap->width = width;
  seammap->height = height;
  seammap->map = NULL;

  seammap->map = (uint16_t*) Ymem_malloc(width * height * sizeof(uint16_t));
  if (seammap->map == NULL) {
    VbitmapSeamMap_release(seammap);
    return NULL;
  }
  
  return seammap;
}

int
VbitmapSeamMap_release(VbitmapSeamMap *seammap)
{
  if (seammap == NULL) {
    return YMAGINE_ERROR;
  }

  if (seammap->map != NULL) {
    Ymem_free(seammap->map);
  }
  Ymem_free(seammap);

  return YMAGINE_OK;
}

VbitmapSeamMap*
Vbitmap_seamPrepare(Vbitmap *vbitmap)
{
  int width;
  int height;
  int pitch;
  int bpp;
  unsigned char *pixels;
  VbitmapSeamMap *seamMap = NULL;
  double progress = 0.0f;
  EnergyType etype = EnergyBackward;
  Vbitmap *weightmap = NULL;

  if (vbitmap == NULL) {
    return NULL;
  }

  if (VbitmapLock(vbitmap) >= 0) {
    pixels = VbitmapBuffer(vbitmap);
    width = VbitmapWidth(vbitmap);
    height = VbitmapHeight(vbitmap);
    pitch = VbitmapPitch(vbitmap);
    bpp = colorBpp(VbitmapColormode(vbitmap));

    /* Parameters verification */
    if (width > 0 && width <= 65535 && height > 0 && height <= 65535) {
      /* Compute seam map */
      seamMap = VbitmapSeamMap_create(width, height);    
    
      if (seamMap != NULL) {
        /* Generate seam map */
        seamcarving(pixels, width, height, bpp, pitch,
                    weightmap, etype, &progress,
                    seamMap);
      }
    }
  }

  return seamMap;
}

int
Vbitmap_seamCarve(Vbitmap *vbitmap, VbitmapSeamMap *seamMap, Vbitmap *outbitmap)
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

  if (vbitmap == NULL) {
    return YMAGINE_ERROR;
  }

  if (VbitmapLock(vbitmap) >= 0) {
    pixels = VbitmapBuffer(vbitmap);
    width = VbitmapWidth(vbitmap);
    height = VbitmapHeight(vbitmap);
    pitch = VbitmapPitch(vbitmap);
    bpp = colorBpp(VbitmapColormode(vbitmap));

    if (seamMap == NULL || seamMap->width != width || seamMap->height != height) {
      ALOGD("invalid seam map");
    } else {
      if (VbitmapLock(outbitmap) >= 0) {
        opixels = VbitmapBuffer(outbitmap);
        owidth = VbitmapWidth(outbitmap);
        oheight = VbitmapHeight(outbitmap);
        opitch = VbitmapPitch(outbitmap);
        obpp = colorBpp(VbitmapColormode(outbitmap));        
        /* Generate resized image */
        drawImageWithWidth(pixels, width, height, bpp, pitch,
                           opixels, owidth, oheight, obpp, opitch,
                           seamMap);

        VbitmapUnlock(outbitmap);
      }
    }

    VbitmapUnlock(vbitmap);
  }

  return YMAGINE_OK;
}

/* Convert sparse seam map into a dense serialized representation */

/* Convert dense seam set into sparse map */

/* Dump seam map into Ychannel */
static YINLINE int
Put16(void *buf, uint16_t v)
{
  unsigned char *c;

  c = (unsigned char*) buf;

  c[0] = (v >> 0) & 0xff;
  c[1] = (v >> 8) & 0xff;

  return 2;
}

int
Vbitmap_seamDump(VbitmapSeamMap *seamMap, Ychannel *channel)
{
  seamid_t *id;
  unsigned char buf[32];
  int n;
  int written;
  int i, j;

  if (seamMap == NULL) {
    return 0;
  }
  if (seamMap->width <= 0 || seamMap->height <= 0) {
    return 0;
  }

  written = 0;

  Put16(buf, seamMap->width);
  Put16(buf+2, seamMap->height);
  n = YchannelWrite(channel, buf, 4);
  if (n != 4) {
    return -1;
  }
  written += n;

  id = seamMap->map;
  for (j = 0; j < seamMap->height; j++) {
    for (i = 0; i < seamMap->width; i++) {
      Put16(buf, id[0]);
      n = YchannelWrite(channel, buf, 2);
      if (n != 2) {
        return -1;
      }
      written += n;

      id++;
    }
  }

  ALOGD("seam map dump (%d bytes)", written);

  return written;
}
