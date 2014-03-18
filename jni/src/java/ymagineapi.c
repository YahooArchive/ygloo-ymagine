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

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <setjmp.h>

#define LOG_TAG "ymagine::jni"

#include "ymagine/ymagine.h"

#include "ymagine_config.h"
#include "ymagine_priv.h"

#if HAVE_BITMAPFACTORY

static jlong convertPointerToJLong(void* ptr) {
  return (jlong) ((uintptr_t) ptr);
}

static void* convertJLongToPointer(jlong l) {
  return (void*) ((uintptr_t) l);
}

#if 0
static volatile int gJniState_initialized = -1;
static pthread_mutex_t gJniState_mutex = PTHREAD_MUTEX_INITIALIZER;
static JniState* jniState = NULL;

/* will always return inited jniState, JNI_OnLoad() always init it */
JniState const* getJniState()
{
  return jniState;
}

int
jni_state_init(JNIEnv *_env,  const char *classPathName)
{
    if (gJniState_initialized < 0) {
        pthread_mutex_lock(&gJniState_mutex);
        if (gJniState_initialized < 0) {
          if (jniState != NULL) {
            ALOGE("trying to init jniState while it is not NULL");
          }

          jniState = Ymem_calloc(sizeof(JniState), 1);

          if (jniState != NULL) {
            jclass clazzVbitmap = 0;

            if (classPathName != NULL && classPathName[0] != '\0') {
              jniState->flickr_classPath = Ymem_strdup(classPathName);
              /* Resolve classes */
              clazzFlickr = (*_env)->FindClass(_env, classPathName);
            }
            
            jniState->flickrPhotoSet_classPath = CLASSNAME_PHOTOSET;
            clazzPhotoSet = (*_env)->FindClass(_env, jniState->flickrPhotoSet_classPath);
            
            if (clazzVbitmap != 0) {
              jniState->flickrPhoto_clazz = (*_env)->NewGlobalRef(_env, clazzPhoto);
              jniState->flickrPhoto_nativeHandleFieldID =
              (*_env)->GetFieldID(_env, jniState->flickrPhoto_clazz, "mNativeHandle", "J");
              jniState->flickrPhoto_constructorID =
              (*_env)->GetMethodID(_env, jniState->flickrPhoto_clazz, "<init>", "(J)V");
            }

            (*_env)->GetJavaVM(_env, &jniState->vm);

            if ( (jniState->vm == 0) ||
                (jniState->flickr_clazz == 0) ||
                (jniState->flickr_nativeHandleFieldID == 0) ) {
              gJniState_initialized = 0;
            }

            gJniState_initialized = 1;
            }
          }
        }
        pthread_mutex_unlock(&gJniState_mutex);
    }

    return (gJniState_initialized > 0);
}

jobject
bindNewObject(JNIEnv* _env, jclass clazz, jmethodID constructorId, yobject *object)
{
  jobject jobj = NULL;
  yobject_retain(object);
  jobj = (*_env)->NewObject(_env, clazz, constructorId,
                            convertPointerToJLong(object));
  return jobj;
}

    result = bindNewObject(_env, getJniState()->flickrPhoto_clazz,
                           getJniState()->flickrPhoto_constructorID, (yobject*) primary);
#endif

/* Shift for little-endian architecture (ARGB) */
#define ASHIFT 24
#define RSHIFT 16
#define GSHIFT 8
#define BSHIFT 0

#define RGBA(r,g,b,a) \
( (((uint32_t) (r)) << RSHIFT) | \
(((uint32_t) (g)) << GSHIFT) | \
(((uint32_t) (b)) << BSHIFT) | \
(((uint32_t) (a)) << ASHIFT) )

static jint JNICALL
ymagine_jni_version(JNIEnv* _env, jobject object)
{
  return 0 * 10000 + 2 * 100 + 5;
}

static jint JNICALL
ymagine_jni_RGBtoHSV(JNIEnv* _env, jobject object, jint rgb)
{
  return YcolorRGBtoHSV((uint32_t) rgb);
}


