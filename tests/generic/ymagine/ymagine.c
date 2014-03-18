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

/*
 * Ymagine image processing test
 *
 */
#include "yosal/yosal.h"
#include "ymagine/ymagine.h"
#if HAVE_PLUGIN_VISION
#include "ymagine/plugins/vision.h"
#endif

#include "ymagine_priv.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#define YMAGINE_PROFILE 1

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define HAVE_TIMER
#ifdef HAVE_TIMER
/* #define NSTIME() Ytime(SYSTEM_TIME_THREAD) */
#define NSTYPE nsecs_t
#define NSTIME() ((NSTYPE) Ytime(YTIME_CLOCK_REALTIME))
#else
#define NSTYPE uint64_t
#define NSTIME() ((NSTYPE) 0)
#endif

static int
decodeImage(char* data, size_t length,
            int ncolors, int detect, PixelShader* shader)
{
  Vbitmap *vbitmap;
  Ychannel *channel;
  int rc = -1;
  YmagineFormatOptions *options = NULL;

  unsigned int maxWidth = 1024;
  unsigned int maxHeight = 1024;
  int scaleMode = YMAGINE_SCALE_LETTERBOX;

  if (detect) {
    maxWidth = -1;
    maxHeight = -1;
  }

  if (ncolors > 0) {
    maxWidth = 64;
    maxHeight = 64;
    scaleMode = YMAGINE_SCALE_FIT;
  }

  options = YmagineFormatOptions_Create();
  if (options != NULL) {
    YmagineFormatOptions_setResize(options, maxWidth, maxHeight, scaleMode);
    YmagineFormatOptions_setShader(options, shader);
  }

  channel = YchannelInitByteArray(data, length);
  if (channel != NULL && options != NULL) {
    if (detect) {
#if HAVE_PLUGIN_VISION
      int i;

      /* Face detection requires only luminance, so decode as grayscale
	     to improve performance and lower memory footprint. Detector
	     supports image in either RGB, RGBA or GRAYSCALE mode.
       */
      vbitmap = VbitmapInitMemory(VBITMAP_COLOR_GRAYSCALE);

      if (vbitmap != NULL) {
        rc = YmagineDecode(vbitmap, channel, options);
        if (rc == 0) {
          int maxmatches = 64;
          int coords[4*maxmatches];
          int scores[maxmatches];
          int nfound;

          nfound = detect_run(vbitmap, 100, maxmatches, coords, scores);
          for (i = 0; i < nfound; i++) {
            fprintf(stdout,
                    " #%d: @(%d,%d)->(%dx%d) score=%d\n",
                    i+1,
                    coords[4*i+0],
                    coords[4*i+1],
                    coords[4*i+2],
                    coords[4*i+3],
                    scores[i]);
            fflush(stdout);
          }
        }
        VbitmapRelease(vbitmap);
      }
#else
      printf("detector not supported\n");
#endif
    } else {
      vbitmap = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
      rc = YmagineDecode(vbitmap, channel, options);

      printf("Decoded: %dx%d\n", VbitmapWidth(vbitmap), VbitmapHeight(vbitmap));
#if 0
      if (ncolors > 0) {
        int i=0;
        Vcolor colors[16];
        int scores[16];
        int ocolors;

        if (ncolors > 16) {
          ncolors = 16;
        }

        ocolors = quantize(vbitmap, ncolors, colors, scores);

        printf("Found %d colors\n", ocolors);
        if (ocolors > 0) {
          for (i = 0; i < ocolors; i++) {
            fprintf(stdout, "Color %d: #%02x%02x%02x, score: %d\n",
                    i, colors[i].red, colors[i].green, colors[i].blue, scores[i]);
          }
        }
      }
#endif
#if 0
      blur(vbitmap, 40);
#endif

      VbitmapRelease(vbitmap);
    }

    YchannelResetBuffer(channel);
    YchannelRelease(channel);
  }

  if (options != NULL) {
    YmagineFormatOptions_Release(options);
  }
    
  return rc;
}

