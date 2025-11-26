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

#define INFO "Brighten or darken values with a gamma"
static const char *unu_gammaInfoL
  = (INFO ". Just as in xv, the gamma value here is actually the "
          "reciprocal of the exponent actually used to transform "
          "the values. Can also do the non-linear transforms used "
          "in the sRGB standard (see https://en.wikipedia.org/wiki/SRGB)\n "
          "* Uses nrrdArithGamma or nrrdArithGammaSRGB");

static int
unu_gammaMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  char *GammaS;
  hestOptAdd_1_String(&hopt, "g,gamma", "gamma", &GammaS, NULL,
                      "gamma > 1.0 brightens; gamma < 1.0 darkens. "
                      "Negative gammas invert values (like in xv). "
                      "Or, can used \"srgb\" for ~2.2 gamma of sRGB encoding, or "
                      "\"1/srgb\" for ~0.455 gamma of inverse sRGB encoding");
  double min, max;
  hestOptAdd_1_Double(&hopt, "min,minimum", "value", &min, "nan",
                      "Value to implicitly map to 0.0 prior to calling pow(). "
                      "Defaults to lowest value found in input nrrd.");
  hestOptAdd_1_Double(&hopt, "max,maximum", "value", &max, "nan",
                      "Value to implicitly map to 1.0 prior to calling pow(). "
                      "Defaults to highest value found in input nrrd.");
  int blind8BitRange;
  hestOptAdd_1_Bool(&hopt, "blind8", "bool", &blind8BitRange,
                    nrrdStateBlind8BitRange ? "true" : "false",
                    "Whether to know the range of 8-bit data blindly "
                    "(uchar is always [0,255], signed char is [-128,127]).");
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_gammaInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  int srgb, forward;
  double Gamma;
  if (!strcmp(GammaS, "srgb")) {
    srgb = AIR_TRUE;
    forward = AIR_TRUE;
  } else if (!strcmp(GammaS, "1/srgb")) {
    srgb = AIR_TRUE;
    forward = AIR_FALSE;
  } else {
    srgb = AIR_FALSE;
    forward = AIR_FALSE;
    if (1 != airSingleSscanf(GammaS, "%lf", &Gamma)) {
      fprintf(stderr,
              "%s: couldn't parse gamma \"%s\" as double, and wasn't either "
              "\"srgb\" or \"1/srgb\"\n",
              me, GammaS);
      airMopError(mop);
      return 1;
    }
  }
  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);

  NrrdRange *range = nrrdRangeNew(min, max);
  airMopAdd(mop, range, nrrdRangeNix_mop, airMopAlways);
  nrrdRangeSafeSet(range, nin, blind8BitRange);
  int E;
  if (srgb) {
    E = nrrdArithSRGBGamma(nout, nin, range, forward);
  } else {
    E = nrrdArithGamma(nout, nin, range, Gamma);
  }
  if (E) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error doing gamma:\n%s", me, err);
    airMopError(mop);
    return 1;
  }
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(gamma, INFO);
