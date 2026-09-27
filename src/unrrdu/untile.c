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

#define INFO "Undo \"unu tile\": merge slow parts of two axis splits"
static const char *unu_untileInfoL
  = (INFO ". Untiling an array means spliting two axes, permuting the slow parts "
          "of those axes to be adjecent in the axis ordering, and then merging "
          "them.  This increases the dimension by one.  Undoing a \"unu tile\" "
          "uses the same \"-s\" argument, and sometimes a different \"-a\" argument, "
          "as demonstrated here for a 3-D array:\n "
          "\"unu untile -a 2 0 1\" undoes \"unu tile -a 2 0 1\"\n "
          "\"unu untile -a 1 0 1\" undoes \"unu tile -a 1 0 2\"\n "
          "\"unu untile -a 0 0 1\" undoes \"unu tile -a 0 1 2\".\n "
          "* Uses nrrdUntile2D");

static int
unu_untileMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  uint axes[3];
  hestOptAdd_3_UInt(&hopt, "a,axis", "axMerge ax0 ax1", axes, NULL,
                    "the slow parts of axes ax0 and ax1 are merged into a (new) "
                    "axis axMerge, with the axis ax0 part being faster than ax1.");
  size_t size[2];
  hestOptAdd_2_Size_t(&hopt, "s,size", "size0 size1", size, NULL,
                      "the slow parts of axes ax0 and ax1 are taken to have size "
                      "size0 and size1, respectively, and axis axMerge will have "
                      "size size0*size1.");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_untileInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (nrrdUntile2D(nout, nin, axes[1], axes[2], axes[0], size[0], size[1])) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error tiling nrrd:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  MY_HISTORY(nout, nin);
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(untile, INFO);
