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

/**
* @file   ymagine.h
* @addtogroup Ymagine
* @brief  Ymagine image decodign and processing
*/

#ifndef _YMAGINE_H
#define _YMAGINE_H 1

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Ymagine Ymagine
 *
 * This module provides an extended API for decoding, transforming
 * and processing images. It aims at being a fully portable, self-contained,
 * efficient native (C/C++) implementation of an alternative to BitmapFactory,
 * allowing executing faster and in a more memory-efficient way a batch of operation,
 * thanks to optimized processing pipeline.
 *
 * @{
 */

#include "yosal/yosal.h"

#define YMAGINE_OK    ((int)  0)
#define YMAGINE_ERROR ((int) -1)

/**
 * scale and leave a letterbox (black bars) around the image
 */
#define YMAGINE_SCALE_LETTERBOX 0

/**
 * scale and crop parts of the image so that the image fills the
 * entire new size
 */
#define YMAGINE_SCALE_CROP      1

/**
 * scale and resize the image, modifying its aspect ratio if necessary
 */
#define YMAGINE_SCALE_FIT       2

/**
 * do not scale or resize
 */
#define YMAGINE_SCALE_NONE         10

/**
 * scale the image to half its width or height, ignoring 3 out of 4 pixels
 */
#define YMAGINE_SCALE_HALF_QUICK   11

/**
 * scale the image to half its width or height, averaging 4 pixels into 1 pixel
 */
#define YMAGINE_SCALE_HALF_AVERAGE 12

#include "ymagine/vbitmap.h"
#include "ymagine/iosapi.h"
#include "ymagine/blur.h"
#include "ymagine/colorize.h"
#include "ymagine/compose.h"
#include "ymagine/color.h"
#include "ymagine/pixelshader.h"
#include "ymagine/format.h"
#include "ymagine/seam.h"

/**
 * @}
 */

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_H */
