/*
 * This implementation of GIF decoder is partially based on GIF decoder
 * in tkIMG:
 *
 * gif.c --
 *
 * Copyright (c) 2002 Andreas Kupries    <andreas_kupries@users.sourceforge.net>
 * Copyright (c) 1997-2003 Jan Nijtmans  <nijtmans@users.sourceforge.net>
 *
 * Copyright (c) Reed Wade (wade@cs.utk.edu), University of Tennessee
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 * Copyright (c) 1997 Australian National University
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * This file also contains code from the giftoppm program, which is
 * copyrighted as follows:
 *
 * +-------------------------------------------------------------------+
 * | Copyright 1990, David Koblas.                                     |
 * |   Permission to use, copy, modify, and distribute this software   |
 * |   and its documentation for any purpose and without fee is hereby |
 * |   granted, provided that the above copyright notice appear in all |
 * |   copies and that both that copyright notice and this permission  |
 * |   notice appear in supporting documentation.  This software is    |
 * |   provided "as is" without express or implied warranty.           |
 * +-------------------------------------------------------------------+
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#define LOG_TAG "ymagine::gif"
#include "yosal/yosal.h"
#include "ymagine/ymagine.h"
#include "ymagine_priv.h"

#include "graphics/bitmap.h"

#define GIF_HEADER_SIZE 10
#define LM_to_uint(a,b) (((b)<<8)|(a))

static int
GifCheckHeader(const char *header, int buflen,int *widthPtr, int *heightPtr)
{
  const unsigned char *buf = (const unsigned char*) header;

  if (buf == NULL || buflen < GIF_HEADER_SIZE) {
    return 0;
  }

  /* Check magic (GIF87a or GIF89a) */
  if (buf[0]!='G' || 
      buf[1]!='I' ||
      buf[2]!='F' ||
      buf[3]!='8' ||
      (buf[4]!='7' && buf[4]!='9') ||
      buf[5]!='a') {
    /* Invalid magic */
    return 0;
  }

  if (widthPtr) {
    *widthPtr = LM_to_uint(buf[6],buf[7]);
  }
  if (heightPtr) {
    *heightPtr = LM_to_uint(buf[8],buf[9]);
  }

  return 1;
}

#if HAVE_GIF
static int
ReadGIFHeader(Ychannel *f, int *widthPtr, int *heightPtr)
{
  char header[GIF_HEADER_SIZE];
  int headerlen;

  /* Read header */
  headerlen = YchannelRead(f, (char *) header, GIF_HEADER_SIZE);
  if (headerlen != GIF_HEADER_SIZE) {
    return 0;
  }

  return GifCheckHeader(header, headerlen, widthPtr, heightPtr);
}

/*
 GIF images accept not more than 256 colors
*/
#define MAXCOLORS		256

#define INTERLACE		0x40
#define LOCALCOLORMAP		0x80

#define BitSet(byte, bit)	(((byte) & (bit)) == (bit))

#define GIFBITS                 12
#define GIFTBLSZ                (1<<GIFBITS)

#if 1
#define GIFDEBUG(x)  ((void) 0)
#else
#define GIFDEBUG(x) LOGD x
#endif

typedef struct {
  Ychannel *fd;

  int ZeroDataBlock;
  int fresh;
  int code_size, set_code_size;
  int max_code, max_code_size;
  int firstcode, oldcode;
  int clear_code, end_code;
  int table[2][GIFTBLSZ];
  int stack[2*GIFTBLSZ];
  int *sp;

  unsigned char codebuf[280];
  int curbit;
  int lastbit;
  int done;
  int last_byte;

  unsigned char buf[256];
} GifReadHandle;

/*
 Local procedures for decompression
*/
static int DoExtension (GifReadHandle *gifr, int label,int *transparent);
static int GetCode (GifReadHandle *gifr, int code_size);
static int GetDataBlock (GifReadHandle *gifr,unsigned char *buf);
static void LWZInit(GifReadHandle *gifr,int code_size);
static int LWZReadByte (GifReadHandle *gifr, int flag,int input_code_size);

/*
 General utilities for reading GIF
*/
static int
ReadColorMap(Ychannel *fd, int number, unsigned char *colormap)
{
  int i;
  unsigned char rgb[3];
  
  if (colormap!=NULL) {
    /* PixResizeColormap(colormap,number); */
  }

  for (i = 0; i < number; ++i) {
    if (YchannelRead(fd, (char*) rgb, sizeof(rgb)) != sizeof(rgb)) {
      break;
    }
    if (colormap!=NULL) {
      colormap[3*i]=rgb[0];
      colormap[3*i+1]=rgb[1];
      colormap[3*i+2]=rgb[2];
    }
  }

  /* Return number of colors found */
  return i;
}

