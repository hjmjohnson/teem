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

#include "../air.h"

/* for testing airSprintSize_t and airSprintPtrdiff_t */

int
main(int argc, char *argv[]) {
  char *me;
  char str[AIR_STRLEN_SMALL + 1];
  size_t sz, oldSz;
  ptrdiff_t pd;
  int change;

  AIR_UNUSED(argc);
  me = argv[0];

#define PRINT(P)                                                                        \
  printf("%s: %s %lu %s\n", me, P, AIR_CAST(unsigned long, sz), airSprintSize_t(str, sz))

  sz = 1;
  PRINT("A1");
  do {
    oldSz = sz;
    sz = AIR_SIZE_T(2.4 * sz);
    PRINT("*A");
    change = (oldSz != sz);
  } while (change);
  *((long int *)(&sz)) = -1;
  PRINT("A-1");
  sz += 1;
  PRINT("A++");
  sz -= 1;
  PRINT("A--");
  sz -= 1;
  PRINT("A--");
  sz -= 1;
  PRINT("A--");

#undef PRINT

#define PRINT(P)                                                                        \
  printf("%s: %s %ld %s\n", me, P, AIR_CAST(long, pd), airSprintPtrdiff_t(str, pd))

  pd = 1;
  PRINT("B1");
  do {
    ptrdiff_t od;
    od = pd;
    pd = (ptrdiff_t)(2.4 * pd);
    PRINT("*B");
    change = (od != pd);
  } while (change);

  pd = -1;
  PRINT("B-1");
  do {
    ptrdiff_t od;
    od = pd;
    pd = (ptrdiff_t)(2.4 * pd);
    PRINT("*B");
    change = (od != pd);
  } while (change);

  pd -= 5;
  PRINT("B-5");
  pd += 1;
  PRINT("B++");
  pd += 1;
  PRINT("B++");
  pd += 1;
  PRINT("B++");
  pd += 1;
  PRINT("B++");
  pd += 1;
  PRINT("B++");
  pd += 1;
  PRINT("B++");
  pd += 1;
  PRINT("B++");
  pd += 1;
  PRINT("B++");
  pd += 1;
  PRINT("B++");

  exit(0);
}
