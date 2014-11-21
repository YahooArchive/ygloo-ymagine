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

#define LOG_TAG "ymagine::vbitmap"

#include "ymagine/ymagine.h"
#include "ymagine_priv.h"

#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#undef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#undef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))

#define VBITMAP_ENABLE_GLOBAL_REF 0
#define VBITMAP_MAX_PSNR 100
#define LOG10 ((double) 2.302585092994046)

YOSAL_OBJECT_DECLARE(Vbitmap)
YOSAL_OBJECT_BEGIN
int bitmaptype;
int locked;
int width;
int height;
int pitch;
int colormode;

/* Panoramic support */
VbitmapXmp xmp;

/* Region is used as a pointer since its existence can be
 a check if the caller designated an active region */
Vrect *region;

unsigned char *pixels;

/* Android bitmap */
JavaVM *jvm;
jobject jbitmap;
int jkeepref;
YOSAL_OBJECT_END

static JNIEnv*
getEnv(Vbitmap *vbitmap)
{
  JNIEnv *jenv = NULL;

  if (vbitmap->bitmaptype == VBITMAP_ANDROID && vbitmap->jvm != NULL) {
    int getEnvStat;
    JavaVM *jvm = vbitmap->jvm;

    getEnvStat = (*jvm)->GetEnv(jvm, (void **) &jenv, JNI_VERSION_1_6);
    if (getEnvStat == JNI_EDETACHED) {
      /* Need to attach current thread to env */
      if ((*jvm)->AttachCurrentThread(jvm, &jenv, NULL) == 0) {
        /* TODO: should we detach this thread later ? */
      } else {
        /* Failed to attach */
        jenv = NULL;
      }
    } else if (getEnvStat == JNI_OK) {
    } else if (getEnvStat == JNI_EVERSION) {
      /* Invalid version */
      jenv = NULL;
    }
  }

  return jenv;
}

static void
vbitmap_release_callback(void *ptr)
{
  Vbitmap *vbitmap;

  if (ptr == NULL) {
    return;
  }

  vbitmap = (Vbitmap*)ptr;

  if (vbitmap->bitmaptype == VBITMAP_MEMORY) {
    if (vbitmap->pixels != NULL) {
      Ymem_free(vbitmap->pixels);
    }
    if (vbitmap->region != NULL) {
      Ymem_free(vbitmap->region);
    }
  }
  if (vbitmap->bitmaptype == VBITMAP_ANDROID) {
    if (vbitmap->jbitmap != NULL) {
      if (vbitmap->jkeepref) {
        JNIEnv *jenv = getEnv(vbitmap);
        if (jenv != NULL) {
          (*jenv)->DeleteGlobalRef(jenv, vbitmap->jbitmap);
        }
      }
      vbitmap->jbitmap = NULL;
    }
  }

  Ymem_free(vbitmap);
}

static Vbitmap*
VbitmapInit()
{
  Vbitmap *vbitmap = NULL;

  vbitmap = (Vbitmap*) yobject_create(sizeof(Vbitmap),
                                      vbitmap_release_callback);
  if (vbitmap == NULL) {
    return NULL;
  }

  vbitmap->bitmaptype = VBITMAP_NONE;
  vbitmap->locked = 0;

  vbitmap->width = 0;
  vbitmap->height = 0;
  vbitmap->pitch = 0;
  vbitmap->colormode = VBITMAP_COLOR_RGBA;

  memset(&(vbitmap->xmp), 0, sizeof(VbitmapXmp));

  vbitmap->region = NULL;

  vbitmap->pixels = NULL;

  vbitmap->jvm = NULL;
  vbitmap->jbitmap = NULL;
  vbitmap->jkeepref = 0;

  return vbitmap;
}

/* Vbitmap constructors */
Vbitmap*
VbitmapInitNone()
{
  return VbitmapInit();
}

