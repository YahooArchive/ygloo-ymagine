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

#ifndef _JADAPTOR_H
#define _JADAPTOR_H 1

#include "jinclude.h"
#include "jpeglib.h"
#include "jerror.h"

#include <stdlib.h>
#include <stdio.h>

#include "ymagine_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

int
ymaginejpeg_input(j_decompress_ptr cinfo, Ychannel *channel);

int
ymaginejpeg_output(j_compress_ptr cinfo, Ychannel *channel);

#ifdef __cplusplus
};
#endif

#endif /* _JADAPTOR_H */
