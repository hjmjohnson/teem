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

#include "air.h"
#include "privateAir.h"

/* Notes on terminology, chronology, design strategies, and functions herein,
partly as GLK notes-to-self about how this code came about, and hopefully as
informative context for anyone else looking at this. To be clear: when GLK starting
writing Teem in ~2000 he had zero understanding of any of this, and 2026 GLK is grateful
to Hans Johnson for addressing this issue in Teem code, diplomatically interfacing
between the ITK and Teem world, and patiently helping GLK be less ignorant.

*** FP means "floating point"

When writing an FP number as a character sequence, different parts of the world use
different ways to separate the integer part from the fractional part. The technical world
at large calls this by various names including "decimal separator", "decimal mark", or
"decimal sign": https://en.wikipedia.org/wiki/Decimal_separator . Some printf/sscanf man
pages call it the "decimal point character", but other docs (e.g. from POSIX) call it the
"radix character" https://pubs.opengroup.org/onlinepubs/9799919799/functions/fprintf.html
which is confusing, given its existing meaning: https://en.wikipedia.org/wiki/Radix
(the radix is always 2 for binary FP; and "radix character" fails to convey anything
about marking the transition from negative to non-negative powers of the radix).

*** We use "decimal separator", or `decisep` as a variable name

For C code around things like the NRRD file format that assume '.' as the FP decimal
separator, using printf/sscanf for printing/parsing is hampered by their dependency
on the current "locale": https://en.wikipedia.org/wiki/C_localization_functions
which includes a numeric category (`LC_NUMERIC`) of locale-specific behavior.
In particular the `lconv` struct from https://en.cppreference.com/c/locale/localeconv
has the `decimal_point` field that in principle is the same thing that will appear in
printf output or be assumed in sscanf input of string representations of non-integer FP
values.  Names for the locale in which '.' is the decimal separator include "C", "POSIX",
"en_US", and many others.

Our goal here is to ensure that when code for handling FP<-->string conversions is
running, no matter the locale in which the program started executing, that the strings
only use '.' as the decimal separator. There are at least three different strategies
for doing this. The most intensive is to entirely avoid library functions that are
affected by locale, and directly re-implement everything about FP<-->string conversion.
This is enabled by things like David Gay's dtoa.c http://www.netlib.org/fp/dtoa.c
but there are newer implementations too: https://news.ycombinator.com/item?id=41501625 .
We can call this a "locale-agnostic" strategy.  Despite strong temptations, Teem has
never pursued this path.

In contrast, there are what we can call "locale-aware" strategies for enforcing '.' as
the decimal separator.  An obvious and principled approach is to wrap any FP<-->string
conversion code in something first that saves the current locale then
`setlocale(LC_NUMERIC, "C")` prior to conversion, and then restores the original locale
afterwards.

** Let's call this the "save/restore strategy"

Since svn r6133 in 2013 a note was in airSingleSscanf to wrap the sscanf() call with
"(save setlocale here)" and "(return setlocale here)", but GLK never implemented it
there, nor around the fprintf/snprintf call in airSinglePrintf that also needed it.
Because the locale-sensitivity of NRRD IO was becoming a problem for ITK (e.g.
https://github.com/InsightSoftwareConsortium/ITK/commit/00d47e755c), Hans Johnson
implemented save/restore in the July 2026 locale-independent-numeric-parse Teem branch:
https://sourceforge.net/p/teem/teem.git/ci/d7e056e008ccc8e9e2b4c98d550f0b5169d72950
However, experience showed that it is hard to implement the save/restore strategy in a
fast, portable, thread-safe way that avoids memory leaks or memory in use at exit.

A more targeted and pragmatic strategy is to accept that library functions like
printf/sscanf are affected by locale, but to not fight or change that. Instead, just
post-process printf output to make '.' be the decimal separator, and pre-process sscanf
input to convert a '.' to the locale's separator.

** Let's call this the "string fixing strategy"

The string fixing strategy has (since the early 2000s) been part of GLib
https://en.wikipedia.org/wiki/GLib, in functions g_ascii_strtod (for parsing) and
g_ascii_formatd (printing): https://github.com/GNOME/glib/blob/main/glib/gstrfuncs.c
This strategy is noted in Python2's July 2003 PEP 331: https://peps.python.org/pep-0331/
about Locale-Independent Float/String Conversions; see how PyOS_ascii_strtod and
PyOS_ascii_formatd are defined here https://github.com/python/cpython/issues/38883 .
Btw Python3 is locale-agnostic https://github.com/python/cpython/blob/main/Python/dtoa.c
The string locale-aware fixing strategy seems to be called "decimal-point substitution"
here: https://github.com/nlohmann/json/issues/5660#issue-5634738577

But within the string fixing strategy, you can either call `localeconv()` to query the
current locale and learn `decimal_point` (as done in GLib), or, you can just snprintf an
FP value to discover what the current decimal separator is from inspecting the output
string. This approach is thus locale-aware, but in an appropriately single-purpose yet
indirect way that dodges any platform-specific `locale` code per se; we can call this a
"locale-compensating" strategy.  Hans Johnson developed a locale-compensating string
fixing strategy in his Sept 2026 bug-air-locale-numeric-io Teem branch that replaced his
earlier save/restore code:
https://sourceforge.net/p/teem/teem.git/ci/b10f0590629f347baf77f25a1ebbb0bb5522b269/

*** Code below uses the locale-compensating string fixing strategy from Hans Johnson

However, because these here are simple low-level IO functions, we do not try to learn the
decimal separator within them; it would be wasteful to re-learn that every time we have a
number to print or parse, since we are very likely being called from a loop or other
context in which the decimal separator can be learned once and re-used. But, replacing
uses of airSinglePrintf or airSingleSscanf involves a significant code changes, because
now a new `decisep` argument has to accompany all FP<-->string calls. This file is the
starting point of that change.

*** `airDecimalSeparatorLearn` learns the current locale's decimal separator. Passing it
    as `decisep` to air{F,D}toS/airSto{F,D} enables locale compensation.
    See note below about its return being a pseudo-Pascal string.
*** Or, `decisep` can be passed as NULL to air{F,D}toS/airSto{F,D} to say:
    "I want to be oblivious to locale: do no string pre- or post-processing"
    which is ok if e.g. the caller already knows that '.' is the decimal separator
*** There is no fall-back method for air{F,D}toS/airSto{F,D} to be locale-aware without
    passing a non-NULL `decisep`.
*** The only locale-compensating that air{F,D}toS/airSto{F,D} can do is to
    (as a printing post-process) convert from `decisep` to '.', and
    (as a parsing pre-process) convert from '.' to `decisep`.
    No general-purpose conversion between two non-"C" locales is attempted here.

AI Disclosure: GLK's chats in Sep 2026 with Claude (Sonnet 4.6) informed parts of the
summary above and the some of the design choices in the code below, but all the words
above and all the code below were written by hand.
*/

