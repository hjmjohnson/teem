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

/* NOTE about C-locale-emulation in this code (BUT FIRST read air/fpio.c)
Internally, hest achieves C-locale-emulation in all parsing and printing of FP
values. One could very well choose that for user-defined objects parsed via
callback by hest, such as the "location" below, in which hest itself is not
doing FP I/O, the user can choose to let the code be locale-dependent: after
all, they are the user and they are choosing their locale.

This file is an example of doing that: the number separator is ':' so that it will not
conflict with either '.' or ',' as the decimal seprator.
BUT (fuuuuu) THE DEFAULT STRINGS supplied to hest then also have to vary with the locale,
which creates new work for the user.  And, it is a new kind of work, because hest
callbacks have only a parse function, not a print function.  As demonstrated here,
if user code wants the smarts about printing floats in a concise yet accurate way
(as now done for air/airDtoS by air/fp2str), or wants consistent printing/interpretation
of special values (also done by air/fp2str), but doesn't want C-locale-emulation,
then Teem has decided to be unhelpful.  We have to use conversion "%.17g" with printf,
and hope that printf/sscanf are doing the right thing for special values.
*/

int
parse(void *_ptr, const char *str, char *err) {
  double *ptr;
  int ret;

  ptr = _ptr;
  ret = sscanf(str, "%lf:%lf", ptr + 0, ptr + 1); /* yes, locale-dependant */
  if (2 != ret) {
    sprintf(err, "parsed %d (not 2) doubles", ret);
    return 1;
  }
  return 0;
}

hestCB cbinfo = {2 * sizeof(double), "location", parse, NULL};

/* print a location into buff (with size buffSize),
   and return the length printed */
size_t
locSprint(char *buff, size_t buffSize, const double loc[2]) {

  snprintf(buff, buffSize, "%.17g:%.17g", loc[0], loc[1]); /* yes, locale-dependant */
  /* the return value of snprintf is how much it would have written
     into an unlimited buffer, which isn't actually want we want to
     know for the purposes of advancing a "sprint here next" pointer */
  return strlen(buff);
}

int
main(int argc, const char **argv) {
  hestOpt *opt = NULL;

  /* to debug how we're doing C-locale-emulation */
  airSetLocaleFromEnvOrExit(argv[0], airStderr());

  /* for these defaults we're being lazy and only supplying integer-valued coordinates */
  double single[2];
  hestOptAdd_1_Other(&opt, "A", "x,y", single, "30:50", "testing A", &cbinfo);
  double triple[6];
  hestOptAdd_3_Other(&opt, "B", "x1:y1 x2:y2 x3:y3", triple, "1:2 3:4 5:6", "testing B",
                     &cbinfo);
  /*
  // 2025 re-write: only allow simple scalar types as single variadic parm (not Other)
  double maybe[2];
  hestOptAdd(&opt, "C", "mx,my", airTypeOther, 0, 1, maybe, "-0.1,-0.2",
             "testing C. The utility of this can be better "
             "demonstrated in the following manner:\n "
             "- wash the dishes\n "
             "- put the dishes in the cupboard\n "
             "- watch football on TV\n "
             "- remember to walk the dog",
             NULL, NULL, &cbinfo);
  */
  /* for this default we accept the challenge of sprinting non-integer coords,
     and want to do it without a dynamic allocation, since there normally isn't
     any allocation to state the default string, but wow it's annoying. */
  char manyDef[AIR_STRLEN_SMALL + 1];
  char *manyDefPtr = manyDef;
  size_t incr, manyDefSz = sizeof(manyDef);
  incr = locSprint(manyDefPtr, manyDefSz, (const double[2]){8.5, 8.25});
  manyDefPtr += incr;
  manyDefSz -= incr;
  snprintf(manyDefPtr, manyDefSz, "  ");
  incr = strlen(manyDefPtr);
  manyDefPtr += incr;
  manyDefSz -= incr;
  locSprint(manyDefPtr, manyDefSz, (const double[2]){7.75, 7.128});
  double *many;
  unsigned howMany;
  hestOptAdd_Mv_Other(&opt, "D", "nx:ny", 1, -1, &many, manyDef, "testing D", &howMany,
                      &cbinfo);
  int N;
  hestOptAdd_1_Int(&opt, "int", "N", &N, NULL, "an integer");

  hestParseOrExit(opt, argc - 1, argv + 1, NULL /* hparm */, argv[0],
                  "info about hest/text/ex4");
  printf("single= %g:%g\n", single[0], single[1]); /* yes, locale-dependant */
  printf("triple= %g:%g  %g:%g  %g:%g\n",          /* yes, locale-dependant */
         triple[0], triple[1], triple[2], triple[3], triple[4], triple[5]);
  /* printf("maybe: (%g,%g)\n", maybe[0], maybe[1]); */
  printf("many(%u)=", howMany);
  for (unsigned int i = 0; i <= howMany - 1; i++) {
    printf("  %g:%g", many[0 + 2 * i], many[1 + 2 * i]); /* yes, locale-dependant */
  }
  printf("\n");

  hestParseFree(opt);
  hestOptFree(opt);
  exit(0);
}
