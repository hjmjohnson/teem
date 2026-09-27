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

#define INFO "Unary operation on a nrrd"
static const char *unu_1opInfoL = (INFO ".\n "
                                        "* Uses nrrdArithUnaryOp");

static int
unu_1opMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  int op;
  hestOptAdd_1_Enum(&hopt, NULL, "operator", &op, NULL,
                    "Unary operator. Possibilities include:\n "
                    "\b\bo \"-\": negative (multiply by -1.0)\n "
                    "\b\bo \"r\": reciprocal (1.0/value)\n "
                    "\b\bo \"sin\", \"cos\", \"tan\", \"asin\", \"acos\", \"atan\": "
                    "same as in C\n "
                    "\b\bo \"exp\", \"log\", \"log10\": same as in C\n "
                    "\b\bo \"log1p\", \"expm1\": accurate log(x+1) and exp(x)-1\n "
                    "\b\bo \"log2\": log base 2\n "
                    "\b\bo \"sqrt\", \"cbrt\", \"ceil\", \"floor\": same as in C\n "
                    "\b\bo \"erf\": error function (integral of Gaussian)\n "
                    "\b\bo \"rup\", \"rdn\": round up or down to integral value\n "
                    "\b\bo \"abs\": absolute value\n "
                    "\b\bo \"sgn\": -1, 0, 1 if value is <0, ==0, or >0\n "
                    "\b\bo \"exists\": 1 iff not NaN or +/-Inf, 0 otherwise\n "
                    "\b\bo \"rand\": random value in [0.0,1.0), "
                    "no relation to input\n "
                    "\b\bo \"nrand\": random sample from normal distribution with "
                    "mean 0.0 and stdv 1.0, no relation to input\n "
                    "\b\bo \"if\": if input is non-zero, 1, else 0\n "
                    "\b\bo \"0\": output always 0\n "
                    "\b\bo \"1\": output always 1",
                    nrrdUnaryOp);
  uint seed;
  uint seedOI = hestOptAdd_1_UInt(&hopt, "s,seed", "seed", &seed, "0",
                                  "seed value for random number generator (RNG), to "
                                  "enable repeatable results between runs, or, "
                                  "by not using this option, the RNG seeding will be "
                                  "based on the current time");
  int type;
  hestOptAdd_1_Other(&hopt, "t,type", "type", &type, "default",
                     "convert input nrrd to this type prior to "
                     "doing operation.  Useful when desired output is float "
                     "(e.g., with log1p), but input is integral. By default "
                     "(not using this option), the types of "
                     "the input nrrds are left unchanged.",
                     &unrrduHestMaybeTypeCB);
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_1opInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);

  Nrrd *ntmp = NULL;
  if (nrrdTypeDefault != type) {
    /* they requested conversion to another type prior to the 1op */
    airMopAdd(mop, ntmp = nrrdNew(), nrrdNuke_mop, airMopAlways);
    if (nrrdConvert(ntmp, nin, type)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error converting input nrrd:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
  } else {
    ntmp = nin;
  }
  /* see note in 2op.c about the hazards of trying to be clever
  ** about minimizing the seeding of the RNG
  ** if (nrrdUnaryOpRand == op
  **     || nrrdUnaryOpNormalRand == op) {
  */
  if (hestSourceDefault != hopt[seedOI].source) {
    airSrandMT(seed);
  } else {
    /* got no request for specific seed */
    airSrandMT(AIR_UINT(airTime()));
  }
  // to avoid valgrind warnings about memory-in-use-at-exit
  airMopAdd(mop, airRandMTStateGlobal, airRandMTStateNix_mop, airMopAlways);
  if (nrrdArithUnaryOp(nout, op, ntmp)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error doing unary operation:\n%s", me, err);
    airMopError(mop);
    return 1;
  }
  /* if we had to create ntmp with nrrdConvert, it will be mopped,
     otherwise ntmp is an alias for nin, which will also be mopped */
  MY_HISTORY(nout, nin);
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(1op, INFO);
