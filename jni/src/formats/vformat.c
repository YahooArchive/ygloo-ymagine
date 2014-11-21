/**
 * Copyright 2013-2014 Yahoo! Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License. You may
 * obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License. See accompanying LICENSE file.
 */

#define LOG_TAG "ymagine::vbitmap"

#include "ymagine/ymagine.h"
#include "ymagine_priv.h"

#include <pthread.h>
#include <errno.h>
#include <string.h>

YOSAL_OBJECT_DECLARE(Vformat)
YOSAL_OBJECT_BEGIN
int format;
YOSAL_OBJECT_END

static void
vformat_release_callback(void *ptr)
{
  Vformat *vformat;

  if (ptr == NULL) {
    return;
  }

  vformat = (Vformat*) ptr;

  Ymem_free(vformat);
}

Vformat*
VformatCreate()
{
  Vformat *vformat = NULL;

  vformat = (Vformat*) yobject_create(sizeof(Vformat),
                                      vformat_release_callback);
  if (vformat == NULL) {
    return NULL;
  }

  vformat->format = YMAGINE_IMAGEFORMAT_UNKNOWN;

  return vformat;
}

/* Destructor */
int
VformatRelease(Vformat *vformat)
{
  if (vformat == NULL) {
    return YMAGINE_OK;
  }

  if (yobject_release((yobject*) vformat) != YOSAL_OK) {
    return YMAGINE_ERROR;
  }

  return YMAGINE_OK;
}

Vformat*
VformatRetain(Vformat *vformat)
{
  return (Vformat*) yobject_retain((yobject*) vformat);
}

/* Methods */
int
VformatPush(Vformat *vformat, const char* buf, int buflen)
{
  return 0;
}

int
VformatStep(Vformat *vformat)
{
  return 0;
}
