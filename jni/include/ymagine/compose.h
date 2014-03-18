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
* @file   compose.h
* @addtogroup Compose
* @brief  Composition
*/

#ifndef _YMAGINE_COMPOSE_H
#define _YMAGINE_COMPOSE_H 1

#include "ymagine/ymagine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Compose Compose
 *
 * This module provides API for composing images
 *
 * @{
 */

typedef enum {
    YMAGINE_COMPOSE_REPLACE = 0,
    YMAGINE_COMPOSE_OVER,
    YMAGINE_COMPOSE_UNDER,
    YMAGINE_COMPOSE_PLUS,
    YMAGINE_COMPOSE_MINUS,
    YMAGINE_COMPOSE_ADD,
    YMAGINE_COMPOSE_SUBTRACT,
    YMAGINE_COMPOSE_DIFFERENCE,
    YMAGINE_COMPOSE_BUMP,
    YMAGINE_COMPOSE_MAP,
    YMAGINE_COMPOSE_MIX,
    YMAGINE_COMPOSE_MULT,
    /* Ad-hoc composition modes */
    YMAGINE_COMPOSE_LUMINANCE,
    YMAGINE_COMPOSE_LUMINANCEINV,
    YMAGINE_COMPOSE_COLORIZE
} ymagineCompose;

int
Ymagine_composeLine(unsigned char *srcdata, int srcbpp, int srcwidth,
                    unsigned char *maskdata, int maskbpp, int maskwidth,
                    int composeMode);

/**
 * @brief Applies composition effect on the buffer included in the vbitmap
 * @ingroup Compose
 *
 * @param vbitmap pointer to a vbitmap that contains the buffer for the image
 * @param color composition color
 * @param composeMode mode of composition
 * @return If succesfully unlocks the Vbitmap YMAGINE_OK, otherwise YMAGINE_ERROR
 */

int
Ymagine_composeColor(Vbitmap *vbitmap, int color,
		     ymagineCompose composeMode);

/**
 * @brief Applies composition effect on the buffer included in the vbitmap
 * @ingroup Compose
 *
 * @param vbitmap pointer to a vbitmap that contains the buffer for the image
 * @param overlay pointer to a vbitmap that contains the buffer for the overlay
 * @param x horizontal starting position
 * @param y vertical starting position
 * @param composeMode mode of composition
 * @return If succesfully unlocks the Vbitmap YMAGINE_OK, otherwise YMAGINE_ERROR
 */
int
Ymagine_composeImage(Vbitmap *vbitmap, Vbitmap *overlay,
             int x, int y, ymagineCompose composeMode);

/**
 * @}
 */

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_COMPOSE_H */
