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

typedef int (*YmagineFormatOptions_ProgressCB)(YmagineFormatOptions *options,
                                               int format, int width, int height);

#define YMAGINE_IMAGEFORMAT_UNKNOWN 0
#define YMAGINE_IMAGEFORMAT_JPEG    1
#define YMAGINE_IMAGEFORMAT_WEBP    2
#define YMAGINE_IMAGEFORMAT_PNG     3
#define YMAGINE_IMAGEFORMAT_GIF     4

#define YMAGINE_METAMODE_NONE       0
#define YMAGINE_METAMODE_COMMENTS   1
#define YMAGINE_METAMODE_ALL        2
#define YMAGINE_METAMODE_DEFAULT    -1

#define YMAGINE_ADJUST_NONE         0
#define YMAGINE_ADJUST_INNER        1
#define YMAGINE_ADJUST_OUTER        2

YmagineFormatOptions*
YmagineFormatOptions_Create();

YmagineFormatOptions*
YmagineFormatOptions_Duplicate(YmagineFormatOptions* refopts);

int
YmagineFormatOptions_Release(YmagineFormatOptions *options);

YmagineFormatOptions*
YmagineFormatOptions_Reset(YmagineFormatOptions *options);

YmagineFormatOptions*
YmagineFormatOptions_setQuality(YmagineFormatOptions *options,
                                int quality);

/**
 * Normalizes the quality parameter between 0 and 100
 *
 * @param options YmagineFormatOptions options
 *
 * @return A quality between 0 and 100, 85 by default
 */
#define YMAGINE_DEFAULT_QUALITY 85
int
YmagineFormatOptions_normalizeQuality(YmagineFormatOptions *options);

YmagineFormatOptions*
YmagineFormatOptions_setAccuracy(YmagineFormatOptions *options,
                                 int accuracy);

YmagineFormatOptions*
YmagineFormatOptions_setSubsampling(YmagineFormatOptions *options,
                                    int subsampling);

YmagineFormatOptions*
YmagineFormatOptions_setProgressive(YmagineFormatOptions *options,
                                    int progressive);

YmagineFormatOptions*
YmagineFormatOptions_setSharpen(YmagineFormatOptions *options,
                                float sigma);

YmagineFormatOptions*
YmagineFormatOptions_setBlur(YmagineFormatOptions *options,
                             float radius);

/**
 * Set rotation (with arbitrary angle) to apply
 *
 * @param options YmagineFormatOptions options
 * @param angle angle for the rotation in degrees
 */
YmagineFormatOptions*
YmagineFormatOptions_setRotate(YmagineFormatOptions *options,
                               float angle);

YmagineFormatOptions*
YmagineFormatOptions_setResize(YmagineFormatOptions *options,
                               int maxWidth, int maxHeight, int scalemode);

YmagineFormatOptions*
YmagineFormatOptions_setAdjust(YmagineFormatOptions *options,
                               int adjustmode);

YmagineFormatOptions*
YmagineFormatOptions_setResizable(YmagineFormatOptions *options,
                                  int resizable);

YmagineFormatOptions*
YmagineFormatOptions_setShader(YmagineFormatOptions *options,
                               PixelShader *shader);

/**
 * Set output format
 *
 * @param options YmagineFormatOptions options
 * @param format output format, defined in the top
 */
YmagineFormatOptions*
YmagineFormatOptions_setFormat(YmagineFormatOptions *options, int format);

/**
 * Set offset of crop region in pixels
 *
 * @param options YmagineFormatOptions options
 * @param x start of crop region on x axis.
 *        x is 0-indexed, position x will be included in the crop region.
 * @param y start of crop region on y axis.
 *        y is 0-indexed, position y will be included in the crop region.
 */
YmagineFormatOptions*
YmagineFormatOptions_setCropOffset(YmagineFormatOptions *options,
                                   int x, int y);

/**
 * Set size of crop region in pixels
 *
 * @param width width of crop region.
 *        x + width - 1 will be in the crop region, but x + width will not.
 * @param height height of crop region.
 *        x + height - 1 will be in the crop region, but x + height will not.
 */
YmagineFormatOptions*
YmagineFormatOptions_setCropSize(YmagineFormatOptions *options,
                                 int width, int height);

/**
 * Set crop region in pixels
 *
 * @param options YmagineFormatOptions options
 * @param x start of crop region on x axis.
 *        x is 0-indexed, position x will be included in the crop region.
 * @param y start of crop region on y axis.
 *        y is 0-indexed, position y will be included in the crop region.
 * @param width width of crop region.
 *        x + width - 1 will be in the crop region, but x + width will not.
 * @param height height of crop region.
 *        x + height - 1 will be in the crop region, but x + height will not.
 */
