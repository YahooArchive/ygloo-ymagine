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
* @file   color.h
* @addtogroup Color
* @brief  Theme colors
*/
#ifndef _YMAGINE_COLOR_H
#define _YMAGINE_COLOR_H 1

#include "ymagine/ymagine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup Color Color
 *
 * This module provides API for extracting the theme color i.e. most dominant color in an image
 *
 * @{
 */

#define YMAGINE_THEME_DEFAULT   -1
#define YMAGINE_THEME_NONE       0
#define YMAGINE_THEME_SATURATION 1

/**
 * @brief Extracts the theme color from a bitmap
 * @ingroup Color
 *
 * @param vbitmap This is a pointer to a vbitmap that contains the buffer for the image
 * @return the theme color of the image in ARGB format (e.g. 0xFFFFFFFF)
 */
int
getThemeColor(Vbitmap *vbitmap);

/**
 * @brief Extracts multiple theme colors from a bitmap
 * @ingroup Color
 *
 * @param vbitmap This is a pointer to a vbitmap that contains the buffer for the image
 * @param ncol maximum amount of colors wanted
 * @param colors an array whose size is at least ncol that would be populated with colors 
 *		extracted from the image. These colors are in ARGB format.
 * @param scores an array whose size is at least ncol that would contain level of confidence
 *        that colors are dominant
 * @return amount of colors found
 */
int
getThemeColors(Vbitmap *vbitmap, int ncol, int *colors, int *scores);

/**
 * @brief Generate an RGBA 32bits color from its red, green, blue and alpha components
 * @ingroup Color
 *
 * @param r Red compoment (in the range 0..255)
 * @param g Green compoment (in the range 0..255)
 * @param b Blue compoment (in the range 0..255)
 * @param a Alpha compoment (in the range 0..255)

 * @return color as a 32bits integer
 */
uint32_t YcolorRGBA(int r, int g, int b, int a);

/**
 * @brief Generate an RGBA 32bits opaque color from its red, green and blue components
 * @ingroup Color
 *
 * @param r Red compoment (in the range 0..255)
 * @param g Green compoment (in the range 0..255)
 * @param b Blue compoment (in the range 0..255)

 * @return color as a 32bits integer
 */
uint32_t YcolorRGB(int r, int g, int b);

/**
 * @brief Generate an HSV 32bits color from its hue, saturation, value and alpha components
 * @ingroup Color
 *
 * @param r Hue (in the range 0..255)
 * @param g Saturation (in the range 0..255)
 * @param b Value (in the range 0..255)
 * @param a Alpha compoment (in the range 0..255)

 * @return color as a 32bits integer
 */
uint32_t YcolorHSVA(int h, int s, int v, int a);

/**
 * @brief Generate an HSV 32bits opaque color from its hue, saturation and value components
 * @ingroup Color
 *
 * @param r Hue (in the range 0..255)
 * @param g Saturation (in the range 0..255)
 * @param b Value (in the range 0..255)

 * @return color as a 32bits integer
 */
uint32_t YcolorHSV(int h, int s, int v);

/**
 * @brief Get Hue component of HSV color
 * @ingroup Color
 *
 * @param hsv HSV color

 * @return Hue in the range [0..255]
 */
int YcolorHSVtoHue(uint32_t hsv);

/**
 * @brief Get Saturation component of HSV color
 * @ingroup Color
 *
 * @param hsv HSV color

 * @return Saturation in the range [0..255]
 */
int YcolorHSVtoSaturation(uint32_t hsv);

/**
 * @brief Get Brightness component of HSV color
 * @ingroup Color
 *
 * @param hsv HSV color

 * @return Brightness in the range [0..255]
 */
int YcolorHSVtoBrightness(uint32_t hsv);

/**
 * @brief Get Alpha component of HSV color
 * @ingroup Color
 *
 * @param hsv HSV color

 * @return Alpha in the range [0..255]
 */
int YcolorHSVtoAlpha(uint32_t hsv);

/**
 * @brief Get Red component of RGB color
 * @ingroup Color
 *
 * @param rgb RGB color

 * @return Red in the range [0..255]
 */
int YcolorRGBtoRed(uint32_t rgb);

/**
 * @brief Get Green component of RGB color
 * @ingroup Color
 *
 * @param rgb RGB color

 * @return Green in the range [0..255]
 */
int YcolorRGBtoGreen(uint32_t rgb);

/**
 * @brief Get Blue component of RGB color
 * @ingroup Color
 *
 * @param rgb RGB color

 * @return Blue in the range [0..255]
 */
int YcolorRGBtoBlue(uint32_t rgb);

/**
 * @brief Get Alpha component of RGB color
 * @ingroup Color
 *
 * @param rgb RGB color

 * @return Alpha in the range [0..255]
 */
int YcolorRGBtoAlpha(uint32_t rgb);

/**
 * @brief Convert an RGB color into a HSV one
 * @ingroup Color
 *
 * @param rgb RGB color

 * @return HSV color as a 32bits integer
 */
uint32_t YcolorRGBtoHSV(uint32_t rgb);

/**
 * @brief Convert an HUV color into a RGB one
 * @ingroup Color
 *
 * @param hsv HSV color

 * @return RGB color as a 32bits integer
 */
uint32_t YcolorHSVtoRGB(uint32_t hsv);

/**
 * @brief Convert a color temperature in Kelvin into its RGB equivalent
 * @ingroup Color
 *
 * @param k temperature in Kelvin

 * @return RGB color as a 32bits integer
 */
uint32_t YcolorKtoRGB(int k);

/**
 * @}
 */

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_COLOR_H */
