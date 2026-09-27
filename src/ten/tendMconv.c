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

#define INFO "convert from one model to another"
static const char *mconvInfoL = (INFO ". More docs here.");

static int
mconvMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nin;
  uint inHidx = hestOptAdd_1_Other(&hopt, "i", "nin", &nin, "-",
                                   "input nrrd of model parms", nrrdHestNrrd);
  hopt[inHidx].priority -= 10;
  char *modelDstS;
  hestOptAdd_1_String(&hopt, "mo", "model", &modelDstS, NULL,
                      "which model to convert to");
  char *modelSrcS;
  hestOptAdd_1_String(&hopt, "mi", "model", &modelSrcS, "",
                      "model converting from; if not set, will try to determine "
                      "from input nrrd");
  char *outS;
  hestOptAdd_1_String(&hopt, "o", "nout", &outS, "-", "output nrrd of model parms");

  airArray *mop = airMopNew();
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  int pret;
  char *perr, *err;
  USAGE_JUSTPARSE(mconvInfoL);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);

  const tenModel *modelDst;
  int saveB0;
  if (tenModelParse(&modelDst, &saveB0, AIR_FALSE, modelDstS)) {
    airMopAdd(mop, err = biffGetDone(TEN), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble parsing model \"%s\":\n%s\n", me, modelDstS, err);
    airMopError(mop);
    return 1;
  }
  if (saveB0) {
    printf("%s: warning: saving B0 is determined by input nrrd "
           "having B0 info.\n",
           me);
  }
  const tenModel *modelSrc;
  if (airStrlen(modelSrcS)) {
    if (tenModelParse(&modelSrc, &saveB0, AIR_FALSE, modelSrcS)) {
      airMopAdd(mop, err = biffGetDone(TEN), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble parsing model \"%s\":\n%s\n", me, modelSrcS, err);
      airMopError(mop);
      return 1;
    }
  } else {
    modelSrc = NULL;
  }
  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (tenModelConvert(nout, NULL, modelDst, nin, modelSrc)) {
    airMopAdd(mop, err = biffGetDone(TEN), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble converting:\n%s\n", me, err);
    airMopError(mop);
    return 1;
  }
  if (nrrdSave(outS, nout, NULL)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble writing:\n%s\n", me, err);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  return 0;
}
MY_TEND_CMD(mconv, INFO);
