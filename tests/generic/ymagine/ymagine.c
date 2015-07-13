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

#if YMAGINE_JAVASCRIPT
#include <emscripten.h>
#endif

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
        return 1;
      }
      i++;
    } else if (argv[i][1] == 'h' && strcmp(argv[i], "-height") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
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

  indata = (unsigned char*) LoadDataFromFile(infile, &indatalen);

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

static int
usage(const char *mode)
{
  fprintf(stdout, "usage: ymagine mode ?-options ...? ?--? filename...\n");
  fprintf(stdout, "supported mode: decode, info, design, tile, transcode, video, seam, sobel, blur, convert, conv_profile and colorconv\n");
  fflush(stdout);

  return 0;
}

#if YMAGINE_JAVASCRIPT
#define main main_ymagine_js
#endif

int main(int argc, const char* argv[])
{
  enum command {
    COMMAND_DECODE,
    COMMAND_INFO,
    COMMAND_DESIGN,
    COMMAND_TILE,
    COMMAND_TRANSCODE,
    COMMAND_VIDEO,
    COMMAND_SEAM,
    COMMAND_SOBEL,
    COMMAND_BLUR,
    COMMAND_CONVERT,
    COMMAND_COLORCONV,
    COMMAND_PSNR,
    COMMAND_SHAPE,
    COMMAND_CONVOLUTION_PROFILE,
  };
  int mode = -1;

#if YMAGINE_JAVASCRIPT
#if YMAGINE_JAVASCRIPT_CLOSURE
  /* Initialization performed in init.js */
#else
  EM_ASM(
    FS['mkdir']('/vfs');
    FS['mount'](NODEFS, { root: '.' }, '/vfs');
    FS['chdir']('/vfs');
  );
#endif
#endif

  if (argc >= 2) {
    if (argv[1][0] == 'd' && strcmp(argv[1], "decode") == 0) {
      mode = COMMAND_DECODE;
    }
    else if (argv[1][0] == 'i' && strcmp(argv[1], "info") == 0) {
      mode = COMMAND_INFO;
    }
    else if (argv[1][0] == 'd' && strcmp(argv[1], "design") == 0) {
      mode = COMMAND_DESIGN;
    }
    else if (argv[1][0] == 't' && strcmp(argv[1], "tile") == 0) {
      mode = COMMAND_TILE;
    }
    else if (argv[1][0] == 't' && strcmp(argv[1], "transcode") == 0) {
      mode = COMMAND_TRANSCODE;
    }
    else if (argv[1][0] == 'v' && strcmp(argv[1], "video") == 0) {
      mode = COMMAND_VIDEO;
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
    else if (argv[1][0] == 'p' && strcmp(argv[1], "psnr") == 0) {
      mode = COMMAND_PSNR;
    }
    else if (argv[1][0] == 's' && strcmp(argv[1], "shape") == 0) {
      mode = COMMAND_SHAPE;
    }
    else if (argv[1][0] == 'c' && strcmp(argv[1], "conv_profile") == 0) {
      mode = COMMAND_CONVOLUTION_PROFILE;
    }
  }

  if (mode < 0) {
    usage(NULL);
    return 1;
  }

  switch ((enum command) mode) {
    case COMMAND_DECODE:
      return main_decode(argc - 2, argv + 2);
    case COMMAND_INFO:
      return main_info(argc - 2, argv + 2);
    case COMMAND_DESIGN:
      return main_design(argc - 2, argv + 2);
    case COMMAND_TILE:
      return main_tile(argc - 2, argv + 2);
    case COMMAND_TRANSCODE:
      return main_transcode(argc - 2, argv + 2);
#if HAVE_PLUGIN_VIDEO
    case COMMAND_VIDEO:
      return main_video(argc - 2, argv + 2);
#endif
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
    case COMMAND_PSNR:
      return main_psnr(argc - 2, argv + 2);
    case COMMAND_SHAPE:
      return main_shape(argc - 2, argv + 2);
    case COMMAND_CONVOLUTION_PROFILE:
      return main_convolution_profile(argc - 2, argv + 2);
    default:
      usage(NULL);
      return 1;
  }
}