static char*
LoadFile(const char *filename, size_t *length)
{
  int fd;
  struct stat statbuf;
  size_t flen = 0;
  char *fbase = NULL;
  char *p;
  size_t rem;
  ssize_t readlen;

  fd = open(filename, O_RDONLY);
  if (fd == -1) {
    return NULL;
  }

  /* Load image content in memory, to get rid of any risk of
   I/O and cache effect during benchmark */
  fstat(fd, &statbuf);

  flen = statbuf.st_size;
  if (flen == 0) {
    close(fd);
    return NULL;
  }

  fbase = (char*) Ymem_malloc(flen);
  if (fbase == NULL) {
    close(fd);
    return NULL;
  }

  p = fbase;
  rem = flen;
  while (rem > 0) {
    readlen = read(fd, p, rem);
    if (readlen < 0) {
      free(fbase);
      close(fd);
      return NULL;
    }

    p += readlen;
    rem -= readlen;
  }
  close(fd);

  if (length != NULL) {
    *length = flen;
  }

  return fbase;
}

static int
usage_decode()
{
  fprintf(stdout, "usage: ymagine decode ?-shaderName <shaderName (seperated "
          "by ';' without whitespace e.g., "
          "color-iced_tea;vignette-white_pinhole)>? "
          "?-ntimes <ntimes>? ?-cascade <path>? ?--? file1 ?...?\n");
  fflush(stdout);

  return 0;
}

static int
main_decode(int argc, const char* argv[])
{
  const char *filename;
  char *fbase;
  size_t flen;

  NSTYPE start, end;
  int nbiters = 1;
  int warmup = 0;
  int detect = 0;
  int pass;
  int i;
  int ncolors = 0;
  const char *classifier_path = NULL;

  if (argc <= 1) {
    usage_decode();
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

    if (argv[i][1] == 'c' && strcmp(argv[i], "-cascade") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
      }
      i++;
      classifier_path = argv[i];
    } else if (argv[i][1] == 'c' && strcmp(argv[i], "-colors") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
      }
      i++;
      ncolors = atoi(argv[i]);
    } else if (argv[i][1] == 'n' && strcmp(argv[i], "-ntimes") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
      }
      i++;
      nbiters = atoi(argv[i]);
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

    fprintf(stdout, "test file: %s\n", filename);
    fflush(stdout);

    /* Load image file into memory, to not benchmark I/O */
    fbase = LoadFile(filename, &flen);
    if (fbase == NULL) {
      fprintf(stdout, "unable to open file \"%s\"\n", filename);
      fflush(stdout);
      return 1;
    }

    if (classifier_path != NULL) {
#if HAVE_PLUGIN_VISION
      fprintf(stdout, "Loading cascade from %s\n", classifier_path);
      fflush(stdout);
      if (detect_load_model(classifier_path) == YMAGINE_OK) {
        fprintf(stdout, "Cascade loaded from %s\n", classifier_path);
        detect = 1;
      } else {
        detect = 0;
      }
#else
      fprintf(stdout, "detector plugin not supported, ignoring classifier\n");
#endif
    }

    if (warmup) {
      /* Make one run in each mode, to not benchmark one-time initialization */
      decodeImage(fbase, flen, 0, detect, NULL);
    }

    start = NSTIME();
    for (pass = 0; pass < nbiters; ++pass) {
      decodeImage(fbase, flen, ncolors, detect, NULL);
    }
    end = NSTIME();

#if YMAGINE_PROFILE
    fprintf(stdout, "Decoded %d times in %ld ns -> %.2f ms per decoding\n",
            nbiters, (long) (end - start),
            ((double) (end - start)) / (nbiters*1000000.0));
    fflush(stdout);
#endif

#if HAVE_PLUGIN_VISION
    if (detect) {
      detect_load_model(NULL);
    }
#endif

    Ymem_free(fbase);
  }

  return 0;
}

static int
usage_transcode()
{
  fprintf(stdout, "usage: ymagine transcode ?-width <integer>? ?-height <integer>? infile outfile\n");
  fflush(stdout);

  return 0;
}

