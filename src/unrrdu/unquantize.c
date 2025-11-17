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

#define INFO "Recover floating point values from quantized data"
static const char *_unrrdu_unquantizeInfoL
  = (INFO ". Uses the oldMin and oldMax fields in the Nrrd of quantized values "
          "to regenerate approximate versions of the original unquantized values. "
          "Can also override these with \"-min\" and \"-max\".\n "
          "* Uses nrrdUnquantize");

static int
unrrdu_unquantizeMain(int argc, const char **argv, const char *me,
                      const hestParm *_hparm) {
  hestOpt *hopt = NULL;
  /*
   * with new nrrdHestNrrdNoTTY, can let "-" be default input as normal; no more need
   * for this awkwardness:
   * hestOptAdd(&hopt, "i,input", "nin", airTypeOther, 1, 1, &nin, NULL,
   *            "input nrrd.  That this argument is required instead of "
   *            "optional, as with most unu commands, is a quirk caused by the "
   *            "need to have \"unu unquantize\" generate usage info, combined "
   *            "with the fact that all the other arguments have sensible "
   *            "defaults",
   *            NULL, NULL, nrrdHestNrrd);
   */
  Nrrd *nin;
  hestOptAdd_1_Other(&hopt, "i,input", "nin", &nin, "-", "input nrrd",
                     nrrdHestNrrdNoTTY);
  double oldMin, oldMax;
  hestOptAdd_1_Double(&hopt, "min,minimum", "value", &oldMin, "nan",
                      "Lowest value prior to quantization.  Defaults to "
                      "nin->oldMin if it exists, otherwise 0.0");
  hestOptAdd_1_Double(&hopt, "max,maximum", "value", &oldMax, "nan",
                      "Highest value prior to quantization.  Defaults to "
                      "nin->oldMax if it exists, otherwise 1.0");
  int dbl;
  hestOptAdd_Flag(&hopt, "double", &dbl, "Use double for output type, instead of float");
  char *out;
  OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for YES some hparm modification)
  hestParm *hparm = hestParmCopy(_hparm);
  hparm->noArgsIsNoProblem = AIR_TRUE; // thanks to nrrdHestNrrdNoTTY
  hestParseOrExit(hopt, argc, argv, hparm, me, _unrrdu_unquantizeInfoL);
  airArray *mop = unrrduMopNew(hopt, hparm);
  char *err;

  if (AIR_EXISTS(oldMin)) nin->oldMin = oldMin;
  if (AIR_EXISTS(oldMax)) nin->oldMax = oldMax;
  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, (airMopper)nrrdNuke, airMopAlways);
  if (nrrdUnquantize(nout, nin, dbl ? nrrdTypeDouble : nrrdTypeFloat)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error unquantizing nrrd:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

UNRRDU_CMD(unquantize, INFO);