/*
 Lecture
*/
static int
GIFDecode(Ychannel *fd, Vbitmap *vbitmap, YmagineFormatOptions *options,
          int maxWidth, int maxHeight, int scaleMode, int quality)
{  
  unsigned char *colormap;
  int nbcolors;

  unsigned char *localcmap;
  int nblocalcmap;

  unsigned char *framecmap;
  int nbframecmap;

  unsigned char buf[12];
  unsigned int colorResolution;
  unsigned int background;
  unsigned int aspectRatio;
  int transparent;
  int interlace;
  
  int xpos,ypos;
  int index,cpt;
  
  int framePosx,framePosy;
  int frameWidth,frameHeight;
  int width,height;

  unsigned char c;
  int pass;

  int i,j;
  unsigned char *data;
  int v;

  GifReadHandle *gifr;

  int oformat;
  int opitch;
  int obpp;
  unsigned char *odata;  

  int rc;

  /* Read header */
  if (!ReadGIFHeader(fd, &width, &height)) {
    GIFDEBUG(("failed to read header\n"));
    return 0;
  }

  /* Incorrect dimension */
  if ((width <= 0) || (height <= 0)) {
    GIFDEBUG(("null geometry\n"));
    return 0;
  }

  /* Image information */
  if (YchannelRead(fd,(char*) buf, 3) != 3) {
    GIFDEBUG(("failed to read image header\n"));
    return 0;
  }

  colorResolution = ((((unsigned int) buf[0]&0x70)>>3)+1);
  aspectRatio = 0;
  if (buf[2] != 0) {
    aspectRatio = (buf[2] + 15) / 64;
  }
    
  if (YmagineFormatOptions_invokeCallback(options, YMAGINE_IMAGEFORMAT_GIF,
                                          width, height) != YMAGINE_OK) {
    return 0;
  }

  /* Create image */
  if (VbitmapResize(vbitmap, width, height) != YMAGINE_OK) {
    return 0;
  }
  if (VbitmapType(vbitmap) == VBITMAP_NONE) {
    return height;
  }

  /* GIF read structure is also allocated in heap, since structure is pretty 
     large (more than 64kB) but stack is limited, especially on embedded systems, 
     e.g. Symbian S60 */
  gifr = (GifReadHandle*) Ymem_malloc(sizeof(GifReadHandle));
  if (gifr == NULL) {
    return 0;
  }

  /* Initialize global and local colormap pointers as soon as possible
     They need to be set to NULL before potential jump to readerror */
  colormap=NULL;
  nbcolors=0;

  localcmap=NULL;
  nblocalcmap=0;

  /* Transparency index */
  transparent = -1;

  /* Which frame to read (0 = first, -1 to decode until last frame) */
  index = 0;

  if (BitSet(buf[0], LOCALCOLORMAP)) {
    /* Image has a Global colormap (and background info is meaningful) */
    nbcolors = 2 << (buf[0]&0x07);
    background = buf[1];

    colormap=Ymem_malloc(3*nbcolors);
    if (colormap==NULL) {
      GIFDEBUG(("failed to allocate memory for global colormap\n"));      
      goto readerror;
    }
    if (ReadColorMap(fd,nbcolors,colormap)!=nbcolors) {
      GIFDEBUG(("failed to read colormap\n"));
      goto readerror;
    }
  } else {
    nbcolors = 0;
    background = 0;
  }
    
  /* Initialize read structure */
  memset(gifr, 0, sizeof(GifReadHandle));

  gifr->fd=fd;
  gifr->ZeroDataBlock=0;
  gifr->fresh=0;

  /* Scan frames */
  cpt=0;
  while (1) {
    if (YchannelRead(fd,(char*) buf, 1) != 1) {
      /* Premature end of image.  We should really notify
	 the user, but for now just show garbage. */
      break;
    }

    if (buf[0] == ';') {
      /* GIF terminator */
      break;
    }

    if (buf[0] == '!') {
      /* GIF extension */
      if (YchannelRead(fd, (char*) buf, 1) != 1) {
	GIFDEBUG(("error reading extension function code"));
	goto readerror;
      }
      if (DoExtension(gifr, buf[0], &transparent) < 0) {
	GIFDEBUG(("invalid extension function code\n"));
	goto readerror;
      }
      GIFDEBUG(("transparent=%d\n",transparent));
      continue;
    }
    
    /* Not a valid start character; ignore it. */
    if (buf[0] != ',') {
      continue;
    }

    if (YchannelRead(fd, (char*) buf, 9) != 9) {
      GIFDEBUG(("invalid geometry for frame\n"));
      goto readerror;
    }

    framePosx = LM_to_uint(buf[0],buf[1]);
    framePosy = LM_to_uint(buf[2],buf[3]);
    frameWidth = LM_to_uint(buf[4],buf[5]);
    frameHeight = LM_to_uint(buf[6],buf[7]);

    printf("Frame %d: @%d,%d %dx%d\n", cpt, framePosx, framePosy, frameWidth, frameHeight);

    interlace = BitSet(buf[8], INTERLACE);

    /* Local colormap, associated with the graphic that immediately 
       follows it. Global colormap must be saved for future frames */
    if (localcmap!=NULL) {
      Ymem_free(localcmap);
      localcmap=NULL;
    }
    
    if (BitSet(buf[8], LOCALCOLORMAP)) {
      nblocalcmap = 2<<(buf[8]&0x07);
      if (index >= 0 && cpt != index) {
	localcmap=NULL;
      } else {
	localcmap=Ymem_malloc(3*nblocalcmap);
      }
      if (ReadColorMap(fd,nblocalcmap,localcmap)!=nblocalcmap) {
	GIFDEBUG(("invalid frame colormap\n"));
	goto readerror;
      }
    } else {
      /* No local colormap, use global one */
      nblocalcmap=0;
    }

    if (frameWidth<=0 || frameHeight<=0) {
      /* Invalid empty frame */
      GIFDEBUG(("invalid empty frame\n"));
      goto readerror;
    }

    if (localcmap==NULL && colormap==NULL) {
      /* Have neither a local cmap nor a global one */
      GIFDEBUG(("Frame has no colormap\n"));
      goto readerror;
    }

    if (localcmap!=NULL) {
      framecmap=localcmap;
      nbframecmap=nblocalcmap;
    } else {
      framecmap=colormap;
      nbframecmap=nbcolors;
    }

    GIFDEBUG(("frame #%d: %dx%d @ (%d,%d) interlace=%d cmap=%d\n",
	      cpt,frameWidth,frameHeight,framePosx,framePosy,
	      interlace,nbframecmap));
    
    /* Initialize decompression routines */
    if (YchannelRead(fd,(char*) &c,1)!=1) {
      GIFDEBUG(("failed to initialize decoder\n"));
      goto readerror;
    }

    LWZInit(gifr,c);
    if (index >= 0 && cpt != index) {
      /* Wrong frame. Extract bytes fast */
      for (j=0;j<frameHeight;j++) {
	for (i=0;i<frameWidth;i++) {
	  if (LWZReadByte(gifr,0,c)<0) {
	    GIFDEBUG(("failed to read byte in dropped frame\n"));
	    goto readerror;
	  }
	}
      }
    } else {
      /* Correct frame. Read image line by line */
      xpos=0;
      ypos=0;
      pass=0;
      
      rc = VbitmapLock(vbitmap);
      if (rc != YMAGINE_OK) {
        GIFDEBUG(("VbitmapLock() failed (code %d)", rc));
        goto readerror;
      }

      odata = VbitmapBuffer(vbitmap);
      opitch = VbitmapPitch(vbitmap);
      oformat = VbitmapColormode(vbitmap);
      obpp = VbitmapBpp(vbitmap);

      if (odata == NULL) {
        ALOGD("failed to get reference to pixel buffer");
        rc = YMAGINE_ERROR;
      }

      /* Allocate temporary buffer for one line */
      data=Ymem_malloc(4*frameWidth);
      while ((v = LWZReadByte(gifr,0,c)) >= 0 ) {
	if (v>=nbframecmap) {
	  /* Pixel index out of range */
	  GIFDEBUG(("pixel index out of range (%d>=%d)\n",v,nbframecmap));
	  goto readerror;
	}

        data[4*xpos]  = framecmap[3*v];
        data[4*xpos+1]= framecmap[3*v+1];
        data[4*xpos+2]= framecmap[3*v+2];
        if (transparent>=0 && v==transparent) {
          data[4*xpos+3]=0x00;
        } else {
          data[4*xpos+3]=0xff;
        }
	xpos++;
	
	if (xpos == frameWidth) {
	  /* Copy scanline into image handle */
          if (odata != NULL) {
            bltLine(odata + (framePosy + ypos) * opitch + framePosx * obpp, frameWidth, oformat,
                    data, frameWidth, VBITMAP_COLOR_RGBA);
          }
	  
	  /* Jump to next line */
	  xpos = 0;	  
	  if (interlace) {
	    switch (pass) {
	    case 0:
	    case 1:
	      ypos += 8; break;
	    case 2:
	      ypos += 4; break;
	    case 3:
	      ypos += 2; break;
	    }
	    
	    while (ypos >= frameHeight) {
	      ++pass;
	      if (pass==1) {
		ypos = 4;
	      } else if (pass==2) {
		ypos = 2;
	      } else if (pass==3) {
		ypos = 1;
	      } else {
		break;
	      }
	    }
	  } 
	  else {
	    ypos++;
	  }
	}
	
	/* End of frame */
	if (ypos>=frameHeight) {
	  break;
	}
      }
      Ymem_free(data);
      VbitmapUnlock(vbitmap);

      if (v<0) {
	GIFDEBUG(("failed to read byte in valid frame\n"));
	goto readerror;
      }

      /* Got requested index, early abort */
      if (index >= 0 && cpt == index) {
        break;
      }
    }

    if (localcmap!=NULL) {
      Ymem_free(localcmap);
      localcmap=NULL;
    }

    /* Got frame */
    cpt++;
  }
  
  if (colormap != NULL) {
    Ymem_free(colormap);
    colormap = NULL;
  }

  return height;

readerror:
  if (gifr!=NULL) {
    Ymem_free(gifr);
    gifr=NULL;
  }
  if (colormap!=NULL) {
    Ymem_free(colormap);
    colormap=NULL;
  }
  if (localcmap!=NULL) {
    Ymem_free(localcmap);
    localcmap=NULL;
  }
  return 0;
}

