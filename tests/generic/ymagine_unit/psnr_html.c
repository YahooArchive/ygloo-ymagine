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
#include "psnr_html.h"

static const char* html_head = "\
<!DOCTYPE html>\n\
<html>\n\
<head>\n\
<style>\n\
img {\n\
max-width: 500px;\n\
max-height: auto;\n\
}\n\
td {\n\
border: 1px solid black;\n\
  text-align: center;\n\
}\n\
pure-text {\n\
padding:5px\n\
}\n\
.fail {\n\
  background-color: #FF6600;\n\
}\n\
.warning {\n\
background-color: #FFFF66;\n\
}\n\
.pass {\n\
background-color: #33CCFF;\n\
}\n\
div#result {\n\
position:absolute;\n\
top: 2px;\n\
}\n\
</style>\n\
</head>\n\
<body>\n\
\n\
<table style='width:100%;position:absolute;top:20px;'>\n\
<tr>\n\
<td>source size</td>\n\
<td>command</td>\n\
<td>psnr</td>\n\
<td>output</td>\n\
<td>reference</td>\n\
</tr>\n\
";

static const char* html_tail_format = "\
</table>\n\
<div id='result' class='pure-text %s'>%s</div>\n\
</body>\n\
</html>\n\
";

static const char* row_format = "\
<tr%s>\n\
<td class='pure-text'>%s</td>\n\
<td class='pure-text'>%s <a href='%s'>%s</a> ./test.%s</td>\n\
<td class='pure-text'>%s</td>\n\
<td>%s<br><img src='%s'/></td>\n\
<td>%s<br><img src='%s'/></td>\n\
</tr>\n\
";

static const char* error_format = "\
<tr><td colspan='5' class='fail pure-text' style='text-align:left'>%s<td></tr>\n\
";

void
YmaginePsnrAppendHtmlHead(Ychannel* channel) {
  YchannelWrite(channel, html_head, strlen(html_head));
}

void
YmaginePsnrAppendHtmlTail(Ychannel* channel, int failedcount, int warningcount) {
  char buffer[1000];
  YBOOL success = failedcount <= 0;
  snprintf(buffer, sizeof(buffer), html_tail_format,
           success ? (warningcount > 0 ? "warning" : "pass") : "fail",
           success ? "all test passed with %d warnings" :
           "%d test failed with %d warning, each failed row will be marked oriange");

  if (success) {
    snprintf(buffer, sizeof(buffer), buffer, warningcount);
  } else {
    snprintf(buffer, sizeof(buffer), buffer, failedcount, warningcount);
  }

  YchannelWrite(channel, buffer, strnlen(buffer, sizeof(buffer) - 1));
}

void
YmaginePsnrAppendRow(Ychannel* channel, YBOOL success, YBOOL warning,
                     const char* command,
                     const char* srcpath, const char* srcrelativepath,
                     const char* srcsize, const char* psnr,
                     const char* outsize, const char* outpath,
                     const char* refsize, const char* refpath,
                     const char* format) {
  char buffer[1000];
  snprintf(buffer, sizeof(buffer), row_format, success ? (warning ? " class='warning'" : "") : " class='fail'",
           srcsize, command, srcrelativepath, srcpath, format, psnr,
           outsize, outpath, refsize, refpath);
  YchannelWrite(channel, buffer, strnlen(buffer, sizeof(buffer) - 1));
}

void
YmaginePsnrAppendError(Ychannel* channel, const char* error) {
  char buffer[1000];
  snprintf(buffer, sizeof(buffer), error_format, error);
  YchannelWrite(channel, buffer, strnlen(buffer, sizeof(buffer) - 1));
}
