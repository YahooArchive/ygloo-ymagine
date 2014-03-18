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

#ifndef _YMAGINE_CONFIG_H
#define _YMAGINE_CONFIG_H 1

/* Default values */
#ifndef HAVE_BITMAPFACTORY
#define HAVE_BITMAPFACTORY 0
#endif
#ifndef HAVE_BITMAPGL
#define HAVE_BITMAPGL 0
#endif

#if defined(HAVE_BITMAPFACTORY) && !HAVE_BITMAPFACTORY
#undef HAVE_BITMAPFACTORY
#endif
#if defined(HAVE_BITMAPGL) && !HAVE_BITMAPGL
#undef HAVE_BITMAPGL
#endif

#if defined(YMAGINE_DEBUG) && !YMAGINE_DEBUG
#undef YMAGINE_DEBUG
#endif

/* Transitive dependencies */
#if defined(HAVE_BITMAPGL) && !defined(HAVE_BITMAPFACTORY)
#define HAVE_BITMAPFACTORY 1
#endif

#if defined(YMAGINE_DEBUG)
#define YMAGINE_DEBUG_BITMAPFACTORY 1
#define YMAGINE_DEBUG_JPEG 1
#define YMAGINE_DEBUG_WEBP 1
#endif

/* Platform settings */
#undef YMAGINE_HAVE_ANDROID
#if defined(ANDROID) || defined(__ANDROID)
#define YMAGINE_HAVE_ANDROID
#endif

#endif /* _YMAGINE_CONFIG_H */