static jint JNICALL
ymagine_jni_HSVtoRGB(JNIEnv* _env, jobject object, jint hsv)
{
  return YcolorHSVtoRGB((uint32_t) hsv);
}

static jintArray JNICALL
ymagine_jni_quantize(JNIEnv* _env, jobject object,
                     jobject filename, jint reqColors,
                     jint maxWidth, jint maxHeight)
{
  Ychannel *channel;
  FILE *file = NULL;
  Vcolor colors[16];
  int scores[16];
  jint jresult[2 * 16];
  int ncolors = 0;
  jintArray result = NULL;
  int i;
  YmagineFormatOptions *options;

  if (filename != NULL) {
    char const * filenameStr = (*_env)->GetStringUTFChars(_env, filename, NULL);
    if (filenameStr != NULL) {
      file = fopen(filenameStr, "rb");
      (*_env)->ReleaseStringUTFChars(_env, filename, filenameStr);
      if (file == NULL) {
        ALOGE("failed to open image file \"%s\"", filenameStr);
      }
    }
  }
  
  if (file == NULL) {
    return NULL;
  }

  options = YmagineFormatOptions_Create();
  if (options != NULL) {
    YmagineFormatOptions_setResize(options, maxWidth, maxHeight, YMAGINE_SCALE_FIT);
    YmagineFormatOptions_setQuality(options, 10);
  }

  channel = YchannelInitFile(file, 0);
  if (channel != NULL && options != NULL) {
    Vbitmap *vbitmap;
    int rc = YMAGINE_ERROR;;

    if (!YchannelReadable(channel)) {
      return NULL;
    }
    
    vbitmap = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
    if (vbitmap != NULL) {
      if (matchJPEG(channel)) {
        rc = YmagineDecode(vbitmap, channel, options);
      } else {
        /* Unsupported image format */
        ALOGE("invalid image format");
      }
      if (rc == YMAGINE_OK) {
        ncolors = quantize(vbitmap, reqColors, colors, scores);
      }
      VbitmapRelease(vbitmap);
    }

    YchannelRelease(channel);
  }

  fclose(file);
  if (options != NULL) {
    YmagineFormatOptions_Release(options);
  }

  if (ncolors > 0) {
    result = (*_env)->NewIntArray(_env, ncolors * 2);
    if (result != NULL) {
      for (i = 0; i < ncolors; i++) {
        jresult[2 * i] = RGBA(colors[i].red,
                              colors[i].green,
                              colors[i].blue,
                              colors[i].alpha);
        jresult[2 * i + 1] = scores[i];
      }
      (*_env)->SetIntArrayRegion(_env, result, 0, 2 * ncolors, jresult);
    }
  }

  return result;
}

static volatile int gVbitmap_inited = -1;
static pthread_mutex_t gInit_mutex = PTHREAD_MUTEX_INITIALIZER;

static jclass gVbitmap_clazz = 0;
static jmethodID gVbitmap_retainMethodID = 0;
static jmethodID gVbitmap_releaseMethodID = 0;
static jfieldID gVbitmap_nativeHandleFieldID = 0;

static jclass gShader_clazz = 0;
static jfieldID gShader_nativeHandleFieldID = 0;

static int
vbitmap_init(JNIEnv *_env, const char *classname)
{
  if (gVbitmap_inited < 0) {
    pthread_mutex_lock(&gInit_mutex);
    if (gVbitmap_inited < 0) {
      jclass clazz;

      /* Resolve classes */
      clazz = (*_env)->FindClass(_env, classname);
      if (clazz != 0) {
        gVbitmap_clazz = (*_env)->NewGlobalRef(_env, clazz);
        gVbitmap_retainMethodID = (*_env)->GetMethodID(_env,
                                                       gVbitmap_clazz,
                                                       "retain", "()J");
        gVbitmap_releaseMethodID = (*_env)->GetMethodID(_env,
                                                        gVbitmap_clazz,
                                                        "release", "()J");
        gVbitmap_nativeHandleFieldID = (*_env)->GetFieldID(_env,
                                                          gVbitmap_clazz,
                                                          "mNativeHandle", "J");
      }

      if ( (gVbitmap_clazz == 0) ||
           (gVbitmap_retainMethodID == 0) ||
           (gVbitmap_releaseMethodID == 0) ) {
        gVbitmap_inited = 0;
      } else {
        gVbitmap_inited = 1;
      }
    }
    pthread_mutex_unlock(&gInit_mutex);
  }

  return (gVbitmap_inited > 0);
}