/* airDecimalSeparatorLearn learns the current locale's decimal separator by printing
1.5 into the given `str` buffer, and isolating whatever is between '1' and '5'.
This returns str+1 as nul-terminated `decisep` string, to be used with e.g.
   char dcsbuff[AIR_STRLEN_SMALL + 1], nbuff[AIR_STRLEN_SMALL + 1];
   const char *decisep = airDecimalSeparatorLearn(dcsbuff);
   ...
   printf("3.0/2.0 = %s\n", airDtoS(nbuff, 3.0/2.0, decisep));
However, so that other functions below do not have to keep learning the length of the
decimal separator (even though it is probably just 1 or 2), that length is saved in
decisep[-1], ala Pascal https://www.gnu-pascal.org/gpc/String-schema.html  Thus it will
never make sense to `free(decisep)` since `decisep` never arose from a heap allocation.
It is on purpose that airDecimalSeparatorLearn returns a `const char *` and takes a
`char str[AIR_STRLEN_SMALL + 1]`, encouraging use of a stack-allocated buffer. */
const char *
airDecimalSeparatorLearn(char str[ASP1_S]) {
  int plen;
  unsigned int len;
  /* get a string representation of 1.5 */
  str[0] = '\0';
  plen = snprintf(str, ASP1_S, "%g", 1.5);
  if (plen < 3 || 4 < plen || '1' != str[0]) {
    /* something is very strange; the string snprintf intends to write is either
       as short as strlen("15"), which implies there was no decimal separator,
       or too long to be from any known (1 or 2 char) decimal separator,
       or the string printed from 1.5 didn't start with '1' */
    return NULL;
  }
#define VERB 0
  if (VERB)
    fprintf(stderr, "!%s: 00 str=|%s| --> plen=%d, strlen=%u\n", __func__, str, plen,
            AIR_UINT(strlen(str)));
  /* find the last char of the string (and its length) */
  len = 1;
  do {
    if (VERB)
      fprintf(stderr, "!%s: ... str[len=%u]='%c'=%d\n", __func__, len, str[len],
              AIR_UCHAR(str[len]));
    len++;
  } while (str[len]);
  if (VERB)
    fprintf(stderr, "!%s: 11 str[len-1=%u]='%c'=%d\n", __func__, len - 1, str[len - 1],
            str[len - 1]);
  if ('5' != str[len - 1]) {
    /* very strange again if sprintf output didn't end with '5' */
    return NULL;
  }
  // drop the '5' from the snprintf result
  str[len - 1] = '\0';
  if (VERB) fprintf(stderr, "!%s: 22 str+1=|%s| len=%u\n", __func__, str + 1, len);
  // save the length into the first slot (above we asserted that plen >= 3)
  str[0] = (char)(len - 2);
  if (VERB)
    fprintf(stderr, "!%s: 33 str+1=|%s| with saved length %u\n", __func__, str + 1,
            (unsigned int)(str[0]));
#undef VERB
  // return the "string" starting in the second slot
  return str + 1;
}

