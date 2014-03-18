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

#ifndef _YMAGINE_GRAPHICS_COLOR_H
#define _YMAGINE_GRAPHICS_COLOR_H 1

#include "ymagine/ymagine.h"
#include "ymagine_priv.h"

#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Prepare YUV to RGB conversion table, call at least once before any
 * YUV to RGB transformations.
 *
 * @return YMAGINE_OK on success
 */
int
ycolor_yuv_prepare();

/**
 * @brief Convert NV21 pixel buffer to RGB(A)888 pixel buffer
 *
 * Take the given NV21 pixel buffer and convert it to an RGB(A)888 pixel buffer.
 * The output buffer of minimum size width*height*3 must be allocated by the
 * caller. If downscaleis true, the resulting image has half the width and height
 * of the original NV21 image. Downscaling simply ignores 3 out of 4 pixels in the Y
 * plane.
 *
 * @param width of the image in indata
 * @param height of the image in outdata
 * @param colormode of the output buffer (VBITMAP_COLOR_RGB or VBITMAP_COLOR_RGBA)
 * @param downscale set to YMAGINE_SCALE_HALF_AVERAGE or YMAGINE_SCALE_HALF_QUICK to obtain an output image of width/2 and height/2
 *
 * @return YMAGINE_OK on success
 */
int
ycolor_nv21torgb(int width, int height, const unsigned char* indata, unsigned char* outdata, int colormode, int downscale);


#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_GRAPHICS_COLOR_H */