static int
shader_init(JNIEnv *_env, const char *classname)
{
  static volatile int shader_inited = -1;

  if (shader_inited < 0) {
    pthread_mutex_lock(&gInit_mutex);
    if (shader_inited < 0) {
      jclass clazz;

      /* Resolve classes */
      clazz = (*_env)->FindClass(_env, classname);
      if (clazz != 0) {
        gShader_clazz = (*_env)->NewGlobalRef(_env, clazz);
        gShader_nativeHandleFieldID = (*_env)->GetFieldID(_env,
                                                          gShader_clazz,
                                                          "mNativeHandle", "J");
      }

      if ( (gShader_clazz == 0) ||
          (gShader_nativeHandleFieldID == 0) ) {
        shader_inited = 0;
      } else {
        shader_inited = 1;
      }
    }
    pthread_mutex_unlock(&gInit_mutex);
  }

  return (shader_inited > 0);
}

static void JNICALL
native_vbitmapDestructor(JNIEnv* _env, jobject object, jlong lhandle)
{
  Vbitmap *vbitmap;

  vbitmap = (Vbitmap*) convertJLongToPointer(lhandle);
  if (vbitmap != NULL) {
    VbitmapRelease(vbitmap);
  }

  return;
}

static jlong JNICALL
native_vbitmapCreate(JNIEnv* _env, jobject object)
{
  Vbitmap *vbitmap;

  vbitmap = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
  VbitmapRetain(vbitmap);

  return convertPointerToJLong(vbitmap);
}

static jlong JNICALL
native_vbitmapRetain(JNIEnv* _env, jobject object, jlong lhandle)
{
  Vbitmap *vbitmap;

  vbitmap = (Vbitmap*) convertJLongToPointer(lhandle);
  if (vbitmap != NULL) {
    VbitmapRetain(vbitmap);
  }

  return lhandle;
}

static jlong JNICALL
native_vbitmapRelease(JNIEnv* _env, jobject object, jlong lhandle)
{
  Vbitmap *vbitmap;

  vbitmap = (Vbitmap*) convertJLongToPointer(lhandle);
  if (vbitmap != NULL) {
    VbitmapRelease(vbitmap);
  }

  return lhandle;
}

static jint JNICALL
native_vbitmapGetWidth(JNIEnv* _env, jobject object, jlong lhandle)
{
  Vbitmap *vbitmap;

  vbitmap = (Vbitmap*) convertJLongToPointer(lhandle);
  if (vbitmap == NULL) {
    return 0;
  }

  return (jint) VbitmapWidth(vbitmap);
}

static jint JNICALL
native_vbitmapGetHeight(JNIEnv* _env, jobject object, jlong lhandle)
{
  Vbitmap *vbitmap;

  vbitmap = (Vbitmap*) convertJLongToPointer(lhandle);
  if (vbitmap == NULL) {
    return 0;
  }

  return (jint) VbitmapHeight(vbitmap);
}

