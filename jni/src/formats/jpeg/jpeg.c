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

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <setjmp.h>

#define LOG_TAG "ymagine::bitmap"
#include "ymagine_priv.h"

#include "graphics/bitmap.h"

#undef  HAVE_JPEG_EXTCOLORSPACE
#define HAVE_JPEGTURBO_EXTCOLORSPACE

#include <stdio.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "jinclude.h"
#include "jpeglib.h"
#include "jerror.h"

/*
 * Definition for markers copy helpers. Not strictly part of libjpeg public API, but as long
 * as this is linked with our own libjpeg (or libjpeg-turbo) build, we have the guarantee
 * those utilities are available, and it's not worth duplicating them here
 */
#include "transupp.h"

#include "formats/jpeg/jpegio.h"

/* No error reporting */
struct noop_error_mgr {           /* Extended libjpeg error manager */
  struct jpeg_error_mgr pub;    /* public fields */
  jmp_buf setjmp_buffer;        /* for return to caller from error exit */
};

static void
noop_append_jpeg_message (j_common_ptr cinfo)
{
#if YMAGINE_DEBUG_JPEG
  char buffer[JMSG_LENGTH_MAX];
  (*cinfo->err->format_message) (cinfo, buffer);
  ALOGE("jpeg error: %s\n", buffer);
#endif
}

static void
noop_output_message (j_common_ptr cinfo)
{
}

static void
noop_error_exit (j_common_ptr cinfo)
{
  struct noop_error_mgr *myerr = (struct noop_error_mgr *) cinfo->err;
  
  longjmp(myerr->setjmp_buffer, 1);
}

static void
noop_emit_message (j_common_ptr cinfo, int msg_level)
{
}

static void
noop_format_message (j_common_ptr cinfo, char * buffer)
{
}

void
noop_reset_error_mgr (j_common_ptr cinfo)
{
  cinfo->err->num_warnings = 0;
  /* trace_level is not reset since it is an application-supplied parameter */
  cinfo->err->msg_code = 0;     /* may be useful as a flag for "no error" */
}

static struct jpeg_error_mgr *
noop_jpeg_std_error (struct jpeg_error_mgr * err)
{
  err->error_exit = noop_error_exit;
  err->emit_message = noop_emit_message;
  err->output_message = noop_output_message;
  err->format_message = noop_format_message;
  err->reset_error_mgr = noop_reset_error_mgr;
  
  err->trace_level = 0;         /* default = no tracing */
  err->num_warnings = 0;        /* no warnings emitted yet */
  err->msg_code = 0;            /* may be useful as a flag for "no error" */
  
  /* Initialize message table pointers */
  err->jpeg_message_table = NULL;
  err->last_jpeg_message = 0;
  
  err->addon_message_table = NULL;
  err->first_addon_message = 0; /* for safety */
  err->last_addon_message = 0;
  
  return err;
}

static int
JpegPixelSize(J_COLOR_SPACE colorspace)
{
  int bpp;
  
  switch (colorspace) {
#ifdef HAVE_JPEG_EXTCOLORSPACE
    case JCS_RGBA_8888:
#endif
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      bpp = 4;
      break;
    case JCS_RGB:
      bpp = 3;
      break;
    case JCS_GRAYSCALE:
      bpp = 1;
      break;
    default:
      /* Unsupported color mode */
      bpp = 0;
      break;
  }
  
  return bpp;
}

static void
set_colormode(struct jpeg_compress_struct *cinfo, int colormode) {
  switch (colormode) {
    case VBITMAP_COLOR_GRAYSCALE:
      cinfo->in_color_space = JCS_GRAYSCALE;
      cinfo->input_components = 1;
      break;
    case VBITMAP_COLOR_RGB:
      cinfo->in_color_space = JCS_RGB;
      cinfo->input_components = 3;
      break;
    case VBITMAP_COLOR_RGBA:
    default:
#ifdef HAVE_JPEG_EXTCOLORSPACE
      cinfo->in_color_space = JCS_RGBA_8888;
#else
      cinfo->in_color_space = JCS_EXT_RGBX;
#endif
      cinfo->input_components = 4;
      break;
  }
}

