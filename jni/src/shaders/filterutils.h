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

#ifndef _YMAGINE_FILTERS_SHADERDETAIL_FILTERUTILS_H
#define _YMAGINE_FILTERS_SHADERDETAIL_FILTERUTILS_H 1

#define YFIXED_SHIFT 10
#define YFIXED_ZERO 0
#define YFIXED_ONE (1<<YFIXED_SHIFT)
#define Y_MUL(x,y) (((x)*(y))>>YFIXED_SHIFT)
#define Y_DIV(x,y) (((x)*YFIXED_ONE)/(y))

uint8_t*
createEffectMap(const unsigned char *preset,
                int contrast, int brightness,
                int exposure, int temperature);

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
};
#endif

#endif /* _YMAGINE_FILTERS_SHADERDETAIL_FILTERUTILS_H */
