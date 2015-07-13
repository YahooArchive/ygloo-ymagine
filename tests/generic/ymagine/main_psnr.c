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
usage_psnr()
{
  fprintf(stdout, "usage: ymagine psnr file1 file2\n");
  fflush(stdout);

  return 0;
}

int
main_psnr(int argc, const char* argv[])
{
  const char* p1;
  const char* p2;
  Vbitmap* vbitmap1;
  Vbitmap* vbitmap2;
  Ychannel* channel1;
  Ychannel* channel2;
  FILE* f1;
  FILE* f2;
  double psnr;
  int rc;
  int ret = 1;

  if (argc < 2) {
    usage_psnr();
    return ret;
  }

  p1 = argv[0];
  p2 = argv[1];

  f1 = fopen(p1, "r");
  if (f1 != NULL) {
    channel1 = YchannelInitFile(f1, 0);
    if (channel1 != NULL) {
      vbitmap1 = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
      rc = YmagineDecode(vbitmap1, channel1, NULL);
      if (rc == YMAGINE_OK){
        f2 = fopen(p2, "r");
        if (f2 != NULL) {
          channel2 = YchannelInitFile(f2, 0);
          if (channel2 != NULL) {
            vbitmap2 = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
            rc = YmagineDecode(vbitmap2, channel2, NULL);
            if (rc == YMAGINE_OK) {
              psnr = VbitmapComputePSNR(vbitmap1, vbitmap2);
              fprintf(stdout, "%f\n", psnr);
              fflush(stdout);
              ret = 0;
            } else {
              fprintf(stdout, "decode file %s failed\n", p2);
            }
            VbitmapRelease(vbitmap2);
            YchannelRelease(channel2);
          }
          fclose(f2);
        } else {
          fprintf(stdout, "open file %s failed\n", p2);
        }
      } else {
        fprintf(stdout, "decode file %s failed\n", p1);
      }

      VbitmapRelease(vbitmap1);
      YchannelRelease(channel1);
    }
    fclose(f1);
  } else {
    fprintf(stdout, "open file %s failed\n", p1);
  }

  return ret;
}