/* fp2str implements airFtoS and airDtoS,
which are intended to be part of replacing airSinglePrintf, for the case where
it is passed a format string that is only "%g" (for float value) or "%lg" (for double),
by replicating these features:
- prints special values nan, inf, -inf as "NaN", "inf", and "-inf"
- if `singlePrec`, prints `fval0` with enough precision to ensure
  post-parsing (round-trip) equality with fval0
  else (if `!singlePrec`) prints `dval0` with enough precision to ensure
  post-parsing (round-trip) equality with dval0
- BUT UNLIKE airSinglePrintf (at least without efforts to make it locale-aware),
  this will use the curent locale's decimal separator `decisep` (if non-NULL),
     ** necessarily as learned from airDecimalSeparatorLearn **
     ** (because of Pascal-style string hack used there) **
  to post-process the output string from snprintf (which is locale-dependent) to
  change an occurance of `decisep` to instead be '.'
*/
static char *
fp2str(char str[ASP1_S], float fval0, double dval0, int singlePrec,
       const char *decisep) {
  int cls, accurate;
  float fval1;
  double dval1;
  /* since code below is special-cased to 1 or 2 byte decimal separators */
  assert(!decisep || (1 == decisep[-1] || 2 == decisep[-1]));
  /* first handle the special (non-finite) cases */
  cls = singlePrec ? airFPClass_f(fval0) : airFPClass_d(dval0);
  if (airFP_NAN == cls) {
    return airStrcpy(str, ASP1_S, "NaN");
  } /* else */
  if (airFP_POS_INF == cls) {
    return airStrcpy(str, ASP1_S, "inf");
  } /* else */
  if (airFP_NEG_INF == cls) {
    return airStrcpy(str, ASP1_S, "-inf");
  }
#define VERB 1
  /* else not a special value; try snprintf'ing it with %g (in current locale) */
  snprintf(str, ASP1_S, "%g", singlePrec ? fval0 : dval0);
  if (VERB) fprintf(stderr, "!%s: snprintf(%%g) -> \"%s\"\n", __func__, str);
  /* try parsing it back (in current locale) */
  if (singlePrec) {
    sscanf(str, "%f", &fval1);
    accurate = (fval1 == fval0);
  } else {
    sscanf(str, "%lf", &dval1);
    accurate = (dval1 == dval0);
  }
  if (VERB)
    fprintf(stderr,
            singlePrec ? "!%s: parsed %.9g vs %.9g (accurate %d)\n"
                       : "!%s: parsed %.17g vs %.17g (accurate %d)\n",
            __func__, singlePrec ? fval1 : dval1, singlePrec ? fval0 : dval0, accurate);
  if (!accurate) {
    snprintf(str, ASP1_S, singlePrec ? "%.9g" : "%.17g", singlePrec ? fval0 : dval0);
    if (VERB) fprintf(stderr, "!%s: 2nd try: snprintf(%%g) -> \"%s\"\n", __func__, str);
  }
  /* we now have an accurate string representation in `str` */
  if (decisep && (2 == decisep[-1] || /* else its length=1 and */ '.' != decisep[0])) {
    /* we need to look for the decimal separator to change to "C" locale;
       both cases here assume the decimal separator can only appear once */
    unsigned int ii, jj;
    if (VERB)
      fprintf(stderr, "!%s: decisep=|%s| (length %u)\n", __func__, decisep,
              AIR_UINT(decisep[-1]));
    if (1 == decisep[-1]) {
      /* simple sweep: replace the single character if we see it */
      for (ii = 0; str[ii]; ii++) {
        if (decisep[0] == str[ii]) {
          str[ii] = '.'; /* change and bail */
          break;
        }
      }
    } else {
      /* decimal separator is two chars, have to shuffle down chars after change */
      for (ii = 0; str[ii + 1]; ii++) {
        if (decisep[0] == str[ii] && decisep[1] == str[ii + 1]) {
          str[ii] = '.';
          jj = ii + 1;
          do {
            str[jj] = str[jj + 1];
            jj++;
          } while (str[jj]);
          break;
        }
      }
    }
    if (VERB) fprintf(stderr, "!%s: after decisep change: \"%s\"\n", __func__, str);
  }
#undef VERB
  return str;
}

