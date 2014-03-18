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

#define  LOG_TAG "ymagine::jni"
#include "ymagine/ymagine.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#if YMAGINE_JNI_BITMAPFACTORY
extern "C" {
extern int register_BitmapFactory(JNIEnv *env, const char *className);
};

static const char* classNameBitmapFactory =
  "com/yahoo/mobile/client/android/ymagine/BitmapFactory";
#endif
#if YMAGINE_JNI_GENERIC
extern "C" {
extern int register_Vbitmap(JNIEnv *env, const char *className);
extern int register_Shader(JNIEnv *env, const char *className);
extern int register_Ymagine(JNIEnv *env, const char *className);
};

static const char* classNameVbitmap = "com/yahoo/ymagine/Vbitmap";
static const char* classNameShader = "com/yahoo/ymagine/Shader";
static const char* classNameYmagine = "com/yahoo/ymagine/Ymagine";
#endif

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* _env = NULL;
    jint result = -1;
    jclass localClass;

    ALOGD("Register start");

    if (vm->GetEnv((void**) &_env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("ERROR: GetEnv failed");
        goto bail;
    }

#if YMAGINE_JNI_BITMAPFACTORY
    /* Bitmap factory */
    localClass = _env->FindClass(classNameBitmapFactory);
    if (localClass != NULL) {
      if (register_BitmapFactory(_env, classNameBitmapFactory) < 0) {
        ALOGE("BitmapFactory native registration failed");
        goto bail;
      }
    }
#endif
#if YMAGINE_JNI_GENERIC
    /* Ymagine */
    localClass = _env->FindClass(classNameVbitmap);
    if (localClass != NULL) {
      if (register_Vbitmap(_env, classNameVbitmap) < 0) {
        ALOGE("Vbitmap native registration failed");
        goto bail;
      }
    }
    localClass = _env->FindClass(classNameShader);
    if (localClass != NULL) {
      if (register_Shader(_env, classNameShader) < 0) {
        ALOGE("Shader native registration failed");
        goto bail;
      }
    }
    localClass = _env->FindClass(classNameYmagine);
    if (localClass != NULL) {
      if (register_Ymagine(_env, classNameYmagine) < 0) {
        ALOGE("Ymagine native registration failed");
        goto bail;
      }
    }
#endif /* YMAGINE_JNI_GENERIC */

    /* Success, return JNI version */
    ALOGD("Register completed");
    result = JNI_VERSION_1_4;

bail:
    return result;
}
