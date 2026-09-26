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

#include <locale.h>
#include <string.h>
#include <teem/air.h>

/*
** Tests:
** airSingleSscanf, airParseStrD, and airSinglePrintf use '.' as the decimal
** separator under a decimal-comma LC_NUMERIC locale, and round-trip.
** Returns 77 (skip) when no decimal-comma locale is installed.
*/

int
main(int argc, const char *argv[]) {
  const char *me = argv[0];
  /* locales whose LC_NUMERIC uses ',' as the decimal separator */
  const char *commaLocales[] = {"de_DE.UTF-8", "fr_FR.UTF-8", "nl_NL.UTF-8",
                                "it_IT.UTF-8"};
  unsigned int li, tested = 0;
  int failed = 0;

  AIR_UNUSED(argc);

  for (li = 0; li < AIR_CAST(unsigned int, sizeof(commaLocales) / sizeof(char *));
       li++) {
    double dval = -1.0;
    float fval = -1.0f;
    if (!setlocale(LC_NUMERIC, commaLocales[li])) {
      continue; /* locale not installed here */
    }
    tested++;

    /* sanity: confirm the locale really is decimal-comma, else it proves nothing */
    if (1 == sscanf("0.878906", "%lf", &dval) && dval > 0.5) {
      /* platform's sscanf ignored the comma locale; skip this one */
      continue;
    }

    dval = -1.0;
    if (1 != airSingleSscanf("0.878906", "%lf", &dval)
        || !AIR_IN_OP(0.878, dval, 0.879)) {
      fprintf(stderr,
              "%s: airSingleSscanf(\"0.878906\",%%lf) = %.9g under %s (want 0.878906)\n",
              me, dval, commaLocales[li]);
      failed = 1;
    }
    if (1 != airSingleSscanf("3.5", "%f", &fval) || !AIR_IN_OP(3.4f, fval, 3.6f)) {
      fprintf(stderr, "%s: airSingleSscanf(\"3.5\",%%f) = %.9g under %s (want 3.5)\n",
              me, (double)fval, commaLocales[li]);
      failed = 1;
    }
    /* a value inside double range but past FLT_MAX must fail a "%f" parse
       rather than silently narrow to infinity */
    fval = -1.0f;
    if (0 != airSingleSscanf("1e300", "%f", &fval)) {
      fprintf(
        stderr,
        "%s: airSingleSscanf(\"1e300\",%%f) succeeded = %.9g under %s (want fail)\n", me,
        (double)fval, commaLocales[li]);
      failed = 1;
    }
    dval = -1.0;
    if (1 != airSingleSscanf("1e300", "%lf", &dval) || !AIR_IN_OP(1e299, dval, 1e301)) {
      fprintf(stderr,
              "%s: airSingleSscanf(\"1e300\",%%lf) = %.9g under %s (want 1e300)\n", me,
              dval, commaLocales[li]);
      failed = 1;
    }
    dval = -1.0;
    if (1 != airParseStrD(&dval, "0.878906", " ", 1) || !AIR_IN_OP(0.878, dval, 0.879)) {
      fprintf(stderr, "%s: airParseStrD(\"0.878906\") = %.9g under %s (want 0.878906)\n",
              me, dval, commaLocales[li]);
      failed = 1;
    }
    /* write side: airSinglePrintf must emit '.' (never the locale's ',')
       and the output must round-trip through airSingleSscanf */
    {
      char buff[AIR_STRLEN_SMALL + 1];
      double back = -1.0;
      if (airSinglePrintf(NULL, buff, sizeof(buff), "%lg", 0.878906) < 0
          || strchr(buff, ',')                        /* */
          || 1 != airSingleSscanf(buff, "%lf", &back) /* */
          || !AIR_IN_OP(0.878, back, 0.879)) {
        fprintf(stderr, "%s: airSinglePrintf(0.878906) = \"%s\" -> %.9g under %s\n", me,
                buff, back, commaLocales[li]);
        failed = 1;
      }
    }
  }

  setlocale(LC_NUMERIC, "C");

  if (!tested) {
    fprintf(stderr, "%s: no decimal-comma locale installed; skipping\n", me);
    return 77;
  }
  if (failed) {
    return 1;
  }
  printf("%s: locale-independent parse verified under %u decimal-comma locale(s)\n", me,
         tested);
  return 0;
}
