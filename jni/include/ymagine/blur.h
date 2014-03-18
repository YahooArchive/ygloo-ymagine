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
* @file   blur.h
* @addtogroup Blur
* @brief  Nearly-Gaussian blur
*/

#ifndef _YMAGINE_BLUR_H
#define _YMAGINE_BLUR_H 1

#include "ymagine/ymagine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Blur Blur
 *
 * This module provides API for blurring images
 *
 * @{
 */

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

#endif /* _YMAGINE_BLUR_H */
