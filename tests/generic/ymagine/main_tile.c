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
usage_tile()
{
  fprintf(stdout, "usage: ymagine tile \\\n"
          "?-width <integer> - output max width \\\n"
          "?-height <integer> - output max height \\\n"
          "?-crop <string> - crop region, following <width>x<height>@<x>,<y> pattern. Example: -crop 100x150@0,65 \\\n"
          "?-cropr <string> - cropr region, following <width>x<height>@<x>,<y> pattern. Example: -cropr 0.5x0.5@0.1,0.1 \\\n"
          "infile outfile\n");
  fflush(stdout);

  return 0;
}

typedef struct {
  int mode;
} privateOptions;

static int
progressCallback(YmagineFormatOptions *options,
                int format, int width, int height)
{
  privateOptions *pdata;

  /* Opaque handle to data set by the caller */
  pdata = (privateOptions*) YmagineFormatOptions_getData(options);

  if (pdata != NULL && pdata->mode == 1) {
    /* It's possible to update format options before they get actually used */
    YmagineFormatOptions_setCropRelative(options,
                                         0.0, 0.25,
                                         0.5, 0.5);
  }

  return YMAGINE_OK;
}

int
main_tile(int argc, const char* argv[])
{
  Ychannel *channelin;
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
  int maxWidth = -1;
  int maxHeight = -1;
  int tileSize = -1;
  /* scaleMode can be YMAGINE_SCALE_CROP or YMAGINE_SCALE_LETTERBOX */
  int scaleMode = YMAGINE_SCALE_LETTERBOX;
  /* Fit mode */
  int adjustMode = YMAGINE_ADJUST_NONE;
  /* metaMode can be one of YMAGINE_METAMODE_ALL, YMAGINE_METAMODE_COMMENTS,
     YMAGINE_METAMODE_NONE or YMAGINE_METAMODE_DEFAULT */
  int metaMode = YMAGINE_METAMODE_DEFAULT;
  int quality = -1;
  int accuracy = -1;
  int subsampling = -1;
  int progressive = -1;
  float sharpen = 0.0f;
  float blur = 0.0f;
  float rotate = 0.0f;
  PixelShader *shader = NULL;
  int compose = YMAGINE_COMPOSE_REPLACE;
  int cropx;
  int cropy;
  int cropw;
  int croph;
  float cropxp;
  float cropyp;
  float cropwp;
  float crophp;
  YBOOL absolutecrop = YFALSE;
  YBOOL relativecrop = YFALSE;

  NSTYPE start = 0;
  NSTYPE end = 0;
  NSTYPE start_decode = 0;
  NSTYPE end_decode = 0;
  NSTYPE total_decode;
  int i, j;
  char *writebuf = NULL;
  int niters = 1;
  int iter;
  int iformat = YMAGINE_IMAGEFORMAT_UNKNOWN;
  int oformat = YMAGINE_IMAGEFORMAT_UNKNOWN;
  YmagineFormatOptions *options = NULL;
  privateOptions *pdata = NULL;
  int dynamicopts = 0;
  Vbitmap *vbitmap = NULL;
  int ntiles = 0;
  int bwidth = 0;
  int bheight = 0;
  int scale;
  int scalelevel;

  if (argc < 1) {
    usage_tile();
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
        return 1;
      }
      i++;
      maxWidth = atoi(argv[i]);
    } else if (argv[i][1] == 'h' && strcmp(argv[i], "-height") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      maxHeight = atoi(argv[i]);
    } else if (argv[i][1] == 't' && strcmp(argv[i], "-tile") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      tileSize = atoi(argv[i]);
    } else if (argv[i][1] == 'r' && strcmp(argv[i], "-repeat") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      niters = atoi(argv[i]);
    } else if (argv[i][1] == 'q' && strcmp(argv[i], "-quality") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
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
    } else if (argv[i][1] == 'f' && strcmp(argv[i], "-format") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      if (strcmp(argv[i], "jpeg") == 0 || strcmp(argv[i], "jpg") == 0) {
        oformat = YMAGINE_IMAGEFORMAT_JPEG;
      } else if (strcmp(argv[i], "webp") == 0) {
        oformat = YMAGINE_IMAGEFORMAT_WEBP;
      } else if (strcmp(argv[i], "png") == 0) {
        oformat = YMAGINE_IMAGEFORMAT_PNG;
      } else {
        oformat = YMAGINE_IMAGEFORMAT_UNKNOWN;
      }
    } else if (argv[i][1] == 's' && strcmp(argv[i], "-scale") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      if (strcmp(argv[i], "letter") == 0 || strcmp(argv[i], "letterbox") == 0) {
        scaleMode = YMAGINE_SCALE_LETTERBOX;
      } else if (strcmp(argv[i], "crop") == 0) {
        scaleMode = YMAGINE_SCALE_CROP;
      } else if (strcmp(argv[i], "fit") == 0) {
        scaleMode = YMAGINE_SCALE_FIT;
      } else {
        fprintf(stdout, "invalid scale mode \"%s\". Should be letterbox, crop or fit\n", argv[i]);
        fflush(stdout);
        return 1;
      }
    } else if (argv[i][1] == 'm' && strcmp(argv[i], "-meta") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      if (strcmp(argv[i], "none") == 0 || strcmp(argv[i], "-") == 0) {
        metaMode = YMAGINE_METAMODE_NONE;
      } else if (strcmp(argv[i], "comments") == 0) {
        metaMode = YMAGINE_METAMODE_COMMENTS;
      } else if (strcmp(argv[i], "all") == 0) {
        metaMode = YMAGINE_METAMODE_ALL;
      } else {
        fprintf(stdout, "invalid meta mode \"%s\". Should be none, comments or all\n", argv[i]);
        fflush(stdout);
        return 1;
      }
    } else if (argv[i][1] == 'f' && strcmp(argv[i], "-fit") == 0) {
      /* Force rescale by power of two */
      scaleMode = YMAGINE_SCALE_FIT;
    } else if (argv[i][1] == 'c' && strcmp(argv[i], "-cropr") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      if (sscanf(argv[i], "%fx%f@%f,%f", &cropwp, &crophp, &cropxp, &cropyp) != 4) {
        fprintf(stdout, "invalid crop region \"%s\". Should be wxh@x,y\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      relativecrop = YTRUE;
      absolutecrop = YFALSE;
    } else if (argv[i][1] == 'c' && strcmp(argv[i], "-crop") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      if (sscanf(argv[i], "%dx%d@%d,%d", &cropw, &croph, &cropx, &cropy) != 4) {
        fprintf(stdout, "invalid crop region \"%s\". Should be wxh@x,y\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      relativecrop = YFALSE;
      absolutecrop = YTRUE;
    } else if (argv[i][1] == 'p' && strcmp(argv[i], "-profile") == 0) {
      profile = 1;
    } else if (argv[i][1] == 'p' && strcmp(argv[i], "-preset") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      presetFile = argv[i];
    } else if (argv[i][1] == 'a' && strcmp(argv[i], "-accuracy") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      accuracy = atoi(argv[i]);
    } else if (argv[i][1] == 's' && strcmp(argv[i], "-sharpen") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      sharpen = (float) atof(argv[i]);
    } else if (argv[i][1] == 'b' && strcmp(argv[i], "-blur") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      blur = (float) atof(argv[i]);
    } else if (argv[i][1] == 'r' && strcmp(argv[i], "-rotate") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      rotate = (float) atof(argv[i]);
    } else if (argv[i][1] == 'a' && strcmp(argv[i], "-adjust") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      if (strcmp(argv[i], "in") == 0 || strcmp(argv[i], "inner") == 0) {
        adjustMode = YMAGINE_ADJUST_INNER;
      } else if (strcmp(argv[i], "out") == 0 || strcmp(argv[i], "outer") == 0) {
        adjustMode = YMAGINE_ADJUST_OUTER;
      } else if (strcmp(argv[i], "none") == 0 || strcmp(argv[i], "-") == 0) {
        scaleMode = YMAGINE_ADJUST_NONE;
      } else {
        fprintf(stdout, "invalid adjust mode \"%s\". Should be inner, outer or none\n", argv[i]);
        fflush(stdout);
        return 1;
      }
    } else if (argv[i][1] == 's' && strcmp(argv[i], "-subsample") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      subsampling = atoi(argv[i]);
    } else if (argv[i][1] == 'p' && strcmp(argv[i], "-progressive") == 0) {
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return 1;
      }
      i++;
      progressive = atoi(argv[i]);
    }
    else {
      int nargs;

      nargs = parseopts_compose(argv, argc, i, &compose);
      if (nargs < 0) {
        /* Invalid option reported by compose parser */
        return 1;
      }

      if (nargs > 0) {
        i += nargs - 1;
      } else {
        if (shader == NULL) {
          shader = Yshader_PixelShader_create();
        }
        nargs = parseopts_shader(argv, argc, i, shader, compose);
        if (nargs > 0) {
          i += nargs - 1;
        } else {
          Yshader_PixelShader_release(shader);
          if (nargs < 0) {
            /* Invalid option reported by shader parser */
            return 1;
          }

          /* Unknown option */
          fprintf(stdout, "unknown option \"%s\"\n", argv[i]);
          fflush(stdout);
          return 1;
        }
      }
    }
  }

  if (i >= argc) {
    usage_tile();
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

  total_decode = 0;
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

    fdout = -1;
    fileout = NULL;

    channelin = YchannelInitFd(fdin, 0);
    if (channelin == NULL) {
      fprintf(stdout, "failed to create input stream\n");
      fflush(stdout);
    } else {
      /* Identify format for input image */
      iformat = YmagineFormat(channelin);
      if (oformat == YMAGINE_IMAGEFORMAT_UNKNOWN) {
        if (iformat == YMAGINE_IMAGEFORMAT_WEBP) {
          oformat = YMAGINE_IMAGEFORMAT_WEBP;
        } else if (iformat == YMAGINE_IMAGEFORMAT_PNG) {
          oformat = YMAGINE_IMAGEFORMAT_PNG;
        } else {
          oformat = YMAGINE_IMAGEFORMAT_JPEG;
        }
      }

      options = YmagineFormatOptions_Create();
      if (options != NULL) {
        YmagineFormatOptions_setFormat(options, oformat);
        YmagineFormatOptions_setResize(options, maxWidth, maxHeight, scaleMode);
        YmagineFormatOptions_setShader(options, shader);
        YmagineFormatOptions_setQuality(options, quality);
        YmagineFormatOptions_setAccuracy(options, accuracy);
        YmagineFormatOptions_setMetaMode(options, metaMode);
        if (subsampling >= 0) {
          YmagineFormatOptions_setSubsampling(options, subsampling);
        }
        if (progressive >= 0) {
          YmagineFormatOptions_setProgressive(options, progressive);
        }
        if (sharpen > 0.0f) {
          YmagineFormatOptions_setSharpen(options, sharpen);
        }
        if (blur > 0.0f) {
          YmagineFormatOptions_setBlur(options, blur);
        }
        if (rotate != 0.0f) {
          YmagineFormatOptions_setRotate(options, rotate);
        }
        YmagineFormatOptions_setAdjust(options, adjustMode);

        if (absolutecrop) {
          YmagineFormatOptions_setCrop(options, cropx, cropy, cropw, croph);
        } else if (relativecrop) {
          YmagineFormatOptions_setCropRelative(options, cropxp, cropyp, cropwp, crophp);
        }

        /* Set our callback for testing (no-op) */
        if (dynamicopts) {
          pdata = Ymem_malloc(sizeof(privateOptions));
          if (pdata != NULL) {
            /* Force arbitrary mode to have callback set a custom crop region */
            pdata->mode = 1;
            YmagineFormatOptions_setData(options, pdata);
          }
        }

        YmagineFormatOptions_setCallback(options, progressCallback);
      }

      start_decode = NSTIME();
      if (iformat == YMAGINE_IMAGEFORMAT_JPEG) {
        vbitmap = VbitmapInitMemory(VBITMAP_COLOR_RGB);
      } else {
        vbitmap = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
      }
      if (vbitmap != NULL) {
        rc = YmagineDecode(vbitmap, channelin, options);
      }

      if (options != NULL) {
        YmagineFormatOptions_Release(options);
        options = NULL;
      }

      YchannelRelease(channelin);
      channelin = NULL;

      if (pdata != NULL) {
        Ymem_free(pdata);
      }

      end_decode = NSTIME();
      total_decode += (end_decode - start_decode);

#if YMAGINE_PROFILE
      if (profile) {
        fprintf(stdout, "Decoded image %dx%d in %.2f ms\n",
                VbitmapWidth(vbitmap), VbitmapHeight(vbitmap),
                ((double) (end_decode - start_decode)) / 1000000.0);
        fflush(stdout);
      }
#endif

      if (vbitmap != NULL) {
        int tilewidth, tileheight;
        Vbitmap *tilebitmap;
        Vbitmap *scaledbitmap;
        int colormode;
        int pitch;
        int bpp;
        unsigned char *pixels;
        int tileid;
        Ychannel *channelout;
        YmagineFormatOptions *encodeoptions = NULL;
        YmagineFormatOptions *scaleoptions = NULL;

        VbitmapLock(vbitmap);

        colormode = VbitmapColormode(vbitmap);
        pitch = VbitmapPitch(vbitmap);
        bpp = VbitmapBpp(vbitmap);
        pixels = VbitmapBuffer(vbitmap);

        bwidth = VbitmapWidth(vbitmap);
        bheight = VbitmapHeight(vbitmap);

        scale = 1;
        scalelevel = 0;
        while (scale * 256 <= bwidth && scale * 256 <= bheight) {
          encodeoptions = YmagineFormatOptions_Create();
          if (encodeoptions != NULL) {
            YmagineFormatOptions_setFormat(encodeoptions, oformat);
          }

          scaleoptions = YmagineFormatOptions_Create();

          /* Generate all tiles */
          if (tileSize <= 0) {
            tileSize = 256;
          }

          tileid = 0;
          for (j = 0; j < bheight; j += tileSize * scale) {
            tileheight = tileSize * scale;
            if (j + tileheight > bheight) {
              tileheight = bheight - j;
            }

            for (i = 0; i < bwidth; i += tileSize * scale) {
              tilewidth = tileSize * scale;
              if (j + tilewidth > bwidth) {
                tilewidth = bwidth - j;
              }

              /* Create Vbitmap for region */
              tilebitmap = VbitmapInitStatic(colormode, tilewidth, tileheight, pitch, pixels + pitch * j + i * bpp);
              scaledbitmap = VbitmapInitMemory(colormode);

              /* Create scaled version */
              YmagineFormatOptions_setResize(scaleoptions, tilewidth / scale, tileheight / scale, YMAGINE_SCALE_LETTERBOX);
              YmagineDecodeCopy(scaledbitmap, tilebitmap, scaleoptions);

              /* Encode tile */  
              if (outfile != NULL) {
                char *tilename;
              
                tilename = Ymem_malloc(strlen(outfile) + 128);
                if (tilename != NULL) {
                  sprintf(tilename, "%s_s%02d_%d_%d.jpg", outfile, scalelevel, i, j);
                  fdout = open(tilename, fmode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                  if (fdout >= 0 ) {
                    channelout = YchannelInitFd(fdout, 1);
                    if (channelout != NULL) {
                      rc = YmagineEncode(scaledbitmap, channelout, encodeoptions);
                    }
                    YchannelRelease(channelout);
                    close(fdout);
                  }
                }
              }

              tileid++;
              ntiles++;

#if YMAGINE_PROFILE
              if (profile) {
                fprintf(stdout, "[1/%d] #%d %dx%d -> %dx%d @%d,%d\n",
                        scale, tileid,
                        VbitmapWidth(tilebitmap), VbitmapHeight(tilebitmap),
                        VbitmapWidth(scaledbitmap), VbitmapHeight(scaledbitmap),
                        i, j);
              }
#endif

              /* Release tile */
              VbitmapRelease(tilebitmap);
              VbitmapRelease(scaledbitmap);
            }
          }

          if (encodeoptions != NULL) {
            YmagineFormatOptions_Release(encodeoptions);
            encodeoptions = NULL;
          }
          if (scaleoptions != NULL) {
            YmagineFormatOptions_Release(scaleoptions);
            scaleoptions = NULL;
          }

          scale = scale * 2;
          scalelevel++;
        }

        VbitmapUnlock(vbitmap);
        VbitmapRelease(vbitmap);
      }
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
    fprintf(stdout, "Created %d %dx%d tiles for master image %dx%d in %.2f ms\n",
            ntiles, tileSize, tileSize, bwidth, bheight,
            ((double) (end - start)) / (niters * 1000000.0));
    fflush(stdout);
  }
#endif

  return rc;
}
