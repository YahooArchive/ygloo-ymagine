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

#ifndef _YMAGINE_SIMPLE_H
#define _YMAGINE_SIMPLE_H 1

#include "ymagine/ymagine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Simple Simplified Native Interface
 *
 * This module provides API to consume from dynamic language wrappers
 *
 * @{
 */

/**
 * Transcode an image
 *
 * @param infile file name of input file
 * @param outfile file name of output file
 * @param maxWidth of transcoded image
 * @param maxHeight of transcoded image
 * @param scaleMode scaling mode
 * @param quality for encoder from 0 to 100, -1 for default quality
 * @param sharpen sharpening level, from 0 (no sharpening) to 100
 * @param subsample Subsampling mode for JPEG output (0, 1 or 2)
 * @param irotate Rotation angle in degrees
 * @param meta Meta data copy option (0 for none, 1 for comments, 2 for all)
 *
 * @return YMAGINE_OK on success
 */
int
YmagineSNI_Transcode(const char *infile,
                     const char *outfile, int oformat,
                     int maxwidth, int maxheight, int scalemode,
                     int quality, int isharpen, int subsample,
                     int irotate, int meta);

/**
 * Decode an image coming from a memory buffer into a Vbitmap.
 * Decoded image will be stored into passed bitmap.
 *
 * @param bitmap to decode into
 * @param data raw image data source
 * @param datalen size of raw image data
 * @param maxwidth of decoded bitmap
 * @param maxheight of decoded bitmap
 * @param scalemode scaling mode
 *
 * @return YMAGINE_OK on success
 */
int
YmagineSNI_Decode(Vbitmap *bitmap, const char *data, int datalen,
                  int maxwidth, int maxheight, int scalemode);

/**
 * @}
 */

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_SIMPLE_H */
