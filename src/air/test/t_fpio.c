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

int
main(int argc, const char *argv[]) {
  const char *me = argv[0];
  if (4 != argc) {
    /*                       0     1     2   3 (4) */
    fprintf(stderr, "usage: %s <loc> <A> <B>\n", me);
    fprintf(stderr, "For integers `A`!=-1 and `B`!=-1, the quotient "
                    "A/B is printed by various means from locale `loc`\n");
    fprintf(stderr, "With `A`==`B`==-1, just the decimal separator is reported\n");
    fprintf(stderr,
            "Locales to try and decimal separators learned from using this program:\n"
            "        de_DE.UTF-8 : \",\" == a comma,\n"
            "   en_US.UTF-8 or C : \".\" == period, or\n"
            "        fa_IR.UTF-8 : \"٫\" == Arabic Decimal Separator != a comma, or\n"
            "         zh_TW.Big5,\n"
            "        zh_CN.eucCN : \"．\" == full width full stop but not in UTF-8, or\n"
            "       zh_CN.GB2312 : \"#.\" == full width full stop but in\n"
            "                              https://en.wikipedia.org/wiki/GB_2312, or\n"
            "\"\" for no call to setlocale() (perhaps it's using $LANG==%s).\n"
            "For a fun time, run:\n"
            "   locale -a | xargs -n 1 -J %% test/t_fpio %% -1 -1 | sort\n\n",
            getenv("LANG"));
    return 1;
  }
  printf("%s: hello $LANG=%s\n", me, getenv("LANG"));
  const char *localestr = argv[1];
  const char *Astr = argv[2];
  const char *Bstr = argv[3];
  int A, B;
  if (2 != sscanf(Astr, "%d", &A) + sscanf(Bstr, "%d", &B)) {
    fprintf(stderr, "%s: couldn't parse both %s and %s as ints\n", me, Astr, Bstr);
    return 1;
  }
  if (strlen(localestr)) {
    const char *lret = setlocale(LC_NUMERIC, localestr);
    if (!lret) {
      fprintf(stderr, "%s: setlocale(LC_NUMERIC, %s) failed (returned NULL)\n", me,
              localestr);
      return 1;
    }
    if (strcmp(lret, localestr)) {
      printf("%s: setlocale(LC_NUMERIC, %s) returned %s\n", me, localestr, lret);
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
  printf(" (%s)\n", localestr);

  if (!(-1 == A && -1 == B)) {
    double dval1, dval0 = (double)A / B;
    float fval1, fval0 = (float)A / B;
    int dret, fret;
    char numBuff[AIR_STRLEN_SMALL + 1];
    printf("%s: dval0 == %d/%d == %.17g (natively)\n", me, A, B, dval0);
    airDtoS(numBuff, dval0, dcs);
    dret = airStoD(&dval1, numBuff, dcs);
    printf("%s: --airDtoS--> |%s| --airStoD--> %.17g (%d) : %s\n", me, numBuff, dval1,
           dret, dval1 == dval0 ? "EQUAL" : "not equal");
    printf("%s: fval0 == %d/%d == %.9g (natively)\n", me, A, B, fval0);
    airFtoS(numBuff, fval0, dcs);
    fret = airStoF(&fval1, numBuff, dcs);
    printf("%s: --airFtoS--> |%s| --airStoF--> %.9g (%d) : %s\n", me, numBuff, fval1,
           fret, fval1 == fval0 ? "EQUAL" : "not equal");
  }

  exit(0);
}
