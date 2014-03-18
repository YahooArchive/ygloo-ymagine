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

#ifndef _YMAGINE_FILTERS_SHADERDETAIL_FILTERUTILS_INLINE_H
#define _YMAGINE_FILTERS_SHADERDETAIL_FILTERUTILS_INLINE_H 1

#include "ymagine_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

static YINLINE unsigned char
byteClamp(int n)
{
#if 0
  n &= -(n >= 0);
  /* if n > 255, ((255 - n) >> 31) will be -1, then return value will be 255 */
  return n | ((255 - n) >> 31);
#else
  return (n < 0 ? 0 : (n > 255 ? 255 : n));
#endif
}

/**
 * @param a ratio for mixing x and y, YFIXED_ZERO stands for 0.0,
 * YFIXED_ONE stands for 1.0
 */
static YINLINE int
yMixi(int x, int y, int a)
{
  return (x * ( YFIXED_ONE - a ) + y * a) >> YFIXED_SHIFT;
}

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_FILTERS_SHADERDETAIL_FILTERUTILS_INLINE_H */
