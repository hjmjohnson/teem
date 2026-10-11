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
#include <locale.h>
#include <sys/errno.h>

int
main(int argc, const char *argv[]) {
  const char *me = argv[0];
  if (!(4 == argc || 5 == argc)) {
    /*                      0    1    2   3  (4) */
    /*                      0    1    2   3   4  (5) */
    fprintf(stderr, "usage: %s <loc> <A> <B> [C]\n", me);
    fprintf(stderr, "For integers `A`!=-1 and `B`!=-1, the quotient "
                    "A/B is printed by various means from locale `loc`\n"
                    "If given C>1, A/B is multiplied by itself C times\n");
    fprintf(stderr, "With `A`==`B`==-1, just the decimal separator is reported\n");
    fprintf(
      stderr,
      "Locales and decimal separators learned (on a Mac!) by using this program:\n"
      "        de_DE.UTF-8 : \",\" == a comma,\n"
      "   en_US.UTF-8 or C : \".\" == period, or\n"
      "        fa_IR.UTF-8 : \"٫\" == Arabic Decimal Separator != a comma, or\n"
      /* some linux (but not my mac) has ps_AF in which the separator is also this */
      "         zh_TW.Big5,\n"
      "        zh_CN.eucCN : \"．\" == full width full stop (?) but not in UTF-8, or\n"
      "       zh_CN.GB2312 : \"#.\" == full width full stop (?) but in\n"
      "                              https://en.wikipedia.org/wiki/GB_2312, or\n"
      "\"\" for no explicit call to setlocale().\n"
      "For a fun time, run:\n"
      "   locale -a | xargs -n 1 -I %% test/t_fpio %% -1 -1 |& sort\n\n");
    // also preserving here for posterity a command that usefully showed the
    // various separators on linux:
    // clang-format off
    //   grep "^decimal_point" /usr/share/i18n/locales/* | sed 's/decimal_point[[:space:]]*/decimal_point /' | LC_ALL=C sort -k2
    // clang-format on
    // (the need for LC_ALL=C on sort was a surprise)
    return 1;
  }
  const char *localestr = argv[1];
  const char *Astr = argv[2];
  const char *Bstr = argv[3];
  int A, B, C = 0;
  if (2 != sscanf(Astr, "%d", &A) + sscanf(Bstr, "%d", &B)) {
    fprintf(stderr, "%s: couldn't parse both %s and %s as ints A, B\n", me, Astr, Bstr);
    return 1;
  }
  if (5 == argc) {
    if (1 != sscanf(argv[4], "%d", &C)) {
      fprintf(stderr, "%s: couldn't parse %s as int C\n", me, argv[4]);
      return 1;
    }
  }
  if (!(-1 == A && -1 == B)) {
    printf("%s: hello $LANG=%s\n", me, getenv("LANG"));
  }
  if (strlen(localestr)) {
    const char *lret = setlocale(LC_NUMERIC, localestr);
    if (!lret) {
      fprintf(stderr, "%s: setlocale(LC_NUMERIC, \"%s\") failed (returned NULL)\n", me,
              localestr);
      return 1;
    }
    if (strcmp(lret, localestr)) {
      printf("%s: setlocale(LC_NUMERIC, \"%s\") returned %s\n", me, localestr, lret);
    }
  } else {
    printf("%s: got empty localestr, did not call setlocale\n", me);
  }
  char dcsBuff[AIR_STRLEN_SMALL + 1];
  const char *dcs = airDecimalSeparatorLearn(dcsBuff);
  if (!(-1 == A && -1 == B)) {
    printf("%s: decisep == |%s| (length %u) ==", me, dcs, AIR_UINT(dcs[-1]));
  } else {
    printf("%s: decisep (length %u) ==", me, AIR_UINT(dcs[-1]));
  }
  for (unsigned char ii = 0; ii < AIR_UCHAR(dcs[-1]); ii++) {
    printf("%c%d", !ii ? ' ' : ',', AIR_UCHAR(dcs[ii]));
  }
  printf(" (%s %s)\n", localestr,
         (char)8 == dcs[-2] ? "HAS BUSTED strtod" : "strtod works");

  if (!(-1 == A && -1 == B)) {
    double dval1, dval0 = (double)A / B;
    float fval1, fval0 = (float)A / B;
    char Cdesc[AIR_STRLEN_MED + 1];
    if (C > 1) { /* raise to higher power */
      int ci;
      for (ci = 1; ci < C; ci++) {
        dval0 *= dval0;
        fval0 *= fval0;
      }
      snprintf(Cdesc, sizeof(Cdesc), "(multiplied by itself %d times)", C);
    } else {
      airStrcpy(Cdesc, sizeof(Cdesc), "");
    }
    int pret, qret;
    char numBuff[AIR_STRLEN_SMALL + 1];
    printf("%s: dval0 == %d/%d %s== %.17g(native)\n", me, A, B, Cdesc, dval0);
    pret = airSinglePrintf(NULL, numBuff, sizeof(numBuff), "%lg", dval0);
    qret = airSingleParse(numBuff, "%lg", &dval1);
    printf("%s: --airSinglePrintf(%%lg)-(%d)-> |%s| "
           "--airSingleParse(%%lg)-(%d)-> %.17g(native)\n",
           me, pret, numBuff, /* */
           qret, dval1);
    errno = 0;
    /* returns numBuff */ airDtoS(numBuff, dval0, dcs);
    pret = airStoD(&dval1, numBuff, dcs);
    printf("%s: --airDtoS--> |%s| --airStoD-(%d)-> %.17g(native) (errno %d): %s\n", /* */
           me, numBuff, pret, dval1, errno,                                         /* */
           dval1 == dval0 ? "equal" : "*NOT* EQUAL!");
    errno = 0;
    pret = airStoF(&fval1, numBuff, dcs);
    printf("%s:                   --airStoF-(%d)-> %.9g(native) (errno %d)\n", /* */
           me, pret, fval1, errno);
    errno = 0;
    pret = sscanf(numBuff, "%f", &fval1); /* intentionally not airSingleParse */
    printf("%s:                   --sscanf-(%d)-> %.9g(native) (errno %d)\n", /* */
           me, pret, fval1, errno);
    printf("%s: fval0 == %d/%d == %.9g(native)\n", me, A, B, fval0);
    pret = airSinglePrintf(NULL, numBuff, sizeof(numBuff), "%g", fval0);
    qret = airSingleParse(numBuff, "%g", &fval1);
    printf("%s: --airSinglePrintf(%%g)-(%d)-> |%s| "
           "--airSingleParse(%%g)-(%d)-> %.9g(native)\n",
           me, pret, numBuff, /* */
           qret, fval1);
    /* returns numBuff */ airFtoS(numBuff, fval0, dcs);
    pret = airStoF(&fval1, numBuff, dcs);
    printf("%s: --airFtoS--> |%s| --airStoF-(%d)-> %.9g (errno %d): %s\n", /* */
           me, numBuff, pret, fval1, errno, fval1 == fval0 ? "equal" : "*NOT* EQUAL!");
  }

  exit(0);
}
