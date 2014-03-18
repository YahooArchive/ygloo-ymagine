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

#ifndef _YMAGINE_GRAPHICS_BITMAP_H
#define _YMAGINE_GRAPHICS_BITMAP_H 1

#include "ymagine/ymagine.h"
#include "ymagine_priv.h"

#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int x;
    int y;
    int width;
    int height;
} Rect;

typedef struct {
  int iwidth;
  int iheight;
  int ibpp;

  int width;
  int height;
  int bpp;
  int pitch;

  int xpos;
  int ypos;

  unsigned char *pixels;
} ImageStream;


int
colorBpp(int colormode);

ImageStream*
createImageStream(int width, int height, int bpp, int pitch, unsigned char *pixels);

int
writeImageStream(ImageStream *image, int npixels, unsigned char *pixels);

int
computeBounds(int srcwidth, int srcheight,
	      int maxwidth, int maxheight, int scalemode,
              int *widthPtr, int *heightPtr);
int
computeTransform(int srcwidth, int srcheight, int destwidth, int destheight, int scalemode,
                 Rect *srcrect, Rect *destrect);

int
copyBitmap(unsigned char *ipixels, int iwidth, int iheight, int ipitch,
	   unsigned char *opixels, int owidth, int oheight, int opitch,
	   int scalemode);

int
imageFill(unsigned char *opixels,
	  int owidth, int oheight, int opitch, int ocolormode,
	  int bx, int by, int bw, int bh);

int
imageFillOut(unsigned char *opixels, int owidth, int oheight,
	     int opitch, int ocolormode,
	     Rect *rect);

int bltLine(unsigned char *opixels, int owidth, int obpp,
	    unsigned char *ipixels, int iwidth, int ibpp);


#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_GRAPHICS_BITMAP_H */
