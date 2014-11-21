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

// #define YMAGINE_DEBUG_JPEG 1

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
  char buffer[JMSG_LENGTH_MAX] = {0};
  (*cinfo->err->format_message)(cinfo, buffer);
  ALOGE("jpeg error: %s", buffer);
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
JpegPixelMode(J_COLOR_SPACE colorspace)
{
  int mode = VBITMAP_COLOR_RGBA;

  switch (colorspace) {
    case JCS_GRAYSCALE:
      mode = VBITMAP_COLOR_GRAYSCALE;
      break;
    case JCS_RGB:
      mode = VBITMAP_COLOR_RGB;
      break;
    case JCS_YCbCr:
      mode = VBITMAP_COLOR_YUV;
      break;
    case JCS_YCCK:
    case JCS_CMYK:
      mode = VBITMAP_COLOR_RGBA;
      break;
#ifdef HAVE_JPEG_EXTCOLORSPACE
    case JCS_RGBA_8888:
#endif
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      mode = VBITMAP_COLOR_RGBA;
      break;
    default:
      /* Unsupported color mode */
      break;
  }

  return mode;
}

static int
JpegPixelSize(J_COLOR_SPACE colorspace)
{
  int bpp;
  
  switch (colorspace) {
    case JCS_GRAYSCALE:
      bpp = 1;
      break;
    case JCS_RGB:
      bpp = 3;
      break;
    case JCS_YCbCr:
      bpp = 3;
      break;
    case JCS_YCCK:
    case JCS_CMYK:
      bpp = 4;
      break;
#ifdef HAVE_JPEG_EXTCOLORSPACE
    case JCS_RGBA_8888:
#endif
    case JCS_EXT_RGBX:
    case JCS_EXT_RGBA:
      bpp = 4;
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
    case VBITMAP_COLOR_YUV:
      cinfo->in_color_space = JCS_YCbCr;
      cinfo->input_components = 3;
      break;
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
JpegWriter(Transformer *transformer, void *writedata, void *line)
{
  JSAMPROW row_pointer[1];
  struct jpeg_compress_struct *cinfoout = (struct jpeg_compress_struct *) writedata;
  
  if (writedata != NULL) {
    row_pointer[0] = line;
    jpeg_write_scanlines(cinfoout, row_pointer, 1);
  }

  return YMAGINE_OK;
}

static int
GetScaleNum(int owidth, int oheight,
            int iwidth, int iheight, int scalemode)
{
  int i;

  if (iwidth <= 0 || iheight <= 0) {
    return 0;
  }

  if (owidth <= 0) {
    owidth = iwidth;
  }
  if (oheight <= 0) {
    oheight = iheight;
  }

  if (scalemode == YMAGINE_SCALE_CROP || scalemode == YMAGINE_SCALE_FIT) {
    /* Use higher reduction ratio while keeping generated image larger than output
     one in both direction */
    for (i = 1; i <= 8; i++) {
      if ( ( (iwidth * i) / 8) >= owidth &&
           ( (iheight * i) / 8) >= oheight ) {
        break;
      }
    }
  } else {
    /* For letterbox, use higher reduction ratio while keeping generated image larger
     than output one in at least one direction */
    for (i = 1; i <= 8; i++) {
      if ( ( (iwidth * i) / 8) >= owidth ||
           ( (iheight * i) / 8) >= oheight ) {
        break;
      }
    }
  }

  if (i == 8) {
    /* when i is 8, means no scale */
    i = 0;
  }

  return i;
}

static int
setCompressorOptions(struct jpeg_compress_struct *cinfo,
                     YmagineFormatOptions *options)
{
  int subsampling = -1;

  if (options != NULL) {
    subsampling = options->subsampling;
  }

  /* Set subsampling options */
  switch (subsampling) {
  case 0:
    /* 4:4:4 (no subsampling) */
    cinfo->comp_info[0].h_samp_factor = 1;
    cinfo->comp_info[0].v_samp_factor = 1;
    cinfo->comp_info[1].h_samp_factor = 1;
    cinfo->comp_info[1].v_samp_factor = 1;
    cinfo->comp_info[2].h_samp_factor = 1;
    cinfo->comp_info[2].v_samp_factor = 1;
    break;
  case 1:
    /* 2x2 chrominance subsampling (4:2:0) */
    cinfo->comp_info[0].h_samp_factor = 2;
    cinfo->comp_info[0].v_samp_factor = 2;
    cinfo->comp_info[1].h_samp_factor = 1;
    cinfo->comp_info[1].v_samp_factor = 1;
    cinfo->comp_info[2].h_samp_factor = 1;
    cinfo->comp_info[2].v_samp_factor = 1;
    break;
  default:
    /* Use the lib's default (should be 4:2:0) */
    break;
  }

  return YMAGINE_OK;
}

static YOPTIMIZE_SPEED int
decompress_jpeg(struct jpeg_decompress_struct *cinfo,
                struct jpeg_compress_struct *cinfoout, JCOPY_OPTION copyoption,
                Vbitmap *vbitmap, YmagineFormatOptions *options)
{
  int scanlines;
  int nlines;
  int totallines;
  int j;
  int scalenum = -1;
  JSAMPARRAY buffer;
  Vrect srcrect;
  Vrect destrect;
  size_t row_stride;
  Transformer *transformer;
  PixelShader *shader = NULL;
  int iwidth, iheight;
  float sharpen = 0.0f;

  if (vbitmap == NULL && cinfoout == NULL) {
    /* No output specified */
    return 0;
  }
  if (options == NULL) {
    /* Options argument is mandatory */
    return 0;
  }

  iwidth = cinfo->image_width;
  iheight = cinfo->image_height;

  if (YmaginePrepareTransform(vbitmap, options,
                              iwidth, iheight,
                              &srcrect, &destrect) != YMAGINE_OK) {
    return 0;
  }

  shader = options->pixelshader;
  sharpen = options->sharpen;

  /* Define if image can be pre-subsampled by a ratio n/8 (n=1..7) */
  scalenum = GetScaleNum(destrect.width, destrect.height,
                         srcrect.width, srcrect.height,
                         options->scalemode);
  if (scalenum > 0 && scalenum < 8) {
    cinfo->scale_num = scalenum;
    cinfo->scale_denom = 8;
  }

  /* Compute actual output dimension for image returned by decoder */
  jpeg_calc_output_dimensions(cinfo);

#if YMAGINE_DEBUG_JPEG
  ALOGD("src=%dx%d@%d,%d dst=%dx%d@%d,%d",
        srcrect.width, srcrect.height, srcrect.x, srcrect.y,
        destrect.width, destrect.height, destrect.x, destrect.y);

  ALOGD("size: %dx%d req: %dx%d %s -> scale: %d/%d output: %dx%d components: %d",
        iwidth, iheight,
        destrect.width, destrect.height,
        Ymagine_scaleModeStr(options->scalemode),
        cinfo->scale_num, cinfo->scale_denom,
        cinfo->output_width, cinfo->output_height,
        cinfo->output_components);
#endif

  /* Scale the crop region to reflect scaling ratio applied by JPEG decoder */
  if (cinfo->image_width != cinfo->output_width) {
    srcrect.x = (srcrect.x * cinfo->output_width) / cinfo->image_width;
    srcrect.width = (srcrect.width * cinfo->output_width) / cinfo->image_width;
  }
  if (cinfo->image_height != cinfo->output_height) {
    srcrect.y = (srcrect.y * cinfo->output_height) / cinfo->image_height;
    srcrect.height = (srcrect.height * cinfo->output_height) / cinfo->image_height;
  }

  /* Number of scan lines to handle per pass. Making it larger actually doesn't help much */
  row_stride = cinfo->output_width * cinfo->output_components;
  scanlines = (32 * 1024) / row_stride;
  if (scanlines < 1) {
    scanlines = 1;
  }
  if (scanlines > cinfo->output_height) {
    scanlines = cinfo->output_height;
  }

#if YMAGINE_DEBUG_JPEG
  ALOGD("BITMAP @(%d,%d) %dx%d bpp=%d -> @(%dx%d) %dx%d (%d lines)",
        srcrect.x, srcrect.y, srcrect.width, srcrect.height, JpegPixelSize(cinfo->out_color_space),
        destrect.x, destrect.y, destrect.width, destrect.height,
        scanlines);
#endif

  /* Resize encoder */
  if (cinfoout != NULL) {
    cinfoout->image_width = destrect.width;
    cinfoout->image_height = destrect.height;
    jpeg_start_compress(cinfoout, TRUE);
    if (copyoption != JCOPYOPT_NONE) {
      /* Copy to the output file any extra markers that we want to preserve */
      jcopy_markers_execute(cinfo, cinfoout, copyoption);
    }
  }

  /* Resize target bitmap */
  if (vbitmap != NULL) {
    if (options->resizable) {
      destrect.x = 0;
      destrect.y = 0;
      if (VbitmapResize(vbitmap, destrect.width, destrect.height) != YMAGINE_OK) {
        return 0;
      }
    }
    if (VbitmapType(vbitmap) == VBITMAP_NONE) {
      /* Decode bounds only, return positive number (number of lines) on success */
      return VbitmapHeight(vbitmap);
    }
  }

  if (!jpeg_start_decompress(cinfo)) {
    if (cinfoout != NULL) {
      jpeg_abort_compress(cinfoout);
    }
    return 0;
  }
  
  buffer = (JSAMPARRAY) (*cinfo->mem->alloc_sarray)((j_common_ptr) cinfo, JPOOL_IMAGE,
                                                    row_stride, scanlines);
  if (buffer == NULL) {
    if (cinfoout != NULL) {
      jpeg_abort_compress(cinfoout);
    }
    jpeg_abort_decompress(cinfo);
    return 0;
  }
  
  totallines = 0;
  
  transformer = TransformerCreate();
  if (transformer != NULL) {
    TransformerSetScale(transformer,
                        cinfo->output_width, cinfo->output_height,
                        destrect.width, destrect.height);
    TransformerSetRegion(transformer,
                         srcrect.x, srcrect.y, srcrect.width, srcrect.height);

    if (vbitmap != NULL) {
      TransformerSetMode(transformer, JpegPixelMode(cinfo->out_color_space), VbitmapColormode(vbitmap));
      TransformerSetBitmap(transformer, vbitmap, destrect.x, destrect.y);
    } else {
      TransformerSetMode(transformer, JpegPixelMode(cinfo->out_color_space),
                         JpegPixelMode(cinfoout->in_color_space));
      TransformerSetWriter(transformer, JpegWriter, cinfoout);
    }
    TransformerSetShader(transformer, shader);
    TransformerSetSharpen(transformer, sharpen);
  }

  while (transformer != NULL && cinfo->output_scanline < cinfo->output_height) {
    nlines = jpeg_read_scanlines(cinfo, buffer, scanlines);
    if (nlines <= 0) {
      /* Decoding error */
      ALOGD("decoding error (nlines=%d)", nlines);
      break;
    }

    for (j = 0; j < nlines; j++) {
      if (TransformerPush(transformer, (const char*) buffer[j]) != YMAGINE_OK) {
        TransformerRelease(transformer);
        transformer = NULL;
        break;
      }
      totallines++;
    }
  }

  /* Clean up */
  if (transformer != NULL) {
    TransformerRelease(transformer);
  }
  if (cinfo->output_scanline > 0 && cinfo->output_scanline == cinfo->output_height) {
    /* Do normal cleanup if whole image has been read and decoded */
    jpeg_finish_decompress(cinfo);
    if (cinfoout != NULL) {
      jpeg_finish_compress(cinfoout);
    }
  }
  else {
    /* else early abort */
    jpeg_abort_decompress(cinfo);
    if (cinfoout != NULL) {
      jpeg_abort_compress(cinfoout);
    }
    totallines = 0;
  }
  
  return totallines;
}

static int
prepareDecompressor(struct jpeg_decompress_struct *cinfo, YmagineFormatOptions *options)
{
  int quality = YmagineFormatOptions_normalizeQuality(options);

  if (cinfo == NULL) {
    return YMAGINE_ERROR;
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

  /* Accuracy specified? If so, set the DCT method accordingly.
     It's toggling between JDCT_ISLOW and JDCT_IFAST;
     JDCT_FLOAT is apparently not a good option
     see: http://svn.code.sf.net/p/libjpeg-turbo/code/trunk/libjpeg.txt */
  if (options->accuracy >= 0) {
    if (options->accuracy < 50) {
      cinfo->dct_method = JDCT_IFAST;      
    } else {
      cinfo->dct_method = JDCT_ISLOW;
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
    return YMAGINE_ERROR;
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
    case VBITMAP_COLOR_YUV:
      cinfo->out_color_space = JCS_YCbCr;
      break;
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
              YmagineFormatOptions *options)
{
  int nlines = -1;

  cinfo->client_data = (void*) vbitmap;
  
  if (prepareDecompressor(cinfo, options) != YMAGINE_OK) {
    return nlines;
  }

  /* Intercept APP1 markers for PhotoSphere parsing */
  jpeg_set_marker_processor(cinfo, JPEG_APP0 + 1, APP1_handler);
  
  if (startDecompressor(cinfo, VbitmapColormode(vbitmap)) != YMAGINE_OK) {
    return nlines;
  }
  
  YmagineFormatOptions_invokeCallback(options, YMAGINE_IMAGEFORMAT_JPEG,
                                      cinfo->image_width, cinfo->image_height);

#if YMAGINE_DEBUG_JPEG
  ALOGD("bitmap_decode: in=%dx%d bm=%dx%d max=%dx%d",
        cinfo->image_width, cinfo->image_height,
        VbitmapWidth(vbitmap), VbitmapHeight(vbitmap),
        options->maxwidth, options->maxheight);
#endif
  
  nlines = decompress_jpeg(cinfo, NULL, JCOPYOPT_NONE,
                           vbitmap, options);
  
  return nlines;
}

int
decodeJPEG(Ychannel *channel, Vbitmap *vbitmap,
           YmagineFormatOptions *options)
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
                             options);
    }
  }
  jpeg_destroy_decompress(&cinfo);
  
  return nlines;
}

int
transcodeJPEG(Ychannel *channelin, Ychannel *channelout,
              YmagineFormatOptions *options)
{
  struct jpeg_decompress_struct cinfo;
  struct noop_error_mgr jerr;
  
  struct jpeg_compress_struct cinfoout;
  struct noop_error_mgr jerr2;
  int rc = YMAGINE_ERROR;
  int nlines = 0;
  int quality = YmagineFormatOptions_normalizeQuality(options);
  
  if (!YchannelReadable(channelin) || !YchannelWritable(channelout)) {
    return rc;
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
      if (prepareDecompressor(&cinfo, options) == YMAGINE_OK) {
        /* markers copy option (NONE, COMMENTS or ALL) */
        JCOPY_OPTION copyoption;
        int metamode = YMAGINE_METAMODE_DEFAULT;

        if (options != NULL) {
          metamode = options->metamode;
        }
        if (metamode == YMAGINE_METAMODE_NONE) {
          copyoption = JCOPYOPT_NONE;
        } else if (metamode == YMAGINE_METAMODE_COMMENTS) {
          copyoption = JCOPYOPT_COMMENTS;
        } else if (metamode == YMAGINE_METAMODE_ALL) {
          copyoption = JCOPYOPT_ALL;
        } else {
          /* Default to copy all markers */
          copyoption = JCOPYOPT_ALL;
        }

        /* Enable saving of extra markers that we want to copy */
        if (copyoption != JCOPYOPT_NONE) {
          jcopy_markers_setup(&cinfo, copyoption);
        }
        
        /* Force image to be decoded without colorspace conversion if possible */
        if (jpeg_read_header(&cinfo, TRUE) == JPEG_HEADER_OK) {
          YmagineFormatOptions_invokeCallback(options, YMAGINE_IMAGEFORMAT_JPEG,
                                              cinfo.image_width, cinfo.image_height);

          /* Other compression settings */
          int optimize = 0;
          int progressive = 0;
          int grayscale = 0;
          
          if (quality >= 90) {
            optimize = 1;
          }

          if (options != NULL && options->pixelshader != NULL) {
            /* If a shader is enabled, force RGBA mode */
            cinfo.out_color_space = JCS_EXT_RGBA;
            cinfoout.in_color_space = cinfo.out_color_space;
            cinfoout.input_components = 4;
          } else if (options != NULL && options->sharpen > 0.0f) {
            cinfo.out_color_space = JCS_RGB;
            cinfoout.in_color_space = cinfo.out_color_space;
            cinfoout.input_components = 3;
          } else {
            /* Otherwise sse same colorspace than input image (typically YCbCr) */
            cinfo.out_color_space = cinfo.jpeg_color_space;
            cinfoout.in_color_space = cinfo.out_color_space;
            cinfoout.input_components = JpegPixelSize(cinfoout.in_color_space);
          }

          jpeg_set_defaults(&cinfoout);
          cinfoout.optimize_coding = FALSE;
          
          jpeg_set_quality(&cinfoout, quality, FALSE);
          if (grayscale) {
            /* Force a monochrome JPEG file to be generated. */
            jpeg_set_colorspace(&cinfoout, JCS_GRAYSCALE);
          }
          if (optimize) {
            /* Enable entropy parm optimization. */
            cinfoout.optimize_coding = TRUE;
          }

          /* If non-zero, the input image is smoothed; the value should
             be 1 for minimal smoothing to 100 for maximum smoothing. */
          cinfoout.smoothing_factor = 0;

          if (progressive) {
            /* Select simple progressive mode. */
            jpeg_simple_progression(&cinfoout);
          }
          setCompressorOptions(&cinfoout, options);

          cinfo.client_data = (void*) NULL;
          cinfoout.client_data = (void*) NULL;

          nlines = decompress_jpeg(&cinfo, &cinfoout, copyoption, NULL, options);
          if (nlines > 0) {
            rc = YMAGINE_OK;
          }
        }
      }
    }
  }
  
  jpeg_destroy_compress(&cinfoout);
  jpeg_destroy_decompress(&cinfo);
  
  return rc;
}

int
encodeJPEG(Vbitmap *vbitmap, Ychannel *channelout, YmagineFormatOptions *options)
{
  struct jpeg_compress_struct cinfoout;
  struct noop_error_mgr jerr;
  int result = YMAGINE_ERROR;
  int rc;
  int nlines = 0;
  JSAMPROW row_pointer[1];
  unsigned char *pixels;
  int width;
  int height;
  int pitch;
  int colormode;
  int i;

  if (!YchannelWritable(channelout)) {
    return result;
  }

  if (vbitmap == NULL) {
    return result;
  }

  rc = VbitmapLock(vbitmap);
  if (rc != YMAGINE_OK) {
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
      int optimize = 0;
      int progressive = 0;
      int grayscale = 0;
      int quality = YmagineFormatOptions_normalizeQuality(options);

      if (quality >= 90) {
        optimize = 1;
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
      setCompressorOptions(&cinfoout, options);
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
