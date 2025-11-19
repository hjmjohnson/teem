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

#define INFO "Affine (lerp) mapping on 5 nrrds or constants"
static const char *_unrrdu_affineInfoL
  = (INFO ". All the 5 arguments can be either nrrds or single "
          "floating-point values.  When all args are single values, this "
          "is subsuming the functionality of the previous stand-alone "
          "\"affine\" program. "
          "Use \"-\" for an operand to signify "
          "a nrrd to be read from stdin (a pipe).  Note, however, "
          "that \"-\" can probably only be used once (reliably).\n "
          "* Uses nrrdArithAffine or nrrdArithIterAffine");

static int
unrrdu_affineMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  NrrdIter *minIn, *in, *maxIn, *minOut, *maxOut;
  hestOptAdd_1_Other(&hopt, NULL, "minIn", &minIn, NULL,
                     "Lower end of input value range.", nrrdHestIter);
  hestOptAdd_1_Other(&hopt, NULL, "in", &in, NULL, "Input value.", nrrdHestIter);
  hestOptAdd_1_Other(&hopt, NULL, "maxIn", &maxIn, NULL,
                     "Upper end of input value range.", nrrdHestIter);
  hestOptAdd_1_Other(&hopt, NULL, "minOut", &minOut, NULL,
                     "Lower end of output value range.", nrrdHestIter);
  hestOptAdd_1_Other(&hopt, NULL, "maxOut", &maxOut, NULL,
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
  OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, _unrrdu_affineInfoL);
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
    printf("%g\n", valOut);
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
    SAVE(out, nout, NULL);
  }

  airMopOkay(mop);
  return 0;
}

UNRRDU_CMD(affine, INFO);