YmagineFormatOptions*
YmagineFormatOptions_setCrop(YmagineFormatOptions *options,
                             int x, int y, int width, int height);

/**
 * Set offset of crop region in percentage relative to image boundary
 *
 * value should be in range [0.0, 1.0]
 *
 * @param options YmagineFormatOptions options
 * @param x start of crop region on x axis, relative to image width.
 * @param y start of crop region on y axis, relative to image height.
 */
YmagineFormatOptions*
YmagineFormatOptions_setCropOffsetRelative(YmagineFormatOptions *options,
                                           float x, float y);

/**
 * Set size of crop region in percentage relative to image boundary
 *
 * value should be in range [0.0, 1.0]
 *
 * @param options YmagineFormatOptions options
 * @param width width of crop region, relative to image width.
 * @param height height of crop region, relative to image height.
 */
YmagineFormatOptions*
YmagineFormatOptions_setCropSizeRelative(YmagineFormatOptions *options,
                                         float width, float height);
/**
 * Set crop region in percentage relative to image boundary
 *
 * value should be in range [0.0, 1.0]
 *
 * @param options YmagineFormatOptions options
 * @param xr start of crop region on x axis, relative to image width.
 * @param yr start of crop region on y axis, relative to image height.
 * @param widthr width of crop region, relative to image width.
 * @param heightr height of crop region, relative to image height.
 */
YmagineFormatOptions*
YmagineFormatOptions_setCropRelative(YmagineFormatOptions *options,
                                     float xr, float yr,
                                     float widthr, float heightr);

/**
 * Set mode for handling meta (e.g. Exif) on transcode
 *
 * @param options YmagineFormatOptions options
 * @param metamode  0 for no copy, 1 for comments only, 2 for copy all
 */
YmagineFormatOptions*
YmagineFormatOptions_setMetaMode(YmagineFormatOptions *options,
                                 int metamode);

/**
 * Set color used as default background, default to transparent
 *
 * @param options YmagineFormatOptions options
 * @param color color in RGB format
 */
YmagineFormatOptions*
YmagineFormatOptions_setBackgroundColor(YmagineFormatOptions *options,
                                        int color);


/**
 * Set opaque data handle attached to this format option
 *
 * @param options YmagineFormatOptions options
 * @param data any value to store as private data
 */
YmagineFormatOptions*
YmagineFormatOptions_setData(YmagineFormatOptions *options,
                             void *data);


/**
 * Get opaque data handle attached to this format option
 *
 * @param options YmagineFormatOptions options
 *
 * @return data attached to options, NULL by default
 */
void*
YmagineFormatOptions_getData(YmagineFormatOptions *options);


/**
 * Set callback function to be invoked during decoding and transcoding pipeline
 *
 * @param options YmagineFormatOptions options
 * @param progresscb Callback function to invoke during decoding
 */
YmagineFormatOptions*
YmagineFormatOptions_setCallback(YmagineFormatOptions *options,
                                 YmagineFormatOptions_ProgressCB progresscb);


/**
 * Invoke callback for options
 *
 * @param options YmagineFormatOptions options
 * @param width Width of the input image, passed to callback as is
 * @param height Height of the input image, passed to callback as is
 * @param format Image format of the input image, passed to callback as is
 *
 * @return Code returned by callback, YMAGINE_OK if no callback or on success
 */
int
YmagineFormatOptions_invokeCallback(YmagineFormatOptions *options,
                                    int width, int height, int format);

/**
 * Detect image format from Ychannel. This is non destructive API, buffer read
 * from channel will be pushed back into it for future read
 * @see Ychannel
 *
 * @param bitmap to decode into
 * @param channel raw data source
 *
 * @return One of the YMAGINE_IMAGEFORMAT constants if image format is detected,
 *         YMAGINE_IMAGEFORMAT_UNKNOWN otherwise
 */
int
YmagineFormat(Ychannel *channel);

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
 * Decode an image from an input Vbitmap into a Vbitmap.
 *
 * @param bitmap to decode into
 * @param srcbitmap source Vbitmap
 * @param options for decoder
 *
 * @return YMAGINE_OK on success
 */
int
YmagineDecodeCopy(Vbitmap *bitmap, Vbitmap *srcbitmap, YmagineFormatOptions *options);

