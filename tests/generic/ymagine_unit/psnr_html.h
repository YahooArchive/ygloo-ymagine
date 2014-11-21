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

#ifndef nativesdk_psnr_html_h
#define nativesdk_psnr_html_h

#include "yosal/yosal.h"

#ifdef __cplusplus
extern "C" {
#endif

void
YmaginePsnrAppendHtmlHead(Ychannel* channel);

void
YmaginePsnrAppendHtmlTail(Ychannel* channel, int failedcount, int warningcount);

void
YmaginePsnrAppendRow(Ychannel* channel, YBOOL success, YBOOL warning,
                     const char* command, const char* srcpath,
                     const char* srcrelativepath,
                     const char* srcsize, const char* psnr,
                     const char* outsize, const char* outpath,
                     const char* refsize, const char* refpath,
                     const char* format);

void
YmaginePsnrAppendError(Ychannel* channel, const char* error);

#ifdef __cplusplus
};
#endif

#endif /* nativesdk_psnr_html_h */
