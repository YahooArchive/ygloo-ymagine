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

#ifndef _YMAGINE_FORMAT_H
#define _YMAGINE_FORMAT_H 1

#include "ymagine/ymagine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup YmagineFormat
 *
 * @brief image formats
 *
 * This module provides an API to decode and encode pictures in various
 * (compressed) formats. Today it is focused on JPEG.
 *
 * @{
 */

/**
 * @brief represents decoding / encoding options.
 * @ingroup YmagineFormat
 */
typedef struct YmagineFormatOptionsStruct YmagineFormatOptions;

YmagineFormatOptions*
YmagineFormatOptions_Create();

int
YmagineFormatOptions_Release(YmagineFormatOptions *options);

YmagineFormatOptions*
YmagineFormatOptions_Reset(YmagineFormatOptions *options);

YmagineFormatOptions*
YmagineFormatOptions_setQuality(YmagineFormatOptions *options,
                                int quality);

YmagineFormatOptions*
YmagineFormatOptions_setResize(YmagineFormatOptions *options,
                               int maxWidth, int maxHeight, int scalemode);

YmagineFormatOptions*
YmagineFormatOptions_setShader(YmagineFormatOptions *options,
                               PixelShader *shader);

/**
 * Decode an image coming from a Ychannel into a Vbitmap. Wrap your source
 * (file on disk, memory buffer) with a Ychannel and then use this generic API.
 * @see Ychannel
 *
 * @param bitmap to decode into
 * @param channel raw JPEG data source
 * @param options for decoder
 *
 * @return YMAGINE_OK on success
 */
int
YmagineDecode(Vbitmap *bitmap, Ychannel *channel,
              YmagineFormatOptions *options);

/**
 * Decode an image coming from a Ychannel into a Vbitmap. Wrap your source
 * (file on disk, memory buffer) with a Ychannel and then use this generic API.
 * @see Ychannel
 *
 * @param bitmap to decode into
 * @param channel raw JPEG data source
 * @param maxWidth of decoded bitmap
 * @param maxHeight of decoded bitmap
 * @param quality of decoder from 0 (fastest) to 100 (highest quality)
 *        A negative value uses default (85) quality
 *
 * @return YMAGINE_OK on success
 */
int
YmagineDecodeResize(Vbitmap *bitmap, Ychannel *channel,
                    int maxWidth, int maxHeight, int scaleMode);

/**
 * Encode a Vbitmap
 *
 * @param vbitmap to encode
 * @param channelout encoded image (default to JPEG format)
 * @param options for encoder
 *
 * @return YMAGINE_OK on success
 */
int
YmagineEncode(Vbitmap *vbitmap, Ychannel *channelout,
              YmagineFormatOptions *options);

/**
 * Test if a Ychannel contains a JPEG. This is an educated estimate, the JPEG
 * might still be invalid even if this function indicates that JPEG data will
 * follow.
 */
YBOOL
matchJPEG(Ychannel *channel);

/**
 * Decode a JPEG coming from a Ychannel into a Vbitmap. Wrap your JPEG source
 * (file on disk, memory buffer) with a Ychannel and then use this generic API.
 * @see Ychannel
 *
 * @param channel raw JPEG data source
 * @param bitmap to decode into
 * @param maxWidth of decoded bitmap
 * @param maxHeight of decoded bitmap
 * @param quality of decoder from 0 (fastest) to 100 (highest quality)
 *        A negative value uses default (85) quality
 * @param scaleMode to use when resizing, @see Ymagine
 * @param pixelShader pixel shader that will be applied on-line during
 * decoding. Use NULL if do not want apply pixel shader @see Pixelshader
 *
 * @return YMAGINE_OK on success
 */
int
decodeJPEG(Ychannel *channel, Vbitmap *bitmap,
           int maxWidth, int maxHeight, int scaleMode,
           int quality, PixelShader* pixelShader);

/**
 * Reencode a JPEG while being able to scale it
 *
 * @param channelin raw JPEG data source
 * @param channelout raw JPEG output
 * @param maxWidth of reencoded JPEG
 * @param maxHeight of reencoded JPEG
 * @param calemode to use when resizing, @see Ymagine
 * @param quality of reencoded JPEG (0-100)
 * @param pixelShader pixel shader that will be applied on-line during
 * transcoding. Use NULL if do not want apply pixel shader @see Pixelshader
 *
 * @return YMAGINE_OK on success
 */
int
transcodeJPEG(Ychannel *channelin, Ychannel *channelout,
              int maxWidth, int maxHeight,
              int scalemode, int quality,
              PixelShader* pixelShader);

/**
 * Encode a Vbitmap using JPEG
 *
 * @param vbitmap to encode
 * @param channelout raw JPEG output
 * @param quality to use when encoding (0-100)
 *
 * @return YMAGINE_OK on success
 */
int
encodeJPEG(Vbitmap *vbitmap, Ychannel *channelout, int quality);

/**
 * Test if a Ychannel contains a WEBP. This is an educated estimate, the WEBP
 * might still be invalid even if this function indicates that WEBP data will
 * follow.
 */
YBOOL
matchWEBP(Ychannel *channel);

/**
 * Decode a WEBP coming from a Ychannel into a Vbitmap. Wrap your WEBP source
 * (file on disk, memory buffer) with a Ychannel and then use this generic API.
 * @see Ychannel
 *
 * @param channel raw WEBP data source
 * @param bitmap to decode into
 * @param maxWidth of decoded bitmap
 * @param maxHeight of decoded bitmap
 * @param quality of decoder from 0 (fastest) to 100 (highest quality)
 *        A negative value uses default (85) quality
 * @param scaleMode to use when resizing, @see Ymagine
 * @param pixelShader pixel shader that will be applied on-line during
 * decoding. Use NULL if do not want apply pixel shader @see Pixelshader
 *
 * @return YMAGINE_OK on success
 */
int
decodeWEBP(Ychannel *channel, Vbitmap *bitmap,
           int maxWidth, int maxHeight, int scaleMode,
           int quality, PixelShader* pixelShader);

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_FORMAT_H */



