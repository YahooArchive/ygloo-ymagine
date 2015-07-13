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

#include "ymagine_main.h"

int
usage_info()
{
  fprintf(stdout, "usage: ymagine info ?--? file1 ?...?\n");
  fflush(stdout);

  return 0;
}

int
main_info(int argc, const char* argv[])
{
  const char *filename;
  Ychannel *channel;
  Vbitmap *vbitmap;
  FILE *f;
  int i;
  int rc;

  if (argc <= 1) {
    usage_info();
    return 1;
  }

  for (i = 0; i < argc; i++) {
    if (argv[i][0] != '-') {
      break;
    }

    if (argv[i][1] == '-' && argv[i][2] == 0) {
      i++;
      break;
    }

    if (argv[i][1] == 'v' && strcmp(argv[i], "-verbose") == 0) {
      i++;
    }
    else {
      /* Unknown option */
      fprintf(stdout, "unknown option \"%s\"\n", argv[i]);
      fflush(stdout);
      return 1;
    }
  }

  for (; i < argc; ++i) {    
    filename = argv[i];

    vbitmap = VbitmapInitNone();

    f = fopen(filename, "r");
    if (f != NULL) {
      channel = YchannelInitFile(f, 0);
      if (channel != NULL) {
        rc = YmagineDecode(vbitmap, channel, NULL);
        if (rc != YMAGINE_OK) {
          VbitmapResize(vbitmap, 0, 0);
        }
        YchannelRelease(channel);
      }
      fclose(f);
    }

    fprintf(stdout, "%s\t%d\t%d\n", filename, VbitmapWidth(vbitmap), VbitmapHeight(vbitmap));
    fflush(stdout);

    VbitmapRelease(vbitmap);
  }

  return 0;
}
