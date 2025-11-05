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

#include "../nrrd.h"

const char *otsuInfo = ("demonstrates nrrd's Otsu thresholding");

int
main(int argc, const char *argv[]) {
  const char *me = argv[0];
  airArray *mop = airMopNew();
  hestParm *hparm = hestParmNew();
  hestOpt *hopt = NULL;
  airMopAdd(mop, hparm, (airMopper)hestParmFree, airMopAlways);
  Nrrd *nhist;
  hestOptAdd_1_Other(&hopt, "i", "nhist", &nhist, NULL, "input histogram", nrrdHestNrrd);
  double expo;
  hestOptAdd_1_Double(&hopt, "e", "expo", &expo, "2.0",
                      "exponent to use for variance calculation");
  hestParseOrDie(hopt, argc - 1, argv + 1, hparm, me, otsuInfo);
  airMopAdd(mop, hopt, (airMopper)hestOptFree, airMopAlways);
  airMopAdd(mop, hopt, (airMopper)hestParseFree, airMopAlways);

  double thresh;
  if (nrrdHistoThresholdOtsu(&thresh, nhist, expo)) {
    char *err;
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  fprintf(stderr, "%s: threshold = %g\n", me, thresh);

  airMopOkay(mop);
  exit(0);
}
