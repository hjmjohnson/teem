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

#include "../pull.h"

const char *info = ("Tests parsing of energy, and its methods.");

int
main(int argc, const char *argv[]) {
  const char *me = argv[0];
  hestOpt *hopt = NULL;
  airArray *mop = airMopNew();
  pullEnergySpec *ensp;
  hestOptAdd_1_Other(&hopt, "energy", "spec", &ensp, NULL,
                     "specification of force function to use", pullHestEnergySpec);
  hestParseOrExit(hopt, argc - 1, argv + 1, NULL, me, info);
  airMopAdd(mop, hopt, (airMopper)hestOptFree, airMopAlways);
  airMopAdd(mop, hopt, (airMopper)hestParseFree, airMopAlways);

  fprintf(stderr, "%s: parsed energy \"%s\", with %u parms.\n", me, ensp->energy->name,
          ensp->energy->parmNum);
  for (unsigned int pi = 0; pi < ensp->energy->parmNum; pi++) {
    fprintf(stderr, "parm[%u] == %g\n", pi, ensp->parm[pi]);
  }
  fprintf(stderr, "\n");

  unsigned int nn = 600;
  double supp = 1.0;
  for (unsigned int xi = 1; xi < nn; xi++) {
    double xx, x0, x1, ee, ff, e0, e1, dummy;
    xx = AIR_AFFINE(0, xi, nn, 0, supp);
    x1 = AIR_AFFINE(0, xi + 1, nn, 0, supp);
    x0 = AIR_AFFINE(0, xi - 1, nn, 0, supp);
    e1 = ensp->energy->eval(&dummy, x1, ensp->parm);
    e0 = ensp->energy->eval(&dummy, x0, ensp->parm);
    ee = ensp->energy->eval(&ff, xx, ensp->parm);
    printf("%g %g %g %g\n", xx, ee, ff, (e1 - e0) / (x1 - x0));
  }

  airMopOkay(mop);
  return 0;
}