static int
decompress_jpeg(struct jpeg_decompress_struct *cinfo,
                struct jpeg_compress_struct *cinfoout,
                unsigned char *opixels, int owidth, int oheight, int opitch,
                int ocolormode, int scalemode, PixelShader *shader)
{
  int scanlines;
  int nlines;
  int totallines;
  int i, j, k;
  int srcbpp, destbpp;
  JSAMPARRAY buffer;
  JSAMPROW row_pointer[1];
  unsigned int destline;
  unsigned int srcline;
  unsigned int prevline;
  unsigned char* destptr;
  unsigned char* srcptr;
  unsigned char* prevptr;
  unsigned char* nextptr;
  unsigned char* tmpptr = NULL;
  unsigned char* alignedptr = NULL;
  Rect srcrect;
  Rect destrect;
  
  if (cinfoout != NULL) {
    opixels = NULL;
    owidth = cinfoout->image_width;
    oheight = cinfoout->image_height;
    opitch = 0;
    ocolormode = VBITMAP_COLOR_RGB;
  }
  
  if (owidth <= 0 || oheight <= 0) {
    opixels = NULL;
    owidth = 0;
    oheight = 0;
    opitch = 0;
    ocolormode = VBITMAP_COLOR_RGB;
  }
  
  if (owidth > 0 && oheight > 0) {
    if (scalemode == YMAGINE_SCALE_CROP || scalemode == YMAGINE_SCALE_FIT) {
      /* Use higher reduction ratio while keeping generated image larger than output
       one in both direction */
      for (i = 1; i <= 8; i++) {
        if ( ( (cinfo->image_width * i) / 8) >= owidth &&
            ( (cinfo->image_height * i) / 8) >= oheight ) {
          break;
        }
      }
    } else {
      /* For letterbox, use higher reduction ratio while keeping generated image larger
       than output one in at least one direction */
      for (i = 1; i <= 8; i++) {
        if ( ( (cinfo->image_width * i) / 8) >= owidth ||
            ( (cinfo->image_height * i) / 8) >= oheight ) {
          break;
        }
      }
    }
    
    if (i > 0 && i < 8) {
      cinfo->scale_num=i;
      cinfo->scale_denom=8;
    }
  }
  
  /* jpeg_calc_output_dimensions(cinfo); */
  
  if (!jpeg_start_decompress(cinfo)) {
    return 0;
  }
  
#if YMAGINE_DEBUG_JPEG
  ALOGD("size: %dx%d req: %dx%d %s -> scale: %d/%d output: %dx%d components: %d\n",
        cinfo->image_width, cinfo->image_height,
        owidth, oheight,
        (scalemode == YMAGINE_SCALE_CROP) ? "crop" :
        (scalemode == YMAGINE_SCALE_FIT ? "fit" : "letterbox"),
        cinfo->scale_num, cinfo->scale_denom,
        cinfo->output_width, cinfo->output_height,
        cinfo->output_components);
#endif
  
  if (owidth > 0 && oheight > 0) {
    computeTransform(cinfo->output_width, cinfo->output_height,
                     owidth, oheight, scalemode,
                     &srcrect, &destrect);
    if (opixels != NULL) {
      imageFillOut(opixels, owidth, oheight, opitch, ocolormode, &destrect);
    }
  } else {
    computeTransform(0, 0, 0, 0, scalemode, &srcrect, &destrect);
  }
  
  srcbpp = JpegPixelSize(cinfo->out_color_space);
  if (opixels != NULL) {
    destbpp = colorBpp(ocolormode);
  } else if (cinfoout != NULL) {
    destbpp = JpegPixelSize(cinfoout->in_color_space);
  } else {
    /* Inject RGB data into jpeg encoder */
    destbpp = 3;
  }
  
  size_t row_stride = cinfo->output_width * cinfo->output_components;
  
  /* Number of scan lines to handle per pass. Making it larger actually doesn't help much */
  scanlines = (32 * 1024) / row_stride;
  if (scanlines < 1) {
    scanlines = 1;
  }
  if (scanlines > cinfo->output_height) {
    scanlines = cinfo->output_height;
  }
  
#if YMAGINE_DEBUG_JPEG
  ALOGD("BITMAP @(%d,%d) %dx%d bpp=%d -> @(%dx%d) %dx%d bpp=%d (%d lines)\n",
        srcrect.x, srcrect.y, srcrect.width, srcrect.height, srcbpp,
        destrect.x, destrect.y, destrect.width, destrect.height, destbpp,
        scanlines);
#endif
  
  if (opixels == NULL && cinfoout != NULL) {
    /* Allocate a temporary buffer large enough to contain 2 full lines
	   at the image resolution */
    opitch = owidth * destbpp;
    if (opitch % 8) {
      opitch += 8 - (opitch % 8);
    }
    tmpptr = Ymem_malloc_aligned(8, opitch * 2, (void**) &alignedptr);
    if (tmpptr == NULL) {
      jpeg_abort_decompress(cinfo);
      return 0;
    }
  }
  
  buffer = (JSAMPARRAY) (*cinfo->mem->alloc_sarray)((j_common_ptr) cinfo, JPOOL_IMAGE,
                                                    row_stride, scanlines);
  if (buffer == NULL) {
    jpeg_abort_decompress(cinfo);
    if (tmpptr == NULL) {
      Ymem_free(tmpptr);
    }
    return 0;
  }
  
  totallines = 0;
  destline = -1;
  destptr = NULL;
  
  while (cinfo->output_scanline < cinfo->output_height) {
    nlines = jpeg_read_scanlines(cinfo, buffer, scanlines);
    if (nlines <= 0) {
      /* Decoding error */
      break;
    }
    
    if (opixels != NULL || cinfoout != NULL) {
      for (j = 0; j < nlines; j++) {
        prevline = destline;
        prevptr = destptr;
        
        srcline = totallines + j;
        if (srcline >= srcrect.y && srcline < srcrect.y + srcrect.height) {
          destline = destrect.y + ((srcline - srcrect.y) * destrect.height) / srcrect.height;
          
          /* Fast vertical upscaling, by copying current scanline as often as necessary */
          /* TODO: downscale / upscale using vertical bilinear filter */
          if (prevptr != NULL && destline > prevline + 1) {
            nextptr = prevptr;
            for (k = prevline + 1; k < destline; k++) {
              if (opixels != NULL) {
                nextptr += opitch;
                memcpy(nextptr, prevptr, destrect.width * destbpp);
              } else {
                row_pointer[0] = nextptr;
                jpeg_write_scanlines(cinfoout, row_pointer, 1);
              }
            }
          }
          
          srcptr = ((unsigned char*) buffer[j]) + srcrect.x * srcbpp;
          if (opixels != NULL) {
            destptr = opixels + opitch * destline;
          } else {
            destptr = alignedptr;
            if (destptr == prevptr) {
              destptr += opitch;
            }
          }
          destptr += destrect.x * destbpp;
          
          /* Scale and convert to correct color space */
          bltLine(destptr, destrect.width, destbpp,
                  srcptr, srcrect.width, srcbpp);

          /* Apply shader on the scaled line */
          if (shader != NULL) {
            Yshader_apply(shader,
                          destptr, destrect.width, destbpp,
                          destrect.width, destrect.height,
                          0, destline);
          }

          if (opixels == NULL && cinfoout != NULL) {
            if (prevline != destline) {
              row_pointer[0] = destptr;
              jpeg_write_scanlines(cinfoout, row_pointer, 1);
            }
          }
          
          if (srcline == srcrect.y + srcrect.height - 1) {
            /* This is last line to be decoded. Pad if necessary. */
            nextptr = destptr;
            for (k = destline + 1; k < destrect.y + destrect.height; k++) {
              if (opixels != NULL) {
                nextptr += opitch;
                memcpy(nextptr, destptr, destrect.width * destbpp);
              } else {
                row_pointer[0] = nextptr;
                jpeg_write_scanlines(cinfoout, row_pointer, 1);
              }
            }
          }
        }
      }
    }
    
    totallines += nlines;
  }
  
  if (tmpptr != NULL) {
    Ymem_free(tmpptr);
  }
  
  if (cinfo->output_scanline > 0 && cinfo->output_scanline == cinfo->output_height) {
    /* Do normal cleanup if we read the whole image */
    jpeg_finish_decompress(cinfo);
  }
  else {
    /* else early abort */
    jpeg_abort_decompress(cinfo);
  }
  
  return totallines;
}