static int
main_transcode(int argc, const char* argv[])
{
  Ychannel *channelin;
  Ychannel *channelout;
  Ychannel *channelpreset;
  int fdin = -1;
  int fdout = -1;
  int fdpreset = -1;
  FILE *fileout = NULL;
  int closeout = 1;
  int closein = 1;
  int force = 0;
  int profile = 0;
  int fmode;
  int rc = 0;
  const char *infile = NULL;
  const char *outfile = NULL;
  const char *presetFile = NULL;
  int maxWidth = 512;
  int maxHeight = 512;
  /* scaleMode can be YMAGINE_SCALE_CROP or YMAGINE_SCALE_LETTERBOX */
  int scaleMode = YMAGINE_SCALE_CROP;
  int quality = -1;
  PixelShader *shader = NULL;

  NSTYPE start = 0;
  NSTYPE end = 0;
  NSTYPE start_transcode = 0;
  NSTYPE end_transcode = 0;
  NSTYPE total_transcode;
  int i;
  char *writebuf = NULL;
  int writebuflen = 0;
  int niters = 1;
  int iter;

  if (argc < 1) {
    usage_transcode();
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

    if (argv[i][1] == 'w' && strcmp(argv[i], "-width") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
      }
      i++;
      maxWidth = atoi(argv[i]);
    } else if (argv[i][1] == 'h' && strcmp(argv[i], "-height") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
      }
      i++;
      maxHeight = atoi(argv[i]);
    } else if (argv[i][1] == 'r' && strcmp(argv[i], "-repeat") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
      }
      i++;
      niters = atoi(argv[i]);
    } else if (argv[i][1] == 'q' && strcmp(argv[i], "-quality") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
      }
      i++;
      quality = atoi(argv[i]);
      if (quality < 0) {
        quality = -1;
      } else if (quality > 100) {
        quality = 100;
      }
    } else if (argv[i][1] == 'f' && strcmp(argv[i], "-force") == 0) {
      force = 1;
    } else if (argv[i][1] == 'f' && strcmp(argv[i], "-fit") == 0) {
      /* Force rescale by power fo two */
      scaleMode = YMAGINE_SCALE_FIT;
    } else if (argv[i][1] == 'p' && strcmp(argv[i], "-profile") == 0) {
      profile = 1;
    } else if (argv[i][1] == 'p' && strcmp(argv[i], "-preset") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
      }
      i++;
      presetFile = argv[i];
    }
    else {
      /* Unknown option */
      fprintf(stdout, "unknown option \"%s\"\n", argv[i]);
      fflush(stdout);

      return 1;
    }
  }

  if (i >= argc) {
    usage_transcode();
    return 1;
  }

  infile = argv[i];
  i++;

  if (i >= argc) {
    outfile = NULL;
  } else {
    outfile = argv[i];
    i++;
  }

  if (niters < 1) {
    niters = 1;
  }

  total_transcode = 0;
  start = NSTIME();
  for (iter = 0; iter < niters; iter++) {
  fdin = open(infile, O_RDONLY | O_BINARY);
  if (fdin < 0) {
    fprintf(stdout, "failed to open input file \"%s\"\n", infile);
    fflush(stdout);
    return 1;
  }

  fmode = O_WRONLY | O_CREAT | O_BINARY;
  if (force) {
    /* Truncate file if it already exisst */
    fmode |= O_TRUNC;
  } else {
    /* Fail if file already exists */
    fmode |= O_EXCL;
  }

    if (presetFile != NULL) {
      if (shader == NULL) {
        shader = Yshader_PixelShader_create();
      }

      if (shader != NULL) {
        fdpreset = open(presetFile, O_RDONLY | O_BINARY);
        if (fdpreset == -1) {
          fprintf(stdout, "failed to open input file \"%s\"\n", presetFile);
          fflush(stdout);
          return 1;
        }

        channelpreset = YchannelInitFd(fdpreset, 0);

        if (channelpreset != NULL) {
          Yshader_PixelShader_preset(shader, channelpreset);
          YchannelRelease(channelpreset);
        }

        close(fdpreset);
      }
    }

  if (outfile == NULL) {
#if 0
    fdout = STDOUT_FILENO;
#else
    fileout = stdout;
#endif
    closeout = 0;
  } else {
    // fileout = fopen(outfile, "wb");
    if (fileout == NULL) {
      fdout = open(outfile, fmode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    }
    if (fdout < 0 && fileout == NULL) {
      if (closein) {
        close(fdin);
      }

      fprintf(stdout, "failed to open output file \"%s\"\n", outfile);
      fflush(stdout);
      return 1;
    }
    closeout = 1;

    if (fileout != NULL) {
      if (writebuflen > 0) {
        writebuf = Ymem_malloc(writebuflen);
        if (writebuf != NULL) {
          if (setvbuf(fileout, writebuf, _IOFBF, writebuflen) != 0) {
            // Error setting write buffer. Ignore and fallback to default buffering
            Ymem_free(writebuf);
            writebuf = NULL;
          }
        }
      }
    }
  }

  channelin = YchannelInitFd(fdin, 0);
  if (channelin == NULL) {
    fprintf(stdout, "failed to create input stream\n");
    fflush(stdout);
  } else {
    if (fileout != NULL) {
      channelout = YchannelInitFile(fileout, 1);
    } else {
      channelout = YchannelInitFd(fdout, 1);
    }
    if (channelout == NULL) {
      fprintf(stdout, "failed to create output stream\n");
      fflush(stdout);
    } else {
      start_transcode=NSTIME();
      rc = transcodeJPEG(channelin, channelout, maxWidth, maxHeight, scaleMode,
                         quality, shader);
      end_transcode=NSTIME();
      total_transcode += (end_transcode - start_transcode);
      YchannelRelease(channelout);
    }
    YchannelRelease(channelin);
  }

  if (closeout) {
    if (fdout >= 0) {
      close(fdout);
    }
    if (fileout != NULL) {
      fclose(fileout);
    }
  } else {
    if (fdout >= 0) {
      fsync(fdout);
    }
    if (fileout != NULL) {
      fflush(fileout);
    }
  }
  if (closein) {
    close(fdin);
  }
  if (writebuf != NULL) {
    Ymem_free(writebuf);
    writebuf = NULL;
  }
  }

  if (shader != NULL) {
    Yshader_PixelShader_release(shader);
    shader = NULL;
  }

  end = NSTIME();

#if YMAGINE_PROFILE
  if (profile) {
    fprintf(stdout, "Transcoded image %d times in average of %.2f ms (%.2f ms total)\n",
            niters,
            ((double) total_transcode) / (niters * 1000000.0),
            ((double) (end - start)) / (niters * 1000000.0));
    fflush(stdout);
  }
#endif

  return rc;
}

