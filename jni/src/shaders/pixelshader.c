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

#include "ymagine/ymagine.h"
#include "ymagine_priv.h"

#include "shaders/filterutils_inline.h"

#define LOG_TAG "ymagine::pixelshader"

#define LUMINANCE_DEFAULT_R 306
#define LUMINANCE_DEFAULT_G 601
#define LUMINANCE_DEFAULT_B 117

#define PRESET_ARRAY_SIZE (256*3)

/* Color temperature in K */
#define TEMPERATURE_NEUTRAL 6500   /* White balance 0 */
#define TEMPERATURE_MIN 1000       /* Temperature for white balance -1 */
#define TEMPERATURE_MAX 20000      /* Temperature for white balance 1 */

enum Yshader_PixelShaderType
{
  YSHADER_PIXEL_SHADER_NONE = 0,
  YSHADER_PIXEL_SHADER_COLOR,
  YSHADER_PIXEL_SHADER_VIGNETTE
};
typedef enum Yshader_PixelShaderType Yshader_PixelShaderType;

YOSAL_OBJECT_DECLARE(Yshader_PixelShaderEffect)
YOSAL_OBJECT_BEGIN
  Yshader_PixelShaderType type;
  int need_update;
  
  /* Vignette meta-data */
  Vbitmap *vignette;
  ymagineCompose compose;
  /* Color transform meta-data */
  int exposure;
  int brightness;
  int contrast;
  int temperature;

  int saturation;
  int monoMix[3];

  uint8_t *curve;
  uint8_t *preset;
YOSAL_OBJECT_END

YOSAL_OBJECT_EXPORT(Yshader_PixelShaderEffect)

struct PixelShaderStruct {
  YArray *kernellist;
};

/*
 * API to manage shader effects
 */
static void
effectReleaseCallback(void *ptr)
{
  Yshader_PixelShaderEffect* effect = (Yshader_PixelShaderEffect*)ptr;

  if (effect == NULL) {
    return;
  }

  if (effect->vignette != NULL) {
    VbitmapRelease(effect->vignette);
    effect->vignette = NULL;
  }

  if (effect->curve != NULL) {
    Ymem_free(effect->curve);
    effect->curve = NULL;
  }

  if (effect->preset != NULL) {
    Ymem_free(effect->preset);
    effect->preset = NULL;
  }

  Ymem_free(effect);
}

Yshader_PixelShaderEffect*
effectCreate()
{
  Yshader_PixelShaderEffect* element;

  element = (Yshader_PixelShaderEffect*)
    yobject_create(sizeof(Yshader_PixelShaderEffect), effectReleaseCallback);
  
  if (element == NULL) {
    return NULL;
  }

  element->type = YSHADER_PIXEL_SHADER_NONE;

  element->need_update = 0;

  element->vignette = NULL;
  element->compose = YMAGINE_COMPOSE_OVER;

  element->exposure = 0;
  element->contrast = 0;
  element->brightness = 0;
  element->temperature = 0;

  /* Saturation of 1.0 to preserve original image */
  element->saturation = YFIXED_ONE;
  element->monoMix[0] = LUMINANCE_DEFAULT_R;
  element->monoMix[1] = LUMINANCE_DEFAULT_G;
  element->monoMix[2] = LUMINANCE_DEFAULT_B;

  element->curve = NULL;
  element->preset = NULL;

  return element;
}

Yshader_PixelShaderEffect*
effectRetain(Yshader_PixelShaderEffect* element)
{
  return (Yshader_PixelShaderEffect*) yobject_retain((yobject*) element);
}

int
effectRelease(Yshader_PixelShaderEffect* element)
{
  return yobject_release((yobject*) element);
}

static Yshader_PixelShaderEffect*
getEffect(PixelShader* shader, int index)
{
  if (shader == NULL || shader->kernellist == NULL) {
    return NULL;
  }

  return YArray_get(shader->kernellist, index);
}

/* A PixelShader is an ordered set of shader kernels */
PixelShader*
Yshader_PixelShader_create()
{
  YArray *kernellist;
  PixelShader *shader;

  shader = Ymem_malloc(sizeof(PixelShader));
  if (shader == NULL) {
    return NULL;
  }
  
  kernellist = YArray_createLength(8);
  if (kernellist == NULL) {
    Ymem_free(shader);
    return NULL;
  }

  shader->kernellist = kernellist;
  YArray_setElementReleaseFunc(kernellist, (YArrayElementReleaseFunc) effectRelease);

  return shader;
}

