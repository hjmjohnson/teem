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

#define INFO "Slice along one or more axes at given positions"
static const char *unu_sliceInfoL
  = (INFO ". Output nrrd dimension is less than input nrrd "
          "dimension by the number of slice axes (except when the "
          "input is or gets down to 1-D). Can slice on all axes "
          "in order to sample a single value from the array. "
          "Per-axis information is preserved.\n "
          "* Uses nrrdSlice (possibly called multiple times)");

static int
unu_sliceMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  uint *axis, axisNum;
  hestOptAdd_Nv_UInt(&hopt, "a,axis", "axis", 1, -1, &axis, NULL,
                     "single axis or multiple axes to slice along.  "
                     "Giving multiple axes here leads to doing multiple slices "
                     "(at the corresponding positions "
                     "given with \"-p\"). Multiple axes should be identified\n "
                     "in terms of the axis numbering of the original nrrd; as "
                     "the slices are done (in the given ordering) the actual "
                     "slice axis will be different if previous slices were on "
                     "lower-numbered (faster) axes.",
                     &axisNum);
  long int *_pos;
  uint posNum;
  hestOptAdd_Nv_Other(&hopt, "p,position", "pos", 1, -1, &_pos, NULL,
                      "position(s) to slice at:\n "
                      "\b\bo <int> gives 0-based index\n "
                      "\b\bo M-<int> give index relative "
                      "to the last sample on the axis (M == #samples-1).",
                      &posNum, &unrrduHestPosCB);
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_sliceInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  if (axisNum != posNum) {
    fprintf(stderr, "%s: # axes %u != # positions %u\n", me, axisNum, posNum);
    airMopError(mop);
    return 1;
  }
  if (axisNum > NRRD_DIM_MAX) {
    fprintf(stderr, "%s: got more slice axes %u than max nrrd dimension %u\n", me,
            axisNum, NRRD_DIM_MAX);
    airMopError(mop);
    return 1;
  }
  size_t pos[NRRD_DIM_MAX];
  for (uint axi = 0; axi < axisNum; axi++) {
    char stmp[AIR_STRLEN_SMALL + 1];
    if (axisNum > 1) {
      snprintf(stmp, sizeof(stmp), "[%u]", axi);
    } else {
      stmp[0] = '\0';
    }
    if (!(axis[axi] < nin->dim)) {
      fprintf(stderr, "%s: axis%s %u not in range [0,%u]\n", me, stmp, axis[axi],
              nin->dim - 1);
      airMopError(mop);
      return 1;
    }
    if (_pos[0 + 2 * axi] == -1) {
      fprintf(stderr, "%s: pos%s m+<int> spec not meaningful here\n", me, stmp);
      airMopError(mop);
      return 1;
    }
    pos[axi] = (_pos[0 + 2 * axi] * (nin->axis[axis[axi]].size - 1) + _pos[1 + 2 * axi]);
    /*
    printf("%s: [%d] axis = %u, pos = %u\n", me, axi, axis[axi],
           AIR_UINT(pos[axi]));
    */
  }
  /* check on possibly adjust slice axes downward */
  if (axisNum > 1) {
    for (uint axi = 0; axi < axisNum - 1; axi++) {
      for (uint axj = axi + 1; axj < axisNum; axj++) {
        if (axis[axi] == axis[axj]) {
          fprintf(stderr, "%s: can't repeat axis: axis[%u] = axis[%u] = %u\n", me, axi,
                  axj, axis[axj]);
          airMopError(mop);
          return 1;
        }
      }
    }
    for (uint axi = 0; axi < axisNum - 1; axi++) {
      for (uint axj = axi + 1; axj < axisNum; axj++) {
        axis[axj] -= (axis[axj] > axis[axi]);
      }
    }
    /*
    for (axi=0; axi<axisNum; axi++) {
      printf("%s: axis[%d] = %u\n", me, axi, axis[axi]);
    }
    */
  }

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);

  if (1 == axisNum) {
    /* old single axis code */
    if (nrrdSlice(nout, nin, axis[0], pos[0])) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error slicing nrrd:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
  } else {
    /* have to do multiple slices */
    Nrrd *ntmp[2];
    ntmp[0] = nrrdNew();
    airMopAdd(mop, ntmp[0], nrrdNuke_mop, airMopAlways);
    ntmp[1] = nrrdNew();
    airMopAdd(mop, ntmp[1], nrrdNuke_mop, airMopAlways);
    uint tidx = 0;
    for (uint axi = 0; axi < axisNum; axi++) {
      if (nrrdSlice((axi < axisNum - 1
                       ? ntmp[1 - tidx]      /* use an ntmp for all but last output */
                       : nout),              /* and use nout for last output */
                    (0 == axi ? nin          /* use nin for only first input */
                              : ntmp[tidx]), /* use an ntmp for all but first input */
                    axis[axi], pos[axi])) {
        airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
        fprintf(stderr, "%s: error with slice %u of %u:\n%s", me, axi + 1, axisNum, err);
        airMopError(mop);
        return 1;
      }
      tidx = 1 - tidx;
    }
  }

  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(slice, INFO);
