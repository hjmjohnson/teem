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
#include <sys/errno.h>
#include <teem/air.h>

/*
** Tests:
** airDeciSepLearn to learn whatever the decimal separator is in
** various locales that we try (with 1 or 2 byte non-'.' decimal separators),
** and then making sure airDtoS, airFtoS, airStoD, airStoF all use '.' as
** the decimal separator, despite the locale using otherwise.
** Returns 77 (skip) when no decimal-comma locale is installed.
*/

int
main(int argc, const char *argv[]) {
  const char *me = argv[0];
  /* Not all systems have all locales, and where available their
     decimal separator may not be what is recorded in comments here */
  const char *funLocale[] = {
    "de_DE.UTF-8",  // (1) ","
    "fr_FR.UTF-8",  // (1) ","
    "nl_NL.UTF-8",  // (1) ","
    "it_IT.UTF-8",  // (1) ","
    "fa_AF.UTF-8",  // (2) "٫" == Arabic Decimal Separator
    "ps_AF.utf8",   // (2) (same)
    "zh_TW.Big5",   // (2) "．" == full width full stop (?) but not in UTF-8
    "zh_CN.eucCN",  // (2) (same)
    "zh_CN.GB2312", // (2) \"#.\"
  };
  unsigned int li, tested[3] = {0, 0, 0};
  int failed = 0;
  char decibuff[AIR_STRLEN_SMALL + 1];
  const char *decisep;

  /* no: we set it explicitly: airSetLocaleFromEnvOrExit(argv[0], airStderr()); */
  AIR_UNUSED(argc);

  for (li = 0; li < AIR_CAST(unsigned int, sizeof(funLocale) / sizeof(char *)); li++) {
    double dval = -1.0;
    float fval = -1.0f;
    int pret, cls;
    char *end, tstr[] = "0.878906" /* test value < 1 with . decimal separator */;
    if (!setlocale(LC_NUMERIC, funLocale[li])) {
      printf("%s: setlocale(LC_NUMERIC, \"%s\") failed (not installed?); moving on\n",
             me, funLocale[li]);
      continue; /* locale not installed here */
    } else {
      printf("%s: setlocale(LC_NUMERIC, \"%s\") succeeded\n", me, funLocale[li]);
    }

    /* sanity: confirm locale's decimal separator isn't '.' (else not useful test) */
    dval = strtod(tstr, &end);
    if (end != tstr && *end == '\0' && dval > 0.5) {
      printf("%s: strtod in \"%s\" accepts '.' as decimal separator; moving on\n", me,
             funLocale[li]);
      continue; /* platform's sscanf ignored the comma locale; skip this one */
    } else {
      printf("%s: strtod in \"%s\" does NOT accept'.' as decimal separator (good)\n", me,
             funLocale[li]);
    }

    if (!(decisep = airDeciSepLearn(decibuff, stderr))) {
      fprintf(stderr, "%s: oops couldn't learn %s's decimal separator", me,
              funLocale[li]);
      return 1;
    }
    unsigned int dslen = (unsigned int)decisep[-1];
    printf("%s: decisep under \"%s\" is length-%u \"%s\"\n", me, funLocale[li], dslen,
           decisep);
    tested[dslen]++; /* now increment count of locales tested */
    errno = 0;
    dval = -1.0;
#define STR_(X)  #X
#define STR(x)   STR_(x)
#define F_MID_LO 0.733f
#define F_MID_V  0.734375
#define F_MID_HI 0.735f
#define F_MID_S  STR(F_MID_V)
    /* a value inside double range but past (32-bit) FLT_MAX should
      (like sscanf does) be parsed (as a float) as infinity,
       but set errno to ERANGE ("Math result not representable") */
#define F_BIG_LO 4.0e49
#define F_BIG_V  4.5e49
#define F_BIG_HI 5.0e49
#define F_BIG_S  STR(F_BIG_V)
    if (!(1 == airStoD(&dval, F_MID_S, decisep) //
          && 0 == errno                         //
          && AIR_IN_OP(F_MID_LO, dval, F_MID_HI))) {
      fprintf(stderr,
              "%s: oops (in %s) airStoD(\"" F_MID_S "\") = %.17g (want " F_MID_S
              ") with errno=%d (want 0)\n",
              me, funLocale[li], dval, errno);
      failed = 1;
    }
    errno = 0;
    fval = -1.f;
    if (!(1 == airStoF(&fval, F_MID_S, decisep) //
          && 0 == errno                         //
          && AIR_IN_OP(F_MID_LO, fval, F_MID_HI))) {
      fprintf(stderr,
              "%s: oops (in %s) airStoF(\"" F_MID_S "\") = %.9g (want " F_MID_S
              ") with errno=%d (want 0)\n",
              me, funLocale[li], fval, errno);
      failed = 1;
    }
    errno = 0;
    fval = 0.f;
    pret = airStoF(&fval, F_BIG_S, decisep);
    cls = airFPClass_f(fval);
    if (!(1 == pret && airFP_POS_INF == cls && ERANGE == errno)) {
      fprintf(stderr,
              "%s: oops (in %s) airStoF(\"" F_BIG_S "\") returned %d (want 1), "
              "with fval==%.9g in class %s (want %s), and "
              "errno==%d (want ERANGE=%d)\n",
              me, funLocale[li], pret, //
              fval, airEnumStr(airFPClass_ae, cls),
              airEnumStr(airFPClass_ae, airFP_POS_INF), //
              errno, ERANGE);
      failed = 1;
    }
    /* the same big value should parse to a finite double */
    errno = 0;
    dval = 0.;
    pret = airStoD(&dval, F_BIG_S, decisep);
    cls = airFPClass_d(dval);
    if (!(1 == pret && airFP_POS_NORM == cls && 0 == errno
          && AIR_IN_OP(F_BIG_LO, dval, F_BIG_HI))) {
      fprintf(stderr,
              "%s: oops (in %s) airStoD(\"" F_BIG_S "\") returned %d (want 1), "
              "with dval==%.17g in class %s (want %s), and "
              "errno==%d (want 0)\n",
              me, funLocale[li], pret, //
              dval, airEnumStr(airFPClass_ae, cls),
              airEnumStr(airFPClass_ae, airFP_POS_NORM), //
              errno);
      failed = 1;
    }
    dval = -1.0;
    if (1 != airParseStrD(&dval, F_MID_S, " ", 1)
        || !AIR_IN_OP(F_MID_LO, dval, F_MID_HI)) {
      fprintf(stderr,
              "%s: oops (in %s) airParseStrD(\"" F_MID_S "\") = %.9g (want " F_MID_S
              ")\n",
              me, funLocale[li], dval);
      failed = 1;
    }
    /* write side: airSinglePrintf must emit '.' (whatever the locale's decimal
       separator) and the output must round-trip through airSingleSscanf */
    if (1) {
      char buff[AIR_STRLEN_SMALL + 1] = "";
      double back = -1.0;
      int wlen = AIR_INT(strlen(F_MID_S)), ret1 = -1, ret2 = -1;
      if (!(wlen == (ret1 = airSinglePrintf(NULL, buff, sizeof(buff), "%lg", F_MID_V))
            && strchr(buff, '.')                                /* */
            && 1 == (ret2 = airSingleParse(buff, "%lf", &back)) /* */
            && AIR_IN_OP(F_MID_LO, back, F_MID_HI)              /* */
            )) {
        fprintf(stderr,
                "%s: oops (in %s) airSinglePrintf(%%lg, \"" F_MID_S "\") "
                "-(%d vs %d)-> \"%s\" -(%d)-> %.17g\n",
                me, funLocale[li], /* */
                ret1, wlen, buff, ret2, back);
        failed = 1;
      }
    }
  }

  /* being a good citizen, fwiw: restore locale to how C programs start */
  setlocale(LC_NUMERIC, "C");
  unsigned total = tested[1] + tested[2];
  if (!total) {
    fprintf(stderr, "%s: oops no non-'.'-decisep locale installed; skipping\n", me);
    return 77;
  }
  if (failed) {
    return 1;
  }
  printf("%s: C-locale-emulation parse verified in %u non-'.'-decisep locale%s: %u "
         "1-byte and %u 2-byte\n",
         me, total, 1 == total ? "" : "s", tested[1], tested[2]);
  return 0;
}
