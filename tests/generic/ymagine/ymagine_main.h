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

#ifndef _YMAGINE_APP_MAIN_H
#define _YMAGINE_APP_MAIN_H 1

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Ymagine image processing test
 *
 */
#include "yosal/yosal.h"
#include "ymagine/ymagine.h"
#if HAVE_PLUGIN_VISION
#include "ymagine/plugins/vision.h"
#endif
#if HAVE_PLUGIN_VIDEO
#include "ymagine/plugins/video.h"
#endif

#include "ymagine_priv.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define YMAGINE_PROFILE 1

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define HAVE_TIMER
#ifdef HAVE_TIMER
/* #define NSTIME() Ytime(SYSTEM_TIME_THREAD) */
#define NSTYPE nsecs_t
#define NSTIME() ((NSTYPE) Ytime(YTIME_CLOCK_REALTIME))
#else
#define NSTYPE uint64_t
#define NSTIME() ((NSTYPE) 0)
#endif

/* Compute minimum of a and b */
#ifdef MIN
#  undef MIN
#endif
#define MIN(a,b) ((a) < (b) ? (a) : (b))

/* Compute maximum of a and b */
#ifdef MAX
#  undef MAX
#endif
#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* Compute infinity norm (i.e. uniform) of (a - b) */
#ifdef LINF
#  undef LINF
#endif
#define LINF(a, b) ((a) > (b) ? (a) - (b) : (b) - (a))

/* Utils */
char*
LoadDataFromFile(const char *filename, size_t *length);

int
parseopts_compose(const char* argv[], int argc, int i, int *composeref);
int
parseopts_shader(const char* argv[], int argc, int i, PixelShader *shader, int compose);

/* Modes */
int
usage_info();
int
main_info(int argc, const char* argv[]);

int
usage_decode();
int
main_decode(int argc, const char* argv[]);

int
usage_transcode();
int
main_transcode(int argc, const char* argv[]);

int
usage_video();
int
main_video(int argc, const char* argv[]);

int
usage_psnr();
int
main_psnr(int argc, const char* argv[]);

int
usage_blur();
int
main_blur(int argc, const char* argv[]);

int
main_convolution_profile(int argc, const char* argv[]);

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_APP_MAIN_H */