void
Yshader_PixelShader_release(PixelShader *shader)
{
  if (shader != NULL) {
    if (shader->kernellist != NULL) {
      YArray_release(shader->kernellist);
    }
    Ymem_free(shader);
  }
}

static int
shader_length(PixelShader* shader)
{
  if (shader == NULL || shader->kernellist == NULL) {
    return 0;
  }

  return YArray_length(shader->kernellist);
}

static int
shader_append(PixelShader* shader,
              Yshader_PixelShaderEffect* element)
{
  Yshader_PixelShaderEffect *p;
  int rc = YMAGINE_OK;

  if (element == NULL) {
    return YMAGINE_OK;
  }

  if (shader == NULL || shader->kernellist == NULL) {
    return YMAGINE_ERROR;
  }

  p = effectRetain(element);
  if (p == NULL) {
    rc = YMAGINE_ERROR;
  } else {
    int result = YArray_append(shader->kernellist, (void*) p);
    if (result != YOSAL_OK) {
      rc = YMAGINE_ERROR;
      effectRelease(p);
    }
  }

  return rc;
}

static Yshader_PixelShaderEffect*
get_active_colorshader(PixelShader *shader)
{
  Yshader_PixelShaderEffect* element = NULL;
  int slength;

  if (shader == NULL || shader->kernellist == NULL) {
    return NULL;
  }

  /* Check if last kernel in queue is already a color shader to update it */
  slength = shader_length(shader);
  if (slength > 0) {
    element = getEffect(shader, slength - 1);
    if (element == NULL || element->type != YSHADER_PIXEL_SHADER_COLOR) {
      element = NULL;
    }
  }

  if (element == NULL) {
    /* Otherwise create a new one */
    element = effectCreate();
    if (element == NULL) {
      return NULL;
    }

    element->type = YSHADER_PIXEL_SHADER_COLOR;
    shader_append(shader, element);
  }

  return element;
}

int
Yshader_PixelShader_prepare(PixelShader *shader)
{
  return YMAGINE_OK;
}

int
Yshader_PixelShader_saturation(PixelShader *shader, float saturation)
{
  Yshader_PixelShaderEffect* element;

  element = get_active_colorshader(shader);
  if (element == NULL) {
    return YMAGINE_ERROR;
  }

  element->saturation = (int) (element->saturation * saturation);
  element->need_update = 1;

  return YMAGINE_OK;
}

int
Yshader_PixelShader_exposure(PixelShader *shader, float exposure)
{
  Yshader_PixelShaderEffect* element;

  element = get_active_colorshader(shader);
  if (element == NULL) {
    return YMAGINE_ERROR;
  }

  element->exposure = element->exposure + (int) (exposure * YFIXED_ONE);
  element->need_update = 1;

  return YMAGINE_OK;
}

int
Yshader_PixelShader_contrast(PixelShader *shader, float contrast)
{
  Yshader_PixelShaderEffect* element;

  element = get_active_colorshader(shader);
  if (element == NULL) {
    return YMAGINE_ERROR;
  }

  element->contrast = element->contrast + (int) (contrast * YFIXED_ONE);
  element->need_update = 1;

  return YMAGINE_OK;
}

int
Yshader_PixelShader_brightness(PixelShader *shader, float brightness)
{
  Yshader_PixelShaderEffect* element;

  element = get_active_colorshader(shader);
  if (element == NULL) {
    return YMAGINE_ERROR;
  }

  element->brightness = element->brightness + (int) (255 * brightness);
  element->need_update = 1;

  return YMAGINE_OK;
}

/*
  Color temperatures for some light sources

     1700-1800        Match flame
     1850-1930        Candle flame
     2000-3000        Sun, at sunrise or sunset
     2500-2900        Household tungsten bulbs
     3000             Tungsten lamp 500W-1k
     3200-3500        Quartz lights
     3200-7500        Fluorescent lights
     3275             Tungsten lamp 2k
     3380             Tungsten lamp 5k, 10k
     5000-5400        Sun, direct at noon
     5500-6500        Daylight (sun and sky)
     5500-6500        Sun, through clouds/haze
     6000-7500        RGB monitor (white point)
     6500             Sky, overcast
     7000-8000        Outdoor shade areas
     8000-10000       Sky, partly cloudy
*/
int
Yshader_PixelShader_temperature(PixelShader *shader, float temperature)
{
  Yshader_PixelShaderEffect* element;

  element = get_active_colorshader(shader);
  if (element == NULL) {
    return YMAGINE_ERROR;
  }

  if (temperature <= 0.0f) {
    element->temperature = TEMPERATURE_NEUTRAL;
  } else if (temperature < TEMPERATURE_MIN) {
    element->temperature = TEMPERATURE_MIN;
  } else if (temperature > TEMPERATURE_MAX) {
    element->temperature = TEMPERATURE_MAX;
  } else {
    element->temperature = (int) (temperature + 0.5);
  }

  printf("T = %d Tf=%g\n", element->temperature, temperature);
  element->need_update = 1;

  return YMAGINE_OK;
}

