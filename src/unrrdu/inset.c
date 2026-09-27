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

#define INFO "Replace a sub-region with a different nrrd"
static const char *unu_insetInfoL = (INFO
                                     ". This functions as the opposite of \"crop\".\n "
                                     "* Uses nrrdInset");

static int
unu_insetMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  long int *minOff;
  uint minLen;
  MY_OPT_ADD_BOUND("min,minimum", 1, minOff, NULL,
                   "coordinates of where to locate sub-volume within "
                   "input nrrd.\n "
                   "\b\bo <int> gives 0-based index\n "
                   "\b\bo M, M+<int>, M-<int> give index relative "
                   "to the last sample on the axis (M == #samples-1).",
                   minLen);
  Nrrd *nsub = NULL;
  uint subHidx
    = hestOptAdd_1_Other(&hopt, "s,subset", "nsub", &nsub, NULL,
                         "sub-region nrrd.  This the data to be inset in \"nin\"",
                         nrrdHestNrrdNoTTY);
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_insetInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  if (!(minLen == nin->dim)) {
    fprintf(stderr, "%s: # min coords (%u) != nrrd dim (%u)\n", me, minLen, nin->dim);
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
  size_t min[NRRD_DIM_MAX];
  for (uint ai = 0; ai <= nin->dim - 1; ai++) {
    min[ai] = minOff[0 + 2 * ai] * (nin->axis[ai].size - 1) + minOff[1 + 2 * ai];
  }

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (nrrdInset(nout, nin, nsub, min)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error insetting nrrd:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  unrrduUnuHistoryClear(nout);
  if (getenv(UNRRDU_TEEM_KVP_HISTORY_TRACE_ENV)) {
    unrrduUnuOutNSPair onsp = {.nout = nout, .name = hopt[outHidx].parmStr};
    unrrduUnuInNSPair insp = {.nin = nin, .name = hopt[inHidx].parmStr};
    unrrduUnuInNSPair snsp = {.nin = nsub, .name = hopt[subHidx].parmStr};
    int E = 0;
    if (!strcmp("-", snsp.name)) {
      // first nin then nsub=stdin
      if (!E) E |= unrrduUnuHistoryAdd(onsp, 0, NULL, insp);
      if (!E) E |= unrrduUnuHistoryAdd(onsp, argc + 1, argv - 1, snsp);
    } else {
      // first sub then nin
      if (!E) E |= unrrduUnuHistoryAdd(onsp, 0, NULL, snsp);
      if (!E) E |= unrrduUnuHistoryAdd(onsp, argc + 1, argv - 1, insp);
    }
    if (E) {
      airMopAdd(mop, err = biffGetDone(UNRRDU), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble generating history:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
  }
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(inset, INFO);