static jint JNICALL
native_vbitmapDecodeFile(JNIEnv* _env, jobject object, jlong lhandle,
                         jobject filename, jint maxWidth, jint maxHeight, jint quality)
{
  Vbitmap *vbitmap;
  FILE *file = NULL;
  int rc = YMAGINE_ERROR;
  Ychannel *channel;

  vbitmap = (Vbitmap*) convertJLongToPointer(lhandle);
  if (vbitmap == NULL) {
    return rc;
  }

  if (filename != NULL) {
    char const * filenameStr = (*_env)->GetStringUTFChars(_env, filename, NULL);
    if (filenameStr != NULL) {
      file = fopen(filenameStr, "rb");
      if (file == NULL) {
        ALOGE("failed to open image file \"%s\"", filenameStr);
      }
      (*_env)->ReleaseStringUTFChars(_env, filename, filenameStr);
    }
  }
  
  if (file == NULL) {
    return rc;
  }

  channel = YchannelInitFile(file, 0);
  if (channel != NULL) {
    int scaleMode = YMAGINE_SCALE_FIT;

    rc = YmagineDecodeResize(vbitmap, channel, maxWidth, maxHeight, scaleMode);

    YchannelRelease(channel);
  }

  fclose(file);

  return (jint) rc;
}

static jint JNICALL
native_vbitmapDecodeStream(JNIEnv* _env, jobject object, jlong lhandle,
                           jobject streamin, jint maxWidth, jint maxHeight,
                           jint quality)
{
  Vbitmap *vbitmap;
  int rc = YMAGINE_ERROR;
  Ychannel *channel;

  if (streamin == NULL) {
    return rc;
  }

  vbitmap = (Vbitmap*) convertJLongToPointer(lhandle);
  if (vbitmap == NULL) {
    return rc;
  }

  channel = YchannelInitJavaInputStream(_env, streamin);
  if (channel != NULL) {
    int scaleMode = YMAGINE_SCALE_FIT;

    rc = YmagineDecodeResize(vbitmap, channel, maxWidth, maxHeight, scaleMode);

    YchannelRelease(channel);
  }

  return (jint) rc;
}

/* Dalvik VM type signatures */
static JNINativeMethod vbitmap_methods[] = {
    {   "native_destructor",
        "(J)V",
        (void*) native_vbitmapDestructor
    },
    {   "native_create",
        "()J",
        (void*) native_vbitmapCreate
    },
    {   "native_retain",
        "(J)J",
        (void*) native_vbitmapRetain
    },
    {   "native_release",
        "(J)J",
        (void*) native_vbitmapRelease
    },
    {   "native_getWidth",
        "(J)I",
        (void*) native_vbitmapGetWidth
    },
    {   "native_getHeight",
        "(J)I",
        (void*) native_vbitmapGetHeight
    },
    {   "native_decodeFile",
        "(JLjava/lang/String;II)I",
        (void*) native_vbitmapDecodeFile
    },
    {   "native_decodeStream",
        "(JLjava/io/InputStream;II)I",
        (void*) native_vbitmapDecodeStream
    }
};

