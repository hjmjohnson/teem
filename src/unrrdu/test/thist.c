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

#include "../unrrdu.h"

void
usage(const char *me) {
  /*                       0   1     2     3   (4) */
  fprintf(stderr, "usage: %s <nin> <nout> foo bar ...\n", me);
  exit(1);
}

int
main(int argc, const char **argv) {
  const char *me = argv[0];
  if (!(argc >= 4)) usage(me);
  char *err;
  airArray *mop = airMopNew();
  Nrrd *nin = nrrdNew();
  airMopAdd(mop, nin, nrrdNuke_mop, airMopAlways);
  if (nrrdLoad(nin, argv[1], NULL)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble loading \"%s\":\n%s", me, argv[1], err);
    airMopError(mop);
    exit(1);
  }
  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  nrrdCopy(nout, nin);
  if (unrrduUnuHistoryAdd((unrrduUnuOutNSPair){.nout = nout, .name = argv[2]}, argc - 3,
                          argv + 3, (unrrduUnuInNSPair){.nin = nin, .name = argv[1]})) {
    airMopAdd(mop, err = biffGetDone(UNRRDU), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble adding command \"%s ...\":\n%s", me, argv[3], err);
    airMopError(mop);
    exit(1);
  }
  if (nrrdSave(argv[2], nout, NULL)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble saving \"%s\":\n%s", me, argv[2], err);
    airMopError(mop);
    exit(1);
  }
  airMopOkay(mop);
  exit(0);
}
