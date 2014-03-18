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

#define LOG_TAG "ymagine::coloreffect"

#include "ymagine/ymagine.h"
#include "ymagine_priv.h"

#include <math.h>
#include <float.h>

struct Yshader_ColorPixelShaderExtraData
{
  int saturation;
  int exposure;
  int contrast;
  int brightness;
  int temperature;
  int monoMix[3];

  uint8_t *map;
  uint8_t *preset;
};

#define LUMINANCE_DEFAULT_R 306
#define LUMINANCE_DEFAULT_G 601
#define LUMINANCE_DEFAULT_B 117

#define COLOR_EFFECT_MAP_ELEMENT_COUNT 256
#define CURVE_VALUES_ELEMENT_COUNT 256
#define CURVE_INVERSE_NUM 256

// #include "effects/coloreffects.h"

#include "shaders/filterutils_inline.h"

static int
applyExposure(int intensity, int exposure)
{
  float fintensity;
  float fexposure;
  float inv;
  float result;

  if (exposure == 0) {
    return intensity;
  }

  if (intensity <= 0) {
    intensity = 1;
  } else if (intensity >= 255) {
    intensity = 254;
  }

  fintensity = intensity / 255.0f;
  fexposure = ((float) exposure) / YFIXED_ONE;

  inv = (log(1.0f/fintensity - 1.0f))/(-0.75f);
  result = 1.0f/(1.0f + exp(-0.75f * (inv + fexposure)));

  if (result <= 0.0f) {
    return 0;
  }

  if (result >= 1.0f) {
    return 255;
  }

  return (int)(result * 255.0f);
}

uint8_t*
createEffectMap(const unsigned char *preset,
                int contrast, int brightness,
                int exposure, int temperature)
{
  uint8_t *effectMap = NULL;
  uint8_t *curvevalues = NULL;
  uint8_t *curveR;
  uint8_t *curveG;
  uint8_t *curveB;
  uint8_t *curveA;

  int wbR;
  int wbG;
  int wbB;
  uint8_t colorR;
  uint8_t colorG;
  uint8_t colorB;
  int i;

  effectMap = (uint8_t*)Ymem_malloc(COLOR_EFFECT_MAP_ELEMENT_COUNT * 4 * sizeof(uint8_t));
  if (effectMap == NULL) {
    return NULL;
  }

  curvevalues = (uint8_t*)Ymem_malloc(CURVE_VALUES_ELEMENT_COUNT * 4 * sizeof(uint8_t));
  if (curvevalues == NULL) {
    Ymem_free(effectMap);
    return NULL;
  }

  if (preset != NULL) {
    /* Custom preset */
    memcpy(curvevalues, preset, 256 * 3);

    curveR = curvevalues;
    curveG = curvevalues + 256;
    curveB = curvevalues + 512;
    curveA = NULL;
  } else {
    curveR = NULL;
    curveG = NULL;
    curveB = NULL;
    curveA = NULL;
  }

  /* Color for white */
  wbR = 255;
  wbG = 255;
  wbB = 255;

  /* Apply white balance */
  if (temperature != 0) {
    uint32_t rgbTemperature = YcolorKtoRGB(temperature);

    wbR = (rgbTemperature >> 16) & 0xff;
    wbG = (rgbTemperature >> 8) & 0xff;
    wbB = (rgbTemperature >>  0) & 0xff;
  }

  for (i = 0; i < COLOR_EFFECT_MAP_ELEMENT_COUNT; i++) {
    int exposureVal = applyExposure(i, exposure);

    colorR = exposureVal;
    colorG = exposureVal;
    colorB = exposureVal;

    /* White balance */
    colorR = byteClamp((colorR * wbR) / 255);
    colorG = byteClamp((colorG * wbG) / 255);
    colorB = byteClamp((colorB * wbB) / 255);

    /* Brightness */
    if (brightness != 0) {
      colorR = byteClamp(colorR + brightness);
      colorG = byteClamp(colorG + brightness);
      colorB = byteClamp(colorB + brightness);
    }

    if (contrast != 0) {
      /* Contrast */
      colorR = byteClamp(yMixi(128, colorR, contrast));
      colorG = byteClamp(yMixi(128, colorG, contrast));
      colorB = byteClamp(yMixi(128, colorB, contrast));
    }

    /* Curves */
    if (curveR != NULL) {
      colorR = curveR[colorR];
    }
    if (curveG != NULL) {
      colorG = curveG[colorG];
    }
    if (curveB != NULL) {
      colorB = curveB[colorB];
    }

    if (curveA != NULL) {
      colorR = curveA[colorR];
      colorG = curveA[colorG];
      colorB = curveA[colorB];
    }

    effectMap[i] = colorR;
    effectMap[256 + i] = colorG;
    effectMap[512 + i] = colorB;
  }

  if (curvevalues != NULL) {
    Ymem_free(curvevalues);
  }

  return effectMap;
}