/**
 * Decode an image coming from a Ychannel into a Vbitmap. Wrap your source
 * (file on disk, memory buffer) with a Ychannel and then use this generic API.
 * @see Ychannel
 *
 * @param bitmap to decode into
 * @param channel raw JPEG data source
 * @param maxWidth of decoded bitmap
 * @param maxHeight of decoded bitmap
 * @param scaleMode scaling mode
 *
 * @return YMAGINE_OK on success
 */
int
YmagineDecodeResize(Vbitmap *bitmap, Ychannel *channel,
                    int maxWidth, int maxHeight, int scaleMode);

/**
 * Decode an image coming from a Ychannel into a Vbitmap. Wrap your source
 * (file on disk, memory buffer) with a Ychannel and then use this generic API.
 * Decoded image will be stored into passed bitmap, which won't be resized.
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
YmagineDecodeInPlace(Vbitmap *bitmap, Ychannel *channel,
                     int maxWidth, int maxHeight, int scaleMode);

/**
 * Trancode image while being able to crop and scale it
 *
 * @param channelin raw data source
 * @param channelout raw output
 * @param options options given to Ymagine
 *
 * @return YMAGINE_OK on success
 */
int
YmagineTranscode(Ychannel *channelin, Ychannel *channelout,
                 YmagineFormatOptions *options);

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
 * @param options options given to Ymagine
 *
 * @return YMAGINE_OK on success
 */
int
decodeJPEG(Ychannel *channel, Vbitmap *bitmap,
              YmagineFormatOptions *options);

/**
 * Reencode a JPEG while being able to scale it
 *
 * @param channelin raw JPEG data source
 * @param channelout raw JPEG output
 * @param options options given to Ymagine
 *
 * @return YMAGINE_OK on success
 */
int
transcodeJPEG(Ychannel *channelin, Ychannel *channelout,
              YmagineFormatOptions *options);

/**
 * Encode a Vbitmap using JPEG
 *
 * @param vbitmap to encode
 * @param channelout raw JPEG output
 * @param options options given to Ymagine
 *
 * @return YMAGINE_OK on success
 */
int
encodeJPEG(Vbitmap *vbitmap, Ychannel *channelout, YmagineFormatOptions *options);

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
 * @param options options given to Ymagine
 *
 * @return YMAGINE_OK on success
 */
int
decodeWEBP(Ychannel *channel, Vbitmap *bitmap,
           YmagineFormatOptions *options);

/**
 * Encode a Vbitmap using WEBP
 *
 * @param vbitmap to encode
 * @param channelout raw WEBP output
 * @param options options given to Ymagine
 *
 * @return YMAGINE_OK on success
 */
int
encodeWEBP(Vbitmap *vbitmap, Ychannel *channelout, YmagineFormatOptions *options);

/**
 * Test if a Ychannel contains a GIF. This is an educated estimate, the GIF
 * might still be invalid even if this function indicates that GIF data will
 * follow.
 */
YBOOL
matchGIF(Ychannel *channel);

/**
 * Decode a GIF coming from a Ychannel into a Vbitmap. Wrap your GIF source
 * (file on disk, memory buffer) with a Ychannel and then use this generic API.
 * @see Ychannel
 *
 * @param channel raw WEBP data source
 * @param bitmap to decode into
 * @param options options given to Ymagine
 *
 * @return YMAGINE_OK on success
 */
int
decodeGIF(Ychannel *channel, Vbitmap *bitmap,
          YmagineFormatOptions *options);

/**
 * Test if a Ychannel contains a PNG. This is an educated estimate, the PNG
 * might still be invalid even if this function indicates that PNG data will
 * follow.
 */
YBOOL
matchPNG(Ychannel *channel);

/**
 * Decode a PNG coming from a Ychannel into a Vbitmap. Wrap your PNG source
 * (file on disk, memory buffer) with a Ychannel and then use this generic API.
 * @see Ychannel
 *
 * @param channel raw WEBP data source
 * @param bitmap to decode into
 * @param options options given to Ymagine
 *
 * @return YMAGINE_OK on success
 */
int
decodePNG(Ychannel *channel, Vbitmap *bitmap,
          YmagineFormatOptions *options);

/**
 * Encode a Vbitmap using PNG
 *
 * @param vbitmap to encode
 * @param channelout raw PNG output
 * @param options options given to Ymagine
 *
 * @return YMAGINE_OK on success
 */
int
encodePNG(Vbitmap *vbitmap, Ychannel *channelout, YmagineFormatOptions *options);

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_FORMAT_H */