Vbitmap*
VbitmapInitAndroid(JNIEnv *_env, jobject jbitmap)
{
    JavaVM *jvm = NULL;
    jobject jbitmapref;
    Vbitmap *vbitmap;
    AndroidBitmapInfo bitmapinfo;
    int ret;

    if (_env == NULL) {
      return NULL;
    }

    if ((*_env)->GetJavaVM(_env, &jvm) != JNI_OK) {
      return NULL;
    }
    if (jvm == NULL) {
      return NULL;
    }

    bitmapinfo.width = 0;
    bitmapinfo.height = 0;
    bitmapinfo.stride = 0;

    vbitmap = VbitmapInit();
    if (vbitmap == NULL) {
      return NULL;
    }

    vbitmap->bitmaptype = VBITMAP_ANDROID;
    vbitmap->colormode = VBITMAP_COLOR_RGBA;
    vbitmap->jvm = jvm;
    vbitmap->jbitmap = NULL;
    vbitmap->jkeepref = 0;

    vbitmap->width = 0;
    vbitmap->height = 0;
    vbitmap->pitch = 0;

    if (jbitmap != NULL) {
      jbitmapref = (*_env)->NewGlobalRef(_env, jbitmap);
      if (jbitmapref == NULL) {
        ALOGE("failed to acquire global ref for jbitmap");
        VbitmapRelease(vbitmap);
        return NULL;
      }

#if VBITMAP_ENABLE_GLOBAL_REF
      vbitmap->jbitmap = jbitmapref;
      vbitmap->jkeepref = 1;
#else
      vbitmap->jbitmap = jbitmap;
      vbitmap->jkeepref = 0;
      (*_env)->DeleteGlobalRef(_env, jbitmapref);
#endif

      ret = AndroidBitmap_getInfo(_env, vbitmap->jbitmap, &bitmapinfo);
      if (ret < 0) {
        VbitmapRelease(vbitmap);
        ALOGE("AndroidBitmap_getInfo() failed ! error=%d", ret);
        return NULL;
      }

      if (bitmapinfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        VbitmapRelease(vbitmap);
        ALOGE("Bitmap format is not RGBA_8888");
        return NULL;
      }

      vbitmap->width = bitmapinfo.width;
      vbitmap->height = bitmapinfo.height;
      vbitmap->pitch = bitmapinfo.stride;
    }

    return vbitmap;
}

Vbitmap*
VbitmapInitMemory(int colormode)
{
    Vbitmap *vbitmap;

    switch (colormode) {
    case VBITMAP_COLOR_RGBA:
    case VBITMAP_COLOR_RGB:
    case VBITMAP_COLOR_GRAYSCALE:
      break;
    default:
      /* Unsupported color mode */
      return NULL;
    }

    vbitmap = VbitmapInit();
    if (vbitmap != NULL) {
      vbitmap->bitmaptype = VBITMAP_MEMORY;
      vbitmap->colormode = colormode;
    }

    return vbitmap;
}

Vbitmap*
VbitmapInitStatic(int colormode, int width, int height, int pitch, unsigned char *pixels)
{
    Vbitmap *vbitmap;

    if (width <= 0 || height <= 0 || pixels == NULL) {
      return NULL;
    }

    switch (colormode) {
    case VBITMAP_COLOR_RGBA:
    case VBITMAP_COLOR_RGB:
    case VBITMAP_COLOR_GRAYSCALE:
      break;
    default:
      /* Unsupported color mode */
      return NULL;
    }

    vbitmap = VbitmapInit();
    if (vbitmap != NULL) {
      vbitmap->bitmaptype = VBITMAP_STATIC;
      vbitmap->pixels = pixels;
      vbitmap->width = width;
      vbitmap->height = height;
      vbitmap->pitch = pitch;
      vbitmap->colormode = colormode;
    }

    return vbitmap;
}

Vbitmap*
VbitmapInitGLTexture()
{
    /* NYI */
    return NULL;
}

/* Destructor */
int
VbitmapRelease(Vbitmap *vbitmap)
{
  if (vbitmap == NULL) {
    return YMAGINE_OK;
  }
  if (vbitmap->locked) {
    ALOGE("vbitmap locked, release failed");
    return YMAGINE_ERROR;
  }

  if (yobject_release((yobject*) vbitmap) != YOSAL_OK) {
    return YMAGINE_ERROR;
  }

  return YMAGINE_OK;
}

Vbitmap*
VbitmapRetain(Vbitmap *vbitmap)
{
  return (Vbitmap*) yobject_retain((yobject*) vbitmap);
}