int
Yshader_PixelShader_whitebalance(PixelShader *shader, float whitebalance)
{
  float temperature;

  if (whitebalance <= 0.0f) {
    if (whitebalance <= -1.0f) {
      temperature = TEMPERATURE_MIN;
    } else {
      temperature = TEMPERATURE_NEUTRAL + whitebalance * (TEMPERATURE_NEUTRAL - TEMPERATURE_MIN);
    }
  } else {
    if (whitebalance >= 1.0f) {
      temperature = TEMPERATURE_MAX;
    } else {
      temperature = TEMPERATURE_NEUTRAL + whitebalance * (TEMPERATURE_MAX - TEMPERATURE_NEUTRAL);
    }
  }

  return Yshader_PixelShader_temperature(shader, temperature);
}

int
Yshader_PixelShader_preset(PixelShader *shader, Ychannel *presetchannel)
{
  Yshader_PixelShaderEffect* element;
  uint8_t *preset;
  int bytesRead;

  if (!YchannelReadable(presetchannel)) {
    return YMAGINE_ERROR;
  }

  /* color mapping 0 - 255 for rgb 3 channels */
  preset = Ymem_malloc(PRESET_ARRAY_SIZE);
  if (preset == NULL) {
    return YMAGINE_ERROR;
  }

  bytesRead = YchannelRead(presetchannel, preset, PRESET_ARRAY_SIZE);

  if (bytesRead != PRESET_ARRAY_SIZE) {
    Ymem_free(preset);
    return YMAGINE_ERROR;
  }

  element = effectCreate();
  if (element == NULL) {
    Ymem_free(preset);
    return YMAGINE_ERROR;
  }

  element->type = YSHADER_PIXEL_SHADER_COLOR;
  element->preset = preset;
  element->need_update = 1;

  shader_append(shader, element);

  return YMAGINE_OK;
}

int
Yshader_PixelShader_vignette(PixelShader *shader, Vbitmap *vmap, ymagineCompose compose)
{
  Yshader_PixelShaderEffect* element = NULL;

  if (vmap == NULL || shader == NULL || shader->kernellist == NULL) {
    return YMAGINE_ERROR;
  }

  element = effectCreate();
  if (element == NULL) {
    return YMAGINE_ERROR;
  }

  VbitmapRetain(vmap);

  element->type = YSHADER_PIXEL_SHADER_VIGNETTE;
  element->vignette = vmap;
  element->compose = compose;

  shader_append(shader, element);

  return YMAGINE_OK;
}

static int
colorShaderFunction(Yshader_PixelShaderEffect* effect,
                    uint8_t* pixelBuffer, int width, int bpp)
{
  int i;
  int luminance;

  if (bpp != 3 && bpp != 4) {
    ALOGE("color pixel shader failed, bpp out of range: %d", bpp);
    return YMAGINE_ERROR;
  }

  if (effect->need_update) {
    if (effect->curve != NULL) {
      Ymem_free(effect->curve);
      effect->curve = NULL;
    }

    effect->curve = createEffectMap(effect->preset,
                                    effect->contrast, effect->brightness,
                                    effect->exposure, effect->temperature);

    effect->need_update = 0;
  }

  for (i = 0; i < width; i++) {
    uint8_t r = pixelBuffer[0];
    uint8_t g = pixelBuffer[1];
    uint8_t b = pixelBuffer[2];

    if (effect->saturation != YFIXED_ONE) {
      /* pre saturation/mono mix */    
      luminance = (r*effect->monoMix[0] + g*effect->monoMix[1] + b*effect->monoMix[2]) >> YFIXED_SHIFT;
      if (effect->saturation <= YFIXED_ZERO) {
        r = luminance;
        g = luminance;
        b = luminance;
      } else {
        r = byteClamp(yMixi(luminance, r, effect->saturation));
        g = byteClamp(yMixi(luminance, g, effect->saturation));
        b = byteClamp(yMixi(luminance, b, effect->saturation));
      }
    }

    if (effect->curve != NULL) {
      /* Apply color shader from precomputed conversion table */
      r = effect->curve[r];
      g = effect->curve[256 + g];
      b = effect->curve[512 + b];
    } else {
      /* Compute color shader dynamically */
    }

    pixelBuffer[0] = r;
    pixelBuffer[1] = g;
    pixelBuffer[2] = b;
    /* Leave alpha unchanged */

    pixelBuffer += bpp;

  }

  return YMAGINE_OK;
}

