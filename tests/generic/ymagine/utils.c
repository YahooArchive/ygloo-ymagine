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

/* Preload data from file */
char*
LoadDataFromFile(const char *filename, size_t *length)
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

/* Helper function to parse shader argument */
int
parseopts_compose(const char* argv[], int argc, int i, int *composeref)
{
  /* Number of arguments processed */
  int nargs = 0;
  int compose = YMAGINE_COMPOSE_REPLACE;

  if (argv[i][1] == 'c' && strcmp(argv[i], "-compose") == 0) {
    if (i+1 >= argc) {
      fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
      fflush(stdout);
      return -1;
    }
    i++;

    if (strcmp(argv[i], "replace") == 0) {
      compose = YMAGINE_COMPOSE_REPLACE;
    }
    else if (strcmp(argv[i], "over") == 0) {
      compose = YMAGINE_COMPOSE_OVER;
    }
    else if (strcmp(argv[i], "under") == 0) {
      compose = YMAGINE_COMPOSE_UNDER;
    }
    else if (strcmp(argv[i], "plus") == 0) {
      compose = YMAGINE_COMPOSE_PLUS;
    }
    else if (strcmp(argv[i], "minus") == 0) {
      compose = YMAGINE_COMPOSE_MINUS;
    }
    else if (strcmp(argv[i], "add") == 0) {
      compose = YMAGINE_COMPOSE_ADD;
    }
    else if (strcmp(argv[i], "subtract") == 0) {
      compose = YMAGINE_COMPOSE_SUBTRACT;
    }
    else if (strcmp(argv[i], "difference") == 0) {
      compose = YMAGINE_COMPOSE_DIFFERENCE;
    }
    else if (strcmp(argv[i], "bump") == 0) {
      compose = YMAGINE_COMPOSE_BUMP;
    }
    else if (strcmp(argv[i], "map") == 0) {
      compose = YMAGINE_COMPOSE_MAP;
    }
    else if (strcmp(argv[i], "mix") == 0) {
      compose = YMAGINE_COMPOSE_MIX;
    }
    else if (strcmp(argv[i], "mult") == 0) {
      compose = YMAGINE_COMPOSE_MULT;
    }
    else if (strcmp(argv[i], "luminance") == 0) {
      compose = YMAGINE_COMPOSE_LUMINANCE;
    }
    else if (strcmp(argv[i], "luminanceinv") == 0) {
      compose = YMAGINE_COMPOSE_LUMINANCEINV;
    } else {
      fprintf(stdout, "invalid composition mode \"%s\"\n", argv[i]);
      fflush(stdout);
      return -1;
    }

    nargs += 2;
    if (composeref != NULL) {
      *composeref = compose;
    }
  }

  return nargs;
}

int
parseopts_shader(const char* argv[], int argc, int i, PixelShader *shader, int compose)
{
  /* Number of arguments processed */
  int nargs = 0;
  if (argv[i][1] == 's' && strcmp(argv[i], "-saturation") == 0) {
    double saturation = 0.0f;
    if (i+1 >= argc) {
      fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
      fflush(stdout);
      return -1;
    }
    i++;
    saturation = atof(argv[i]);
    Yshader_PixelShader_saturation(shader, saturation);
    nargs += 2;
  } else if (argv[i][1] == 'e' && strcmp(argv[i], "-exposure") == 0) {
      double exposure = 0.0f;
      if (i+1 >= argc) {
        fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
        fflush(stdout);
        return -1;
      }
      i++;
      exposure = atof(argv[i]);
      Yshader_PixelShader_exposure(shader, exposure);
      nargs += 2;
  } else if (argv[i][1] == 'c' && strcmp(argv[i], "-contrast") == 0) {
    double contrast = 0.0f;
    if (i+1 >= argc) {
      fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
      fflush(stdout);
      return -1;
    }
    i++;
    contrast = atof(argv[i]);
    Yshader_PixelShader_contrast(shader, contrast);
    nargs += 2;
  } else if (argv[i][1] == 'b' && strcmp(argv[i], "-brightness") == 0) {
    double brightness = 0.0f;
    if (i+1 >= argc) {
      fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
      fflush(stdout);
      return -1;
    }
    i++;
    brightness = atof(argv[i]);
    Yshader_PixelShader_brightness(shader, brightness);
    nargs += 2;
  } else if (argv[i][1] == 't' && strcmp(argv[i], "-temperature") == 0) {
    double temperature = 0.0f;
    if (i+1 >= argc) {
      fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
      fflush(stdout);
      return -1;
    }
    i++;
    temperature = atof(argv[i]);
    Yshader_PixelShader_temperature(shader, temperature);
    nargs += 2;
  } else if (argv[i][1] == 'w' && strcmp(argv[i], "-whitebalance") == 0) {
    double whitebalance = 0.0f;
    if (i+1 >= argc) {
      fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
      fflush(stdout);
      return -1;
    }
    i++;
    whitebalance = atof(argv[i]);
    Yshader_PixelShader_whitebalance(shader, whitebalance);
    nargs += 2;
  } else if (argv[i][1] == 'v' && strcmp(argv[i], "-vignette") == 0) {
    Vbitmap *vmap;
    const char* shaderfile;
    int fd;
    Ychannel* channel;

    if (i+1 >= argc) {
      fprintf(stdout, "missing value after option \"%s\"\n", argv[i]);
      fflush(stdout);
      return -1;
    }
    i++;

    /* load map for vignette */
    shaderfile = argv[i];
    fd = open(shaderfile, O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "error loading vignette map \"%s\"\n", shaderfile);
      return -1;
    }

    vmap = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
    channel = YchannelInitFd(fd, 0);
    YmagineDecodeResize(vmap, channel, -1, -1, YMAGINE_SCALE_LETTERBOX);
    YchannelRelease(channel);
    /* Either set auto-release to true, or close the file descriptor Ychannel doesn't own */
    close(fd);
    fprintf(stderr, "Loaded vignette map from '%s' %dx%d\n",
            shaderfile, VbitmapWidth(vmap), VbitmapHeight(vmap));

    /* Append vignette shader to pipeline */
    VbitmapRetain(vmap);
    Yshader_PixelShader_vignette(shader, vmap, compose);
    VbitmapRelease(vmap);

    nargs += 2;
  }

  return nargs;
}