/* Methods */
int
VbitmapResize(Vbitmap *vbitmap, int width, int height)
{
  if (vbitmap == NULL) {
    return YMAGINE_ERROR;
  }

  if (width <= 0 || height <= 0) {
    return YMAGINE_ERROR;
  }

  if (width == vbitmap->width && height == vbitmap->height) {
    /* Size not changed, ignore */
    return YMAGINE_OK;
  }

  if (vbitmap->bitmaptype == VBITMAP_NONE) {
    vbitmap->width = width;
    vbitmap->height = height;

    return YMAGINE_OK;
  }

  if (vbitmap->bitmaptype == VBITMAP_ANDROID) {
    AndroidBitmapInfo bitmapinfo;
    jobject jbitmap;
    jobject jbitmapref;
    int ret;

    JNIEnv *jenv = getEnv(vbitmap);
    if (jenv == NULL) {
      return YMAGINE_ERROR;
    }

    jbitmap = createAndroidBitmap(jenv, width, height);
    if (jbitmap == NULL) {
      return YMAGINE_ERROR;
    }

    ret = AndroidBitmap_getInfo(jenv, jbitmap, &bitmapinfo);
    if (ret < 0 ||
        bitmapinfo.format != ANDROID_BITMAP_FORMAT_RGBA_8888 ||
        bitmapinfo.width != width || bitmapinfo.height != height) {
      return YMAGINE_ERROR;
    }

    jbitmapref = (*jenv)->NewGlobalRef(jenv, jbitmap);
    if (jbitmapref == NULL) {
      return YMAGINE_ERROR;
    }

    /* Replace Bitmap */
    if (vbitmap->jbitmap != NULL) {
      if (vbitmap->jkeepref) {
        (*jenv)->DeleteGlobalRef(jenv, vbitmap->jbitmap);
        vbitmap->jkeepref = 0;
      }
      vbitmap->jbitmap = NULL;
    }

#if VBITMAP_ENABLE_GLOBAL_REF
      vbitmap->jbitmap = jbitmapref;
      vbitmap->jkeepref = 1;
#else
      vbitmap->jbitmap = jbitmap;
      vbitmap->jkeepref = 0;
      (*jenv)->DeleteGlobalRef(jenv, jbitmapref);
#endif

    vbitmap->width = bitmapinfo.width;
    vbitmap->height = bitmapinfo.height;
    vbitmap->pitch = bitmapinfo.stride;

    return YMAGINE_OK;
  }

  if (vbitmap->bitmaptype == VBITMAP_MEMORY) {
    int bpp = colorBpp(VbitmapColormode(vbitmap));
    int pitch = width * bpp;
    unsigned char *pixels = NULL;

    if (pitch > 0) {
      pixels = Ymem_malloc(pitch * height);
    }
    if (pixels == NULL) {
      return YMAGINE_ERROR;
    }

    if (vbitmap->pixels != NULL) {
      Ymem_free(vbitmap->pixels);
    }

    vbitmap->pixels = pixels;
    vbitmap->width = width;
    vbitmap->height = height;
    vbitmap->pitch = pitch;

    return YMAGINE_OK;
  }

  if (vbitmap->bitmaptype == VBITMAP_STATIC) {
    /* Can't resize a static bitmap */
    return YMAGINE_ERROR;
  }

  return YMAGINE_ERROR;
}

jobject
VbitmapGetAndroid(Vbitmap *vbitmap)
{
  if (vbitmap != NULL && vbitmap->bitmaptype == VBITMAP_ANDROID) {
    return vbitmap->jbitmap;
  }

  return NULL;
}

int
VbitmapLock(Vbitmap *vbitmap)
{
  int ret;
  unsigned char *pixels;

  if (vbitmap == NULL) {
    return YMAGINE_ERROR;
  }

  if (vbitmap->bitmaptype == VBITMAP_ANDROID) {
    JNIEnv *jenv;

    if (vbitmap->jbitmap == NULL) {
      return YMAGINE_ERROR;
    }

    jenv = getEnv(vbitmap);
    if (jenv == NULL) {
      return YMAGINE_ERROR;
    }

    ret = AndroidBitmap_lockPixels(jenv, vbitmap->jbitmap, (void**) &pixels);
    if (ret < 0) {
      return YMAGINE_ERROR;
    }
    vbitmap->pixels = pixels;
  }

  vbitmap->locked = 1;
  return YMAGINE_OK;
}

int
VbitmapUnlock(Vbitmap *vbitmap)
{
  if (vbitmap == NULL) {
    return YMAGINE_ERROR;
  }

  if (vbitmap->bitmaptype == VBITMAP_ANDROID) {
    if (vbitmap->locked) {
      JNIEnv *jenv = getEnv(vbitmap);
      if (jenv != NULL) {
        AndroidBitmap_unlockPixels(jenv, vbitmap->jbitmap);
      }
      vbitmap->pixels = NULL;
    }
  }

  vbitmap->locked = 0;
  return YMAGINE_OK;
}

int
VbitmapWrite(Vbitmap *vbitmap, void *buf, int npixels)
{
  return 0;
}

int
VbitmapColormode(Vbitmap *vbitmap)
{
  if (vbitmap == NULL) {
    return VBITMAP_COLOR_RGBA;
  }

  return vbitmap->colormode;
}

int
VbitmapBpp(Vbitmap *vbitmap)
{
  if (vbitmap == NULL) {
    return 0;
  }

  return colorBpp(VbitmapColormode(vbitmap));
}

