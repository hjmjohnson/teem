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

#include "../hest.h"

char *tmplInfo = ("shows minimal usage of hest with mops");

int
main(int argc, const char *argv[]) {
  const char *me = argv[0];
  hestOpt *hopt;
  hestParm *hparm;
  airArray *mop = airMopNew();
  hparm = hestParmNew();
  airMopAdd(mop, hparm, (airMopper)hestParmFree, airMopAlways);

  hopt = NULL;
  char *name;
  hestOptAdd_1_String(&hopt, NULL, "name", &name, NULL, "name of something");
  int size[3];
  hestOptAdd_3_Int(&hopt, "s", "sx sy sz", size, "128 128 128",
                   "dimensions of something");
  hestParseOrExit(hopt, argc - 1, argv + 1, hparm, me, tmplInfo);
  airMopAdd(mop, hopt, (airMopper)hestOptFree, airMopAlways);
  airMopAdd(mop, hopt, (airMopper)hestParseFree, airMopAlways);

  airMopOkay(mop);
  exit(0);
}