static int
prepareDecompressor(struct jpeg_decompress_struct *cinfo, int quality)
{
  if (cinfo == NULL) {
    return 1;
  }
  
  if (quality < 0) {
    quality = 85;
  }
  if (quality > 100) {
    quality = 100;
  }

  if (1) {
    cinfo->mem->max_memory_to_use = 30 * 1024 * 1024;
  } else {
    cinfo->mem->max_memory_to_use = 5 * 1024 * 1024;
  }
  
  if (quality < 90) {
    /* DCT method, one of JDCT_FASTEST, JDCT_IFAST, JDCT_ISLOW or JDCT_FLOAT */
    cinfo->dct_method = JDCT_IFAST;
    
    /* To perform 2-pass color quantization, the decompressor also needs a
     128K color lookup table and a full-image pixel buffer (3 bytes/pixel). */
    cinfo->two_pass_quantize = FALSE;
    
    /* No dithering with RGBA output. Use JDITHER_ORDERED only for JCS_RGB_565 */
    cinfo->dither_mode = JDITHER_NONE;
    
    /* Low visual impact but big performance benefit when turning off fancy up-sampling */
    cinfo->do_fancy_upsampling = FALSE;
    
    cinfo->do_block_smoothing = FALSE;
    
    cinfo->enable_2pass_quant = FALSE;
  } else {
    cinfo->dct_method = JDCT_ISLOW;

    if (quality >= 92) {
      cinfo->do_block_smoothing = TRUE;
    }

    /* Low visual impact but big performance benefit when turning off fancy up-sampling */
    if (quality >= 98) {
      cinfo->do_fancy_upsampling = TRUE;
    }
    if (quality >= 97) {
      cinfo->enable_2pass_quant = TRUE;
    }
  }
  
  return YMAGINE_OK;
}

