/*
  Teem: Tools to process and visualize scientific data and images
  Copyright (C) 2009--2026  University of Chicago
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

/*
** what a NrrdFormat can assume:
** -- that nio->format has been set to you already
** -- for read(): that nio->path has been set to the path of the file being
**    read in, if the information was ever available
** -- for contentStartsLike() and read(): that nio->line contains the
**    first line of of the file, in order to determine the file type
**
** what a NrrdFormat has to do:
** -- respect nio->skipData to whatever extent makes sense on top of how the
**    NrrdEncoding respects it (by making read and write no-ops).
**    nrrdFormatNRRD, for instance, won't create empty detached data files
**    if nio->skipData.
** -- determine what NrrdEncoding to use, if there's a choice
** -- respect nrrdStateVerboseIO with messages to stderr, if possible
**
** The "unknown" format is intended as a template for writing new formats.
*/

static int
formatUnknown_available(void) {

  /* insert code here */

  return AIR_FALSE;
}

static int
formatUnknown_nameLooksLike(const char *filename) {

  /* insert code here */
  AIR_UNUSED(filename);

  return AIR_FALSE;
}

static int /* Biff: maybe:3:AIR_FALSE */
formatUnknown_fitsInto(const Nrrd *nrrd, const NrrdEncoding *encoding, int useBiff) {
  static const char me[] = "formatUnknown_fitsInto";

  if (!(nrrd && encoding)) {
    biffMaybeAddf(useBiff, NRRD, "%s: got NULL nrrd (%p) or encoding (%p)", me,
                  AIR_CVOIDP(nrrd), AIR_CVOIDP(encoding));
    return AIR_FALSE;
  }

  /* insert code here */

  return AIR_FALSE;
}

static int
formatUnknown_contentStartsLike(NrrdIoState *nio) {

  /* insert code here */
  AIR_UNUSED(nio);

  return AIR_FALSE;
}

static int /* Biff: 1 */
formatUnknown_read(FILE *file, Nrrd *nrrd, NrrdIoState *nio) {
  static const char me[] = "formatUnknown_read";

  /* insert code here, and remove error handling below */
  AIR_UNUSED(file);
  AIR_UNUSED(nrrd);
  AIR_UNUSED(nio);

  biffAddf(NRRD, "%s: ERROR!!! trying to read unknown format", me);
  return 1;
}

static int /* Biff: 1 */
formatUnknown_write(FILE *file, const Nrrd *nrrd, NrrdIoState *nio) {
  static const char me[] = "formatUnknown_write";

  /* insert code here, and remove error handling below */
  AIR_UNUSED(file);
  AIR_UNUSED(nrrd);
  AIR_UNUSED(nio);

  biffAddf(NRRD, "%s: ERROR!!! trying to write unknown format", me);
  return 1;
}

static const NrrdFormat formatUnknown = {"unknown",
                                         AIR_FALSE, /* isImage */
                                         AIR_TRUE,  /* readable */
                                         formatUnknown_available,
                                         formatUnknown_nameLooksLike,
                                         formatUnknown_fitsInto,
                                         formatUnknown_contentStartsLike,
                                         formatUnknown_read,
                                         formatUnknown_write};

const NrrdFormat *const nrrdFormatUnknown = &formatUnknown;

const NrrdFormat *const nrrdFormatArray[NRRD_FORMAT_TYPE_MAX + 1] = {
  &formatUnknown,    /* */
  &nrrd__FormatNRRD, /* */
  &nrrd__FormatPNM,  /* */
  &nrrd__FormatPNG,  /* */
  &nrrd__FormatVTK,  /* */
  &nrrd__FormatText, /* */
  &nrrd__FormatEPS   /* */
};
