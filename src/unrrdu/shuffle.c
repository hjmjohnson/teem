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

#define INFO "Permute slices along one axis"
static const char *_unrrdu_shuffleInfoL
  = (INFO ". Slices along one axis are re-arranged as units "
          "according to the given permutation (or its inverse). "
          "The permutation tells which old slice to put at each "
          "new position.  For example, the shuffle "
          "0->1,\t1->2,\t2->0 would be \"2 0 1\".  Obviously, "
          "if you have to rearrange the many slices of a large "
          "dataset, you should probably store the permutation "
          "in a plain text file and use it as a "
          "\"response file\".\n "
          "* Uses nrrdShuffle");

static int
unrrdu_shuffleMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  hestOpt *hopt = NULL;
  uint *perm, permLen;
  hestOptAdd_Nv_UInt(&hopt, "p,permute", "slc0 slc1", 1, -1, &perm, NULL,
                     "new slice ordering", &permLen);
  int inverse;
  hestOptAdd_Flag(&hopt, "inv,inverse", &inverse, "use inverse of given permutation");
  uint axis;
  OPT_ADD_AXIS(axis, "axis to shuffle along");
  Nrrd *nin;
  OPT_ADD_NIN(nin, "input nrrd");
  char *out;
  OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for YES some hparm modification)
  hestParm *hparm = hestParmCopy(_hparm);
  hparm->responseFileEnable = AIR_TRUE; // to read long permutations from a file
  hestParseOrExit(hopt, argc, argv, hparm, me, _unrrdu_shuffleInfoL);
  airArray *mop = unrrduMopNew(hopt, hparm);
  char *err;

  /* we have to do error checking on axis in order to do error
     checking on length of permutation */
  if (!(axis < nin->dim)) {
    fprintf(stderr, "%s: axis %u not in valid range [0,%u]\n", me, axis, nin->dim - 1);
    airMopError(mop);
    return 1;
  }
  if (!(permLen == nin->axis[axis].size)) {
    char stmp[AIR_STRLEN_SMALL + 1];
    fprintf(stderr, "%s: permutation length (%u) != axis %u's size (%s)\n", me, permLen,
            axis, airSprintSize_t(stmp, nin->axis[axis].size));
    airMopError(mop);
    return 1;
  }
  uint *iperm = NULL, *whichperm;
  if (inverse) {
    iperm = AIR_CALLOC(permLen, unsigned int);
    airMopAdd(mop, iperm, airFree, airMopAlways);
    if (nrrdInvertPerm(iperm, perm, permLen)) {
      fprintf(stderr, "%s: couldn't compute inverse of given permutation\n", me);
      airMopError(mop);
      return 1;
    }
    whichperm = iperm;
  } else {
    whichperm = perm;
  }

  size_t *realperm = AIR_CALLOC(permLen, size_t);
  airMopAdd(mop, realperm, airFree, airMopAlways);
  for (uint di = 0; di < permLen; di++) {
    realperm[di] = whichperm[di];
  }
  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (nrrdShuffle(nout, nin, axis, realperm)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error shuffling nrrd:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

UNRRDU_CMD(shuffle, INFO);