static unsigned int
jpeg_getc (j_decompress_ptr cinfo)
{
  struct jpeg_source_mgr *datasrc = cinfo->src;

  if (datasrc->bytes_in_buffer == 0) {
    if (! (*datasrc->fill_input_buffer) (cinfo))
      ERREXIT(cinfo, JERR_CANT_SUSPEND);
  }
  datasrc->bytes_in_buffer--;
  return GETJOCTET(*datasrc->next_input_byte++);
}

static const char* XMP_MARKER = "http://ns.adobe.com/xap/1.0/";

static boolean
APP1_handler (j_decompress_ptr cinfo) {
  int length;
  int i;
  unsigned char *data = NULL;

  if (cinfo == NULL) {
    return FALSE;
  }

  length = jpeg_getc(cinfo) << 8;
  length += jpeg_getc(cinfo);
  if (length < 2) {
    return FALSE;
  }
  length -= 2;
  
  /* Read marker data in memory. Also get sure buffer is null terminated
     Null terminates buffer to make it printable for debugging */
  data = Ymem_malloc(length + 1);
  if (data == NULL) {
    return FALSE;
  }  
  for (i = 0; i < length; i++) {
    data[i] = (unsigned char) jpeg_getc(cinfo);
  }
  data[length] = '\0';

  int l = strlen(XMP_MARKER);
  if (length >= l + 1 && memcmp(data, XMP_MARKER, l) == 0 && data[l] == '\0') {
    VbitmapXmp xmp;
    Vbitmap *vbitmap = (Vbitmap*) cinfo->client_data;
    char *xmpbuf = (char*) (data + l + 1);
    int xmplen = length - (l + 1);

    /* Parse XML data */
    if (parseXMP(&xmp, xmpbuf, xmplen) == YMAGINE_OK) {
      if (vbitmap != NULL) {
        VbitmapSetXMP(vbitmap, &xmp);
      }
    }
  }

  Ymem_free(data);

  return TRUE;
}


