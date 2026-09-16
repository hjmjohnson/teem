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

#include <teem/nrrd.h>
#include <testutil.h>

/*
** Tests:
** rnParse_kinds "none" token sets the axis kind and leaves a previously
** parsed centering intact
*/

int
main(int argc, const char **argv) {
  const char *me;
  Nrrd *nrrd;
  airArray *mop;
  char *hdrName, *dataName;
  FILE *fout;
  static const char header[] = "NRRD0004\n"
                               "type: unsigned char\n"
                               "dimension: 2\n"
                               "sizes: 2 2\n"
                               "centers: cell cell\n"
                               "kinds: none domain\n"
                               "encoding: raw\n"
                               "data file: tkindsnone.raw\n";

  AIR_UNUSED(argc);
  me = argv[0];
  mop = airMopNew();
  nrrd = nrrdNew();
  airMopAdd(mop, nrrd, (airMopper)nrrdNuke, airMopAlways);
  airMopAdd(mop, hdrName = teemTestTmpPath("tkindsnone.nhdr"), airFree, airMopAlways);
  airMopAdd(mop, dataName = teemTestTmpPath("tkindsnone.raw"), airFree, airMopAlways);

  if (!(fout = fopen(hdrName, "wb"))) {
    fprintf(stderr, "%s: couldn't open %s for writing\n", me, hdrName);
    airMopError(mop);
    return 1;
  }
  fputs(header, fout);
  fclose(fout);
  if (!(fout = fopen(dataName, "wb"))) {
    fprintf(stderr, "%s: couldn't open %s for writing\n", me, dataName);
    airMopError(mop);
    return 1;
  }
  fputs("ABCD", fout);
  fclose(fout);

  if (nrrdLoad(nrrd, hdrName, NULL)) {
    airMopAdd(mop, biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble reading %s\n", me, hdrName);
    airMopError(mop);
    return 1;
  }
  if (nrrdCenterCell != nrrd->axis[0].center) {
    fprintf(stderr,
            "%s: axis 0 center is %d, expected cell (%d) -- clobbered by kinds none\n",
            me, nrrd->axis[0].center, nrrdCenterCell);
    airMopError(mop);
    return 1;
  }
  fprintf(stderr, "%s: kinds none preserved axis 0 cell centering\n", me);
  airMopOkay(mop);
  return 0;
}