static int
usage(const char *mode)
{
  fprintf(stdout, "usage: ymagine mode ?-options ...? ?--? filename...\n");
  fprintf(stdout, "supported jpeg mode: decode or transcode\n");
  fprintf(stdout, "supported conversion mode: nv21torgb\n");
  fflush(stdout);

  return 0;
}

static void usage_seam()
{
  printf("usage: ymagine seam [-width X] [-height X] infile ?outfile? ?seamfile?\n");
}

static int
main_filters(int sobel, int argc, const char* argv[])
{
  int i;
  const char* infile = NULL;
  const char* outfile = NULL;
  const char* seamfile = NULL;

  Vbitmap *vbitmap = NULL;
  Vbitmap *outbitmap = NULL;
  Ychannel *channel;
  int fd;

  int width = -1;
  int height = -1;

  int nbiters = 1;
  int pass;

  NSTYPE start,end;
  int scaleMode = YMAGINE_SCALE_LETTERBOX;

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
      }
      i++;
    } else if (argv[i][1] == 'h' && strcmp(argv[i], "-height") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
      }
      i++;
    } else {
      fprintf(stdout, "unknown option \"%s\"\n", argv[i]);
      fflush(stdout);
      return 1;
    }
  }

  if (i >= argc) {
    usage_seam();
    return 1;
  }

  infile = argv[i];
  i++;

  if (i < argc) {
    outfile = argv[i];
    i++;
  }
  if (i < argc) {
    seamfile = argv[i];
    i++;
  }

  fd = open(infile, O_RDONLY);
  if (fd >= 0) {
    channel = YchannelInitFd(fd, 0);
    vbitmap = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
    start = NSTIME();
    YmagineDecodeResize(vbitmap, channel, width, height, scaleMode);
    end = NSTIME();
    YchannelRelease(channel);
    close(fd);
  }

  if (vbitmap != NULL) {
    int inwidth = VbitmapWidth(vbitmap);
    int inheight = VbitmapHeight(vbitmap);
    VbitmapSeamMap *seammap = NULL;

    fprintf(stderr,
            "Input %dx%d decoded in %d ms\n",
            inwidth, inheight,
            (int) ((end - start)/ 1000000L));

    if (sobel) {
      outbitmap = VbitmapInitMemory(VBITMAP_COLOR_GRAYSCALE);
    } else {
      outbitmap = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
      VbitmapResize(outbitmap, (inwidth * 2) / 3, inheight);
      // VbitmapResize(outbitmap, (inwidth * 3) / 2, inheight);
    }

    for (pass = 0; pass < nbiters; pass++) {
      int outwidth = 0;
      int outheight = 0;

      if (sobel) {
        start = NSTIME();
        Vbitmap_sobel(outbitmap, vbitmap);
        end = NSTIME();

        fprintf(stderr,
                "sobel edge-detection (%dx%d) -> %d ms\n",
                inwidth, inheight,
                (int) ((end - start)/ 1000000L));
      } else {
        start = NSTIME();
        seammap = Vbitmap_seamPrepare(vbitmap);
        end = NSTIME();
        fprintf(stderr,
                "Prepared seam map in %d ms\n",
                (int) ((end - start)/ 1000000L));

        start = NSTIME();
        Vbitmap_seamCarve(vbitmap, seammap, outbitmap);
        end = NSTIME();

        outwidth = VbitmapWidth(outbitmap);
        outheight = VbitmapHeight(outbitmap);

        fprintf(stderr,
                "Seam apply (%dx%d) -> (%dx%d) in %d us\n",
                inwidth, inheight,
                outwidth, outheight,
                (int) ((end - start)/ 1000L));
      }

      if (outfile != NULL) {
        /* Save result */
        int fdout = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fdout >= 0) {
          Ychannel *ochannel = YchannelInitFd(fdout, 1);
          if (ochannel == NULL) {
          } else {
            YmagineEncode(outbitmap, ochannel, NULL);
            YchannelRelease(ochannel);
          }
          close(fdout);
        }
      }

      if (seammap != NULL && seamfile != NULL) {
        /* Render seams */
        Vbitmap_seamRender(vbitmap, seammap, inwidth - outwidth);
        /* Save result */
        int fdout = open(seamfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
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

      VbitmapRelease(outbitmap);
      VbitmapSeamMap_release(seammap);
    }

    VbitmapRelease(vbitmap);
  }

  return 0;
}