/* Decompression */
static int
DoExtension(GifReadHandle *gifr, int label, int *transparent)
{
  int count = 0;

  switch (label) {
  case 0x01:      /* Plain Text Extension */
    break;
    
  case 0xff:      /* Application Extension */
    break;
    
  case 0xfe:      /* Comment Extension */
    do {
      count = GetDataBlock(gifr, (unsigned char*) gifr->buf);
    } while (count > 0);
    return count;
    
  case 0xf9:      /* Graphic Control Extension */
    count = GetDataBlock(gifr, (unsigned char*) gifr->buf);
    if (count < 0) {
      return 1;
    }
    if ((gifr->buf[0] & 0x1) != 0) {      
      if (transparent!=NULL) {
	*transparent = gifr->buf[3];
      }
    }
    
    do {
      count = GetDataBlock(gifr, (unsigned char*) gifr->buf);
    } while (count > 0);
    return count;
  }
  
  do {
    count = GetDataBlock(gifr, (unsigned char*) gifr->buf);
  } while (count > 0);

  return count;
}

/* Read a block and copy content into buf. Buffer must be at least 256 bytes 
   large to prevent any possible buffer overflow */
static int
GetDataBlock(GifReadHandle *gifr, unsigned char *buf)
{
  unsigned char count;

  if (YchannelRead(gifr->fd,(char*) &count,1)!=1) {
    return -1;
  }
  
  gifr->ZeroDataBlock = (count == 0);
  if (count != 0) {
    /* Read block (up to 255 characteres, since count is unsigned char */
    if (YchannelRead(gifr->fd, (char*) buf, count)!=count) {
      /* Failed to read block */
      return -1;
    }
  }
  
  return count;
}