char *
airFtoS(char str[ASP1_S], float fval, const char *decisep) {
  return fp2str(str, fval, 0, AIR_TRUE, decisep);
}

char *
airDtoS(char str[ASP1_S], double dval, const char *decisep) {
  return fp2str(str, 0, dval, AIR_FALSE, decisep);
}

/* --------------------------------------------------------------------------- */
/* str2fp implements airStoF and airStoD,
which are intended to be **part** of replacing airSingleSscanf, for the case where
it is passed a format string that is only "%g" (for float value) or "%lg" (for double):
We interpret "NaN", "inf", and "-inf" (case-insensitive) as those special values,
as well as (if non-NrrdIO) "-pi" and "pi" as those useful values,
HOWEVER, airSingleSscanf would do this with a `strstr` substring search, which
meant that "banana" would be parsed as a NaN. That's confusing and too cute.
We are more stringent here, requiring (case-insensitive) string equality.
Also, while airSingleSscanf also directly handled parsing a size_t with a "%zu"
conversion sequence, that is naturally not part of this FP-related code.
*/
static int
str2fp(float *fvalP, double *dvalP, int singlePrec, const char *str0,
       const char *decisep) {
  char *str;
  size_t str0len;
  int ret;
  if (!str0) {
    return EOF;
  }
#define VERB 1
  str0len = strlen(str0);
  /* allocate local copy for one extra char, in case needed for decisep conversion
     (+1 for extra decisep character, +1 for nul-termination ==> +2 */
  str = AIR_CALLOC(str0len + 2, char);
  if (!str) {
    return EOF;
  }
  airStrcpy(str, str0len + 2, str0);
  if (VERB) fprintf(stderr, "!%s: airStrcpy -> \"%s\"\n", __func__, str);
  airToLower(str);
  if (VERB)
    fprintf(stderr, "!%s: airToLower -> \"%s\" (length %u)\n", __func__, str,
            AIR_UINT(strlen(str)));
  /* See if str matches strings for one of the special values. See NOTE above about
     how airSingleSscanf would parse "banana" as a NaN; we don't do that here. */
#define HANDLE(STR, VAL)                                                                \
  if (!strcmp(str, (STR))) {                                                            \
    if (singlePrec) {                                                                   \
      *fvalP = AIR_FLOAT(VAL);                                                          \
    } else {                                                                            \
      *dvalP = AIR_DOUBLE(VAL);                                                         \
    }                                                                                   \
    free(str);                                                                          \
    return 1;                                                                           \
  }
  HANDLE("nan", AIR_NAN);
  HANDLE("-inf", AIR_NEG_INF);
  HANDLE("inf", AIR_POS_INF);
  /* ---- BEGIN non-NrrdIO */
  HANDLE("-pi", -AIR_PI);
  HANDLE("pi", AIR_PI);
  /* ---- END non-NrrdIO */
#undef HANDLE

  /* else we try in earnest to parse an FP value */
  if (decisep && (2 == decisep[-1] || /* else its length=1 and */ '.' != decisep[0])) {
    /* we need to look for '.' to change to the decimal separator,
       both cases here assume the decimal separator can only appear once */
    unsigned int ii, jj;
    if (VERB)
      fprintf(stderr, "!%s: decisep=|%s| (length %u)\n", __func__, decisep,
              AIR_UINT(decisep[-1]));
    if (1 == decisep[-1]) {
      /* simple sweep: replace the single character if we see it */
      for (ii = 0; str[ii]; ii++) {
        if ('.' == str[ii]) {
          str[ii] = decisep[0]; /* change and bail */
          break;
        }
      }
    } else {
      /* decimal separator is two chars, have to shuffle up chars after change */
      for (ii = 0; str[ii]; ii++) {
        if ('.' == str[ii]) {
          str[ii] = decisep[0];
          if (VERB)
            fprintf(stderr, "!%s: 00 decisep swap -> \"%s\" (length %u)\n", __func__,
                    str, AIR_UINT(strlen(str)));
          str[str0len + 1] = '\0';
          jj = str0len;
          do {
            str[jj] = str[jj - 1];
            jj--;
            if (VERB)
              fprintf(stderr, "!%s: ... (%u) decisep swap -> \"%s\" (length %u)\n",
                      __func__, jj, str, AIR_UINT(strlen(str)));
          } while (jj > ii + 1);
          str[ii + 1] = decisep[1];
          break;
        }
      }
    }
    if (VERB)
      fprintf(stderr, "!%s: post decisep swap -> \"%s\" (length %u)\n", __func__, str,
              AIR_UINT(strlen(str)));
  }
  /* now we run sscanf */
  if (singlePrec) {
    ret = sscanf(str, "%f", fvalP);
    if (VERB) fprintf(stderr, "!%s: parsed %d float %.9g\n", __func__, ret, *fvalP);
  } else {
    ret = sscanf(str, "%lf", dvalP);
    if (VERB) fprintf(stderr, "!%s: parsed %d double %.17g\n", __func__, ret, *dvalP);
  }
  free(str);
  return ret;
}

int
airStoF(float *fvalP, const char *str, const char *decisep) {
  return str2fp(fvalP, NULL, AIR_TRUE, str, decisep);
}

int
airStoD(double *dvalP, const char *str, const char *decisep) {
  return str2fp(NULL, dvalP, AIR_FALSE, str, decisep);
}
