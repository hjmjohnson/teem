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

#define INFO "Condense axis-0 scanlines into \"blocks\""
static const char *unu_blockInfoL
  = (INFO
     ". Output nrrd will be of type \"block\": the type for an opaque chunk of "
     "memory.  Block samples can be sliced, cropped, shuffled, permuted, etc., but "
     "there is no scalar value associated with them, so they can not be histogrammed, "
     "quantized, resampled, converted, etc.  The output nrrd will have one less "
     "dimension than input; axis N information will be shifted down to axis N-1.  "
     "Underlying data is unchanged.");

static int
unu_blockMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nin;
  hestOptAdd_1_Other(&hopt, "i", "nin", &nin, "-", "input nrrd", nrrdHestNrrdNoTTY);
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for YES some hparm modification)
  hestParm *hparm = hestParmCopy(_hparm);
  hparm->noArgsIsNoProblem = AIR_TRUE; // thanks to nrrdHestNrrdNoTTY
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_blockInfoL);
  airArray *mop = unrrduMopNew(hopt, hparm);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);

  if (nrrdBlock(nout, nin)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error blocking nrrd:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD_HIDE(block, INFO);
