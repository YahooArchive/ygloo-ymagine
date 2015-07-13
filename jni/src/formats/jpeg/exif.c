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

#include "ymagine_priv.h"

static int32_t
getInt32(const unsigned char* start, int littleendian)
{
  int32_t result;

  if (littleendian) {
    result = (( (int32_t) start[3] ) << 24) | (( (int32_t) start[2] ) << 16) |
    (( (int32_t) start[1] ) << 8) | ((int32_t) start[0]);
  } else {
    result = (( (int32_t) start[0] ) << 24) | (( (int32_t) start[1] ) << 16) |
    (( (int32_t) start[2] ) << 8) | ((int32_t) start[3]);
  }

  return result;
}

static int16_t
getInt16(const unsigned char* start, int littleendian)
{
  int16_t result;

  if (littleendian) {
    result = (( (int16_t)start[1] ) << 8) | ((int16_t) start[0]);
  } else {
    result = (( (int16_t)start[0] ) << 8) | ((int16_t) start[1]);
  }

  return result;
}

int
parseExifOrientation(const unsigned char *exifbuf, int buflen)
{
  int littleendian;
  int i = 0;
  int16_t tagcount;
  int32_t offset;

  if (buflen < 8) {
    /* not enough len for TIFF header */
    return VBITMAP_ORIENTATION_UNDEFINED;
  }

  if (exifbuf[0] == 0x49 && exifbuf[0] == 0x49) {
    littleendian = 1;
  } else if (exifbuf[0] == 0x4D && exifbuf[0] == 0x4D) {
    littleendian = 0;
  } else {
    return VBITMAP_ORIENTATION_UNDEFINED;
  }

  /* IFD0 offset */
  offset = getInt32(exifbuf + i + 4, littleendian);
  i += offset;

  if (i + 2 > buflen) {
    return VBITMAP_ORIENTATION_UNDEFINED;
  }

  tagcount = getInt16(exifbuf + i, littleendian);
  i = i + 2;

  /* Each tag takes 12 bytes */
  if ((i + tagcount * 12) > buflen) {
    return VBITMAP_ORIENTATION_UNDEFINED;
  }

  /* Check through tags for exif tag */
  while (tagcount--) {
    int16_t tag;
    tag = getInt16(exifbuf + i, littleendian);
    if (tag == 0x0112) {
      int32_t count;
      int16_t type;
      int16_t orientation;

      type = getInt16(exifbuf + i + 2, littleendian);
      count = getInt32(exifbuf + i + 4, littleendian);

      /* Validate orientation field */
      if (type != 3 || count != 1) {
        return VBITMAP_ORIENTATION_UNDEFINED;
      }

      orientation = getInt16(exifbuf + i + 8, littleendian);
      return orientation <= 8 ? orientation : VBITMAP_ORIENTATION_UNDEFINED;
    }

    /* move to next 12-byte tag field. */
    i = i + 12;
  }

  return VBITMAP_ORIENTATION_UNDEFINED;
}
