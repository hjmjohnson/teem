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

#include "ten.h"
#include "privateTen.h"

#define INFO "Slice 3D tensors to get slab/image of 3D/2D  tensors"
static const char *sliceInfoL = (INFO ". ");

static int
sliceMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  int pret;
  hestOpt *hopt = NULL;
  char *perr, *err;
  airArray *mop;

  char *outS;
  unsigned int axis, pos, dim;
  Nrrd *nin, *nout;

  hestOptAdd_1_UInt(&hopt, "a", "axis", &axis, NULL, "axis along which to slice");
  hestOptAdd_1_UInt(&hopt, "p", "pos", &pos, NULL, "position to slice at");
  hestOptAdd_1_UInt(&hopt, "d", "dim", &dim, "3",
                    "dimension of desired tensor output, can be either 2 or 3");
  hestOptAdd_1_Other(&hopt, "i", "nin", &nin, "-", "input diffusion tensor volume",
                     nrrdHestNrrd);
  hestOptAdd_1_String(&hopt, "o", "nout", &outS, "-", "output tensor slice");

  mop = airMopNew();
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  USAGE_PARSE(sliceInfoL);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);
  airMopAdd(mop, nout = nrrdNew(), nrrdNuke_mop, airMopAlways);

  if (tenSlice(nout, nin, axis, pos, dim)) {
    airMopAdd(mop, err = biffGetDone(TEN), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble\n%s\n", me, err);
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
MY_TEND_CMD(slice, INFO);