static int
startDecompressor(struct jpeg_decompress_struct *cinfo, int colormode)
{
  
  if (jpeg_read_header(cinfo, TRUE) != JPEG_HEADER_OK) {
    return 1;
  }
  
  /*
   currently supports the following transformations:
   RGB => YCbCr
   RGB => GRAYSCALE
   YCbCr => GRAYSCALE
   CMYK => YCCK
   
   Conversion to GRAYSCALE is supported from any color mode. Default
   is otherwise JCS_RGB
   */
  switch (colormode) {
    case VBITMAP_COLOR_GRAYSCALE:
      cinfo->out_color_space = JCS_GRAYSCALE;
      break;
    case VBITMAP_COLOR_RGB:
      cinfo->out_color_space = JCS_RGB;
      break;
    case VBITMAP_COLOR_RGBA:
    default:
#ifdef HAVE_JPEG_EXTCOLORSPACE
      cinfo->out_color_space = JCS_RGBA_8888;
#else
      cinfo->out_color_space = JCS_EXT_RGBA;
#endif
      break;
  }
  
  return YMAGINE_OK;
}


static int
bitmap_decode(struct jpeg_decompress_struct *cinfo, Vbitmap *vbitmap,
              int maxWidth, int maxHeight, int scalemode, int quality,
              PixelShader* shader)
{
  unsigned char *pixels;
  int inwidth, inheight;
  int width, height;
  int pitch, colormode;
  int nlines = -1;
  int rc;
  
  cinfo->client_data = (void*) vbitmap;
  
  if (prepareDecompressor(cinfo, quality) != 0) {
    return nlines;
  }

  /* Intercept APP1 markers for PhotoSphere parsing */
  jpeg_set_marker_processor(cinfo, JPEG_APP0 + 1, APP1_handler);
  
  if (startDecompressor(cinfo, VbitmapColormode(vbitmap)) != 0) {
    return nlines;
  }
  
  inwidth = VbitmapWidth(vbitmap);
  inheight = VbitmapHeight(vbitmap);
  
  if (inwidth > 0 && inheight > 0) {
    width = inwidth;
    height = inheight;
    pitch = VbitmapPitch(vbitmap);
    colormode = VbitmapColormode(vbitmap);
  } else {
    int reqwidth, reqheight;
    
    computeBounds(cinfo->image_width, cinfo->image_height,
                  maxWidth, maxHeight, scalemode,
                  &reqwidth, &reqheight);
    if (VbitmapResize(vbitmap, reqwidth, reqheight) != YMAGINE_OK) {
      return nlines;
    }
    
    width = VbitmapWidth(vbitmap);
    height = VbitmapHeight(vbitmap);
    pitch = VbitmapPitch(vbitmap);
    colormode = VbitmapColormode(vbitmap);
  }

#if YMAGINE_DEBUG_JPEG
  ALOGD("bitmap_decode: in=%dx%d bm=%dx%d max=%dx%d out=%dx%d",
        cinfo->image_width, cinfo->image_height,
        inwidth, inheight,
        maxWidth, maxHeight,
        width, height);
#endif
  
  if (VbitmapType(vbitmap) == VBITMAP_NONE) {
    nlines = VbitmapHeight(vbitmap);

    return nlines;
  }

  rc = VbitmapLock(vbitmap);
  if (rc != YMAGINE_OK) {
    ALOGE("AndroidBitmap_lockPixels() failed (code %d)", rc);
  } else {
    pixels = VbitmapBuffer(vbitmap);
    if (pixels != NULL) {
      nlines = decompress_jpeg(cinfo, NULL, pixels,
                               width, height, pitch, colormode,
                               scalemode, shader);
    }
    VbitmapUnlock(vbitmap);
  }
  
  return nlines;
}

