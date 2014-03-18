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

#ifndef _YMAGINE_PRIV_H
#define _YMAGINE_PRIV_H 1

/* Public API */
#include "ymagine/ymagine.h"

#include "ymagine_config.h"
#if YOSAL_CONFIG_ANDROID_EMULATION
#include "compat/android/bitmap.h"
#else
#include "android/bitmap.h"
#endif

/* Standard Posix headers required for private APIs */
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifndef WIN32
#  include <unistd.h>
#endif

/* Private API */
#include "formats/format.h"
#include "graphics/xmp.h"
#include "graphics/bitmap.h"
#include "graphics/quantize.h"
#include "filters/blur.h"
#include "filters/sobel.h"
#include "graphics/color.h"
#include "shaders/filterutils.h"

#ifdef __cplusplus
extern "C" {
#endif

jobject
createAndroidBitmap(JNIEnv* _env, int width, int height);

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_PRIV_H */
