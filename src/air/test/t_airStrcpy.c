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

#include "../air.h"

int
main(int argc, const char *argv[]) {
  const char *me = argv[0];
  if (3 != argc) {
    /*                       0 1  2 (3) */
    fprintf(stderr, "usage: %s N str\n", me);
    exit(1);
  }
  const char *NStr = argv[1];
  const char *src = argv[2];
  unsigned int Nui;
  if (1 != sscanf(NStr, "%u", &Nui)) {
    fprintf(stderr, "%s: couldn't parse \"%s\" as uint\n", me, NStr);
    exit(1);
  }
  size_t N = Nui;
  char *ret, *dst;
  if (N) {
    dst = AIR_CALLOC(N, char);
  } else {
    dst = NULL;
  }
  /* char *airStrcpy(char *dst, size_t dstSize, const char *src) */
  ret = airStrcpy(dst, N, !strcmp(src, "NULL") ? NULL : src);
  printf("(%s) |%s|=%zu\n", (!ret ? "NULL" : (ret == dst ? "dst" : "???")),
         dst ? dst : "NULL", dst ? strlen(dst) : 0);

  free(dst);
  exit(0);
}