int
decodeJPEG(Ychannel *channel, Vbitmap *vbitmap,
           int maxWidth, int maxHeight,
           int scaleMode, int quality,
           PixelShader* pixelShader)
{
  struct jpeg_decompress_struct cinfo;
  struct noop_error_mgr jerr;
  int nlines = -1;
  
  if (!YchannelReadable(channel)) {
#if YMAGINE_DEBUG_JPEG
    ALOGD("input channel not readable");
#endif
    return nlines;
  }
  
  memset(&cinfo, 0, sizeof(struct jpeg_decompress_struct));
  cinfo.err = noop_jpeg_std_error(&jerr.pub);
  
  /* Establish the setjmp return context for noop_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error. */
    noop_append_jpeg_message((j_common_ptr) &cinfo);
  } else {
    jpeg_create_decompress(&cinfo);
    if (ymaginejpeg_input(&cinfo, channel) >= 0) {
      nlines = bitmap_decode(&cinfo, vbitmap,
                             maxWidth, maxHeight, scaleMode, quality,
                             pixelShader);
    }
  }
  jpeg_destroy_decompress(&cinfo);
  
  return nlines;
}

int
transcodeJPEG(Ychannel *channelin, Ychannel *channelout,
              int maxWidth, int maxHeight,
              int scalemode, int quality, PixelShader* shader)
{
  struct jpeg_decompress_struct cinfo;
  struct noop_error_mgr jerr;
  
  struct jpeg_compress_struct cinfoout;
  struct noop_error_mgr jerr2;
  int rc = YMAGINE_ERROR;
  int nlines = 0;
  
  if (!YchannelReadable(channelin) || !YchannelWritable(channelout)) {
    return rc;
  }
  
  /* Quality, 0=worst, 100=best, default to 80 */
  if (quality <= 0) {
    quality = 80;
  }
  if (quality >= 100) {
    quality = 100;
  }
  
  memset(&cinfo, 0, sizeof(struct jpeg_decompress_struct));
  cinfo.err = noop_jpeg_std_error(&jerr.pub);
  
  memset(&cinfoout, 0, sizeof(struct jpeg_compress_struct));
  cinfoout.err = noop_jpeg_std_error(&jerr2.pub);
  
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error in decoder */
    noop_append_jpeg_message((j_common_ptr) &cinfo);
  } else if (setjmp(jerr2.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error in encoder */
    noop_append_jpeg_message((j_common_ptr) &cinfoout);
  } else {
    jpeg_create_decompress(&cinfo);
    
    /* Equivalent to:
	   jpeg_CreateCompress(&cinfoout, JPEG_LIB_VERSION,
	   (size_t) sizeof(struct jpeg_compress_struct));
     */
    jpeg_create_compress(&cinfoout);
    
    if (ymaginejpeg_input(&cinfo, channelin) >= 0 &&
        ymaginejpeg_output(&cinfoout, channelout) >= 0) {
      if (prepareDecompressor(&cinfo, quality) == 0) {

        /* shader relies on composing which only supports RGBA 4 channels */
        int colormode =
            (shader == NULL) ? VBITMAP_COLOR_RGB : VBITMAP_COLOR_RGBA;

        /* markers copy option (NONE, COMMENTS or ALL) */
        JCOPY_OPTION copyoption = JCOPYOPT_ALL;
        /* Enable saving of extra markers that we want to copy */
        if (copyoption != JCOPYOPT_NONE) {
          jcopy_markers_setup(&cinfo, copyoption);
        }
        
        /* Force image to be decoded in RGB mode, since this is
         preferred input mode for encoder */
        if (startDecompressor(&cinfo, colormode) == 0) {
          int reqwidth, reqheight;
          /* Other compression settings */
          int smooth = 0;
          int optimize = 0;
          int progressive = 0;
          int grayscale = 0;
          
          if (quality >= 90) {
            optimize = 1;
            smooth = 50;
          }
          
          computeBounds(cinfo.image_width, cinfo.image_height,
                        maxWidth, maxHeight, scalemode,
                        &reqwidth, &reqheight);
          
          cinfoout.image_width = reqwidth;
          cinfoout.image_height = reqheight;
          set_colormode(&cinfoout, colormode);
          jpeg_set_defaults(&cinfoout);
          
#if YMAGINE_DEBUG_JPEG
          ALOGD("transcode: %dx%d req: %dx%d\n",
                cinfoout.image_width, cinfoout.image_height,
                maxWidth, maxHeight);
#endif
          
          jpeg_set_quality(&cinfoout, quality, FALSE);
          cinfoout.smoothing_factor = smooth;
          if (grayscale) {
            /* Force a monochrome JPEG file to be generated. */
            jpeg_set_colorspace(&cinfoout, JCS_GRAYSCALE);
          }
          if (optimize) {
            /* Enable entropy parm optimization. */
            cinfoout.optimize_coding = TRUE;
          }
          if (progressive) {
            /* Select simple progressive mode. */
            jpeg_simple_progression(&cinfoout);
          }

          jpeg_start_compress(&cinfoout, TRUE);
          if (copyoption != JCOPYOPT_NONE) {
            /* Copy to the output file any extra markers that we want to preserve */
            jcopy_markers_execute(&cinfo, &cinfoout, copyoption);
          }

          cinfo.client_data = (void*) NULL;
          cinfoout.client_data = (void*) NULL;

          nlines = decompress_jpeg(&cinfo, &cinfoout, NULL, 0, 0, 0,
                                   colormode, scalemode, shader);
          if (nlines > 0) {
            rc = YMAGINE_OK;
          }
          
          /* Clean up compressor */
          jpeg_finish_compress(&cinfoout);
        }
      }
    }
  }
  
  jpeg_destroy_compress(&cinfoout);
  jpeg_destroy_decompress(&cinfo);
  
  return rc;
}

