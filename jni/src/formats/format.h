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

#ifndef _YMAGINE_FORMATS_FORMAT_H
#define _YMAGINE_FORMATS_FORMAT_H 1

#include "ymagine/ymagine.h"
#include "ymagine_priv.h"

#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct YmagineFormatOptionsStruct {
  int maxwidth;
  int maxheight;
  int scalemode;
  int quality;
  PixelShader* pixelshader;
};

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_FORMATS_FORMAT_H */
