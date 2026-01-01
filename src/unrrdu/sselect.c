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

#define INFO "Select subset of slices along an axis"
static const char *unu_sselectInfoL
  = (INFO ". The choice to keep or nix a slice is determined by whether the "
          "values in a given 1-D line of values is above or below a given "
          "threshold.\n "
          "* Uses nrrdSliceSelect");

static int
unu_sselectMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  AIR_UNUSED(inHidx); // TODO handle history; hard because of two outputs
  uint axis;
  MY_OPT_ADD_AXIS(axis, "axis to slice along");
  Nrrd *nline;
  hestOptAdd_1_Other(&hopt, "s,selector", "nline", &nline, NULL,
                     "the 1-D nrrd of values to compare with threshold",
                     nrrdHestNrrdNoTTY);
  double thresh;
  hestOptAdd_1_Double(&hopt, "th", "thresh", &thresh, NULL,
                      "threshold on selector line");
  char *outS[2];
  hestOptAdd_2_String(&hopt, "o,output", "above below", outS, "- x",
                      "outputs for slices corresponding to values "
                      "above (first) and below (second) given threshold. "
                      "Use \"x\" to say that no output is desired.");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_sselectInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  if (!strcmp(outS[0], "x") && !strcmp(outS[1], "x")) {
    fprintf(stderr,
            "%s: need to save either above or below slices "
            "(can't use \"x\" for both)\n",
            me);
    airMopError(mop);
    return 1;
  }
  Nrrd *noutAbove, *noutBelow;
  if (strcmp(outS[0], "x")) {
    noutAbove = nrrdNew();
    airMopAdd(mop, noutAbove, nrrdNuke_mop, airMopAlways);
  } else {
    noutAbove = NULL;
  }
  if (strcmp(outS[1], "x")) {
    noutBelow = nrrdNew();
    airMopAdd(mop, noutBelow, nrrdNuke_mop, airMopAlways);
  } else {
    noutBelow = NULL;
  }

  if (nrrdSliceSelect(noutAbove, noutBelow, nin, axis, nline, thresh)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error selecting slices:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  if (noutAbove) {
    MY_SAVE(outS[0], noutAbove, NULL);
  }
  if (noutBelow) {
    MY_SAVE(outS[1], noutBelow, NULL);
  }

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(sselect, INFO);
