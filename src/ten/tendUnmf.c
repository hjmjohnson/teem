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

#define INFO "Applies and removes the measurement frame"
static const char *unmfInfoL
  = (INFO ". When the given tensor volume has a measurement frame associated "
          "with it, this will apply the measurement frame transform to all "
          "tensors to convert them into world space, and remove the measurement "
          "frame from the nrrd.");

static int
unmfMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  int pret;
  hestOpt *hopt = NULL;
  char *perr, *err;

  Nrrd *nin, *nout;
  char *outS;

  airArray *mop = airMopNew();
  hestParm *hparm = hestParmCopy(_hparm);
  airMopAdd(mop, hparm, hestParmFree_mop, airMopAlways);
  hparm->noArgsIsNoProblem = AIR_TRUE;

  hestOptAdd_1_Other(&hopt, "i", "nin", &nin, "-", "input diffusion tensor volume ",
                     nrrdHestNrrdNoTTY);
  hestOptAdd_1_String(&hopt, "o", "nout", &outS, "-", "output tensor volume");

  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  USAGE_PARSE(unmfInfoL);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);

  nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (tenMeasurementFrameReduce(nout, nin)) {
    airMopAdd(mop, err = biffGetDone(TEN), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble:\n%s\n", me, err);
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
MY_TEND_CMD(unmf, INFO);