static void
LWZInit(GifReadHandle *gifr, int code_size)
{
  int i;

  gifr->fresh=0;
  gifr->set_code_size = code_size;
  gifr->code_size = gifr->set_code_size+1;
  gifr->clear_code = 1 << gifr->set_code_size ;
  gifr->end_code = gifr->clear_code + 1;
  gifr->max_code_size = 2*gifr->clear_code;
  gifr->max_code = gifr->clear_code+2;
    
  gifr->curbit = 0;
  gifr->lastbit = 0;
  gifr->done = 0;
  gifr->last_byte=0;
    
  gifr->fresh = 1;
    
  for (i = 0; i < gifr->clear_code; ++i) {
    gifr->table[0][i] = 0;
    gifr->table[1][i] = i;
  }
  for (; i < (1<<GIFBITS); ++i) {
    gifr->table[0][i] = gifr->table[1][0] = 0;
  }
  
  gifr->sp = gifr->stack;

  return;
}

static YINLINE YOPTIMIZE_SPEED int
LWZReadByte(GifReadHandle *gifr, int flag, int input_code_size)
{
  int code, incode;
  int i;

  if (gifr->fresh) {    
    gifr->fresh = 0;
    do {
      gifr->firstcode = gifr->oldcode = GetCode(gifr, gifr->code_size);
    } while (gifr->firstcode == gifr->clear_code);

    return gifr->firstcode;
  }
  
  if (gifr->sp > gifr->stack) {
    code=*--gifr->sp;
    return code;
  }
  
  while ((code = GetCode(gifr, gifr->code_size)) >= 0) {
    if (code == gifr->clear_code) {
      for (i = 0; i < gifr->clear_code; ++i) {
	gifr->table[0][i] = 0;
	gifr->table[1][i] = i;
      }
      
      for (; i < GIFTBLSZ; ++i) {
	gifr->table[0][i] = gifr->table[1][i] = 0;
      }
      
      gifr->code_size = gifr->set_code_size+1;
      gifr->max_code_size = 2*gifr->clear_code;
      gifr->max_code = gifr->clear_code+2;
      gifr->sp = gifr->stack;
      gifr->firstcode = gifr->oldcode = GetCode(gifr, gifr->code_size);

      return gifr->firstcode;      
    }

    if (code == gifr->end_code) {
      int     count;
      unsigned char   buf[260];
      
      if (gifr->ZeroDataBlock) {
	return -2;
      }      
      while ((count = GetDataBlock(gifr, buf)) > 0) {
      }
      if (count != 0) {
	return -2;
      }
    }
    
    incode = code;
    
    if (code >= gifr->max_code) {
      *gifr->sp++ = gifr->firstcode;
      code = gifr->oldcode;
    }
    
    /* Check code bounds */
    if (code>=GIFTBLSZ) {
      return -1;
    }

    while (code >= gifr->clear_code) {
      *gifr->sp++ = gifr->table[1][code];
      /* Circular table entry */
      if (code == gifr->table[0][code]) {
	return -2;
      }
      code = gifr->table[0][code];
      
      /* Check code bounds */
      if (code>=GIFTBLSZ) {
	return -1;
      }
    }
    
    *gifr->sp++ = gifr->firstcode = gifr->table[1][code];
    
    if ((code = gifr->max_code) <(1<<GIFBITS)) {
      
      gifr->table[0][code] = gifr->oldcode;
      gifr->table[1][code] = gifr->firstcode;
      ++gifr->max_code;
      if ((gifr->max_code>=gifr->max_code_size) && 
	  (gifr->max_code_size < (1<<GIFBITS))) {
	gifr->max_code_size *= 2;
	++gifr->code_size;
      }
    }
    
    gifr->oldcode = incode;
    
    if (gifr->sp > gifr->stack) {
      code=*--gifr->sp;
      return code;
    }
  }

  return code;
}


