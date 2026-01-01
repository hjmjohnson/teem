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

#include "unrrdu.h"
#include "privateUnrrdu.h"

#define INFO "Permute ordering of axes"
static const char *unu_permuteInfoL
  = (INFO ". The permutation gives the new ordering of the old "
          "axes (in 0-based numbering). For example, the "
          "permutation 0->1,\t1->2,\t2->0 would be \"2 0 1\".\n "
          "* Uses nrrdAxesPermute");

static int
unu_permuteMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  uint *perm, permLen;
  hestOptAdd_Nv_UInt(&hopt, "p,permute", "ax0 ax1", 1, -1, &perm, NULL,
                     "new axis ordering", &permLen);
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_permuteInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  if (!(permLen == nin->dim)) {
    fprintf(stderr, "%s: # axes in permutation (%u) != nrrd dim (%u)\n", me, permLen,
            nin->dim);
    airMopError(mop);
    return 1;
  }

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (nrrdAxesPermute(nout, nin, perm)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error permuting nrrd:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  MY_HISTORY(nout, nin);
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(permute, INFO);
