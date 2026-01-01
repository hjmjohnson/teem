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

/*
  For testing how much airSinglePrintf can print floats or doubles with enough
  precision to recover all bits
 */

static void
usage(const char *me) {
  /*                      0     1       2       3     4  (5) */
  fprintf(stderr, "usage: %s f|d|F|D <FPval> <log2> <fmt>\n", me);
  exit(1);
}

int
main(int argc, const char *argv[]) {
  const char *me;

  me = argv[0];
  if (5 != argc) usage(me);
  int verbose = 0;
  int dblPrec;
  if (!strcmp(argv[1], "f")) {
    dblPrec = 0;
  } else if (!strcmp(argv[1], "d")) {
    dblPrec = 1;
  } else if (!strcmp(argv[1], "F")) {
    verbose = 1;
    dblPrec = 0;
  } else if (!strcmp(argv[1], "D")) {
    verbose = 1;
    dblPrec = 1;
  } else {
    usage(me);
  }
  const char *dblStr = argv[2];
  const char *lgStr = argv[3];
  const char *fmt = argv[4];

  double dbl0, dbl1;
  if (1 != airSingleSscanf(dblStr, "%lf", &dbl0)) {
    fprintf(stderr, "%s: couldn't parse \"%s\" as double\n", me, dblStr);
    exit(1);
  }
  float flt0 = (float)dbl0;
  airDouble adbl = {.d = dbl0};
  airFloat aflt = {.f = flt0};

  unsigned int lg;
  if (1 != sscanf(lgStr, "%u", &lg)) {
    fprintf(stderr, "%s: couldn't parse \"%s\" as uint\n", me, lgStr);
    exit(1);
  }
  size_t I, N = ((size_t)1) << lg;

  if ('%' != fmt[0]) {
    /* some whole string to test with */
    size_t buffSize = 2 * strlen(fmt) + 1;
    char *buff = AIR_CALLOC(buffSize, char);
    airSinglePrintf(NULL, buff, buffSize, fmt, dbl0);
    printf("%s: sprint(buff, |%s|, %.17g) = |%s|\n", me, fmt, dbl0, buff);
    free(buff);
  } else {
    /* assuming `fmt` is JUST the conversion sequence, most usefully %g or %lg, and scan
       values to test */
    printf("%s: we shall begin scanning\n", me);
    char buff[AIR_STRLEN_SMALL + 1];
    size_t buffSize = AIR_STRLEN_SMALL + 1;
    char buff2[AIR_STRLEN_SMALL + 1];
    for (I = 0; I < N; I++) {
      if (dblPrec) {
        airSinglePrintf(NULL, buff, buffSize, fmt, adbl.d);
        if (verbose) {
          printf("%s: %.17g ---aSPrint(%s)---> \"%s\"\n", me, adbl.d, fmt, buff);
          sprintf(buff2, fmt, adbl.d);
          printf("%s: %.17g ---sprintf(%s)---> \"%s\"\n\n", me, adbl.d, fmt, buff2);
        }
        airSingleSscanf(buff, "%lf", &dbl1);
        if (AIR_EXISTS(dbl0) && dbl1 != adbl.d) {
          printf(
            "%s: %.17g ---aSPrint(%s)---> \"%s\" ---sscanf---> %.17g != start val!\n",
            me, adbl.d, fmt, buff, dbl1);
          break;
        }
        adbl.i++;
      } else {
        float flt1;
        airSinglePrintf(NULL, buff, buffSize, fmt, aflt.f);
        airSingleSscanf(buff, "%f", &flt1);
        if (verbose) {
          printf("%s: %.9g ---aSPrint(%s)---> \"%s\"\n", me, aflt.f, fmt, buff);
          sprintf(buff2, fmt, aflt.f);
          printf("%s: %.9g ---sprintf(%s)---> \"%s\"\n\n", me, aflt.f, fmt, buff2);
        }
        if (AIR_EXISTS(dbl0) && flt1 != aflt.f) {
          printf("%s: %.9g ---aSPrint(%s)---> \"%s\" ---sscanf---> %.9g != start val!\n",
                 me, aflt.f, fmt, buff, flt1);
          break;
        }
        aflt.i++;
      }
      if (!AIR_EXISTS(dbl0)) {
        printf("%s: special value \"%s\" --> %g --> \"%s\" --> %g (done)\n", me, dblStr,
               dbl0, buff, dbl1);
        exit(0);
      }
    }
    if (I == N) {
      printf("%s: good round-trip for 2^%u values in [ %s , %s ]\n", me, lg, dblStr,
             buff);
    }
  }

  exit(0);
}