int register_Vbitmap(JNIEnv *_env, const char *classPathName)
{
  int rc;
  int l;

  if (classPathName == NULL) {
    return JNI_FALSE;
  }

  l = strlen(classPathName);
  if (l > 128) {
    return JNI_FALSE;
  }

  rc = vbitmap_init(_env, classPathName);
  if (rc <= 0) {
    return JNI_FALSE;
  }

  rc = jniutils_registerNativeMethods(_env, classPathName,
                                      vbitmap_methods, NELEM(vbitmap_methods));
  if (rc != JNI_TRUE) {
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

PixelShader* getPixelShader(JNIEnv *_env, jobject jshader) {

  if (jshader == NULL || _env == NULL || gShader_nativeHandleFieldID == 0) {
    return NULL;
  }

  long lhandle = (*_env)->GetLongField(_env,
                                       jshader, gShader_nativeHandleFieldID);

  return (PixelShader*) convertJLongToPointer(lhandle);
}

static Vbitmap* getVbitmap(JNIEnv *_env, jobject jvbitmap) {

  if (jvbitmap == NULL || _env == NULL || gVbitmap_nativeHandleFieldID == 0) {
    return NULL;
  }

  long lhandle = (*_env)->GetLongField(_env,
                                       jvbitmap, gVbitmap_nativeHandleFieldID);

  return (Vbitmap*) convertJLongToPointer(lhandle);
}

static jint JNICALL
ymagine_jni_transcodeStream(JNIEnv* _env, jobject object,
                           jobject streamin, jobject streamout,
                           jint maxWidth, jint maxHeight,
                           jint scaleMode, jint quality, jobject jshader)
{
  Ychannel *channelin;
  Ychannel *channelout;
  PixelShader *shader = NULL;
  jint rc = -1;

  if (streamin == NULL || streamout == NULL) {
    return rc;
  }

  if (jshader != NULL) {
    shader = getPixelShader(_env, jshader);

    if (shader == NULL) {
      return rc;
    }
  }

  channelin = YchannelInitJavaInputStream(_env, streamin);
  if (channelin != NULL) {
    channelout = YchannelInitJavaOutputStream(_env, streamout);
    if (channelout != NULL) {
      if (transcodeJPEG(channelin, channelout, maxWidth, maxHeight,
                        scaleMode, quality, shader) == YMAGINE_OK) {
        rc = 0;
      }
      YchannelRelease(channelout);
    }
    YchannelRelease(channelin);
  }

  return rc;
}

/* Dalvik VM type signatures */
static JNINativeMethod ymagine_methods[] = {
  {   "native_version",
    "()I",
    (void*) ymagine_jni_version
  },
  {   "native_RGBtoHSV",
    "(I)I",
    (void*) ymagine_jni_RGBtoHSV
  },
  {   "native_HSVtoRGB",
    "(I)I",
    (void*) ymagine_jni_HSVtoRGB
  },
  {   "native_quantize",
    "(Ljava/lang/String;III)[I",
    (void*) ymagine_jni_quantize
  },
  {   "native_transcodeStream",
    "(Ljava/io/InputStream;Ljava/io/OutputStream;IIIILcom/yahoo/ymagine/Shader;)I",
    (void*) ymagine_jni_transcodeStream
  }
};

int register_Ymagine(JNIEnv *_env, const char *classPathName)
{
  int rc;
  int l;

  if (classPathName == NULL) {
    return JNI_FALSE;
  }

  l = strlen(classPathName);
  if (l > 128) {
    return JNI_FALSE;
  }

  rc = jniutils_registerNativeMethods(_env, classPathName,
                                      ymagine_methods, NELEM(ymagine_methods));
  if (rc != JNI_TRUE) {
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

/*
 * Shader API
 */
static void JNICALL
native_shaderDestructor(JNIEnv* _env, jobject object, jlong lhandle)
{
  PixelShader *shader;

  shader = (PixelShader*) convertJLongToPointer(lhandle);
  if (shader != NULL) {
    Yshader_PixelShader_release(shader);
  }

  return;
}

static jlong JNICALL
native_shaderCreate(JNIEnv* _env, jobject object)
{
  PixelShader *shader;

  shader = Yshader_PixelShader_create();

  return convertPointerToJLong(shader);
}

static jint JNICALL
native_shaderSaturation(JNIEnv* _env, jobject object, jlong lhandle, jfloat value)
{
  PixelShader *shader;

  shader = (PixelShader*) convertJLongToPointer(lhandle);
  if (shader == NULL) {
    return 0;
  }

  return Yshader_PixelShader_saturation(shader, value);
}

static jint JNICALL
native_shaderExposure(JNIEnv* _env, jobject object, jlong lhandle, jfloat value)
{
  PixelShader *shader;

  shader = (PixelShader*) convertJLongToPointer(lhandle);
  if (shader == NULL) {
    return 0;
  }

  return Yshader_PixelShader_exposure(shader, value);
}

static jint JNICALL
native_shaderContrast(JNIEnv* _env, jobject object, jlong lhandle, jfloat value)
{
  PixelShader *shader;

  shader = (PixelShader*) convertJLongToPointer(lhandle);
  if (shader == NULL) {
    return 0;
  }

  return Yshader_PixelShader_contrast(shader, value);
}

static jint JNICALL
native_shaderBrightness(JNIEnv* _env, jobject object, jlong lhandle, jfloat value)
{
  PixelShader *shader;

  shader = (PixelShader*) convertJLongToPointer(lhandle);
  if (shader == NULL) {
    return 0;
  }

  return Yshader_PixelShader_brightness(shader, value);
}

static jint JNICALL
native_shaderTemperature(JNIEnv* _env, jobject object, jlong lhandle, jfloat value)
{
  PixelShader *shader;

  shader = (PixelShader*) convertJLongToPointer(lhandle);
  if (shader == NULL) {
    return 0;
  }

  return Yshader_PixelShader_temperature(shader, value);
}

static jint JNICALL
native_shaderWhiteBalance(JNIEnv* _env, jobject object, jlong lhandle, jfloat value)
{
  PixelShader *shader;

  shader = (PixelShader*) convertJLongToPointer(lhandle);
  if (shader == NULL) {
    return 0;
  }

  return Yshader_PixelShader_whitebalance(shader, value);
}

static jint JNICALL
native_shaderVignette(JNIEnv* _env, jobject object, jlong lhandle,
                      jobject jvbitmap, int composemode)
{
  PixelShader *shader;
  Vbitmap *vbitmap;
  int rc;

  shader = (PixelShader*) convertJLongToPointer(lhandle);
  if (shader == NULL) {
    return YMAGINE_ERROR;
  }

  vbitmap = getVbitmap(_env, jvbitmap);
  if (vbitmap == NULL) {
    return YMAGINE_ERROR;
  }

  VbitmapRetain(vbitmap);
  rc = Yshader_PixelShader_vignette(shader, vbitmap, composemode);
  VbitmapRelease(vbitmap);

  return rc;
}

static jint JNICALL
native_shaderPreset(JNIEnv* _env, jobject object,
                   jlong lhandle, jobject streamin)
{
  Ychannel *channelin;
  PixelShader *shader;
  jint result = YMAGINE_ERROR;

  if (streamin == NULL) {
    return result;
  }

  shader = (PixelShader*) convertJLongToPointer(lhandle);

  if (shader == NULL) {
    return result;
  }

  channelin = YchannelInitJavaInputStream(_env, streamin);

  if (channelin != NULL) {
    result = Yshader_PixelShader_preset(shader, channelin);
    YchannelRelease(channelin);
  }

  return result;
}

/* Dalvik VM type signatures */
static JNINativeMethod shader_methods[] = {
    {   "native_destructor",
        "(J)V",
        (void*) native_shaderDestructor
    },
    {   "native_create",
        "()J",
        (void*) native_shaderCreate
    },
    {   "native_saturation",
        "(JF)I",
        (void*) native_shaderSaturation
    },
    {   "native_exposure",
        "(JF)I",
        (void*) native_shaderExposure
    },
    {   "native_contrast",
        "(JF)I",
        (void*) native_shaderContrast
    },
    {   "native_brightness",
        "(JF)I",
        (void*) native_shaderBrightness
    },
    {   "native_temperature",
        "(JF)I",
        (void*) native_shaderTemperature
    },
    {   "native_whitebalance",
        "(JF)I",
        (void*) native_shaderWhiteBalance
    },
    {   "native_vignette",
        /* TODO: replace with class name passed as argument */
        "(JLcom/yahoo/ymagine/Vbitmap;I)I",
        (void*) native_shaderVignette
    },
    {   "native_preset",
        "(JLjava/io/InputStream;)I",
        (void*) native_shaderPreset
    }
};

int register_Shader(JNIEnv *_env, const char *classPathName)
{
  int rc;
  int l;

  if (classPathName == NULL) {
    return JNI_FALSE;
  }

  l = strlen(classPathName);
  if (l > 128) {
    return JNI_FALSE;
  }

  rc = shader_init(_env, classPathName);
  if (rc <= 0) {
    return JNI_FALSE;
  }

  rc = jniutils_registerNativeMethods(_env, classPathName,
                                      shader_methods, NELEM(shader_methods));
  if (rc != JNI_TRUE) {
    return JNI_FALSE;
  }

  return JNI_TRUE;
}



#endif /* HAVE_BITMAPFACTORY */
