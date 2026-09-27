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

#include "ten.h"
#include "privateTen.h"

#define INFO "Calculate one or more eigenvectors in a DT volume"
static const char *evecInfoL = (INFO ". ");

static int
evecMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nin;
  uint inHidx = hestOptAdd_1_Other(&hopt, "i", "nin", &nin, "-",
                                   "input diffusion tensor volume", nrrdHestNrrd);
  hopt[inHidx].priority -= 10;
  uint *comp, compLen;
  hestOptAdd_Mv_UInt(&hopt, "c", "c0 ", 1, 3, &comp, NULL,
                     "which eigenvalues should be saved out. \"0\" for the "
                     "largest, \"1\" for the middle, \"2\" for the smallest, "
                     "\"0 1\", \"1 2\", \"0 1 2\" or similar for more than one",
                     &compLen);
  float thresh;
  hestOptAdd_1_Float(&hopt, "t", "thresh", &thresh, "0.5", "confidence threshold");
  char *outS;
  hestOptAdd_1_String(&hopt, "o", "nout", &outS, "-", "output image (floating point)");

  airArray *mop = airMopNew();
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  int pret;
  char *perr, *err;
  USAGE_PARSE(evecInfoL);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);

  for (uint cc = 0; cc < compLen; cc++) {
    if (!(comp[cc] <= 2)) {
      fprintf(stderr, "%s: requested component %u (%u of 3) not in [0..2]\n", me,
              comp[cc], cc + 1);
      airMopError(mop);
      return 1;
    }
  }
  if (tenTensorCheck(nin, nrrdTypeFloat, AIR_TRUE, AIR_TRUE)) {
    airMopAdd(mop, err = biffGetDone(TEN), airFree, airMopAlways);
    fprintf(stderr, "%s: didn't get a valid DT volume:\n%s\n", me, err);
    airMopError(mop);
    return 1;
  }

  size_t sx = nin->axis[1].size;
  size_t sy = nin->axis[2].size;
  size_t sz = nin->axis[3].size;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  int ret = nrrdMaybeAlloc_va(nout, nrrdTypeFloat, 4, AIR_SIZE_T(3 * compLen), sx, sy,
                              sz);
  if (ret) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble allocating output:\n%s\n", me, err);
    airMopError(mop);
    return 1;
  }
  size_t N = sx * sy * sz;
  float *edata = (float *)nout->data;
  float *tdata = (float *)nin->data;
  float eval[3], evec[9];
  if (1 == compLen) {
    for (size_t I = 0; I < N; I++) {
      tenEigensolve_f(eval, evec, tdata);
      float scl = AIR_FLOAT(tdata[0] >= thresh);
      ELL_3V_SCALE(edata, scl, evec + 3 * comp[0]);
      edata += 3;
      tdata += 7;
    }
  } else {
    for (size_t I = 0; I < N; I++) {
      tenEigensolve_f(eval, evec, tdata);
      float scl = AIR_FLOAT(tdata[0] >= thresh);
      for (uint cc = 0; cc < compLen; cc++) {
        ELL_3V_SCALE(edata + 3 * cc, scl, evec + 3 * comp[cc]);
      }
      edata += 3 * compLen;
      tdata += 7;
    }
  }
  if (nrrdAxisInfoCopy(nout, nin, NULL, NRRD_AXIS_INFO_SIZE_BIT)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble:\n%s\n", me, err);
    airMopError(mop);
    return 1;
  }
  if (nrrdBasicInfoCopy(nout, nin, NRRD_BASIC_INFO_ALL ^ NRRD_BASIC_INFO_SPACE)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble:\n%s\n", me, err);
    airMopError(mop);
    return 1;
  }
  nout->axis[0].label = (char *)airFree(nout->axis[0].label);
  nout->axis[0].kind = nrrdKindUnknown;

  if (nrrdSave(outS, nout, NULL)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble writing:\n%s\n", me, err);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  return 0;
}
MY_TEND_CMD(evec, INFO);
