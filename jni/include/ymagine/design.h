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

#ifndef _YMAGINE_DESIGN_H
#define _YMAGINE_DESIGN_H 1

#include "ymagine/ymagine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Design Design
 *
 * This module provides API for some basic design components
 *
 * @{
 */

/**
 * @brief Create a Vbitmap with an orb mask
 * @ingroup Design
 *
 * @param canvas Vbitmap to render the orb into
 * @param sz size (width and height) of bitmap
 * @return If succesfully return Vbitmap with orb mask rendered as solid black on transparent background
 */
  int VbitmapOrbLoad(Vbitmap *canvas, int sz);

/**
 * @brief Render group orb
 * @ingroup Design
 *
 * @param canvas Vbitmap to render into
 * @param ntiles total number of images to compose into canvas
 * @param tileid index of the tile to render
 * @param channelin input channel for the image to render
 * @return If succesfully return YMAGINE_OK
 */
int
VbitmapOrbRenderTile(Vbitmap *canvas, int ntiles, int tileid, Ychannel* channelin);

/**
 * @brief Render group orb
 * @ingroup Design
 *
 * @param canvas Vbitmap to render into
 * @param ntiles total number of images to compose into canvas
 * @param tileid index of the tile to render
 * @param srcbitmap source bitmap to render into tile
 * @return If succesfully return YMAGINE_OK
 */
int
VbitmapOrbRenderTileBitmap(Vbitmap *canvas, int ntiles, int tileid, Vbitmap *srcbitmap);

/**
 * @}
 */

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_DESIGN_H */