static int
vignetteShaderFunction(Yshader_PixelShaderEffect* effect,
                       uint8_t* pixelBuffer, int bpp,
                       int imageWidth, int imageHeight,
                       int imageX, int imageY)
{
  Vbitmap *map;
  unsigned char *mapdata;
  unsigned char *mapline;
  int mapwidth;
  int mapheight;
  int mappitch;
  int mapbpp;
  int mapy;

  if (bpp <= 0 || bpp > 4) {
    ALOGE("vignette pixel shader failed, bpp out of range: %d", bpp);
    return YMAGINE_ERROR;
  }

  map = effect->vignette;
  if (map == NULL) {
    return YMAGINE_OK;
  }

  VbitmapLock(map);

  mapdata = VbitmapBuffer(map);
  mapwidth = VbitmapWidth(map);
  mapheight = VbitmapHeight(map);
  mappitch = VbitmapPitch(map);
  mapbpp = VbitmapBpp(map);
  
  mapy = (imageY * (mapheight - 1)) / (imageHeight - 1);
  mapline = mapdata + mapy * mappitch;

  Ymagine_composeLine(pixelBuffer, bpp, imageWidth - imageX,
                      mapline, mapbpp, mapwidth,
                      effect->compose);

  VbitmapUnlock(map);

  return YMAGINE_OK;
}

/**
 * @brief Apply pixel shader to a pixel buffer.
 * @ingroup Pixelshader
 *
 * Pixel buffer could be entire image pixel buffer, or a portion of image pixel
 * buffer.
 *
 * @param shader shader to apply
 * @param pixelBuffer pixel buffer. E.g., RGBA buffer of image
 * @param bufferSize number of bytes in pixelBuffer
 * @param bufferIndex pixelBuffer's byte position relative to image's entire
 * pixel buffer
 * @param bpp number of byte per pixel of pixel buffer
 * @return YMAGINE_OK if operation is succesfull, else YMAGINE_ERROR.
 */
int
Yshader_apply(PixelShader* shader,
              uint8_t* pixelBuffer,
              int width, int bpp,
              int imageWidth, int imageHeight,
              int imageX, int imageY)
{
  Yshader_PixelShaderEffect* element;
  int count;
  int i;

  if (shader == NULL) {
    return YMAGINE_OK;
  }

  count = shader_length(shader);
  if (count <= 0) {
    return YMAGINE_OK;
  }

  /* Apply sequentially each kernel */
  for (i = 0; i < count; i++) {
    int result;
    element = getEffect(shader, i);

    switch (element->type) {
      case YSHADER_PIXEL_SHADER_COLOR:
        result = colorShaderFunction(element, pixelBuffer, width, bpp);
        break;
      case YSHADER_PIXEL_SHADER_VIGNETTE:
        result = vignetteShaderFunction(element, pixelBuffer, bpp, imageWidth, imageHeight, imageX, imageY);
        break;
      case YSHADER_PIXEL_SHADER_NONE:
        result = YMAGINE_OK;
        break;
      default: {
        result = YMAGINE_ERROR;
        break;
      }
    }

    if (result != YMAGINE_OK) {
      return YMAGINE_ERROR;
    }
  }

  return YMAGINE_OK;
}

int
Ymagine_PixelShader_applyOnBitmap(Vbitmap* bitmap, PixelShader* shader)
{
  int rc = YMAGINE_ERROR;
  int j;
  int count;

  if (shader == NULL) {
    return YMAGINE_OK;
  }

  count = shader_length(shader);
  if (count <= 0) {
    return YMAGINE_OK;
  }

  if (VbitmapLock(bitmap) != YMAGINE_OK) {
    rc = YMAGINE_ERROR;
  } else {
    uint8_t* pixelBuffer = VbitmapBuffer(bitmap);
    int imageWidth = VbitmapWidth(bitmap);
    int imageHeight = VbitmapHeight(bitmap);
    int bpp = VbitmapBpp(bitmap);
    int pitch = VbitmapPitch(bitmap);

    rc = YMAGINE_OK;

    for (j = 0; j < imageHeight; j++) {
      rc = Yshader_apply(shader,
                         pixelBuffer + j * pitch,
                         imageWidth, bpp,
                         imageWidth, imageHeight,
                         0, j);
      if (rc != YMAGINE_OK) {
        rc = YMAGINE_ERROR;
        break;
      }
    }

    VbitmapUnlock(bitmap);
  }

  return rc;
}
