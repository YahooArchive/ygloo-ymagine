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

#ifdef HAVE_DLIB

static int
shapedetector(const char *fdetector, const char *infile)
{  
  /* Initialize shape detector */
  shape_load_model(fdetector);
    
  /* Initialize face detector */
  detect_load_model("@face");

  Vbitmap *vbitmap;
  Ychannel *channelin;
  int rc = YMAGINE_ERROR;
  int i, k;
  int w;
  int h;
  int fdin;
  YmagineFormatOptions *options = NULL;
  const char *outfile = "out.jpg";

  vbitmap = VbitmapInitMemory(VBITMAP_COLOR_RGBA);
  if (vbitmap != NULL) {
    fdin = open(infile, O_RDONLY | O_BINARY);
    if (fdin < 0) {
      fprintf(stdout, "failed to open input file \"%s\"\n", infile);
      fflush(stdout);
    } else {
      channelin = YchannelInitFd(fdin, 0);
      if (channelin == NULL) {
	fprintf(stdout, "failed to create input stream\n");
	fflush(stdout);
      } else {
	rc = YmagineDecode(vbitmap, channelin, options);
      }
    }

    if (rc == YMAGINE_OK) {
      w = VbitmapWidth(vbitmap);
      h = VbitmapHeight(vbitmap);
    } else {
      w = 0;
      h = 0;
    }

    if (w > 0 && h > 0) {
      int nfound;
      int maxmatches = 16;
      int coords[4*maxmatches];
      int scores[maxmatches];
      Cbitmap *cbitmap;
      NSTYPE start_detect;
      NSTYPE end_detect;

      cbitmap = CbitmapCreate(vbitmap);

      start_detect = NSTIME();
      // nfound = detect_run(vbitmap, -1, maxmatches, coords, scores);
      nfound = shape_face(cbitmap, -1, maxmatches, coords, scores);
      end_detect = NSTIME();

      fprintf(stdout, "found %d faces in %.2f ms", nfound, (float) ((end_detect - start_detect) / 1000000.0f));
      fflush(stdout);      

      for (i = 0; i < nfound; i++) {
        long left, top, right, bottom;
        long cx, cy;
        NSTYPE start_shape;
        NSTYPE end_shape;
        int points[256 * 2];
        int npoints;

        fprintf(stdout,
                " #%d: @(%d,%d)->(%dx%d) score=%d\n",
                i+1,
                coords[4*i+0],
                coords[4*i+1],
                coords[4*i+2],
                coords[4*i+3],
                scores[i]);
        fflush(stdout);

        left = (long) coords[4*i+0];
        top = (long) coords[4*i+1];
        right = (long) coords[4*i+2];
        bottom = (long) coords[4*i+3];

        cx = (left + right) / 2;
        cy = (top + bottom) / 2;

        left = cx + ((left - cx) * 110) / 100;
        top = cy + ((top - cy) * 110) / 100;
        right = cx + ((right - cx) * 110) / 100;
        bottom = cy + ((bottom - cy) * 110) / 100;

        if (left < 0) {
          left = 0;
        }
        if (left > w - 1) {
          left = w - 1;
        }
        if (top < 0) {
          top = 0;
        }
        if (top > h - 1) {
          top = h - 1;
        }
        if (right < 0) {
          right = 0;
        }
        if (right > w - 1) {
          right = w - 1;
        }
        if (bottom < 0) {
          bottom = 0;
        }
        if (bottom > h - 1) {
          bottom = h - 1;
        }

        fprintf(stderr, "Running detection\n");
        start_shape = NSTIME();
        npoints = shape_run(cbitmap, left, top, right - left + 1, bottom - top + 1, points, sizeof(points) / (2 * sizeof(points[0])));
        end_shape = NSTIME();

        fprintf(stdout, "Detection of %d parts in image %dx%d in %.2f ms\n",
                npoints,
                VbitmapWidth(vbitmap), VbitmapHeight(vbitmap),
                ((double) (end_shape - start_shape)) / 1000000.0);
        fflush(stdout);

        Ymagine_drawRect(vbitmap, left, top, right - left, bottom - top,
                         YcolorRGBA(0xff, 0x00, 0x00, 0x40), YMAGINE_COMPOSE_OVER);
        for (k = 0; k < npoints; k++) {
          Ymagine_drawRect(vbitmap,
                           points[2 * k] - 1,
                           points[2 * k + 1] - 1,
                           3, 3,
                           YcolorRGBA(0x00, 0xff, 0x00, 0xc0), YMAGINE_COMPOSE_OVER);
        }
      }

      CbitmapRelease(cbitmap);

      if (outfile != NULL) {
        /* Save bitmap with rendered detected shape */
        Ychannel *channelout;
        YmagineFormatOptions *encodeoptions = NULL;
        int fdout;
        int oformat = YMAGINE_IMAGEFORMAT_JPEG;
        int fmode;
        int force = 1;

        fmode = O_WRONLY | O_CREAT | O_BINARY;
        if (force) {
          /* Truncate file if it already exisst */
          fmode |= O_TRUNC;
        } else {
          /* Fail if file already exists */
          fmode |= O_EXCL;
        }

        encodeoptions = YmagineFormatOptions_Create();
        if (encodeoptions != NULL) {
          YmagineFormatOptions_setFormat(encodeoptions, oformat);
        }

        fdout = open(outfile, fmode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (fdout >= 0 ) {
          channelout = YchannelInitFd(fdout, 1);
          if (channelout != NULL) {
            rc = YmagineEncode(vbitmap, channelout, encodeoptions);
          }
          YchannelRelease(channelout);
          close(fdout);
        }
      }
    }

    VbitmapRelease(vbitmap);
  }

  return 0;
}

#else
static int
shapedetector(const char *fdetector, const char *infile)
{
  printf("support for shape detector disabled\n");

  return 1;
}
#endif

int usage_shape()
{
  printf("usage: ymagine shape shape_predictor_68.dat face.jpg\n");

  return 0;
}

int main_shape(int argc, const char* argv[])
{
  const char *fdetector;
  const char *infile;

  if (argc < 2) {
    usage_shape();
    return 1;
  }

  fdetector = argv[0];
  infile = argv[1];

  return shapedetector(fdetector, infile);
}
