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
 * @file   transformer.h
 * @addtogroup Transformer
 * @brief  Pixel buffer transformer
 */

#ifndef _YMAGINE_TRANSFORMER_H
#define _YMAGINE_TRANSFORMER_H 1

#include "ymagine/ymagine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Transformer Transformer
 *
 * This module provides API to manage pixel buffer transformations
 *
 * @{
 */

/**
 * @brief represents pixel buffer transformation
 * @ingroup Transformer
 */
YOSAL_OBJECT_EXPORT(Transformer)

       typedef int (*TransformerWriterFunc)(Transformer *transformer, void *writerdata, void *line);


/**
 * @brief Create transformer.
 * @ingroup Transformer
 *
 * Caller is responsible for releasing the transformer
 * @see Transformer_release
 *
 * @return pointer to Transformer.
 */
Transformer*
TransformerCreate();

/**
 * @brief Retain the transformer
 * @ingroup Transformer
 *
 * @param transformer that will be retained.
 * @return reference to the retained transformer (ptr)
 */
Transformer*
TransformerRetain(Transformer* transformer);

/**
 * @brief If reference counter for Transformer is less or equal to 1,
 *        release the Transformer object, otherwise decrement the
 *        transformer reference count.
 * @ingroup Transformer
 *
 * @param transformer whose reference count will be decreased
 * @return If succesful YMAGINE_OK, otherwise YMAGINE_ERROR
 */
int
TransformerRelease(Transformer* transformer);

int
TransformerSetWriter(Transformer *transformer, TransformerWriterFunc writer, void *writerdata);

int
TransformerSetStats(Transformer *transformer, int statsmode);

int
TransformerSetScale(Transformer *transformer,
                    int srcw, int srch,
                    int dstw, int dsth);

int
TransformerSetRegion(Transformer *transformer,
                     int srcx, int srcy,
                     int srcw, int srch);

int
TransformerSetMode(Transformer *transformer, int srcmode, int destmode);

int
TransformerSetShader(Transformer *transformer, PixelShader *shader);

int
TransformerSetSharpen(Transformer *transformer, float sigma);

int
TransformerSetBitmap(Transformer *transformer, Vbitmap *vbitmap, int offsetx, int offsety);

int
TransformerPush(Transformer* transformer, const char *line);

/**
 * @}
 */

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_TRANSFORMER_H */
