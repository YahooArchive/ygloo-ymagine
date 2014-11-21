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

static int
decodeImage(char* data, size_t length,
            int ncolors, int detect, int classify, PixelShader* shader)
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
  if (classify) {
    maxWidth = 256;
    maxHeight = 256;
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
      // vbitmap = VbitmapInitMemory(VBITMAP_COLOR_GRAYSCALE);
      vbitmap = VbitmapInitMemory(VBITMAP_COLOR_RGBA);

      if (vbitmap != NULL) {
        rc = YmagineDecode(vbitmap, channel, options);
        if (rc == YMAGINE_OK) {
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
            Ymagine_drawRect(vbitmap,
                             coords[4*i+0],
                             coords[4*i+1],
                             coords[4*i+2] - coords[4*i+0],
                             coords[4*i+3] - coords[4*i+1],
                             YcolorRGBA(0xff, 0x00, 0x00, 0x80), YMAGINE_COMPOSE_OVER);
          }
        }

        /* Export image with deteted regions */
        if (1) {
          int fmode;
          int fdout;
          int force = 1;
          int oformat = YMAGINE_IMAGEFORMAT_PNG;
          char *outfile = NULL;
          Ychannel *channelout = NULL;

          switch (oformat) {
          case YMAGINE_IMAGEFORMAT_PNG:
            outfile = "out.png";
            break;
          case YMAGINE_IMAGEFORMAT_GIF:
            outfile = "out.gif";
            break;
          case YMAGINE_IMAGEFORMAT_WEBP:
            outfile = "out.webp";
            break;
          case YMAGINE_IMAGEFORMAT_JPEG:
          default:
            oformat = YMAGINE_IMAGEFORMAT_JPEG;
            outfile = "out.jpg";
            break;
          }

          fmode = O_WRONLY | O_CREAT | O_BINARY;
          if (force) {
            /* Truncate file if it already exists */
            fmode |= O_TRUNC;
          } else {
            /* Fail if file already exists */
            fmode |= O_EXCL;
          }

          if (outfile != NULL) {
            fdout = open(outfile, fmode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            if (fdout >= 0) {
              channelout = YchannelInitFd(fdout, 1);
            }
          }
          if (channelout == NULL) {
            fprintf(stdout, "failed to create output stream\n");
            fflush(stdout);
          } else {
            YmagineFormatOptions *options = NULL;

            options = YmagineFormatOptions_Create();
            if (options != NULL) {
              YmagineFormatOptions_setFormat(options, oformat);
              rc = YmagineEncode(vbitmap, channelout, options);
              YmagineFormatOptions_Release(options);
            }
            YchannelRelease(channelout);
          }
        }

        VbitmapRelease(vbitmap);
      }
#else
      printf("detector not supported\n");
#endif
    } else if (classify) {
#if HAVE_PLUGIN_VISION
      vbitmap = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
      if (vbitmap != NULL) {
        rc = YmagineDecode(vbitmap, channel, options);
        if (rc == YMAGINE_OK) {
          classify_run(vbitmap);
        }
      }
#else
      printf("classifier not supported\n");
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

int
usage_decode()
{
  fprintf(stdout, "usage: ymagine decode ?-shaderName <shaderName (seperated "
          "by ';' without whitespace e.g., "
          "color-iced_tea;vignette-white_pinhole)>? "
          "?-ntimes <ntimes>? ?-cascade <path>? ?--? file1 ?...?\n");
  fflush(stdout);

  return 0;
}

int
main_decode(int argc, const char* argv[])
{
  const char *filename;
  char *fbase;
  size_t flen;

  NSTYPE start, end;
  int nbiters = 1;
  int warmup = 0;
  int detect = 0;
  int classify = 0;
  int pass;
  int i;
  int ncolors = 0;
  const char *classifier_path = NULL;
  const char *convnet_path = NULL;

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
        return 1;
      }
      i++;
      classifier_path = argv[i];
    } else if (argv[i][1] == 'c' && strcmp(argv[i], "-convnet") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      convnet_path = argv[i];
    } else if (argv[i][1] == 'c' && strcmp(argv[i], "-colors") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      ncolors = atoi(argv[i]);
    } else if (argv[i][1] == 'n' && strcmp(argv[i], "-ntimes") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
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
    fbase = LoadDataFromFile(filename, &flen);
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
    if (convnet_path != NULL) {
#if HAVE_PLUGIN_VISION
      fprintf(stdout, "Loading convnet from %s\n", convnet_path);
      fflush(stdout);
      if (classify_load_model(convnet_path) == YMAGINE_OK) {
        fprintf(stdout, "Convnet loaded from %s\n", convnet_path);
        classify = 1;
      } else {
        classify = 0;
      }
#else
      fprintf(stdout, "classifier plugin not supported, ignoring convnet\n");
#endif
    }

    if (warmup) {
      /* Make one run in each mode, to not benchmark one-time initialization */
      decodeImage(fbase, flen, 0, detect, classify, NULL);
    }

    start = NSTIME();
    for (pass = 0; pass < nbiters; ++pass) {
      decodeImage(fbase, flen, ncolors, detect, classify, NULL);
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

