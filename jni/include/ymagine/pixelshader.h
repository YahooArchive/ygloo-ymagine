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
 * @file   pixelshader.h
 * @addtogroup Pixelshader
 * @brief  Pixel shader
 */

#ifndef _YMAGINE_PIXELSHADER_H
#define _YMAGINE_PIXELSHADER_H 1

#include "ymagine/ymagine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Pixelshader Pixelshader
 *
 * This module provides API to manage pixel shader and apply pixel shader to
 * image or pixel buffer.
 *
 * @{
 */

/**
 * @brief represents pixel shader operation.
 * @ingroup Pixelshader
 *
 * It can be applied to Vbitmap or pixel buffer.
 * Or applied during decoding or transcoding.
 * @see Ymagine_PixelShader_applyOnBitmap Ymagine_PixelShader_apply
 * decodeJPEG transcodeJPEG
 *
 */
typedef struct PixelShaderStruct PixelShader;

/**
 * @brief Create pixel shader.
 * @ingroup Pixelshader
 *
 * Caller is responsible for releasing the pixel shader
 * @see Ymagine_PixelShader_release
 *
 * @param name pixel shader operation names seperated by ';' without whitespace.
 * e.g., color-iced_tea;vignette-white_pinhole. Invalid operation name will be
 * omitted.
 * @see Ymagine_PixelShader_listName
 *
 * @return pointer to PixelShader if at least one shader operation name is
 * valid. Otherwise return NULL.
 */
PixelShader*
Yshader_PixelShader_create();

void
Yshader_PixelShader_release(PixelShader* shader);

int
Yshader_apply(PixelShader* shader,
              uint8_t* pixelBuffer,
              int width, int bpp,
              int imageWidth, int imageHeight,
              int imageX, int imageY);

/**
 * @brief Releases pixel shader.
 * @ingroup Pixelshader
 */
void
Ymagine_PixelShader_release(PixelShader* shader);

/**
 * @brief set vignette information for pixel shader.
 * @ingroup Pixelshader
 *
 * @param vmap the Vbitmap to be composed on, the Vbitmap will be retained
 *        @see Vbitmap
 * @param compose @see Compose#ymagineCompose
 */
int
Yshader_PixelShader_vignette(PixelShader *shader, Vbitmap *vmap, ymagineCompose compose);

int
Yshader_PixelShader_saturation(PixelShader *shader, float saturation);

int
Yshader_PixelShader_exposure(PixelShader *shader, float exposure);

int
Yshader_PixelShader_contrast(PixelShader *shader, float contrast);

int
Yshader_PixelShader_brightness(PixelShader *shader, float brightness);

int
Yshader_PixelShader_temperature(PixelShader *shader, float temperature);

int
Yshader_PixelShader_whitebalance(PixelShader *shader, float whitebalance);

/**
 * set preset for Shader
 *
 * Preset is a mapping which maps a color from color space 0 - 255 to
 * the same 0 - 255 color space.
 *
 * @param presetchannel Ychannel containing array of bytes which serve as preset
 *        for pixels. The array of bytes should be of size 3*256. First section
 *        of 256 bytes is mapping for red channel, second section of 256 bytes
 *        is mapping for green channel, last section of 256 bytes is mapping for
 *        blue channel.
 *
 * @return YMAGINE_OK if succesfull, else YMAGINE_ERROR.
 */
int
Yshader_PixelShader_preset(PixelShader *shader, Ychannel *presetchannel);

/**
 * @brief Apply in-place pixel shader to bitmap.
 * @ingroup Pixelshader
 *
 * @param bitmap bitmap to be applied with in-place pixel shader.
 * @return YMAGINE_OK if applying pixel shader is succesfull,
 * else YMAGINE_ERROR.
 */
int
Ymagine_PixelShader_applyOnBitmap(Vbitmap* bitmap, PixelShader* shader);
    
/**
 * @}
 */

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_PIXELSHADER_H */
