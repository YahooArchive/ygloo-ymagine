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
usage_blur()
{
  printf("usage: ymagine blur [-width width] [-height height] [-radius radius] infile.jpg outfile.jpg\n");

  return 0;
}

int
main_blur(int argc, const char* argv[])
{
  int i;
  const char* infile;
  const char* outfile;

  int fd;
  Ychannel* channel;

  int width = -1;
  int height = -1;

  int nbiters = 1;
  int pass;

  Vbitmap *vbitmap = NULL;
  int radius = 0;

  NSTYPE start,end;

  for (i = 0; i < argc; i++) {
    if (argv[i][0] != '-') {
      break;
    }
    if (argv[i][1] == '-' && argv[i][2] == 0) {
      i++;
      break;
    }

    if (argv[i][1] == 'w' && strcmp(argv[i], "-width") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      width = atoi(argv[i]);
    } else if (argv[i][1] == 'h' && strcmp(argv[i], "-height") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      height = atoi(argv[i]);
    } else if (argv[i][1] == 'r' && strcmp(argv[i], "-radius") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      radius = atoi(argv[i]);
    } else {
      fprintf(stdout, "unknown option \"%s\"\n", argv[i]);
      fflush(stdout);
      return 1;
    }
  }

  if (i+1 >= argc) {
    usage_blur();
    return 1;
  }

  infile = argv[i];
  i++;
  outfile = argv[i];
  i++;

  fd = open(infile, O_RDONLY);
  if (fd >= 0) {
    vbitmap = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
    channel = YchannelInitFd(fd, 0);
    YmagineDecodeResize(vbitmap, channel, width, height, YMAGINE_SCALE_LETTERBOX);
    YchannelRelease(channel);
  }

  if (radius <= 0) {
    if (VbitmapWidth(vbitmap) < VbitmapHeight(vbitmap)) {
      radius = VbitmapWidth(vbitmap) / 10;
    } else {
      radius = VbitmapHeight(vbitmap) / 10;
    }
  }

  start = NSTIME();
  for (pass=0; pass<nbiters; pass++) {
    Ymagine_blur(vbitmap, radius);
  }
  end = NSTIME();

#if YMAGINE_PROFILE
  fprintf(stdout, "Blured %d times (%dx%d) image with radius %d in %lld ns -> %.2f ms per conversion\n",
          nbiters,
          VbitmapWidth(vbitmap), VbitmapHeight(vbitmap),
          radius,
          (long long) (end - start),
          ((double) (end - start)) / (nbiters*1000000.0));
  fflush(stdout);
#endif

  if (outfile != NULL) {
    /* Save result */
    int fdout = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fdout >= 0) {
      Ychannel *ochannel = YchannelInitFd(fdout, 1);
      if (ochannel == NULL) {
      } else {
        YmagineEncode(vbitmap, ochannel, NULL);
        YchannelRelease(ochannel);
      }
      close(fdout);
    }
  }

  VbitmapRelease(vbitmap);

  return 0;
}
