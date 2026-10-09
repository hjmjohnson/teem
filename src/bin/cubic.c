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

#include <teem/ell.h>

char *me;

void
usage(void) {
  /*                      0   1   2   3   (4) */
  fprintf(stderr, "usage: %s <A> <B> <C>\n", me);
  fprintf(stderr, "for cubic x^3 + Ax^2 + Bx + C == 0\n");
  exit(1);
}

int
main(int argc, char **argv) {
  char buf[512], decibuff[AIR_STRLEN_SMALL + 1];
  const char *DS;
  double ans0, ans1, ans2, A, B, C, ABC[3];
  int ret;
  double r[3];
  char nb[6][AIR_STRLEN_SMALL + 1];

  me = argv[0];
  if (argc != 4) {
    usage();
  }
  airSetLocaleFromEnvOrExit(argv[0], airStdout());
  DS = airDecimalSeparatorLearn(decibuff);
  snprintf(buf, sizeof(buf), "%s %s %s", argv[1], argv[2], argv[3]);
  /* not sure why I packed the args back into one string ...
     ... but this gives reason to try airParseStrDdecisep.
     This little demo program ends up being a good demo of  how the requirement
     of sending all FP io through locale-compensating code, even if really
     simple, ends up being a bit cumbersome . . . */
  /* if (3 != sscanf(buf, "%lf %lf %lf", &A, &B, &C)) { */
  if (3 != airParseStrDdecisep(ABC, buf, " ", DS, 3)) {
    fprintf(stderr, "%s: couldn't parse 3 floats from command line\n", me);
    exit(1);
  }
  A = ABC[0];
  B = ABC[1];
  C = ABC[2];
  printf("%s: got A = %s    B = %s    C = %s\n", me, airDtoS(nb[0], A, DS),
         airDtoS(nb[1], B, DS), airDtoS(nb[2], C, DS));

  ell_debug = AIR_TRUE;
  ret = ell_cubic(r, A, B, C, AIR_TRUE);
  ans0 = C + r[0] * (B + r[0] * (A + r[0]));
  switch (ret) {
  case ell_cubic_root_single:
    printf("1 single root: %s -> %s\n", airDtoS(nb[0], r[0], DS),
           airDtoS(nb[1], ans0, DS));
    break;
  case ell_cubic_root_triple:
    printf("1 triple root: %s -> %s\n", airDtoS(nb[0], r[0], DS),
           airDtoS(nb[1], ans0, DS));
    break;
  case ell_cubic_root_single_double:
    ans1 = C + r[1] * (B + r[1] * (A + r[1]));
    printf("1 single root %s -> %s, 1 double root %s -> %s\n", airDtoS(nb[0], r[0], DS),
           airDtoS(nb[1], ans0, DS), airDtoS(nb[2], r[1], DS), airDtoS(nb[3], ans1, DS));
    break;
  case ell_cubic_root_three:
    ans1 = C + r[1] * (B + r[1] * (A + r[1]));
    ans2 = C + r[2] * (B + r[2] * (A + r[2]));
    printf("3 distinct roots:\n %s -> %s\n %s -> %s\n %s -> %s\n",
           airDtoS(nb[0], r[0], DS), airDtoS(nb[1], ans0, DS), airDtoS(nb[2], r[1], DS),
           airDtoS(nb[3], ans1, DS), airDtoS(nb[4], r[2], DS), airDtoS(nb[5], ans2, DS));
    break;
  default:
    printf("%s: something fatally wacky happened\n", me);
    exit(1);
  }
  exit(0);
}
