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

#define INFO "Remap CC values down to lowest contiguous values"
static const char *unu_ccsettleInfoL = (INFO ".\n "
                                             "* Uses nrrdCCSettle");

static int
unu_ccsettleMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nin;
  hestOptAdd_1_Other(&hopt, "i,input", "nin", &nin, "-",
                     "input nrrd. By default try to read from stdin", nrrdHestNrrdNoTTY);
  char *valS;
  hestOptAdd_1_String(&hopt, "v,values", "filename", &valS, "",
                      "Giving a filename here allows you to save out the mapping "
                      "from new (settled) values to old values, in the form of a "
                      "1-D lookup table");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for YES some hparm modification)
  hestParm *hparm = hestParmCopy(_hparm);
  hparm->noArgsIsNoProblem = AIR_TRUE; // thanks to nrrdHestNrrdNoTTY
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_ccsettleInfoL);
  airArray *mop = unrrduMopNew(hopt, hparm);
  char *err;

  Nrrd *nval = NULL, *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (nrrdCCSettle(nout, airStrlen(valS) ? &nval : NULL, nin)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error settling connected components:\n%s", me, err);
    airMopError(mop);
    return 1;
  }
  if (nval) {
    airMopAdd(mop, nval, nrrdNuke_mop, airMopAlways);
  }

  if (airStrlen(valS)) {
    MY_SAVE(valS, nval, NULL);
  }
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(ccsettle, INFO);
