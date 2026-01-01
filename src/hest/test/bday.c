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

#include "../hest.h"

unsigned int
runexp(unsigned int *year, unsigned int N, unsigned int M) {
  unsigned int i, m;

  memset(year, 0, N * sizeof(unsigned int));
  for (m = 0; m < M; m++) {
    year[airRandInt(N)]++;
  }
  for (i = 0; i < N; i++) {
    if (year[i] > 1) {
      return 1;
    }
  }
  return 0;
}

char *info = ("simulates `M` people in a room finding out if two or more "
              "of them share a birthday.  For fun, can vary the number `N` "
              "of days in the year.");

int
main(int argc, const char *argv[]) {
  airArray *mop;
  hestOpt *hopt = NULL;
  unsigned int yes, P, i, E, N, M, *year;
  const char *me;
  double calc, calcOld, chng, crct;

  me = argv[0];
  mop = airMopNew();
  hestParm *hparm = hestParmMopNew();
  hparm->responseFileEnable = AIR_TRUE;
  airMopAdd(mop, hparm, hestParmFree_mop, airMopAlways);
  hestOptAdd_1_UInt(&hopt, "N", "days", &N, "365", "# of days in year");
  /* E != P */
  hestOptAdd_1_UInt(&hopt, "E", "exps", &P, "100000",
                    "number of experiments after which to print out newly "
                    "computed probability");
  double chngEps;
  hestOptAdd_1_Double(&hopt, "eps", "eps", &chngEps, "0.0000001",
                      "Stop running experiments when relative change in computed "
                      "probability goes below this");
  hestOptAdd(&hopt, NULL, "M", airTypeInt, 1, 1, &M, NULL, "# of people in room");
  hestParseOrExit(hopt, argc - 1, argv + 1, hparm, me, info);
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);

  if (!(N > 1 && M > 0 && P > 1)) {
    fprintf(stderr, "%s: need both M, P all > 1, M > 0\n", me);
    airMopError(mop);
    exit(1);
  }
  if (!(year = AIR_CALLOC(N, unsigned int))) {
    fprintf(stderr, "%s: couldn't calloc %u uints\n", me, N);
    airMopError(mop);
    exit(1);
  }
  airMopMem(mop, year, airMopAlways);

  crct = 1;
  for (i = 0; i < M; i++) {
    crct *= (double)(N - i) / N;
  }
  crct = 1 - crct;
  yes = 0;
  E = 0;
  airSrandMT(AIR_UINT(airTime()));
  chng = calc = 1; /* just to thwart convergence */
  do {
    double blah;
    yes += runexp(year, N, M);
    E++;
    calcOld = calc;
    calc = (double)yes / E;
    blah = fabs(calcOld - calc) / ((calcOld + calc) / 2);
    chng = (chng + (AIR_EXISTS(blah) ? blah : 1)) / 2;
    if (!(E % P)) {
      printf("P = %10u/%10u = %22.20f =?= %22.20f  (chng=%g)\n", yes, E, calc, crct,
             chng);
    }
  } while (chng > chngEps);
  printf("P = %10u/%10u = %22.20f =?= %22.20f  (chng=%g)\n", yes, E, calc, crct, chng);

  airMopOkay(mop);
  exit(0);
}
