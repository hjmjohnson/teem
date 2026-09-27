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

#define INFO "Merge two adjacent axes into one"
static const char *unu_axmergeInfoL
  = (INFO ". A more general version of \"unu axdelete\". "
          "The underlying linear ordering of the samples is "
          "unchanged, and the information about the other axes is "
          "shifted downwards as needed.\n "
          "* Uses nrrdAxesMerge");

static int
unu_axmergeMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  uint *axes, axesLen;
  hestOptAdd_Mv_UInt(&hopt, "a,axis", "ax0", 1, -1, &axes, NULL,
                     "axis (or axes) to merge.  Each axis index identified is the "
                     "lower of the pair of axes that will be merged.  Saying \"-a 2\" "
                     "means to merge axis 2 and axis 3 into axis 2.  If multiple "
                     "merges are to be done, the indices listed here are for "
                     "the axes prior to any merging.",
                     &axesLen);
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_axmergeInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  Nrrd *nout[2];
  /* make two nout buffers, will alternate between them */
  airMopAdd(mop, nout[0] = nrrdNew(), nrrdNuke_mop, airMopAlways);
  airMopAdd(mop, nout[1] = nrrdNew(), nrrdNuke_mop, airMopAlways);

  if (axesLen > 1) {
    /* sort merge axes into ascending order */
    qsort(axes, axesLen, sizeof(*axes), nrrdValCompare[nrrdTypeInt]);
  }

  uint ni = 0;
  for (uint ii = 0; ii < axesLen; ii++) {
    if (nrrdAxesMerge(nout[ni], !ii ? nin : nout[1 - ni], axes[ii])) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error merging axes:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
    for (uint jj = ii + 1; jj < axesLen; jj++) {
      axes[jj] -= 1;
    }
    ni = 1 - ni;
  }

  MY_HISTORY(nout[1 - ni], nin);
  MY_SAVE(out, nout[1 - ni], NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(axmerge, INFO);
