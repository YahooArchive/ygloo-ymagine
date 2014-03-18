/*
 * Generic JPEG I/O adapter on top of Ychannel
 * Inherits support for memory, file and JAVA stream from the
 * supported Ychannel backends
 */

/*
 * This implementation is based on jdatasrc.c in libjpeg library:
 *
 * This file was part of the Independent JPEG Group's software:
 * Copyright (C) 1994-1996, Thomas G. Lane.
 * Modified 2009-2011 by Guido Vollbeding.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains decompression data source routines for the case of
 * reading JPEG data from memory or from a file (or any stdio stream).
 * While these routines are sufficient for most applications,
 * some will want to use a different source manager.
 * IMPORTANT: we assume that fread() will correctly transcribe an array of
 * JOCTETs from 8-bit-wide elements on external storage.  If char is wider
 * than 8 bits on your machine, you may need to do some tweaking.
 */

#include "ymagine_priv.h"

#include "formats/jpeg/jpegio.h"

#include "jinclude.h"
#include "jpeglib.h"
#include "jerror.h"

/* Expanded data source object for stdio and stream input */
typedef struct {
  /* Public fields */
  struct jpeg_source_mgr pub;

  /* Private fields */
  Ychannel *channel;

  /* Flags (for now, only start of file) */
  int start_of_file;
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

/*
 * Initialize source --- called by jpeg_read_header
 * before any data is actually read.
 */

static void
init_source (j_decompress_ptr cinfo)
{
  my_src_ptr src = (my_src_ptr) cinfo->src;

  /* We reset the empty-input-file flag for each image,
   * but we don't clear the input buffer.
   * This is correct behavior for reading a series of images from one source.
   */
  src->start_of_file = 1;
}

/*
 * Fill the input buffer --- called whenever buffer is emptied.
 *
 * In typical applications, this should read fresh data into the buffer
 * (ignoring the current state of next_input_byte & bytes_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been reloaded.  It is not necessary to
 * fill the buffer entirely, only to obtain at least one more byte.
 *
 * There is no such thing as an EOF return.  If the end of the file has been
 * reached, the routine has a choice of ERREXIT() or inserting fake data into
 * the buffer.  In most cases, generating a warning message and inserting a
 * fake EOI marker is the best course of action --- this will allow the
 * decompressor to output however much of the image is there.  However,
 * the resulting error message is misleading if the real problem is an empty
 * input file, so we handle that case specially.
 *
 * In applications that need to be able to suspend compression due to input
 * not being available yet, a FALSE return indicates that no more data can be
 * obtained right now, but more may be forthcoming later.  In this situation,
 * the decompressor will return to its caller (with an indication of the
 * number of scanlines it has read, if any).  The application should resume
 * decompression after it has loaded more data into the input buffer.  Note
 * that there are substantial restrictions on the use of suspension --- see
 * the documentation.
 *
 * When suspending, the decompressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_input_byte & bytes_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point must be rescanned after resumption, so move it to
 * the front of the buffer rather than discarding it.
 */

static const JOCTET EOI_buffer[2] = {
  (JOCTET) 0xFF,
  (JOCTET) JPEG_EOI
};

static boolean
fill_input_buffer(j_decompress_ptr cinfo)
{
  const char *buffer;
  int nbytes = 0;
  my_src_ptr src = (my_src_ptr) cinfo->src;

  // nbytes = YchannelRead(src->infile, src->buffer, INPUT_BUF_SIZE);
  buffer = YchannelFetch(src->channel, 32*1024, &nbytes);

  if (buffer == NULL || nbytes <= 0) {
    if (src->start_of_file) {
      /* Treat empty input file as fatal error */
      ERREXIT(cinfo, JERR_INPUT_EMPTY);
    }
    WARNMS(cinfo, JWRN_JPEG_EOF);
    /* Insert a fake EOI marker */
    src->pub.next_input_byte = EOI_buffer;
    src->pub.bytes_in_buffer = sizeof(EOI_buffer);
  } else {
    src->pub.next_input_byte = (const unsigned char*) buffer;
    src->pub.bytes_in_buffer = (size_t) nbytes;
    src->start_of_file = 0;
  }

  return TRUE;
}

/*
 * Skip data --- used to skip over a potentially large amount of
 * uninteresting data (such as an APPn marker).
 *
 * Writers of suspendable-input applications must note that skip_input_data
 * is not granted the right to give a suspension return.  If the skip extends
 * beyond the data currently in the buffer, the buffer can be marked empty so
 * that the next read will cause a fill_stdio_input_buffer call that can suspend.
 * Arranging for additional bytes to be discarded before reloading the input
 * buffer is the application writer's problem.
 */

static void
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
  struct jpeg_source_mgr * src = cinfo->src;

  /* Just a dumb implementation for now.  Could use fseek() except
   * it doesn't work on pipes.  Not clear that being smart is worth
   * any trouble anyway --- large skips are infrequent.
   */
  if (num_bytes > 0) {
    while (num_bytes > (long) src->bytes_in_buffer) {
	    num_bytes -= (long) src->bytes_in_buffer;
	    (void) (*src->fill_input_buffer) (cinfo);
	    /* note we assume that fill_input_buffer will never return FALSE,
	     * so suspension need not be handled.
	     */
    }
    src->next_input_byte += (size_t) num_bytes;
    src->bytes_in_buffer -= (size_t) num_bytes;
  }
}


