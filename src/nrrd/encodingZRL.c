/*
  Teem: Tools to process and visualize scientific data and images
  Copyright (C) 2009--2025  University of Chicago
  Copyright (C) 2005--2008  Gordon Kindlmann
  Copyright (C) 1998--2004  University of Utah

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License (LGPL) as published by the Free Software
  Foundation; either version 2.1 of the License, or (at your option) any later version.
  The terms of redistributing and/or modifying this software also include exceptions to
  the LGPL that facilitate static linking.

  This library is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
  PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public License
  along with this library; if not, see <https://www.gnu.org/licenses/>.
*/

#include "nrrd.h"
#include "privateNrrd.h"

static int
_nrrdEncodingZRL_available(void) {

  return AIR_TRUE;
}

static int
_nrrdEncodingZRL_read(FILE *file, void *data, size_t elementNum, Nrrd *nrrd,
                      NrrdIoState *nio) {
  static const char me[] = "_nrrdEncodingZRL_read";
  /* assume output buffer has been initialized to all zeros */
  unsigned char *output_buffer = (unsigned char *)data;
  size_t toread = elementNum * nrrdElementSize(nrrd);
  unsigned int red = 0; /* how much has been read */
  AIR_UNUSED(nio);

  while (red < toread) {
    int cc = fgetc(file);
    if (cc == 0) {
      int dd = fgetc(file);
      if (dd == 0) {
        /* two zero bytes: next two bytes give (unsigned short) number of zero bytes */
        int lo = fgetc(file);
        if (EOF == lo) {
          biffAddf(NRRD, "%s: after %u, hit EOF at low byte of zero count", me, red);
          return 1;
        }
        int hi = fgetc(file);
        if (EOF == hi) {
          biffAddf(NRRD, "%s: after %u, hit EOF at high byte of zero count", me, red);
          return 1;
        }
        red += AIR_UINT(lo + hi * 256);
      } else {
        /* single zero byte followed by non-zero byte `dd`: `dd` zero bytes */
        red += (unsigned char)dd;
      }
    } else {
      /* non-zero bytes are copied one by one */
      output_buffer[red] = (unsigned char)cc;
      red++;
    }
  }

  return 0;
}

static int /* Biff: 0 */
_nrrdEncodingZRL_write(FILE *file, const void *data, size_t elementNum, const Nrrd *nrrd,
                       NrrdIoState *nio) {
  static const char me[] = "_nrrdEncodingZRL_write";

  AIR_UNUSED(file);
  AIR_UNUSED(data);
  AIR_UNUSED(elementNum);
  AIR_UNUSED(nrrd);
  AIR_UNUSED(nio);
  biffAddf(NRRD, "%s: sorry, currently a read-only encoding", me);

  return 0;
}

const NrrdEncoding _nrrdEncodingZRL
  = {"zrl",     /* name */
     "zrl",     /* suffix */
     AIR_TRUE,  /* endianMatters */
     AIR_FALSE, /* isCompression: HEY this is a hack: this IS certainly a
                   compression. However, with compressed encodings the nrrd
                   format has no way of specifying whether a byteskip
                   between be outside the encoding (in the uncompressed
                   data) vs inside the encoding (within the compuressed
                   data).  To date the convention has been that byte skip is
                   done *inside* compressions, but for the ZRL-encoded data
                   as currently generated, the relevant byte skipping is
                   certainly *outside* the compression.  Thus we claim
                   ignorance about how ZRL is a compression, so that byte
                   skipping can be used. */
     _nrrdEncodingZRL_available,
     _nrrdEncodingZRL_read,
     _nrrdEncodingZRL_write};

const NrrdEncoding *const nrrdEncodingZRL = &_nrrdEncodingZRL;