static int
main_seam(int argc, const char* argv[])
{
  return main_filters(0, argc, argv);
}

static int
main_sobel(int argc, const char* argv[])
{
  return main_filters(1, argc, argv);
}

static void usage_convert()
{
  printf("usage: ymagine convert [-width X] [-height X] infile.yuv outfile.jpg\n");
}

static int
main_convert(int argc, const char* argv[])
{
  int i;
  const char* infile;
  const char* outfile;

  unsigned char* indata;
  size_t indatalen = 0;

  unsigned char *outdata;

  int width = -1;
  int height = -1;

  int nbiters = 1;
  int pass;

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
      }
      i++;
      width = atoi(argv[i]);
    } else if (argv[i][1] == 'h' && strcmp(argv[i], "-height") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
      }
      i++;
      height = atoi(argv[i]);
    } else {
      fprintf(stdout, "unknown option \"%s\"\n", argv[i]);
      fflush(stdout);
      return 1;
    }
  }

  if (i+1 >= argc || width<=0 || height<=0) {
    usage_convert();
    return 1;
  }

  infile = argv[i];
  i++;
  outfile = argv[i];
  i++;

  printf("converting from NV21 raw to RGB\n");

  indata = (unsigned char*) LoadFile(infile, &indatalen);

  if (indatalen != width*height + (width*height)/2) {
    printf("size missmatches widthxheight\n");
    fflush(stdout);
    return 1;
  }

  printf("loaded raw YUV input image\n");

  ycolor_yuv_prepare();

  Vbitmap *vbitmap = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
  VbitmapResize(vbitmap, width/2, height/2);

  start = NSTIME();
  for (pass=0; pass<nbiters; pass++) {
    VbitmapLock(vbitmap);
    VbitmapWriteNV21Buffer(vbitmap, indata, width, height, YMAGINE_SCALE_HALF_QUICK);
    VbitmapUnlock(vbitmap);
    //Ymagine_blur(vbitmap, 40);
  }
  end = NSTIME();