/*
 * An additional method that can be provided by data source modules is the
 * resync_to_restart method for error recovery in the presence of RST markers.
 * For the moment, this source module just uses the default resync method
 * provided by the JPEG library.  That method assumes that no backtracking
 * is possible.
 */


/*
 * Terminate source --- called by jpeg_finish_decompress
 * after all data has been read.  Often a no-op.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

static void
term_source (j_decompress_ptr cinfo)
{
}

/*
 * Prepare for input from a channel.
 * The caller must have already opened the stream, and is responsible
 * for closing it after finishing decompression.
 */

int
ymaginejpeg_input(j_decompress_ptr cinfo, Ychannel *channel)
{
  my_src_ptr src;

  if (!YchannelReadable(channel)) {
    return YMAGINE_ERROR;
  }

  /* The source object and input buffer are made permanent so that a series
   * of JPEG images can be read from the same file by calling jpeg_stdio_src
   * only before the first one.  (If we discarded the buffer at the end of
   * one image, we'd likely lose the start of the next one.)
   * This makes it unsafe to use this manager and a different source
   * manager serially with the same JPEG object.  Caveat programmer.
   */
  if (cinfo->src == NULL) {	/* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                SIZEOF(my_source_mgr));
  }

  src = (my_src_ptr) cinfo->src;

  src->channel = channel;

  src->pub.init_source = init_source;
  src->pub.fill_input_buffer = fill_input_buffer;
  src->pub.skip_input_data = skip_input_data;
  src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
  src->pub.term_source = term_source;
  src->pub.bytes_in_buffer = 0; /* forces fill_input_buffer on first read */
  src->pub.next_input_byte = NULL; /* until buffer loaded */

  return YMAGINE_OK;
}

/*
 libjpeg destination manager for writing to Ychannel
 */

#define STRING_BUF_SIZE  (16*1024)

/* Expanded data source object for stdio and stream input */
typedef struct {
  /* Public fields */
  struct jpeg_destination_mgr pub;

  /* Private fields */
  Ychannel *channel;
  JOCTET buffer[STRING_BUF_SIZE];
} my_destination_mgr;

typedef my_destination_mgr * my_dest_ptr;

static void
init_destination (j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = STRING_BUF_SIZE;
}

static boolean
empty_output_buffer (j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

  if (YchannelWrite(dest->channel,
                    dest->buffer,
                    STRING_BUF_SIZE) != STRING_BUF_SIZE) {
    ERREXIT(cinfo, JERR_FILE_WRITE);
  }

  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = STRING_BUF_SIZE;

  return TRUE;
}

static void
term_destination (j_compress_ptr cinfo)
{
  my_dest_ptr dest = (my_dest_ptr) cinfo->dest;
  size_t datacount = STRING_BUF_SIZE - dest->pub.free_in_buffer;

  if (datacount>0) {
    if (YchannelWrite(dest->channel, dest->buffer, datacount) != datacount) {
	    ERREXIT(cinfo, JERR_FILE_WRITE);
    }
  }

  YchannelFlush(dest->channel);
}

int
ymaginejpeg_output(j_compress_ptr cinfo, Ychannel *channel)
{
  my_dest_ptr dest;

  if (!YchannelWritable(channel)) {
    return YMAGINE_ERROR;
  }

  if (cinfo->dest == NULL) {    /* first time for this JPEG object? */
    cinfo->dest = (struct jpeg_destination_mgr *)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                SIZEOF(my_destination_mgr));
  }

  dest = (my_dest_ptr) cinfo->dest;

  dest->channel = channel;

  dest->pub.init_destination = init_destination;
  dest->pub.empty_output_buffer = empty_output_buffer;
  dest->pub.term_destination = term_destination;

  return YMAGINE_OK;
}
