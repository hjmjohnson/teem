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

#define INFO "Create joint histogram of two or more nrrds"
static const char *_unrrdu_jhistoInfoL
  = (INFO ". Each axis of the output corresponds to one of the "
          "input nrrds, and each bin in the output records the "
          "number of corresponding positions in the inputs with "
          "a combination of values represented by the coordinates "
          "of the bin.\n "
          "* Uses nrrdHistoJoint");

static int
unrrdu_jhistoMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  size_t *bin;
  uint binLen;
  hestOptAdd_Nv_Size_t(&hopt, "b,bin", "bins0 bins1", 2, -1, &bin, NULL,
                       "bins<i> is the number of bins to use along axis i (of joint "
                       "histogram), which represents the values of nin<i> ",
                       &binLen);
  Nrrd *nwght = NULL;
  hestOptAdd_1_Other(&hopt, "w,weight", "nweight", &nwght, "",
                     "how to weigh contributions to joint histogram.  By default "
                     "(not using this option), the increment is one bin count per "
                     "sample, but by giving a nrrd, the value in the nrrd at the "
                     "corresponding location will be the bin count increment ",
                     nrrdHestNrrdNoTTY);
  double *min, *max;
  uint minLen, maxLen;
  hestOptAdd_Nv_Double(&hopt, "min,minimum", "min0 min1", 2, -1, &min, "nan nan",
                       "min<i> is the low range of values to be quantized along "
                       "axis i; use \"nan\" to represent lowest value present ",
                       &minLen);
  hestOptAdd_Nv_Double(&hopt, "max,maximum", "max0 max1", 2, -1, &max, "nan nan",
                       "max<i> is the high range of values to be quantized along "
                       "axis i; use \"nan\" to represent highest value present ",
                       &maxLen);
  int type;
  OPT_ADD_TYPE(type,
               "type to use for output (the type used to store hit "
               "counts in the joint histogram).  Clamping is done on hit "
               "counts so that they never overflow a fixed-point type",
               "uint");
  Nrrd **nin;
  uint ninLen;
  hestOptAdd_Nv_Other(&hopt, "i,input", "nin0 [nin1]", 1, -1, &nin, "-",
                      "list of nrrds (one for each axis of joint histogram), "
                      "or, single nrrd that will be sliced along specified axis.",
                      &ninLen, nrrdHestNrrdNoTTY);
  uint diceax;
  hestOptAdd_1_UInt(&hopt, "a,axis", "axis", &diceax, "0",
                    "axis to slice along when working with single nrrd. ");
  char *out;
  OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, _unrrdu_jhistoInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  Nrrd **npass = NULL;
  if (ninLen == 1) {
    /* Slice a nrrd on the fly */
    size_t asize;
    if (!(diceax <= nin[0]->dim - 1)) {
      fprintf(stderr, "%s: slice axis %u not valid for single %u-D nrrd", me, diceax,
              nin[0]->dim);
      airMopError(mop);
      return 1;
    }
    asize = nin[0]->axis[diceax].size;
    if (asize != binLen) {
      fprintf(stderr, "%s: size (%u) of slice axis %u != # bins given (%u)\n", me,
              AIR_UINT(asize), diceax, AIR_UINT(binLen));
      airMopError(mop);
      return 1;
    }
    /* create buffer for slices */
    if (!(npass = AIR_CALLOC(binLen, Nrrd *))) {
      fprintf(stderr, "%s: couldn't allocate nrrd array (size %u)\n", me, binLen);
      airMopError(mop);
      return 1;
    }
    airMopMem(mop, &npass, airMopAlways);
    /* slice this nrrd, allocate new nrrds, and store the slices in nin */
    for (uint ai = 0; ai < binLen; ai++) {
      /* Allocate each slice nrrd */
      if (!(npass[ai] = nrrdNew())) {
        fprintf(stderr, "%s: couldn't allocate npass[%u]\n", me, ai);
        airMopError(mop);
        return 1;
      }
      airMopAdd(mop, npass[ai], (airMopper)nrrdNuke, airMopAlways);
      if (nrrdSlice(npass[ai], nin[0], diceax, ai)) {
        airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
        fprintf(stderr, "%s: error slicing:\n%s", me, err);
        airMopError(mop);
        return 1;
      }
    }
  } else {
    /* we were given multiple nrrds */
    if (ninLen != binLen) {
      fprintf(stderr, "%s: # input nrrds (%u) != # bin specifications (%u)\n", me,
              AIR_UINT(ninLen), AIR_UINT(binLen));
      airMopError(mop);
      return 1;
    }
    /* create buffer for slices (HEY copy and paste) */
    if (!(npass = AIR_CALLOC(binLen, Nrrd *))) {
      fprintf(stderr, "%s: couldn't allocate nrrd array (size %u)\n", me, binLen);
      airMopError(mop);
      return 1;
    }
    for (uint ai = 0; ai < binLen; ai++) {
      npass[ai] = nin[ai];
    }
  }
  NrrdRange **range = AIR_CALLOC(binLen, NrrdRange *);
  airMopAdd(mop, range, airFree, airMopAlways);
  for (uint ai = 0; ai < binLen; ai++) {
    range[ai] = nrrdRangeNew(AIR_NAN, AIR_NAN);
    airMopAdd(mop, range[ai], (airMopper)nrrdRangeNix, airMopAlways);
  }
  if (2 != minLen || (AIR_EXISTS(min[0]) || AIR_EXISTS(min[1]))) {
    if (minLen != binLen) {
      fprintf(stderr, "%s: # mins (%u) != # input nrrds (%u)\n", me, minLen, binLen);
      airMopError(mop);
      return 1;
    }
    for (uint ai = 0; ai < binLen; ai++) {
      range[ai]->min = min[ai];
    }
  }
  if (2 != maxLen || (AIR_EXISTS(max[0]) || AIR_EXISTS(max[1]))) {
    if (maxLen != binLen) {
      fprintf(stderr, "%s: # maxs (%u) != # input nrrds (%u)\n", me, maxLen, binLen);
      airMopError(mop);
      return 1;
    }
    for (uint ai = 0; ai < binLen; ai++) {
      range[ai]->max = max[ai];
    }
  }
  int clamp[NRRD_DIM_MAX];
  for (uint ai = 0; ai < binLen; ai++) {
    clamp[ai] = 0;
  }
  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, (airMopper)nrrdNuke, airMopAlways);
  if (nrrdHistoJoint(nout, (const Nrrd *const *)npass, (const NrrdRange *const *)range,
                     binLen, nwght, bin, type, clamp)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error doing joint histogram:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

UNRRDU_CMD(jhisto, INFO);
