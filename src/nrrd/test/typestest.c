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

#include "../nrrd.h"

int
main(int argc, char *argv[]) {
  int tt;

  AIR_UNUSED(argc);
  airSetLocaleFromEnvOrExit(argv[0], airStderr());
  for (tt = nrrdTypeChar; tt <= nrrdTypeDouble; tt++) {
    printf(" ----- %s -----\n", airEnumStr(nrrdType, tt));
    printf("nrrdTypeSize: %d\n", (int)nrrdTypeSize[tt]);
    printf("nrrdTypeIsUnsigned: %d\n", nrrdTypeIsUnsigned[tt]);
    printf("nrrdTypeIsIntegral: %d\n", nrrdTypeIsIntegral[tt]);
    airSinglePrintf(stdout, NULL, 0, "nrrdTypeMin: %lg\n", nrrdTypeMin[tt]);
    airSinglePrintf(stdout, NULL, 0, "nrrdTypeMax: %lg\n", nrrdTypeMax[tt]);
  }
  printf("sizeof(size_t) = %d\n", (int)sizeof(size_t));

  exit(0);
}
