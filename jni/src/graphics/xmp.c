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

#define LOG_TAG "ymagine::bitmap"
#include "ymagine_priv.h"

#include "graphics/bitmap.h"

#ifdef HAVE_JPEG_XMP
#include <expat.h>
#endif

#ifdef HAVE_JPEG_XMP
/* first when start element is encountered */
static void
start_element(void *data, const char *element, const char **attribute)
{
  int i;
  VbitmapXmp *xmp = (VbitmapXmp*) data;

  if (xmp == NULL) {
    return;
  }

  for (i = 0; attribute[i] != NULL; i += 2) {
    const char *key = attribute[i];
    const char *value = attribute[i+1];

    if (key != NULL && key[0] != '\0') {
      if (strncmp(key, "GPano:", 6) == 0) {
        const char *gpanokey = key + 6;
        if (strcmp(gpanokey, "UsePanoramaViewer") == 0) {
          if (strcasecmp(value, "True") == 0) {
            xmp->UsePano = 1;
          }
        }
        else if (strcmp(gpanokey, "ProjectionType") == 0) {
          if (value != NULL && strcasecmp(value, "equirectangular") == 0) {
            xmp->ProjectionType = 1;
          }
        }
        else if (strcmp(gpanokey, "CroppedAreaImageWidthPixels") == 0) {
          xmp->CroppedWidth = atoi(value);
        }
        else if (strcmp(gpanokey, "CroppedAreaImageHeightPixels") == 0) {
          xmp->CroppedHeight = atoi(value);
        }
        else if (strcmp(gpanokey, "FullPanoWidthPixels") == 0) {
          xmp->FullWidth = atoi(value);
        }
        else if (strcmp(gpanokey, "FullPanoHeightPixels") == 0) {
          xmp->FullHeight = atoi(value);
        }
        else if (strcmp(gpanokey, "CroppedAreaLeftPixels") == 0) {
          xmp->Left = atoi(value);
        }
        else if (strcmp(gpanokey, "CroppedAreaTopPixels") == 0) {
          xmp->Top = atoi(value);
        }
      }
    }
  }
}

static void
end_element(void *data, const char *el)
{
  /* TODO: keep track of current element being processed */
}

static void
handle_data(void *data, const char *content, int length)
{
  /* TODO: dump data for debugging */
}
#endif

int
parseXMP(VbitmapXmp *xmp, const char *xmpbuf, int xmplen)
{
  int rc = YMAGINE_ERROR;

#ifdef HAVE_JPEG_XMP
  XML_Parser parser;

  if (xmp == NULL) {
    return rc;
  }

  /* Optionall default to true when XMP data is available */
  xmp->UsePano = 1;
  /* Required members */
  xmp->ProjectionType = 0;
  xmp->CroppedWidth = -1;
  xmp->CroppedHeight = -1;
  xmp->FullWidth = -1;
  xmp->FullHeight = -1;
  xmp->Left = -1;
  xmp->Top = -1;

  parser = XML_ParserCreate(NULL);
  if (parser == NULL) {
    ALOGE("Failed to create XML parser for XMP");
    return rc;
  }

  XML_SetElementHandler(parser, start_element, end_element);
  XML_SetCharacterDataHandler(parser, handle_data);
  XML_SetUserData(parser, xmp);

  /* parse the xml */
  if (XML_Parse(parser, xmpbuf, xmplen, XML_TRUE) == XML_STATUS_ERROR) {
    ALOGE("XMP parsing error %s", XML_ErrorString(XML_GetErrorCode(parser)));
  } else {
    rc = YMAGINE_OK;
  }
  XML_ParserFree(parser);
    
  if (rc == YMAGINE_OK) {
    /* Verify all required fields were set correctly */
    if (xmp->ProjectionType < 0 ||
        xmp->CroppedWidth < 0 || xmp->CroppedHeight < 0 ||
        xmp->FullWidth < 0 || xmp->FullHeight < 0 ||
        xmp->Left < 0 || xmp->Top < 0) {
      rc = YMAGINE_ERROR;
    } else {
      ALOGE("XMP: pano=%d cropped=%dx%d full=%dx%d offset=%d,%d",
            xmp->UsePano,
            xmp->CroppedWidth, xmp->CroppedHeight,
            xmp->FullWidth, xmp->FullHeight,
            xmp->Left, xmp->Top);
    }
  }
#endif

  return rc;
}