int
VbitmapWidth(Vbitmap *vbitmap)
{
  if (vbitmap == NULL) {
    return 0;
  }

  return vbitmap->width;
}

int
VbitmapHeight(Vbitmap *vbitmap)
{
  if (vbitmap == NULL) {
    return 0;
  }

  return vbitmap->height;
}

int
VbitmapPitch(Vbitmap *vbitmap)
{
  if (vbitmap == NULL) {
    return 0;
  }

  return vbitmap->pitch;
}

unsigned char*
VbitmapBuffer(Vbitmap *vbitmap)
{
  if (vbitmap == NULL) {
    return NULL;
  }

  if (!vbitmap->locked) {
    return NULL;
  }

  if (vbitmap->bitmaptype == VBITMAP_ANDROID) {
    return vbitmap->pixels;
  }
  if (vbitmap->bitmaptype == VBITMAP_MEMORY) {
    return vbitmap->pixels;
  }
  if (vbitmap->bitmaptype == VBITMAP_STATIC) {
    return vbitmap->pixels;
  }

  return NULL;
}

int
VbitmapType(Vbitmap *vbitmap)
{
  if (vbitmap == NULL) {
    return VBITMAP_NONE;
  }

  return vbitmap->bitmaptype;
}

int
colorBpp(int colormode)
{
  switch(colormode) {
  case VBITMAP_COLOR_RGBA:
    return 4;
  case VBITMAP_COLOR_RGB:
    return 3;
  case VBITMAP_COLOR_GRAYSCALE:
    return 1;
  case VBITMAP_COLOR_YUV:
    return 3;
  default:
    return 0;
  }
}

static int
VbitmapComputeIntersection(Vbitmap *vbitmap, const Vrect *region, Vrect *output)
{
    Vrect vrect;
    if (output == NULL) return YMAGINE_OK;
    if (vbitmap == NULL) return YMAGINE_ERROR;
    if (region == NULL) {
        output->x = 0;
        output->y = 0;
        output->width = VbitmapWidth(vbitmap);
        output->height = VbitmapHeight(vbitmap);
    }
    // Nothing is NULL
    vrect.x = 0;
    vrect.y = 0;
    vrect.width = VbitmapWidth(vbitmap);
    vrect.height = VbitmapHeight(vbitmap);
    VrectComputeIntersection(&vrect, region, output);

    return YMAGINE_OK;
}

int
VbitmapRegionSelect(Vbitmap *vbitmap, int xmin, int ymin, int width, int height)
{
    if (vbitmap == NULL) return YMAGINE_ERROR;
    if (vbitmap->region == NULL) {
        vbitmap->region = Ymem_malloc(sizeof(Vrect));
    }
    // Checking again for NULL in case Ymem_malloc failed
    if (vbitmap->region != NULL) {
        vbitmap->region->x = xmin;
        vbitmap->region->y = ymin;
        vbitmap->region->width = (width < 0) ? 0 : width;
        vbitmap->region->height = (height < 0) ? 0 : height;
        return YMAGINE_OK;
    }
    return YMAGINE_ERROR;
}

int
VbitmapRegionReset(Vbitmap *vbitmap)
{
    if (vbitmap != NULL) {
        if (vbitmap->region != NULL) {
            Ymem_free(vbitmap->region);
            vbitmap->region = NULL;
        }
        return YMAGINE_OK;
    }
    return YMAGINE_ERROR;
}

int
VbitmapRegionWidth(Vbitmap *vbitmap)
{
    Vrect intersection;
    if (vbitmap != NULL) {
        if (VbitmapComputeIntersection(vbitmap, vbitmap->region, &intersection) == YMAGINE_OK) {
            return intersection.width;
        }
    }
    return 0;
}

int
VbitmapRegionHeight(Vbitmap *vbitmap)
{
    Vrect intersection;
    if (vbitmap != NULL) {
        if (VbitmapComputeIntersection(vbitmap, vbitmap->region, &intersection) == YMAGINE_OK) {
            return intersection.height;
        }
    }
    return 0;
}

unsigned char*
VbitmapRegionBuffer(Vbitmap *vbitmap)
{
    int pitch, bpp;
    unsigned char *buffer;
    Vrect intersection;

    buffer = VbitmapBuffer(vbitmap);
    if (buffer != NULL) {
        pitch = VbitmapPitch(vbitmap);
        bpp = VbitmapBpp(vbitmap);
        VbitmapComputeIntersection(vbitmap, vbitmap->region, &intersection);
        buffer += (pitch * intersection.y + intersection.x * bpp);
    }

    return buffer;
}