#if YMAGINE_PROFILE
  fprintf(stdout, "Converted YUV2RGB %d times in %lld ns -> %.2f ms per conversion\n",
          nbiters, (long long) (end - start),
          ((double) (end - start)) / (nbiters*1000000.0));
  fflush(stdout);
#endif

  VbitmapLock(vbitmap);
  outdata = VbitmapBuffer(vbitmap);

  printf("finished YUV to RGB\n");

  int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (write(fd, outdata, VbitmapWidth(vbitmap)*VbitmapHeight(vbitmap)*3) < 0) {
    printf("error writing output file");
  }
  close(fd);

  VbitmapUnlock(vbitmap);
  VbitmapRelease(vbitmap);
  Ymem_free(indata);
  return 0;
}

static void usage_blur()
{
  printf("usage: ymagine blur [-width width] [-height heught] infile.jpg outfile.jpg\n");
}

static int
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
      }
      i++;
      width = atoi(argv[i]);
    } else if (argv[i][1] == 'h' && strcmp(argv[i], "-height") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
      }
      i++;
      height = atoi(argv[i]);
    } else {
      fprintf(stdout, "unknown option \"%s\"\n", argv[i]);
      fflush(stdout);
      return 1;
    }
  }

  if (i+1 >= argc || width<=0 || height<=0) {
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

  start = NSTIME();
  for (pass=0; pass<nbiters; pass++) {
    Ymagine_blur(vbitmap, 40);
  }
  end = NSTIME();

#if YMAGINE_PROFILE
  fprintf(stdout, "Blured %d times in %lld ns -> %.2f ms per conversion\n",
          nbiters, (long long) (end - start),
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

#ifdef MAX
#  undef MAX
#endif
#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* Compute infinity norm (i.e. uniform) of (a - b) */
#ifdef LINF
#  undef LINF
#endif
#define LINF(a, b) ((a) > (b) ? (a) - (b) : (b) - (a))

static int
main_colorconv(int argc, const char* argv[])
{
  uint32_t rgb, rgb2;
  uint32_t hsv;
  uint32_t ndiff[9];
  uint32_t maxdiff;
  int i;

  for (i = 0; i <= 8; i++) {
    ndiff[i] = 0;
  }
  maxdiff = 0;

  /* #906050 -> HSV (15, 44.44, 56.47) = 0A7190 */
  rgb = YcolorRGBA(0x90, 0x60, 0x50, 0xff);
  hsv = YcolorRGBtoHSV(rgb);
  rgb2 = YcolorHSVtoRGB(hsv);
  printf("RGB=#%06x -> HSV=#%08x -> RGB=#%08x\n", rgb, hsv, rgb2);

  /* #07A9C8 -> HSV (189.64, 96.5, 78.43) = 87F6C8 */
  rgb = YcolorRGBA(0x07, 0xA9, 0xC8, 0xff);
  hsv = YcolorRGBtoHSV(rgb);
  rgb2 = YcolorHSVtoRGB(hsv);
  printf("RGB=#%06x -> HSV=#%08x -> RGB=#%08x\n", rgb, hsv, rgb2);
  
  for (rgb = 0xff000000; rgb < 0xffffffff; rgb++) {
    hsv = YcolorRGBtoHSV(rgb);
    rgb2 = YcolorHSVtoRGB(hsv);

    if (rgb2 != rgb) {
      /* Compute max norm */
      int cdiff = 0;
      cdiff = MAX(cdiff, LINF((rgb2 >> 24) & 0xff, (rgb >> 24) & 0xff));
      cdiff = MAX(cdiff, LINF((rgb2 >> 16) & 0xff, (rgb >> 16) & 0xff));
      cdiff = MAX(cdiff, LINF((rgb2 >>  8) & 0xff, (rgb >>  8) & 0xff));
      cdiff = MAX(cdiff, LINF((rgb2 >>  0) & 0xff, (rgb >>  0) & 0xff));

      maxdiff = MAX(maxdiff, cdiff);

      /* Allow up to 3 bits to be lost in conversion due do fixed point approximation */
      if (cdiff > 7) {
        printf("ERROR RGB=#%08X -> HSV=#%08X -> RGB=#%08X\n", rgb, hsv, rgb2);
        ndiff[8]++;
      } else {
        ndiff[cdiff]++;
      }
    }
  }

  printf("Rounding: 0bits:%d 1:%d 2:%d 3:%d 4:%d 5:%d 6:%d 7:%d\n",
         ndiff[0], ndiff[1], ndiff[2], ndiff[3], ndiff[4], ndiff[5], ndiff[6], ndiff[7]);

  if (maxdiff > 7) {
    printf("  -> ERROR: found %d conversions with more than 3 bits lost (max error is %d)\n",
           ndiff[8], maxdiff);
  } else {
    printf("  -> PASSED\n");
  }

  return 0;
}


int main(int argc, const char* argv[])
{
  enum command {
    COMMAND_DECODE,
    COMMAND_TRANSCODE,
    COMMAND_SEAM,
    COMMAND_SOBEL,
    COMMAND_BLUR,
    COMMAND_CONVERT,
    COMMAND_COLORCONV
  };
  int mode = -1;

  if (argc >= 2) {
    if (argv[1][0] == 'd' && strcmp(argv[1], "decode") == 0) {
      mode = COMMAND_DECODE;
    }
    else if (argv[1][0] == 't' && strcmp(argv[1], "transcode") == 0) {
      mode = COMMAND_TRANSCODE;
    }
    else if (argv[1][0] == 's' && strcmp(argv[1], "seam") == 0) {
      mode = COMMAND_SEAM;
    }
    else if (argv[1][0] == 's' && strcmp(argv[1], "sobel") == 0) {
      mode = COMMAND_SOBEL;
    }
    else if (argv[1][0] == 'b' && strcmp(argv[1], "blur") == 0) {
      mode = COMMAND_BLUR;
    }
    else if (argv[1][0] == 'c' && strcmp(argv[1], "convert") == 0) {
      mode = COMMAND_CONVERT;
    }
    else if (argv[1][0] == 'c' && strcmp(argv[1], "colorconv") == 0) {
      mode = COMMAND_COLORCONV;
    }
  }

  if (mode < 0) {
    usage(NULL);
    return 1;
  }

  switch ((enum command) mode) {
    case COMMAND_DECODE:
      return main_decode(argc - 2, argv + 2);
    case COMMAND_TRANSCODE:
      return main_transcode(argc - 2, argv + 2);
    case COMMAND_SEAM:
      return main_seam(argc - 2, argv + 2);
    case COMMAND_SOBEL:
      return main_sobel(argc - 2, argv + 2);
    case COMMAND_CONVERT:
      return main_convert(argc - 2, argv + 2);
    case COMMAND_BLUR:
      return main_blur(argc - 2, argv + 2);
    case COMMAND_COLORCONV:
      return main_colorconv(argc - 2, argv + 2);
    default:
      usage(NULL);
      return 1;
  }
}
