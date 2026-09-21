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

#define INFO "Affine (lerp) mapping on 5 nrrds or constants"
static const char *unu_affineInfoL
  = (INFO ". All the 5 arguments can be either nrrds or single "
          "floating-point values.  When all args are single values, this "
          "is subsuming the functionality of the previous stand-alone "
          "\"affine\" program. "
          "Use \"-\" for an operand to signify "
          "a nrrd to be read from stdin (a pipe).  Note, however, "
          "that \"-\" can probably only be used once (reliably).\n "
          "* Uses nrrdArithAffine or nrrdArithIterAffine");

static int
unu_affineMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  NrrdIter *minIn, *in, *maxIn, *minOut, *maxOut;
  uint inHidx[5];
  inHidx[0] = hestOptAdd_1_Other(&hopt, NULL, "minIn", &minIn, NULL,
                                 "Lower end of input value range.", nrrdHestIter);
  inHidx[1] = hestOptAdd_1_Other(&hopt, NULL, "in", &in, NULL, "Input value.",
                                 nrrdHestIter);
  inHidx[2] = hestOptAdd_1_Other(&hopt, NULL, "maxIn", &maxIn, NULL,
                                 "Upper end of input value range.", nrrdHestIter);
  inHidx[3] = hestOptAdd_1_Other(&hopt, NULL, "minOut", &minOut, NULL,
                                 "Lower end of output value range.", nrrdHestIter);
  inHidx[4] = hestOptAdd_1_Other(&hopt, NULL, "maxOut", &maxOut, NULL,
                                 "Upper end of output value range.", nrrdHestIter);
  int type;
  hestOptAdd_1_Other(&hopt, "t,type", "type", &type, "default",
                     "type to convert all nrrd inputs to, prior to "
                     "doing operation.  This also determines output type. "
                     "By default (not using this option), the types of the input "
                     "nrrds are left unchanged.",
                     &unrrduHestMaybeTypeCB);
  int clamp;
  hestOptAdd_1_Bool(&hopt, "clamp", "bool", &clamp, "false",
                    "clamp output values to specified output range");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_affineInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);

  NrrdIter *args[5] = {minIn, in, maxIn, minOut, maxOut};
  uint nn = 0;
  for (uint ai = 0; ai < 5; ai++) {
    nn += !!args[ai]->ownNrrd;
  }
  if (nrrdTypeDefault != type) {
    /* they wanted to convert nrrds to some other type first */
    int E = 0;
    Nrrd *ntmp;
    for (uint ai = 0; ai < 5; ai++) {
      if (args[ai]->ownNrrd) {
        if (!E) E |= nrrdConvert(ntmp = nrrdNew(), args[ai]->ownNrrd, type);
        if (!E) nrrdIterSetOwnNrrd(args[ai], ntmp);
      }
    }
    if (E) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error converting input nrrd(s):\n%s", me, err);
      airMopError(mop);
      return 1;
    }
  }

  if (0 == nn) {
    /* actually, there are no nrrds; we represent the functionality
       of the previous stand-alone binary "affine" */
    double valOut;
    valOut = AIR_AFFINE(minIn->val, in->val, maxIn->val, minOut->val, maxOut->val);
    if (clamp) {
      double mmin = AIR_MIN(minOut->val, maxOut->val);
      double mmax = AIR_MAX(minOut->val, maxOut->val);
      valOut = AIR_CLAMP(mmin, valOut, mmax);
    }
    airSinglePrintf(stdout, NULL, 0, "%lg\n", valOut);
  } else {
    /* we have a nrrd output */
    int E;
    if (1 == nn && in->ownNrrd) {
      E = nrrdArithAffine(nout, minIn->val, in->ownNrrd, maxIn->val, minOut->val,
                          maxOut->val, clamp);
    } else {
      E = nrrdArithIterAffine(nout, minIn, in, maxIn, minOut, maxOut, clamp);
    }
    if (E) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error doing ternary operation:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
    unrrduUnuHistoryClear(nout);
    if (getenv(UNRRDU_TEEM_KVP_HISTORY_TRACE_ENV)) {
      // HEY how to make all this less clunky?
      unrrduUnuOutNSPair onsp = {.nout = nout, .name = hopt[outHidx].parmStr};
      NrrdIter *inItr[5] = {minIn, in, maxIn, minOut, maxOut};
      const Nrrd *inNrrd[5];
      unrrduUnuInNSPair insp[5];
      uint numStdin = 0, stdindx = UINT_MAX;
      for (uint ii = 0; ii < 5; ii++) {
        inNrrd[ii] = inItr[ii]->nrrd ? inItr[ii]->nrrd
                                     : (inItr[ii]->ownNrrd ? inItr[ii]->ownNrrd : NULL);
        insp[ii].nin = inNrrd[ii];
        insp[ii].name = hopt[inHidx[ii]].parmStr;
        if (!strcmp("-", insp[ii].name)) {
          numStdin += 1;
          stdindx = ii;
        }
      };
      if (numStdin > 1) {
        fprintf(stderr, "%s: confused by having %u > 1 inputs from stdin\n", me,
                numStdin);
        airMopError(mop);
        return 1;
      }
      // else numStdin == 0 or numStdin == 1
      E = 0;
      if (!numStdin) {
        for (uint ii = 0; ii < 5; ii++) {
          if (!E && insp[ii].nin) E |= unrrduUnuHistoryAdd(onsp, 0, NULL, insp[ii]);
        }
        if (!E) E |= unrrduUnuHistoryAdd(onsp, argc + 1, argv - 1, MY_NULL_INSP);
      } else {
        for (uint ii = 0; ii < 5; ii++) {
          if (!E && insp[ii].nin && ii != stdindx)
            E |= unrrduUnuHistoryAdd(onsp, 0, NULL, insp[ii]);
        }
        if (!E) E |= unrrduUnuHistoryAdd(onsp, argc + 1, argv - 1, insp[stdindx]);
      }
      if (E) {
        airMopAdd(mop, err = biffGetDone(UNRRDU), airFree, airMopAlways);
        fprintf(stderr, "%s: trouble generating history:\n%s", me, err);
        airMopError(mop);
        return 1;
      }
    }
    MY_SAVE(out, nout, NULL);
  }

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(affine, INFO);
