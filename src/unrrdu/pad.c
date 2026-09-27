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

#define INFO "Pad along each axis to make a bigger nrrd"
static const char *unu_padInfoL = (INFO ".\n "
                                        "* Uses nrrdPad_nva");

static int
unu_padMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  long int *minOff, *maxOff;
  uint minLen, maxLen;
  MY_OPT_ADD_BOUND("min,minimum", 1, minOff, NULL,
                   "low corner of bounding box.\n "
                   "\b\bo <int> gives 0-based index\n "
                   "\b\bo M, M+<int>, M-<int> give index relative "
                   "to the last sample on the axis (M == #samples-1).",
                   minLen);
  MY_OPT_ADD_BOUND("max,maximum", 1, maxOff, NULL,
                   "high corner of bounding box.  "
                   "Besides the specification styles described above, "
                   "there's also:\n "
                   "\b\bo m+<int> give index relative to minimum.",
                   maxLen);
  /* TeemV2 changed this from parsing nrrdBoundary enum value to parsing a
  nrrdBoundarySpec. To support the many existing scripts that say "-b pad -v <val>", we
  parse with the nrrdHestBoundarySpecAllowMissingPadValue that accepts "pad" as a
  boundary spec (with padValue==NaN), even though normally it would need to be
  "pad:<val>".  Then nrrdHestBoundarySpecFinish finishes the cleverness. */
  NrrdBoundarySpec *bspec;
  uint bspecIdx = hestOptAdd_1_Other(&hopt, "b,boundary", "behavior", &bspec, "bleed",
                                     "How to handle samples beyond the input bounds:\n "
                                     "\b\bo \"pad\": pad with value specified via -v\n "
                                     "\b\bo \"pad:<V>\": pad with value V\n "
                                     "\b\bo \"bleed\": extend border values outward\n "
                                     "\b\bo \"mirror\": repeated reflections\n "
                                     "\b\bo \"wrap\": wrap-around to other side",
                                     nrrdHestBoundarySpecAllowMissingPadValue);
  double padVal;
  uint padValIdx
    = hestOptAdd_1_Double(&hopt, "v,value", "val", &padVal, "0.0",
                          "[DEPRECATED: use \"-b pad:<V>\" instead.]\n "
                          "for \"pad\" boundary behavior, pad with this value");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_padInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  if (nrrdHestBoundarySpecFinish(bspec, hopt + bspecIdx, padVal, hopt + padValIdx, me)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error setting boundary behavior:\n%s", me, err);
    airMopError(mop);
    return 1;
  }
  if (!(minLen == nin->dim && maxLen == nin->dim)) {
    fprintf(stderr,
            "%s: # \"-min\" coords (%u) or \"-max\" coords (%u) given "
            "!= nrrd dim (%u)\n",
            me, minLen, maxLen, nin->dim);
    airMopError(mop);
    return 1;
  }
  for (uint ai = 0; ai < nin->dim; ai++) {
    if (-1 == minOff[0 + 2 * ai]) {
      fprintf(stderr, "%s: can't use m+<int> specification for axis %u min\n", me, ai);
      airMopError(mop);
      return 1;
    }
  }
  ptrdiff_t min[NRRD_DIM_MAX], max[NRRD_DIM_MAX];
  for (uint ai = 0; ai < nin->dim; ai++) {
    min[ai] = minOff[0 + 2 * ai] * (nin->axis[ai].size - 1) + minOff[1 + 2 * ai];
    if (-1 == maxOff[0 + 2 * ai]) {
      max[ai] = min[ai] + maxOff[1 + 2 * ai];
    } else {
      max[ai] = maxOff[0 + 2 * ai] * (nin->axis[ai].size - 1) + maxOff[1 + 2 * ai];
    }
    /*
    fprintf(stderr, "%s: ai %2d: min = %4d, max = %4d\n",
            me, ai, min[ai], mai[ai]);
    */
  }

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (nrrdPad_nva(nout, nin, min, max, bspec->boundary, bspec->padValue)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error padding nrrd:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  MY_HISTORY(nout, nin);
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(pad, INFO);