int
encodeJPEG(Vbitmap *vbitmap, Ychannel *channelout, int quality)
{
  struct jpeg_compress_struct cinfoout;
  struct noop_error_mgr jerr;
  int result = YMAGINE_ERROR;
  int rc;
  int nlines = 0;
  JSAMPROW row_pointer[1];
  unsigned char *pixels;
  int i;
  int width;
  int height;
  int pitch;
  int colormode;

  /* Quality, 0=worst, 100=best, default to 80 */
  if (quality < 0) {
    quality = 80;
  }
  if (quality >= 100) {
    quality = 100;
  }

  if (!YchannelWritable(channelout)) {
    return result;
  }

  if (vbitmap == NULL) {
    return result;
  }

  rc = VbitmapLock(vbitmap);
  if (rc < 0) {
    ALOGE("AndroidBitmap_lockPixels() failed");
    return result;
  }

  memset(&cinfoout, 0, sizeof(struct jpeg_compress_struct));
  cinfoout.err = noop_jpeg_std_error(&jerr.pub);

  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error in encoder */
    noop_append_jpeg_message((j_common_ptr) &cinfoout);
  } else {
    /* Equivalent to:
	   jpeg_CreateCompress(&cinfoout, JPEG_LIB_VERSION,
	   (size_t) sizeof(struct jpeg_compress_struct));
     */
    jpeg_create_compress(&cinfoout);

    if (ymaginejpeg_output(&cinfoout, channelout) >= 0) {
      /* Other compression settings */
      int smooth = 0;
      int optimize = 0;
      int progressive = 0;
      int grayscale = 0;

      if (quality >= 90) {
        optimize = 1;
        smooth = 1;
      }

      width = VbitmapWidth(vbitmap);
      height = VbitmapHeight(vbitmap);
      pitch = VbitmapPitch(vbitmap);
      colormode = VbitmapColormode(vbitmap);

      cinfoout.image_width = width;
      cinfoout.image_height = height;

      set_colormode(&cinfoout, colormode);

      jpeg_set_defaults(&cinfoout);

      jpeg_set_quality(&cinfoout, quality, FALSE);
      cinfoout.smoothing_factor = smooth;
      if (grayscale) {
        /* Force a monochrome JPEG file to be generated. */
        jpeg_set_colorspace(&cinfoout, JCS_GRAYSCALE);
      }
      if (optimize) {
        /* Enable entropy parm optimization. */
        cinfoout.optimize_coding = TRUE;
      }
      if (progressive) {
        /* Select simple progressive mode. */
        jpeg_simple_progression(&cinfoout);
      }
      jpeg_start_compress(&cinfoout, TRUE);

      pixels = VbitmapBuffer(vbitmap);
      for (i = 0; i < height; i++) {
        row_pointer[0] = pixels + i * pitch;
        jpeg_write_scanlines(&cinfoout, row_pointer, 1);
        nlines++;
      }
      if (nlines > 0) {
        rc = YMAGINE_OK;
      }

      /* Clean up compressor */
      jpeg_finish_compress(&cinfoout);
    }
  }

  jpeg_destroy_compress(&cinfoout);
  VbitmapUnlock(vbitmap);

  return rc;
}

