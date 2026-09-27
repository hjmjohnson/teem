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

#include "ten.h"
#include "privateTen.h"

#define INFO "Converts a 9-value DT volume to a 7-value DT volume"
static const char *shrinkInfoL
  = (INFO ". The confidence value is set to 1.0 everwhere.  You can \"unu splice\" "
          "or nrrdSplice() something else in its place later.");

static int
shrinkMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  hestOpt *hopt = NULL;

  airArray *mop = airMopNew();
  hestParm *hparm = hestParmCopy(_hparm);
  airMopAdd(mop, hparm, hestParmFree_mop, airMopAlways);
  hparm->noArgsIsNoProblem = AIR_TRUE;

  Nrrd *nin;
  uint inHidx = hestOptAdd_1_Other(&hopt, "i", "nin", &nin, "-",
                                   "input diffusion tensor volume, "
                                   "with 9 matrix components per sample",
                                   nrrdHestNrrdNoTTY);
  hopt[inHidx].priority -= 10;
  char *outS;
  hestOptAdd_1_String(&hopt, "o", "nout", &outS, "-",
                      "output tensor volume, with the 7 values per sample");

  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  int pret;
  char *perr, *err;
  USAGE_PARSE(shrinkInfoL);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (tenShrink(nout, NULL, nin)) {
    airMopAdd(mop, err = biffGetDone(TEN), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble shrinking tensors:\n%s\n", me, err);
    airMopError(mop);
    return 1;
  }

  if (nrrdSave(outS, nout, NULL)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble writing:\n%s\n", me, err);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  return 0;
}
MY_TEND_CMD(shrink, INFO);