static int
GetCode(GifReadHandle *gifr, int code_size)
{
  int i, j, ret;
  unsigned char count;
  
  if ( (gifr->curbit+code_size) >= gifr->lastbit) {
    if (gifr->done) {
      return -1;
    }
    gifr->codebuf[0] = gifr->codebuf[gifr->last_byte-2];
    gifr->codebuf[1] = gifr->codebuf[gifr->last_byte-1];
    
    if ((count = GetDataBlock(gifr,gifr->codebuf+2)) == 0) {
      gifr->done = 1;
    }
    
    gifr->last_byte = 2 + count;
    gifr->curbit = (gifr->curbit - gifr->lastbit) + 16;
    gifr->lastbit = (2+count)*8 ;
  }
  
  ret = 0;
  for (i = gifr->curbit, j = 0; j < code_size; ++i, ++j) {
    ret |= ((gifr->codebuf[ i / 8 ] & (1 << (i % 8))) != 0) << j;
  }
    
  gifr->curbit += code_size;
  
  return ret;
}

#endif /* HAVE_GIF */

int
decodeGIF(Ychannel *channel, Vbitmap *vbitmap,
          YmagineFormatOptions *options)
{
  int nlines = -1;
  
  if (!YchannelReadable(channel)) {
#if YMAGINE_DEBUG_GIF
    ALOGD("input channel not readable");
#endif
    return nlines;
  }

#if HAVE_GIF
  nlines = GIFDecode(channel, vbitmap, options,
                     maxWidth, maxHeight, scaleMode, quality);
#endif

  return nlines;
}

int
encodeGIF(Vbitmap *vbitmap, Ychannel *channelout, int quality)
{
  int rc = YMAGINE_ERROR;

  return rc;
}

int
matchGIF(Ychannel *channel)
{
  char header[GIF_HEADER_SIZE];
  int hlen;

  if (!YchannelReadable(channel)) {
    return YFALSE;
  }

  hlen = YchannelRead(channel, header, sizeof(header));
  if (hlen > 0) {
    YchannelPush(channel, header, hlen);
  }

  if (GifCheckHeader(header, hlen, NULL, NULL) <= 0) {
    return YFALSE;
  }

  return YTRUE;
}