YBOOL
verifyJPEG(Ychannel *channel)
{
  unsigned char buf[8];
  int i;
  
  i = YchannelRead(channel, buf, 3);
  if ( (i != 3) || (buf[0] != 0xff) || (buf[1] != 0xd8) || (buf[2] != 0xff) ) {
    return YFALSE;
  }
  
#if 0
  buf[0] = buf[2];
  /* at top of loop: have just read first FF of a marker into buf[0] */
  for (;;) {
    /* get marker type byte, skipping any padding FFs */
    while (buf[0] == (char) 0xff) {
      if (YchannelRead(channel, buf, 1) != 1) {
        return YFALSE;
      }
    }
    
    /* look for SOF0, SOF1, or SOF2, which are the only JPEG variants
     * currently accepted by libjpeg.
     */
    if (buf[0] == '\xc0' || buf[0] == '\xc1' || buf[0] == '\xc2') {
      break;
    }
    
    /* nope, skip the marker parameters */
    if (YchannelRead(channel, buf, 2) != 2) {
      return YFALSE;
    }
    i = ((buf[0] & 0x0ff)<<8) + (buf[1] & 0x0ff) - 1;
    while (i > 256) {
      YchannelRead(channel, buf, 256);
      i -= 256;
    }
    
    if ((i<1) || (YchannelRead(channel, buf, i)) != i) {
      return YFALSE;
    }
    
    buf[0] = buf[i-1];
    /* skip any inter-marker junk (there shouldn't be any, really) */
    while (buf[0] != (char) 0xff) {
      if (YchannelRead(channel, buf, 1) != 1) {
        return YFALSE;
      }
    }
  }
  
  /* Found the SOFn marker, get image dimensions */
  if (YchannelRead(channel, buf, 7) != 7) {
    return YFALSE;
  }
  
  height = ((buf[3] & 0x0ff)<<8) + (buf[4] & 0x0ff);
  width = ((buf[5] & 0x0ff)<<8) + (buf[6] & 0x0ff);
  
  if (width <= 0 || height <= 0) {
    return YFALSE;
  }
#endif
  
  return YTRUE;
}

int
matchJPEG(Ychannel *channel)
{
  unsigned char header[8];
  int hlen;

  if (!YchannelReadable(channel)) {
    return YFALSE;
  }

  hlen = YchannelRead(channel, header, sizeof(header));
  if (hlen > 0) {
    YchannelPush(channel, (const char*) header, hlen);
  }

  if (hlen < 3) {
    return YFALSE;
  }

  if ( (header[0] != 0xff) || (header[1] != 0xd8) || (header[2] != 0xff) ) {
    return YFALSE;
  }

  return YTRUE;
}
