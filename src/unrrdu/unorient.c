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

#include "unrrdu.h"
#include "privateUnrrdu.h"

#define INFO "Make image orientation be axis-aligned"
static const char *unu_unorientInfoL = (INFO ". Does various tricks.\n "
                                             "* Uses nrrdOrientationReduce");

static int
unu_unorientMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  hestOpt *hopt = NULL;
  /*
   * with new nrrdHestNrrdNoTTY, can let "-" be default input as normal; no more need
   * for this awkwardness:
   *
   * / * if we gave a default for this, then there it would fine to have
   * no command-line arguments whatsoever, and then the user would not
   * know how to get the basic usage information * /
   * hestOptAdd(&hopt, "i,input", "nin", airTypeOther, 1, 1, &nin, NULL,
   *            "input nrrd "
   *            "(sorry, can't use usual default of \"-\" for stdin "
   *            "because of hest quirk)",
   *            NULL, NULL, nrrdHestNrrd);
   */
  Nrrd *nin;
  uint inHidx = hestOptAdd_1_Other(&hopt, "i,input", "nin", &nin, "-",
                                   "input nrrd. By default reads from stdin",
                                   nrrdHestNrrdNoTTY);
  int setMinsFromOrigin;
  hestOptAdd_Flag(&hopt, "smfo", &setMinsFromOrigin,
                  "set some axis mins based on space origin (hack)");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for YES some hparm modification)
  hestParm *hparm = hestParmCopy(_hparm);
  hparm->noArgsIsNoProblem = AIR_TRUE; // thanks to nrrdHestNrrdNoTTY
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_unorientInfoL);
  airArray *mop = unrrduMopNew(hopt, hparm);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (nrrdOrientationReduce(nout, nin, setMinsFromOrigin)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error unorienting nrrd:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  MY_HISTORY(nout, nin);
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(unorient, INFO);
