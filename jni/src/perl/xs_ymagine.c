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

#define LOG_TAG "ymagine::simple"

#include "ymagine/ymagine.h"

#ifdef HAVE_YMAGINE_PERL_XS

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

XS(XS_Ymagine_transcode)
{
  const char *infile;
  STRLEN inlen;
  const char *outfile;
  STRLEN outlen;
  int oformat = YMAGINE_IMAGEFORMAT_UNKNOWN;
  int maxwidth = -1;
  int maxheight = -1;
  int scalemode = YMAGINE_SCALE_LETTERBOX;
  int quality = -1;
  int sharpen = 0;
  int subsample = -1;
  int rc = -1;
  int rotate = 0;
  int meta = -1;
  
  dXSARGS;
  if (items < 2) {
    croak("Usage: Ymagine::transcode(infile, outfile, width, height, scalemode, sharpen)");
  }
  
  infile = (char*) SvPV(ST(0), inlen);
  outfile = (char*) SvPV(ST(1), outlen);
  if (items >= 3) {
    oformat = (int) SvIV(ST(2));
  }
  if (items >= 4) {
    maxwidth = (int) SvIV(ST(3));
    if (items >= 5) {
      maxheight = (int) SvIV(ST(4));
    } else {
      maxheight = maxwidth;
    }
  }
  if (items >= 6) {
    scalemode = (int) SvIV(ST(5));
  }
  if (items >= 7) {
    quality = (int) SvIV(ST(6));
  }
  if (items >= 8) {
    sharpen = (int) SvIV(ST(7));
  }
  if (items >= 9) {
    subsample = (int) SvIV(ST(8));
  }
  if (items >= 10) {
    rotate = (int) SvIV(ST(9));
  }
  if (items >= 11) {
    meta = (int) SvIV(ST(10));
  }

  rc = YmagineSNI_Transcode(infile, outfile, oformat,
                            maxwidth, maxheight, scalemode,
                            quality, sharpen, subsample,
                            rotate, meta);

  ST(0) = sv_newmortal();
  sv_setiv(ST(0), (int) rc);

  XSRETURN(1);
}

XS(boot_yahoo_ymaginexs)
{
    dXSARGS;
    char* file = __FILE__;

    XS_VERSION_BOOTCHECK ;

    /* This is just a work around for "unused variable 'items' warning */
    if (items < 0) {
      croak("init failed");
    }

    newXSproto("Ymagine::_xs_transcode", XS_Ymagine_transcode, file, "$$");
    XSRETURN_YES;
}

#endif /* HAVE_YMAGINE_PERL_XS */

