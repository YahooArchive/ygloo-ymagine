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
* @file   vformat.h
* @addtogroup Vformat
* @brief  Vformat abstract type for images
*/

#ifndef _YMAGINE_VFORMAT_H
#define _YMAGINE_VFORMAT_H 1

#include "ymagine/ymagine.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Vformat Vformat
 *
 * This module provides container for stateful image format decoder and encoder
 *
 * @{
 */

YOSAL_OBJECT_EXPORT(Vformat)

/* Vformat constructors */

/**
 * @brief Constructs a Vformat
 * @ingroup Vformat
 *
 * @return pointer to a Vformat
 */
Vformat*
VformatCreate();

/**
 * @brief Retain the Vformat
 * @ingroup Vformat
 *
 * @param vformat that will be retained.
 * @return reference to the retained vformat (ptr)
 */
Vformat*
VformatRetain(Vformat *vformat);

/* Destructor */

/**
 * @brief If reference counter for Vformat is less or equal to 1,
 *        release the Vformat object, otherwise decrement the vformat
 *        reference count.
 * @ingroup Vformat
 *
 * @param vformat whose reference count will be decreased
 * @return If succesful YMAGINE_OK, otherwise YMAGINE_ERROR
 */
int
VformatRelease(Vformat *vformat);

/* Methods */

/**
 * @}
 */

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_VBITMAP_H */
