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
* @file   filter.h
* @addtogroup Filter
* @brief  Bitmap filtering and transformation
*/

#ifndef _YMAGINE_FILTER_H
#define _YMAGINE_FILTER_H 1

#include "ymagine/ymagine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Filter Filter
 *
 * This module provides API for transforming and filtering images
 *
 * @{
 */

/**
 * @brief Rotate image
 * @ingroup Filter
 *
 * Rotate image
 *
 * @param vbitmap This is a pointer to a vbitmap to be rotated
 * @param outbitmap This is a pointer to a vbitmap to contain the rotated image
 * @param centerx X coordinate of the center of rotation in source image
 * @param centery Y coordinate of the center of rotation in source image
 * @param angle Angle of rotation in degrees
 * @return YMAGINE_OK if rotation is succesfull, else YMAGINE_ERROR
 */
int
Ymagine_rotate(Vbitmap *outbitmap, Vbitmap *vbitmap,
               int centerx, int centery, float angle);

/**
 * @brief Applies gaussian blur
 * @ingroup Blur
 *
 * Applies nearly-gaussian blur on the buffer included in the vbitmap 
 *
 * @param vbitmap This is a pointer to a vbitmap that the blur will be applied on
 * @return YMAGINE_OK if blurring is succesfull, else YMAGINE_ERROR
 */
int
Ymagine_blur(Vbitmap *vbitmap, int radius);

/**
 * @}
 */

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_FILTER_H */
