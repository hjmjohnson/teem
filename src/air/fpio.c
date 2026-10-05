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
writing Teem in ~2000 he had zero understanding of any of this, and GLK is grateful to
Hans Johnson for addressing this issue in Teem code, and helping GLK be less ignorant.

*** FP means "floating point"

When writing an FP number as a character sequence, different parts of the world use
different ways to separate the integer part from the fractional part. The technical world
at large calls this by various names including "decimal separator", "decimal mark", or
"decimal sign": https://en.wikipedia.org/wiki/Decimal_separator . Some printf/sscanf man
pages call it the "decimal point character", but other docs (e.g. from POSIX) call it the
"radix character" https://pubs.opengroup.org/onlinepubs/9799919799/functions/fprintf.html
which is wildly confusing, given existing meaning https://en.wikipedia.org/wiki/Radix
(the radix is always 2 for binary FP; and "radix character" fails to convey anything
about marking the transition from negative to non-negative powers of the radix).

*** We use "decimal separator", or `decisep` as a variable name

For C code around things like the NRRD file format that assume '.' as the FP decimal
separator, using printf/sscanf for printing/parsing is hampered by their dependency
on the current "locale": https://en.wikipedia.org/wiki/C_localization_functions
which includes a numeric category (`LC_NUMERIC`) of locale-specific behavior.
In particular the `lconv` struct from https://en.cppreference.com/c/locale/localeconv
has the `decimal_point` field that in principle is the same thing that will appear in
printf output or be assumed in sscanf input.  Names for the locale in which '.'
is the decimal separator inlude "C", "POSIX", "en_US", and many others.

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
This strategy is noted in Python's July 2003 PEP 331: https://peps.python.org/pep-0331/
about Locale-Independent Float/String Conversions; see how PyOS_ascii_strtod and
PyOS_ascii_formatd are defined here https://github.com/python/cpython/issues/38883 .
Btw Python3 is locale-agnostic: https://github.com/python/cpython/blob/main/Python/dtoa.c

But within the string fixing strategy, you can either call `localeconv()` to query the
current locale and learn `decimal_point` (as done in GLib).  Or, you can abstain from
any `locale`-specific API and just snprintf an FP value to discover what the curent
decimal separator is from the output string, and work from there to enforce '.' being the
decimal separator. So this approach is locale-aware, but in an appropriately
single-purpose way that dodges any platform-specific `locale` code per se; we can call
this a "locale-compensating" strategy.  Hans Johnson developed a locale-compensating
string fixing strategy in his Sept 2026 bug-air-locale-numeric-io Teem branch that
replaced his earlier save/restore code:
https://sourceforge.net/p/teem/teem.git/ci/b10f0590629f347baf77f25a1ebbb0bb5522b269/

*** We adopt here the locale-compensating string fixing strategy from Hans Johnson

However, because these here are simple low-level IO functions, we do not try to learn the
decimal separator within them; it would be wasteful to re-learn that with every number to
print or parse, since we are very likely being called from a loop or other context in
which the decimal separator can be learned once and re-used. But in the case of
airSinglePrintf that implies significant code changes, because now a new `decisep`
argument has to accompany all FP<-->string calls. This file is the starting point of
that change.

*** `airDecimalSeparatorLearn` learns the current locale's decimal separator. Passing it
    as `decisep` to air{Flt,Dbl}ToStr/airStrTo{Flt,Dbl} enables locale compensation.
*** Or, `decisep` can be passed as NULL to air{Flt,Dbl}ToStr/airStrTo{Flt,Dbl} to say:
    "Fine, be oblivious to locale, and do no string pre- or post-processing".
*** There is no fall-back method for air{Flt,Dbl}ToStr/airStrTo{Flt,Dbl} to be
    locale-aware without passing a non-NULL `decisep`.
*** The only locale-compensating that air{Flt,Dbl}ToStr/airStrTo{Flt,Dbl} can do is to
    (as a printing post-process) convert from `decisep` to '.', and
    (as a parsing pre-process) convert from '.' to `decisep`.
    No general-purpose conversion between two non-"C" locales is attempted.

AI Disclosure: GLK's chats in Sep 2026 with Claude (Sonnet 4.6) informed parts of the
summary above and the some of the design choices in the code below, but all the words and
all the code was written by hand.
*/

char *
airDecimalSeparatorLearn(char str[ASP1_S]) {
  int plen;
  unsigned int ii;
  /* get a string representation of 1.5 */
  str[0] = '\0';
  plen = snprintf(str, ASP1_S, "%f", 1.5);
  if (plen <= 2 || 10 < plen || !str[0]) {
    /* something is very wrong; the string snprintf intends to write is either
       as short as strlen("15"), which implies there was no decimal separator (?),
       or too crazy long to capture any known decimal separator,
       or snprintf didn't successfully print anything */
    return NULL;
  }
  fprintf(stderr, "!%s: 0 str=|%s|\n", __func__, str);
  /* shift the characters down to drop the leading 1 and trailing 5 */
  ii = 0;
  do {
    str[ii] = str[ii + 1];
    ii++;
  } while (str[ii]);
  fprintf(stderr, "!%s: 1 str=|%s|\n", __func__, str);
  // drop the '5' from the snprintf result
  str[ii - 1] = '\0';
  fprintf(stderr, "!%s: 2 str=|%s|\n", __func__, str);
  return str;
}

static char *
fpToStr(char str[ASP1_S], double fval, double dval, int singlePrec,
        const char *decisep) {

  return str;
}

char *
airFltToStr(char str[ASP1_S], float fval, const char *decisep) {
  return fpToStr(str, fval, 0, AIR_TRUE, decisep);
}

char *
airDblToStr(char str[ASP1_S], double dval, const char *decisep) {
  return fpToStr(str, 0, dval, AIR_FALSE, decisep);
}
