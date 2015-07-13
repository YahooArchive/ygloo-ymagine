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

#include "ymagine/ymagine_ymaginejni.h"

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
static volatile int gYmagine_inited = -1;
static pthread_mutex_t gInit_mutex = PTHREAD_MUTEX_INITIALIZER;

static jclass gVbitmap_clazz = 0;
static jmethodID gVbitmap_retainMethodID = 0;
static jmethodID gVbitmap_releaseMethodID = 0;
static jfieldID gVbitmap_nativeHandleFieldID = 0;

static jclass gShader_clazz = 0;
static jfieldID gShader_nativeHandleFieldID = 0;

static jclass gOptions_clazz = 0;
static jfieldID gOptions_sharpenFieldID;
static jfieldID gOptions_rotateFieldID;
static jfieldID gOptions_blurFieldID;
static jfieldID gOptions_backgroundColorFieldID;
static jfieldID gOptions_maxWidthFieldID;
static jfieldID gOptions_maxHeightFieldID;
static jfieldID gOptions_shaderFieldID;
static jfieldID gOptions_scaleTypeFieldID;
static jfieldID gOptions_adjustModeFieldID;
static jfieldID gOptions_metaModeFieldID;
static jfieldID gOptions_outputFormatFieldID;
static jfieldID gOptions_qualityFieldID;
static jfieldID gOptions_offsetCropModeFieldID;
static jfieldID gOptions_sizeCropModeFieldID;
static jfieldID gOptions_cropAbsoluteXFieldID;
static jfieldID gOptions_cropAbsoluteYFieldID;
static jfieldID gOptions_cropAbsoluteWidthFieldID;
static jfieldID gOptions_cropAbsoluteHeightFieldID;
static jfieldID gOptions_cropRelativeXFieldID;
static jfieldID gOptions_cropRelativeYFieldID;
static jfieldID gOptions_cropRelativeWidthFieldID;
static jfieldID gOptions_cropRelativeHeightFieldID;

