/**
 * Copyright 2013-2015 Yahoo! Inc.
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
usage_design()
{
  fprintf(stdout, "usage: ymagine design ?-size n? ?-out out.png? ?--? infile1 ?infile2 ...?\n");
  fflush(stdout);

  return 0;
}

typedef struct {
  int mode;
} privateOptions;

int
main_design(int argc, const char* argv[])
{
  int fdout = -1;
  int force = 1;
  int profile = 1;
  int fmode;
  int rc = 0;
  int nsuccess;
  NSTYPE start = 0;
  NSTYPE end = 0;
  NSTYPE start_decode = 0;
  NSTYPE end_decode = 0;
  int oformat = YMAGINE_IMAGEFORMAT_PNG;
  int niters = 1;
  int ntiles = 0;
  const char** infiles;
  Vbitmap *canvas = NULL;
  int canvasw;
  int canvash;
  int reqSize = -1;
  const char* outfile = NULL;
  int i;

  if (argc < 1) {
    usage_design();
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

    if (argv[i][1] == 's' && strcmp(argv[i], "-size") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      reqSize = atoi(argv[i]);
    } else if (argv[i][1] == 'o' && strcmp(argv[i], "-out") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      outfile = argv[i];
    } else {
      /* Unknown option */
      fprintf(stdout, "unknown option \"%s\"\n", argv[i]);
      fflush(stdout);
      return 1;
    }
  }

  ntiles = argc - i;
  infiles = argv + i;

  start = NSTIME();

  /* Prepare the ORB canvas */
  canvas = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
  start_decode = NSTIME();
  rc = VbitmapOrbLoad(canvas, reqSize);
  end_decode = NSTIME();

  canvasw = VbitmapWidth(canvas);
  canvash = VbitmapHeight(canvas);

  if (profile) {
    fprintf(stdout, "Created orb mask %dx%d in %.2f ms\n",
            canvasw, canvash,
            ((double) (end_decode - start_decode)) / 1000000.0);
    fflush(stdout);
  }

  nsuccess = 0;
  for (i = 0; i < ntiles; i++) {
    int fdin = -1;
    if (infiles != NULL && infiles[i] != NULL && infiles[i][0] != '\0') {
      fdin = open(infiles[i], O_RDONLY | O_BINARY);
    }
    if (fdin < 0) {
      fprintf(stdout, "failed to open input file \"%s\"\n", infiles[i]);
      fflush(stdout);
    } else {
      Ychannel *channelin = YchannelInitFd(fdin, 0);
      if (VbitmapOrbRenderTile(canvas, ntiles, i, channelin) == YMAGINE_OK) {
        nsuccess++;
      } else {
        fprintf(stdout, "failed to render orb\n");
        fflush(stdout);
      }
      YchannelRelease(channelin);
      close(fdin);
    }
  }

  if (nsuccess == ntiles) {
    if (outfile != NULL) {
      /* Save output */
      fmode = O_WRONLY | O_CREAT | O_BINARY;
      if (force) {
        /* Truncate file if it already exists */
        fmode |= O_TRUNC;
      } else {
        /* Fail if file already exists */
        fmode |= O_EXCL;
      }

      fdout = open(outfile, fmode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
      if (fdout >= 0 ) {
        Ychannel *channelout;
        YmagineFormatOptions *encodeoptions = NULL;

        channelout = YchannelInitFd(fdout, 1);
        if (channelout != NULL) {  
          encodeoptions = YmagineFormatOptions_Create();
          if (encodeoptions != NULL) {
            YmagineFormatOptions_setFormat(encodeoptions, oformat);
            rc = YmagineEncode(canvas, channelout, encodeoptions);
            YmagineFormatOptions_Release(encodeoptions);
          }
        }
        YchannelRelease(channelout);
        close(fdout);
      }
    }

    end = NSTIME();

    if (profile) {
      fprintf(stdout, "Created %dx%d orb from %d inputs in %.2f ms\n",
              canvasw, canvash, ntiles,
              ((double) (end - start)) / (niters * 1000000.0));
      fflush(stdout);
    }
  }

  if (canvas != NULL) {
    VbitmapRelease(canvas);
    canvas = NULL;
  }

  return rc;
}
