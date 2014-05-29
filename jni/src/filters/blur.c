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

int
Ymagine_blurBuffer(unsigned char *pix,
                   int w, int h, int pitch, int bpp,
                   int radius)
{
  if (radius <= 0) {
    return YMAGINE_OK;
  }

  return Ymagine_blurSuperfast(pix, w, h, pitch, bpp, radius, 2);
}

int
Ymagine_blur(Vbitmap *vbitmap, int radius)
{
  int rc = YMAGINE_ERROR;

  if (VbitmapLock(vbitmap) >= 0) {
    unsigned char *pixels = VbitmapBuffer(vbitmap);
    int width = VbitmapWidth(vbitmap);
    int height = VbitmapHeight(vbitmap);
    int pitch = VbitmapPitch(vbitmap);
    int bpp = colorBpp(VbitmapColormode(vbitmap));

    if (Ymagine_blurBuffer(pixels,
                           width, height, pitch, bpp,
                           (int) radius) == YMAGINE_OK) {
      rc = YMAGINE_OK;
    }

    VbitmapUnlock(vbitmap);
  }

  return rc;
}
