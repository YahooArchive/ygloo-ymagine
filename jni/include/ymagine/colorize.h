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
* @file   colorize.h
* @addtogroup Colorize
* @brief  Colorize filter
*/

#ifndef _YMAGINE_COLORIZE_H
#define _YMAGINE_COLORIZE_H 1

#include "ymagine/ymagine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Colorize Colorize
 *
 * This module provides API for colorizing images
 *
 * @{
 */

/**
 * @brief Applies colorize filter
 * @ingroup Colorize
 *
 * Applies colorize filter on pixel buffer of a Vbitmap
 *
 * @param vbitmap This is a pointer to a vbitmap that the colorize will be applied on
 * @param color color in RGB format
 * @return YMAGINE_OK if colorizing is succesfull, else YMAGINE_ERROR
 */
int
Ymagine_colorize(Vbitmap *vbitmap, int color);

/**
 * @}
 */

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_COLORIZE_H */