int
VbitmapWriteNV21Buffer(Vbitmap *vbitmap, const unsigned char *nv21buffer, int width, int height, int downscale)
{
  unsigned char* buffer;

  if (vbitmap == NULL) {
    ALOGE("attempting to write into uninitialized vbitmap");
    return YMAGINE_ERROR;
  }

  buffer = VbitmapBuffer(vbitmap);

  return ycolor_nv21torgb(width, height, nv21buffer, buffer, vbitmap->colormode, downscale);
}

VbitmapXmp*
VbitmapSetXMP(Vbitmap *vbitmap, VbitmapXmp *xmp)
{
  VbitmapXmp *bitmapxmp;

  if (vbitmap == NULL) {
    return NULL;
  }

  bitmapxmp = &(vbitmap->xmp);
  if (bitmapxmp != NULL && xmp != NULL) {
    memcpy(bitmapxmp, xmp, sizeof(VbitmapXmp));
  }

  return bitmapxmp;
}

VbitmapXmp*
VbitmapGetXMP(Vbitmap *vbitmap)
{
  VbitmapXmp *bitmapxmp;

  if (vbitmap == NULL) {
    return NULL;
  }

  bitmapxmp = &(vbitmap->xmp);

  return bitmapxmp;
}

YINLINE YOPTIMIZE_SPEED static double
calculateError(int width, int height,
               unsigned char* buffer1, unsigned char* buffer2,
               int bpp1, int bpp2,
               int pitch1, int pitch2,
               int bpp) {
  int x;
  int y;
  int k;
  double error = 0.0;

  if (width <= 0 || height <= 0) {
    return error;
  }

  for (y = 0; y < height; y++) {
    unsigned char* v1y = buffer1 + y * pitch1;
    unsigned char* v2y = buffer2 + y * pitch2;
    for (x = 0; x < width; x++) {
      for (k = 0; k < bpp; k++) {
        int d = ((int) v1y[k]) - ((int) v2y[k]);
        error += d * d;
      }
      v1y += bpp1;
      v2y += bpp2;
    }
  }

  error = error / (width * height * bpp);
  return error;
}

double
VbitmapComputePSNR(Vbitmap *vbitmap, Vbitmap *reference)
{
  int width;
  int height;
  unsigned char* v1buffer;
  unsigned char* v2buffer;
  int bpp1;
  int bpp2;
  int v1pitch;
  int v2pitch;
  double error = 0.0;
  double psnr = -1.0;

  if (vbitmap == NULL || reference == NULL ||
      VbitmapWidth(vbitmap) != VbitmapWidth(reference) ||
      VbitmapHeight(vbitmap) != VbitmapHeight(reference)) {
    return psnr;
  }

  if (vbitmap == reference) {
    return VBITMAP_MAX_PSNR;
  }

  bpp1 = VbitmapBpp(vbitmap);
  bpp2 = VbitmapBpp(reference);

  if (VbitmapLock(vbitmap) == YMAGINE_OK) {
    if (VbitmapLock(reference) == YMAGINE_OK) {
      v1buffer = VbitmapBuffer(vbitmap);
      v2buffer = VbitmapBuffer(reference);

      if (v1buffer != NULL && v2buffer != NULL) {
        width = VbitmapWidth(vbitmap);
        height = VbitmapHeight(vbitmap);
        v1pitch = VbitmapPitch(vbitmap);
        v2pitch = VbitmapPitch(reference);

        if (bpp1 == 1 && bpp2 == 1) {
          error = calculateError(width, height, v1buffer, v2buffer, 1, 1, v1pitch, v2pitch, 1);
        } else if (bpp1 == 3 && bpp2 == 3) {
          error = calculateError(width, height, v1buffer, v2buffer, 3, 3, v1pitch, v2pitch, 3);
        } else if (bpp1 == 4 && bpp2 == 4) {
          error = calculateError(width, height, v1buffer, v2buffer, 4, 4, v1pitch, v2pitch, 4);
        } else {
          int bpp = bpp1 < bpp2 ? bpp1 : bpp2;
          error = calculateError(width, height, v1buffer, v2buffer, bpp1, bpp2, v1pitch, v2pitch, bpp);
        }
      }
      VbitmapUnlock(reference);
    }
    VbitmapUnlock(vbitmap);
  }

  if (error > 1.0e-10) {
    psnr = (10.0 / LOG10) * log((255.0 * 255.0) / error);
    if (psnr > VBITMAP_MAX_PSNR) {
      psnr = VBITMAP_MAX_PSNR;
    }
  } else {
    psnr = VBITMAP_MAX_PSNR;
  }

  return psnr;
}