static int
ymagine_init(JNIEnv *_env, const char *classname)
{
  char buf[256];

  if (gYmagine_inited < 0) {
    pthread_mutex_lock(&gInit_mutex);
    if (gYmagine_inited < 0) {
      jclass clazz;

      /* Resolve classes */
      snprintf(buf, sizeof(buf), "%s$Options", classname);
      clazz = (*_env)->FindClass(_env, buf);
      if (clazz != 0) {
        gOptions_clazz = (*_env)->NewGlobalRef(_env, clazz);
        gOptions_sharpenFieldID = (*_env)->GetFieldID(_env,
                                                      gOptions_clazz,
                                                      "sharpen", "F");
        gOptions_rotateFieldID = (*_env)->GetFieldID(_env,
                                                     gOptions_clazz,
                                                     "rotate", "F");
        gOptions_blurFieldID = (*_env)->GetFieldID(_env,
                                                   gOptions_clazz,
                                                   "blur", "F");
        gOptions_backgroundColorFieldID = (*_env)->GetFieldID(_env,
                                                              gOptions_clazz,
                                                              "backgroundColor", "I");
        gOptions_maxWidthFieldID = (*_env)->GetFieldID(_env,
                                                       gOptions_clazz,
                                                       "maxWidth", "I");
        gOptions_maxHeightFieldID = (*_env)->GetFieldID(_env,
                                                        gOptions_clazz,
                                                        "maxHeight", "I");
        gOptions_shaderFieldID = (*_env)->GetFieldID(_env,
                                                     gOptions_clazz,
                                                     "shader", "Lcom/yahoo/ymagine/Shader;");
        gOptions_scaleTypeFieldID = (*_env)->GetFieldID(_env,
                                                        gOptions_clazz,
                                                        "scaleType", "I");
        gOptions_adjustModeFieldID = (*_env)->GetFieldID(_env,
                                                         gOptions_clazz,
                                                         "adjustMode", "I");
        gOptions_metaModeFieldID = (*_env)->GetFieldID(_env,
                                                       gOptions_clazz,
                                                       "metaMode", "I");
        gOptions_outputFormatFieldID = (*_env)->GetFieldID(_env,
                                                           gOptions_clazz,
                                                           "outputFormat", "I");
        gOptions_qualityFieldID = (*_env)->GetFieldID(_env,
                                                           gOptions_clazz,
                                                           "quality", "I");
        gOptions_offsetCropModeFieldID = (*_env)->GetFieldID(_env,
                                                             gOptions_clazz,
                                                             "offsetCropMode", "I");
        gOptions_sizeCropModeFieldID = (*_env)->GetFieldID(_env,
                                                           gOptions_clazz,
                                                           "sizeCropMode", "I");
        gOptions_cropAbsoluteXFieldID = (*_env)->GetFieldID(_env,
                                                            gOptions_clazz,
                                                            "cropAbsoluteX", "I");
        gOptions_cropAbsoluteYFieldID = (*_env)->GetFieldID(_env,
                                                            gOptions_clazz,
                                                            "cropAbsoluteY", "I");
        gOptions_cropAbsoluteWidthFieldID = (*_env)->GetFieldID(_env,
                                                                gOptions_clazz,
                                                                "cropAbsoluteWidth", "I");
        gOptions_cropAbsoluteHeightFieldID = (*_env)->GetFieldID(_env,
                                                                 gOptions_clazz,
                                                                 "cropAbsoluteHeight", "I");
        gOptions_cropRelativeXFieldID = (*_env)->GetFieldID(_env,
                                                            gOptions_clazz,
                                                            "cropRelativeX", "F");
        gOptions_cropRelativeYFieldID = (*_env)->GetFieldID(_env,
                                                            gOptions_clazz,
                                                            "cropRelativeY", "F");
        gOptions_cropRelativeWidthFieldID = (*_env)->GetFieldID(_env,
                                                                gOptions_clazz,
                                                                "cropRelativeWidth", "F");
        gOptions_cropRelativeHeightFieldID = (*_env)->GetFieldID(_env,
                                                                 gOptions_clazz,
                                                                 "cropRelativeHeight", "F");
      }

      if ( (gOptions_clazz == 0) ||
           (gOptions_sharpenFieldID == 0) ||
           (gOptions_rotateFieldID == 0) ||
           (gOptions_blurFieldID == 0) ||
           (gOptions_backgroundColorFieldID == 0) ||
           (gOptions_maxWidthFieldID == 0) ||
           (gOptions_maxHeightFieldID == 0) ||
           (gOptions_shaderFieldID == 0) ||
           (gOptions_scaleTypeFieldID == 0) ||
           (gOptions_adjustModeFieldID == 0) ||
           (gOptions_metaModeFieldID == 0) ||
           (gOptions_outputFormatFieldID == 0) ||
           (gOptions_qualityFieldID == 0) ||
           (gOptions_offsetCropModeFieldID == 0) ||
           (gOptions_sizeCropModeFieldID == 0) ||
           (gOptions_cropAbsoluteXFieldID == 0) ||
           (gOptions_cropAbsoluteYFieldID == 0) ||
           (gOptions_cropAbsoluteWidthFieldID == 0) ||
           (gOptions_cropAbsoluteHeightFieldID == 0) ||
           (gOptions_cropRelativeXFieldID == 0) ||
           (gOptions_cropRelativeYFieldID == 0) ||
           (gOptions_cropRelativeWidthFieldID == 0) ||
           (gOptions_cropRelativeHeightFieldID == 0) ) {
        gYmagine_inited = 0;
      } else {
        gYmagine_inited = 1;
      }
    }
    pthread_mutex_unlock(&gInit_mutex);
  }

  return (gYmagine_inited > 0);
}

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
           (gVbitmap_releaseMethodID == 0) ||
           (gVbitmap_nativeHandleFieldID == 0) ) {
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

static jint JNICALL
native_vbitmapDecodeYUV(JNIEnv* _env, jobject object, jlong lhandle,
                        jint width, jint height, jint stride,
                        jbyteArray dataObject)
{
  Vbitmap *vbitmap;
  int rc = YMAGINE_ERROR;
  jbyte *data;
  jsize datalen;
  int owidth, oheight;
  int scalemode = YMAGINE_SCALE_HALF_QUICK;

  vbitmap = (Vbitmap*) convertJLongToPointer(lhandle);
  if (vbitmap == NULL) {
    return rc;
  }

  if (dataObject == NULL) {
    return rc;
  }

  datalen = (*_env)->GetArrayLength(_env, dataObject);
  if (datalen <= 0 || stride < width || datalen < height * stride) {
    /* Invalid byte array */
    return rc;
  }

  data = (jbyte *) (*_env)->GetByteArrayElements(_env, dataObject, NULL);
  if (data == NULL) {
    return rc;
  }

  if (scalemode == YMAGINE_SCALE_HALF_QUICK) {
    owidth = width / 2;
    oheight = height / 2;
  } else {
    owidth = width;
    oheight = height;
  }

  /* Resize bitmap to match output size */
  if (VbitmapResize(vbitmap, owidth, oheight) == YMAGINE_OK) {
    VbitmapLock(vbitmap);
    VbitmapWriteNV21Buffer(vbitmap, (const unsigned char*)data, width, height, scalemode);
    VbitmapUnlock(vbitmap);

    rc = YMAGINE_OK;
  }
  (*_env)->ReleaseByteArrayElements(_env, dataObject, data, 0 );

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
    },
    {   "native_decodeYUV",
        "(JIII[B)I",
        (void*) native_vbitmapDecodeYUV
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

/**
 * converts Ymagine$Options to YmagineFormatOptions*
 *
 * @param _env jni environment
 * @param joptions java object Ymagine$Options, NULL is a valid value
 * @return NULL if convertion failed
 */
static YmagineFormatOptions* convertOptions(JNIEnv* _env, jobject joptions) {
  YmagineFormatOptions* options;
  PixelShader *shader = NULL;
  jobject jshader;
  jint offsetCropMode;
  jint sizeCropMode;

  if (joptions == NULL) {
    return YmagineFormatOptions_Create();
  }

  jshader = (*_env)->GetObjectField(_env, joptions, gOptions_shaderFieldID);
  if (jshader != NULL) {
    shader = getPixelShader(_env, jshader);
    if (shader == NULL) {
      return NULL;
    }
  }

  options = YmagineFormatOptions_Create();
  if (options == NULL) {
    return NULL;
  }

  if (shader != NULL) {
    YmagineFormatOptions_setShader(options, shader);
  }

  YmagineFormatOptions_setSharpen(options,
                                  (*_env)->GetFloatField(_env, joptions, gOptions_sharpenFieldID));
  YmagineFormatOptions_setRotate(options,
                                 (*_env)->GetFloatField(_env, joptions, gOptions_rotateFieldID));
  YmagineFormatOptions_setAdjust(options,
                                 (*_env)->GetIntField(_env, joptions, gOptions_adjustModeFieldID));
  YmagineFormatOptions_setMetaMode(options,
                               (*_env)->GetIntField(_env, joptions, gOptions_metaModeFieldID));
  YmagineFormatOptions_setBlur(options,
                                 (*_env)->GetFloatField(_env, joptions, gOptions_blurFieldID));
  YmagineFormatOptions_setBackgroundColor(options,
                                          (*_env)->GetIntField(_env, joptions, gOptions_backgroundColorFieldID));
  YmagineFormatOptions_setResize(options,
                                 (*_env)->GetIntField(_env, joptions, gOptions_maxWidthFieldID),
                                 (*_env)->GetIntField(_env, joptions, gOptions_maxHeightFieldID),
                                 (*_env)->GetIntField(_env, joptions, gOptions_scaleTypeFieldID));
  YmagineFormatOptions_setFormat(options,
                                 (*_env)->GetIntField(_env, joptions, gOptions_outputFormatFieldID));
  YmagineFormatOptions_setQuality(options,
                                  (*_env)->GetIntField(_env, joptions, gOptions_qualityFieldID));

  offsetCropMode = (*_env)->GetIntField(_env, joptions, gOptions_offsetCropModeFieldID);
  sizeCropMode = (*_env)->GetIntField(_env, joptions, gOptions_sizeCropModeFieldID);

  if (offsetCropMode == CROP_MODE_ABSOLUTE) {
    YmagineFormatOptions_setCropOffset(options,
                                       (*_env)->GetIntField(_env, joptions, gOptions_cropAbsoluteXFieldID),
                                       (*_env)->GetIntField(_env, joptions, gOptions_cropAbsoluteYFieldID));
  } else if (offsetCropMode == CROP_MODE_RELATIVE) {
    YmagineFormatOptions_setCropOffsetRelative(options,
                                               (*_env)->GetFloatField(_env, joptions, gOptions_cropRelativeXFieldID),
                                               (*_env)->GetFloatField(_env, joptions, gOptions_cropRelativeYFieldID));
  }
  if (sizeCropMode == CROP_MODE_ABSOLUTE) {
    YmagineFormatOptions_setCropSize(options,
                                     (*_env)->GetIntField(_env, joptions, gOptions_cropAbsoluteWidthFieldID),
                                     (*_env)->GetIntField(_env, joptions, gOptions_cropAbsoluteHeightFieldID));
  } else if (sizeCropMode == CROP_MODE_RELATIVE) {
    YmagineFormatOptions_setCropSizeRelative(options,
                                             (*_env)->GetFloatField(_env, joptions, gOptions_cropRelativeWidthFieldID),
                                             (*_env)->GetFloatField(_env, joptions, gOptions_cropRelativeHeightFieldID));
  }

  return options;
}

static jint JNICALL
ymagine_jni_transcodeStream(JNIEnv* _env, jobject object,
                            jobject streamin, jobject streamout,
                            jobject joptions)
{
  Ychannel *channelin;
  Ychannel *channelout;
  jint rc = -1;
  YmagineFormatOptions *options;

  if (streamin == NULL || streamout == NULL) {
    return rc;
  }

  channelin = YchannelInitJavaInputStream(_env, streamin);
  if (channelin != NULL) {
    channelout = YchannelInitJavaOutputStream(_env, streamout);
    if (channelout != NULL) {
      options = convertOptions(_env, joptions);
      if (options != NULL) {
        if (YmagineTranscode(channelin, channelout, options) == YMAGINE_OK) {
          rc = 0;
        }
        YmagineFormatOptions_Release(options);
      }
      YchannelRelease(channelout);
    }
    YchannelRelease(channelin);
  }

  return rc;
}

static jint JNICALL
ymagine_jni_encodeStream(JNIEnv* _env, jobject object,
                         jobject vbitmapobj, jobject streamout, jobject joptions)
{
  Vbitmap *vbitmap;
  int rc = YMAGINE_ERROR;
  Ychannel *channel;

  if (streamout == NULL) {
    return rc;
  }

  vbitmap = YmagineJNI_VbitmapRetain(_env, vbitmapobj);
  if (vbitmap == NULL) {
    return rc;
  }

  channel = YchannelInitJavaOutputStream(_env, streamout);
  if (channel != NULL) {
    YmagineFormatOptions *options = NULL;

    options = convertOptions(_env, joptions);
    if (options != NULL) {
      rc = YmagineEncode(vbitmap, channel,NULL);
      YmagineFormatOptions_Release(options);
    }

    YchannelRelease(channel);
  }

  YmagineJNI_VbitmapRelease(_env, vbitmapobj);

  return (jint) rc;
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
  }
};

int register_Ymagine(JNIEnv *_env, const char *ymagineClassPathName)
{
  int rc;
  int l;
  char buf[256];
  JNINativeMethod options_methods[1];
  char *vbitmapClassPathName = "com/yahoo/ymagine/Vbitmap";

  if (ymagineClassPathName == NULL) {
    return JNI_FALSE;
  }

  l = strlen(ymagineClassPathName);
  if (l > 128) {
    return JNI_FALSE;
  }

  rc = ymagine_init(_env, ymagineClassPathName);
  if (rc <= 0) {
    return JNI_FALSE;
  }

  rc = jniutils_registerNativeMethods(_env, ymagineClassPathName,
                                      ymagine_methods, NELEM(ymagine_methods));
  if (rc != JNI_TRUE) {
    return JNI_FALSE;
  }

  snprintf(buf, sizeof(buf), "(Ljava/io/InputStream;Ljava/io/OutputStream;L%s$Options;)I", ymagineClassPathName);

  options_methods[0].name = "native_transcodeStream";
  options_methods[0].signature = buf;
  options_methods[0].fnPtr = (void*) ymagine_jni_transcodeStream;

  rc = jniutils_registerNativeMethods(_env, ymagineClassPathName,
                                      options_methods, 1);
  if (rc != JNI_TRUE) {
    return JNI_FALSE;
  }

  snprintf(buf, sizeof(buf), "(L%s;Ljava/io/OutputStream;L%s$Options;)I", vbitmapClassPathName, ymagineClassPathName);

  options_methods[0].name = "native_encodeStream";
  options_methods[0].signature = buf;
  options_methods[0].fnPtr = (void*) ymagine_jni_encodeStream;

  rc = jniutils_registerNativeMethods(_env, ymagineClassPathName,
                                      options_methods, 1);
  if (rc != JNI_TRUE) {
    return JNI_FALSE;
  }


  return JNI_TRUE;
}

/* Public helper to extract native Vbitmap handle from Vbitmap Java object */
Vbitmap*
YmagineJNI_VbitmapRetain(JNIEnv* _env, jobject vbitmapobj)
{
  jlong lhandle = (jlong) 0;
  Vbitmap *vbitmap = NULL;

  if (vbitmapobj != NULL) {
    lhandle = (*_env)->CallLongMethod(_env, vbitmapobj, gVbitmap_retainMethodID);
    if ((*_env)->ExceptionCheck(_env)) {
      /* method threw an exception, abort */
      /* (*env)->ExceptionDescribe(env); */
      (*_env)->ExceptionClear(_env);
      lhandle = (jlong) 0;
    }

    vbitmap = (Vbitmap*) convertJLongToPointer(lhandle);
  }

  return vbitmap;
}

Vbitmap*
YmagineJNI_VbitmapRelease(JNIEnv* _env, jobject vbitmapobj)
{
  jlong lhandle = (jlong) 0;

  if (vbitmapobj != NULL) {
    lhandle = (*_env)->CallLongMethod(_env, vbitmapobj, gVbitmap_releaseMethodID);
    if ((*_env)->ExceptionCheck(_env)) {
      /* method threw an exception, abort */
      /* (*env)->ExceptionDescribe(env); */
      (*_env)->ExceptionClear(_env);
      lhandle = (jlong) 0;
    }
  }

  return (Vbitmap*) convertJLongToPointer(lhandle);
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
