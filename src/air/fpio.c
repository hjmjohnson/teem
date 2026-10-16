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
informative albeit verbose context for anyone else looking at this.
Disclaimer: when GLK starting writing Teem in ~2000 he had zero understanding of any of
this, and 2026 GLK is grateful to Hans Johnson for working on this in Teem while
patiently and diplomatically interfacing between the ITK and Teem world.

When writing out an non-integer number, different parts of the world use different ways
to separate the integer part from the fractional part. The technical world at large calls
this by various names including "decimal separator", "decimal mark", or "decimal sign":
https://en.wikipedia.org/wiki/Decimal_separator . Some printf/strtod man pages call it
the "decimal point character", but other docs (e.g. from POSIX) call it the "radix
character" https://pubs.opengroup.org/onlinepubs/9799919799/functions/fprintf.html which
is confusing, given its existing meaning: https://en.wikipedia.org/wiki/Radix (the radix
is always 2 for binary FP; and "radix character" fails to convey anything about marking
the transition from negative to non-negative powers of the radix).  Also, some decimal
separators are encoded with multi-byte sequences (like '٫' or '．'), which may be a
single character, but are more than one 8-bit `char`.

*** We use "decimal separator", and use `decisep` as a variable and parameter name.

For C code around things like the NRRD file format that assume '.' as the FP (floating
point) decimal separator, using printf/strtod is complicated by their dependency on the
current "locale" https://en.wikipedia.org/wiki/C_localization_functions which includes a
numeric category (`LC_NUMERIC`) of locale-specific behavior. In particular the `lconv`
struct from https://en.cppreference.com/c/locale/localeconv has the `decimal_point` field
that in principle is the same thing that will appear in printf output or be assumed in
strtod input of string representations of non-integer FP values.  Names for the locale in
which '.' is the decimal separator include "C", "POSIX", "en_US", and many others. The
semantics of the C language include that the equivalent of `setlocale(LC_ALL, "C");` is
called at the start of program execution https://en.cppreference.com/c/locale/setlocale
(and that page lists functions that depend on locale).

The Teem/NrrdIO code here could be part of an application that has other user-facing
parts that usefully match the locale, and does so by calling `setlocale` to something
with a non-'.' decimal separator. Our implementation goal is thus to ensure that, no
matter the current locale, our code for handling FP<-->string conversions will always
function the same, emulating running under the original "C" locale: producing ONLY '.'
as the decimal separator in outputs, and assuming '.' on inputs.

*** Let's call this behavior "C-locale-emulation".

The goal of C-locale-emulation started in Teem from requiring that NRRD files be written
and read consistently, no matter who was calling them from what locale. But if part of
Teem does it, how do you draw a line between the parts that enforce C-locale-emulation,
versus other parts of Teem (say, in `hest`, the very user-facing command line parser, or
in `dye` for color computations), for which perhaps the locale *should* determine the
decimal separator?  Do you make two versions of the low-level and parsing functions, or
add flags so that they can emulate two different behaviors?  Managing the boundary
between code that does vs does not enforce C-locale-emulation creates significant
implementation and mental cost, for modest aesthetic and ergonomic gain.

*** The TeemV2 goal is thus that all Teem (even hest) adopts C-locale-emulation.

There are at least three (3) different strategies for doing C-locale-emulation. The
most intensive is to entirely abstain from library functions like printf and strtod that
are affected by locale, and directly compute everything for a FP<-->string conversion,
including hard-coding '.' as decimal separator. We can call this a (1) "locale-agnostic"
strategy.  An oft-cited example is David Gay's dtoa.c http://www.netlib.org/fp/dtoa.c but
there are newer methods too https://news.ycombinator.com/item?id=41501625 such as this
nice single-header library https://github.com/kolemannix/ffc.h . After learning that
under some locales `sscanf` can fail to parse a string into which `sprintf` just wrote
a non-integer), this route looked more tempting, but thankfully so far it seems that
`strtod` can succeed in FP parsing where `sscanf` fails (still, it shouldn't be our job
to replace or fix your broken standard library).

In contrast, there are what we can call "locale-aware" strategies for C-locale-emulation.
A principled but heavy-weight approach is to wrap any FP<-->string conversion code in
something first that saves the current locale then `setlocale(LC_NUMERIC, "C")` prior to
conversion, and then restores the original locale afterwards. This is heavy-weight
because it requires locking any other thread from calling locale-dependent functions.

*** Let's call this the (2) "save/restore strategy".

Since svn r6133 in 2013 a note was in airSingleSscanf to wrap the sscanf() call with
"(save setlocale here)" and "(return setlocale here)", but GLK never implemented it
there, nor around the fprintf/snprintf call in airSinglePrintf that also needed it.
The locale-sensitivity of NRRD IO has long been a problem for ITK, e.g.:
   https://github.com/InsightSoftwareConsortium/ITK/pull/2297
   https://github.com/InsightSoftwareConsortium/ITK/issues/3375
   https://github.com/InsightSoftwareConsortium/ITK/issues/5683
Hans Johnson took the initiative to address the problem in the ITK, e.g.:
   https://github.com/InsightSoftwareConsortium/ITK/pull/5684
AND, to address the problem in the upstream Teem code that creates NrrdIO. He
implemented save/restore in the July 2026 locale-independent-numeric-parse Teem branch:
https://sourceforge.net/p/teem/teem.git/ci/d7e056e008ccc8e9e2b4c98d550f0b5169d72950
However, experience showed that it is hard to implement the save/restore strategy in a
fast, portable, thread-safe way that avoids memory leaks or memory in use at exit.

A more targeted and pragmatic strategy is to accept how library functions like printf and
strtod depend on locale.  Instead, just post-process printf output to make '.' be the
decimal separator, and pre-process strtod input to convert '.' to the locale's separator.

*** Let's call this the (3) "string fixing strategy"

The string fixing strategy has been (since the early 2000s) part of GLib
https://en.wikipedia.org/wiki/GLib, in functions g_ascii_strtod (for parsing) and
g_ascii_formatd (printing): https://github.com/GNOME/glib/blob/main/glib/gstrfuncs.c
This strategy is noted in Python2's July 2003 PEP 331: https://peps.python.org/pep-0331/
about Locale-Independent Float/String Conversions; see how PyOS_ascii_strtod and
PyOS_ascii_formatd are defined here https://github.com/python/cpython/issues/38883 .
Btw Python3 is locale-agnostic https://github.com/python/cpython/blob/main/Python/dtoa.c
The string fixing strategy seems to be called "decimal-point substitution" here:
https://github.com/nlohmann/json/issues/5660#issue-5634738577

But within the string fixing strategy, you can either (3.a) call `localeconv()` to query
the current locale and learn `decimal_point` (as done in GLib). Or, you can just
(3.b) snprintf an FP value to discover what the current decimal separator is by
inspecting the output string. This approach is thus locale-aware, but in an appropriately
single-purpose way that neatly dodges any platform-specific `locale` code per se.

*** Let's call this `locale`-free but locale-aware strategy (3.b) "locale-compensating"

Hans Johnson developed a locale-compensating string fixing strategy in his Sept 2026
bug-air-locale-numeric-io Teem branch that replaced his earlier save/restore code:
https://sourceforge.net/p/teem/teem.git/ci/b10f0590629f347baf77f25a1ebbb0bb5522b269/
The code below adopts the locale-compensating string fixing method from that.  However,
because these (below) are simple low-level IO functions, we do not try to learn the
decimal separator within them: it would be wasteful to re-learn that every time we have a
number to print or parse, since we are very likely being called from a loop or other
context in which the decimal separator can be learned once and re-used.  But, replacing
uses of airSinglePrintf or airSingleSscanf->airSingleParse involves significant code
changes, because now a new `decisep` argument should accompany all FP<-->string calls,
so it doesn't have to be relearned.  This file is the starting point of that change.

*** `airDeciSepLearn` learns the current locale's decimal separator. Passing it as
    `decisep` to air{F,D}toS/airSto{F,D} enables (3.b) locale compensation. See note
    below about its return being a weird pseudo-Pascal string: decisep works as a `char
    *` string, but decisep[-1] and decisep[-2] store other learned information. In
    locales where we learn that non-integers cannot be parsed via strtod (even for values
    like 12.5 which can be exactly represented in FP), a warning message is printed, and
    NULL is returned.
*** Currently only 1 or 2 byte decimal separators are handled; anything else will cause
    airDeciSepLearn to return NULL. Places in all the code below that assume this
    limitation on decimal separator size are tagged with "1-OR-2-BYTE-SEPARATOR".
*** `decisep` is what allows air{F,D}toS/airSto{F,D} to be locale-aware; there is no
    fallback. airSinglePrintf and airSingleParse can be used for one-off FP printing
    and parsing (they internally re-learn decisep with each call).
*** Avoiding the expense of re-learning the `decisep` inside air{F,D}toS/airSto{F,D}
    does incur a risk that the caller calls `setlocale` AFTER airDeciSepLearn but BEFORE
    calling air{F,D}toS/airSto{F,D}. Clearly, if `setlocale` changed to a new locale with
    a different decimal separator, passing the old/invalid `decisep` to air{F,D}toS or
    airSto{F,D} prevents them from correctly emulating "C"-locale. In principle,
    the call to airDeciSepLearn goes just before some looped FP IO, with many air{F,D}toS
    or airSto{F,D} calls inside the loop.
*** NOTE/WARNING: Teem's goal of C-locale-emulation means that when running under
    e.g. `de_DE`, "1,5" will fail to parse with airStoD (when properly called) because
    it checks for a non-'.' `decisep` already being in the input string:
    it does not belong there, since accepting both "1.5" and "1,5" is not correctly
    accomplishing C-locale-emulation.
*** The only locale-compensation that air{F,D}toS/airSto{F,D} can do is to
    (as a printing post-process) convert from `decisep` to '.', and
    (as a parsing pre-process) convert from '.' to `decisep`; THAT IS IT.
*** Also, no general-purpose conversion between two non-"C" locales is attempted here,
    since that is far outside the scope of C-locale-emulation.

AI Disclosure: GLK's chats in Sep and Oct 2026 with Claude (Sonnet 4.6 and 5.5) informed
parts of the summary above and the some implementatn choices in the code below, but all
the words above and all the code below were written by hand. */

/******** airDeciSepLearn
Learns the current locale's decimal separator by printing 12.5 into the given `str`
buffer, and isolating whatever is between '2' and '5'. This returns str+2 as the
nul-terminated `decisep` string, to be used with e.g.
   char decibuff[AIR_STRLEN_SMALL + 1], nbuff[AIR_STRLEN_SMALL + 1];
   const char *decisep = airDeciSepLearn(decibuff);
   ...
   printf("3.0/2.0 = %s\n", airDtoS(nbuff, 3.0/2.0, decisep));

This function will return NULL if snprintf didn't work as expected or the decimal
separator observed is something other than 1 or 2 bytes long.

NOTE: so that other functions below do not have to keep learning the length of the
decimal separator (even though it is just 1 or 2), that length is saved in decisep[-1],
ala Pascal https://www.gnu-pascal.org/gpc/String-schema.html  Thus it will never make
sense to `free(decisep)` since `decisep` never arose from a heap allocation.  It is on
purpose that airDeciSepLearn returns a `const char *` and takes a `char
str[AIR_STRLEN_SMALL + 1]`, encouraging use of a stack-allocated buffer.

ALSO: decisep[-2] is set 1 to flag if the current locale does *not* have a functioning
strtod, or 0 otherwise. Such a decisep is useful for printing but not for parsing.

trivia: on GLK's Mac in 2026 under fa_IR.UTF-8, zh_TW.Big5, or zh_CN.eucCN, sscanf failed
to parse a non-integer with those two-byte decimal separators (weirdly, the two-byte
separator under zh_CN.GB2312 *did* work). In all those cases, however, strtod did work.
*/
const char *
airDeciSepLearn(char str[ASP1_S], FILE *fwarn) {
  static const char me[] = "airDeciSepLearn";
  char *decisep, *dend, *fend;
  int plen;
  unsigned int slen, deciseplen;
  double dval;
  double fval;
#define STR_(X) #X
#define STR(x)  STR_(x)
#define TVAL_V  12.5 /* test value, exact in both 32- and 64- bit float */
  /*   indices  0123 for 4 bytes total with '.' decimal separator,
     or 5 for a two-byte decimal separator */
#define TVAL_S  STR(TVAL_V)
  /* set in `str` a string for TVAL_V */
  str[0] = '\0';
  str[1] = '\0';
  plen = snprintf(str, ASP1_S, "%g", TVAL_V);
  if (!(4 <= plen && plen <= 5 /* 1-OR-2-BYTE-SEPARATOR */
        && '1' == str[0] && '2' == str[1])) {
    /* something is very strange; the string snprintf intends to write is either
       as short as strlen("125"), which implies there was no decimal separator,
       or too long to be from any known (1 or 2 byte) decimal separator,
       or the string printed from TVAL_V didn't start with '1' and '2' */
    return NULL;
  }
#define VERB 0
  if (VERB)
    fprintf(stderr, "!%s: 00 str=|%s| --> plen=%d, strlen=%u\n", me, str, plen,
            AIR_UINT(strlen(str)));
  /* find the last char of the string (and its length) */
  slen = 2;
  do {
    if (VERB)
      fprintf(stderr, "!%s: ... str[slen=%u]='%c'=%d\n", me, slen, str[slen],
              AIR_UCHAR(str[slen]));
    slen++;
  } while (str[slen]);
  if (VERB)
    fprintf(stderr, "!%s: 11 str[slen-1=%u]='%c'=%d\n", me, slen - 1, str[slen - 1],
            str[slen - 1]);
  if ('5' != str[slen - 1]) {
    /* very strange again if string didn't end with '5' */
    return NULL;
  }
  /* set the decisep pointer that will be returned, save strlen(decisep) */
  decisep = str + 2;
  deciseplen = slen - 3; /* -3 because of '1', '2', and '5' */
  /* now see if strtod/strtof actually works here to parse a double/float */
  dval = strtod(str, &dend);
  fval = strtof(str, &fend);
  if (VERB)
    fprintf(stderr,
            "!%s: 22 dval=%.17g dend-str=%u *dend=%d;  "
            "fval=%.9g fend-str=%u *fend=%d\n",
            me, dval, AIR_UINT(dend - str), *dend, fval, AIR_UINT(fend - str), *fend);
  /* drop the '5' from the snprintf result */
  str[slen - 1] = '\0';
  if (!(TVAL_V == dval && *dend == '\0' /* */
        && AIR_FLOAT(TVAL_V) == fval && *fend == '\0')) {
    if (fwarn) {
      fprintf(stderr,
              "%s: Warning: in this locale (with %u-byte decimal separator \"%s\") "
              "`strtod` and `strtof` do not both work to recover the simple non-integer "
              "value " TVAL_S " from \"%s\": airStoD or airStoF won't work.\n",
              me, deciseplen, decisep, str);
    }
    decisep[-2] = 1; /* in this locale we can print but not parse */
  } else {
    decisep[-2] = 0; /* strtod/strtof worked without error */
  }
  /* save the length Pascal-style */
  decisep[-1] = (char)(deciseplen);
  if (VERB)
    fprintf(stderr, "!%s: 33 decisep=|%s| with saved length %u and error %u\n", me,
            decisep, (unsigned int)(decisep[-1]), (unsigned int)(decisep[-2]));
#undef VERB
#undef STR_
#undef STR
#undef TVAL_V
#undef TVAL_S
  return decisep;
}

static int
decisepGood(const char *decisep, int forParsing) {
  return (decisep                                   /* need non-NULL decisep, */
          && (1 == decisep[-1] || 2 == decisep[-1]) /* a 1-OR-2-BYTE-SEPARATOR, and */
          && (!forParsing || !decisep[-2])          /* parsing working where needed */
  );
}

/******** airDeciSepRemove
removes the first (hopefully only) occurance of `decisep` from `str`, in place,
replacing `decisep` with '.', and returns the same given `str` (or NULL if given NULL
`str` or `decisep`). Because this is only for C-locale-emulation, the string can only get
shorter from this subsitution, enabling the in-place work.
*/
char *
airDeciSepRemove(char *str, const char *decisep) {
  static const char me[] = "airDeciSepRemove";
  if (!(str && decisep)) {
    return NULL;
  }
#define VERB 0
  if (2 == decisep[-1]
      || /* else (1-OR-2-BYTE-SEPARATOR) its length=1 and */ '.' != decisep[0]) {
    /* we need to look for and change the decimal separator for C-locale-emulation;
       both cases here assume the decimal separator only appears once */
    unsigned int ii;
    if (VERB)
      fprintf(stderr, "!%s: decisep=|%s| (length %u)\n", me, decisep,
              AIR_UINT(decisep[-1]));
    if (1 == decisep[-1]) {
      /* simple sweep: replace the single character if we see it */
      for (ii = 0; str[ii]; ii++) {
        if (decisep[0] == str[ii]) {
          str[ii] = '.'; /* change and bail */
          break;
        }
      }
    } else { /* 1-OR-2-BYTE-SEPARATOR */
      /* decimal separator is two `char`, have to shuffle *down* after change */
      for (ii = 0; str[ii + 1]; ii++) {
        if (decisep[0] == str[ii] && decisep[1] == str[ii + 1]) {
          unsigned int jj = ii + 1;
          str[ii] = '.';
          do {
            str[jj] = str[jj + 1];
            jj++;
          } while (str[jj]);
          break;
        }
      }
    }
    if (VERB) fprintf(stderr, "!%s: after decisep change: \"%s\"\n", me, str);
  }
#undef VERB
  return str;
}

/* fp2str
implements airFtoS and airDtoS, which are intended to be part of replacing
airSinglePrintf, for the case where it is passed a format string that is only "%g"
(for float value) or "%lg" (for double), by replicating these features:
- prints special values nan, inf, -inf as "NaN", "inf", and "-inf"
- if `singlePrec`, prints `fval0` with enough precision to ensure
  post-parsing (round-trip) equality with fval0
  else (if `!singlePrec`) prints `dval0` with enough precision to ensure
  post-parsing (round-trip) equality with dval0
- always returns the given `str`.  If given bad decisep, the empty string
  is written into (non-NULL) `str`.

NOTE: the `decisep` passed here *MUST* be the return from airDeciSepLearn, because of how
that creates a special string with earlier bytes carrying extra info.
*/
static char *
fp2str(char str[ASP1_S], float fval0, double dval0, int singlePrec,
       const char *decisep) {
  static const char me[] = "fp2str";
  int cls, accurate;
  float fval1;
  double dval1;

  if (!(str && decisepGood(decisep, AIR_FALSE /* not for parsing */))) {
    if (str) str[0] = '\0';
    return str;
  }
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
#define VERB 0
  /* else not a special value; try snprintf'ing it with %g (in current locale) */
  snprintf(str, ASP1_S, "%g", singlePrec ? fval0 : dval0);
  if (VERB)
    fprintf(stderr, "!%s: snprintf of %s with %%g -> \"%s\"\n", me,
            singlePrec ? "flt" : "dbl", str);
  if (singlePrec) {
    fval1 = strtof(str, NULL); /* (yes locale-dependent) */
    accurate = (fval1 == fval0);
  } else {
    dval1 = strtod(str, NULL); /* (yes locale-dependent) */
    accurate = (dval1 == dval0);
  }
  if (VERB)
    fprintf(stderr,
            singlePrec ? "!%s: parsed flt %.9g vs %.9g (accurate==%d)\n"
                       : "!%s: parsed dbl %.17g vs %.17g (accurate==%d)\n",
            me, singlePrec ? fval1 : dval1, singlePrec ? fval0 : dval0, accurate);
  if (!accurate) {
    /* print again with precision enough for 32- or 64-bit float */
    snprintf(str, ASP1_S, singlePrec ? "%.9g" : "%.17g", singlePrec ? fval0 : dval0);
    if (VERB) fprintf(stderr, "!%s: 2nd try: snprintf(%%g) -> \"%s\"\n", me, str);
  }
  /* we now have an accurate string representation in `str` */
  airDeciSepRemove(str, decisep);
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

/* str2fp
implements airStoF and airStoD, which are intended to be **part** of replacing
airSingleParse (née airSingleSscanf), for the case where it is passed a format string
that is only "%g" (for float value) or "%lg" (for double): We interpret "NaN", "inf",
and "-inf" (case-insensitive) as those special values, as well as (if non-NrrdIO) "-pi"
and "pi" as those useful values.
HOWEVER, airSingleSscanf would do this with a `strstr` substring search, which meant that
"banana" would be parsed as a NaN. That's confusing and too cute. We are more stringent
now, requiring (case-insensitive) string equality. Also, while airSingleParse directly
handles parsing a size_t with a "%zu" conversion sequence, that is naturally not part of
this FP-focused code.

NOTE that this is not a `strtof` or `strtod` replacement:
- strto{f,d} ignore leading whitespace
- strto{f,d} can handle "0x" hex specification of floating point
- strto{f,d} can report where they stopped consuming characters at input end,
  and will merely stop parsing at chars that aren't part of a number
str2fp does none of of these, and requires that the entire passed string be
parsed as a number.

Returns:
EOF if given bad arguments,
0 if parsing of `str0` did not succeed (saw extra character at end, or saw the
  non-'.' decimal separator in the input), or
1 if it did succeed, in which case either `*fvalP` or `*dvalP` is assigned a value
  (according to`singlePrec`).

NOTE: the `decisep` passed here *MUST* be the return from airDeciSepLearn, because of how
that creates a special string with earlier bytes carrying extra info.
*/
static int
str2fp(float *fvalP, double *dvalP, int singlePrec, const char *str0,
       const char *decisep) {
  static const char me[] = "str2fp";
  char *str, *end;
  float fval;
  double dval;
  size_t str0len;
  int ret;
  if (!(str0 && decisepGood(decisep, AIR_TRUE /* for parsing */))) {
    return EOF;
  }
#define VERB 0
  str0len = strlen(str0);
  /* allocate local copy for one extra char, in case needed for decisep conversion
     +1 for extra decisep character (1-OR-2-BYTE-SEPARATOR),
     +1 for nul-termination ==> +2 */
  str = AIR_CALLOC(str0len + 2, char);
  if (!str) {
    return EOF;
  }
  airStrcpy(str, str0len + 2, str0);
  if (VERB) fprintf(stderr, "!%s: airStrcpy -> \"%s\"\n", me, str);
  airToLower(str);
  if (VERB)
    fprintf(stderr, "!%s: airToLower -> \"%s\" (length %u)\n", me, str,
            AIR_UINT(strlen(str)));
  /* See if str matches strings for one of the special values. See NOTE above about
     how old airSingleSscanf would parse "banana" as a NaN; we don't do that here. */
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
  if (2 == decisep[-1]
      || /* else (1-OR-2-BYTE-SEPARATOR) its length=1 and */ '.' != decisep[0]) {
    /* There is work to do to implement C-locale-emulation:
       Look for incoming '.' to change to the local decimal separator,
       both cases here assume the decimal separator can only appear once */
    unsigned int ii;
    if (VERB)
      fprintf(stderr, "!%s: decisep=|%s| (length %u)\n", me, decisep,
              AIR_UINT(decisep[-1]));
    if (1 == decisep[-1]) {
      /* first check for locale's decisep in original (non-lower-case) string */
      for (ii = 0; str0[ii]; ii++) {
        if (decisep[0] == str0[ii]) {
          /* whoops: we saw the locale's decisep already in the input, which is not
             valid with our goal of C-locale-emulation, sorry. */
          free(str);
          return 0;
        }
      }
      /* simple sweep: replace the single char if we see it */
      for (ii = 0; str[ii]; ii++) {
        if ('.' == str[ii]) {
          str[ii] = decisep[0]; /* change and bail */
          break;
        }
      }
    } else { /* 1-OR-2-BYTE-SEPARATOR */
      /* its a 2-byte separator */
      for (ii = 1; str0[ii]; ii++) {
        if (decisep[0] == str0[ii - 1] && decisep[1] == str0[ii]) {
          /* same whoops as above: accepting this is not C-locale-emulation */
          free(str);
          return 0;
        }
      }
      /* decimal separator is two `char`, have to shuffle *up* after change */
      for (ii = 0; str[ii]; ii++) {
        if ('.' == str[ii]) {
          unsigned int jj;
          str[ii] = decisep[0];
          if (VERB)
            fprintf(stderr, "!%s: 00 decisep swap -> \"%s\" (length %u)\n", me, str,
                    AIR_UINT(strlen(str)));
          str[str0len + 1] = '\0';
          jj = str0len;
          do {
            str[jj] = str[jj - 1];
            jj--;
            if (VERB)
              fprintf(stderr, "!%s: ... (%u) decisep swap -> \"%s\" (length %u)\n", me,
                      jj, str, AIR_UINT(strlen(str)));
          } while (jj > ii + 1);
          str[ii + 1] = decisep[1];
          break;
        }
      }
    }
    if (VERB)
      fprintf(stderr, "!%s: post decisep swap -> \"%s\" (length %u)\n", me, str,
              AIR_UINT(strlen(str)));
  }
  /* now we run strtod/strtof */
  if (singlePrec) {
    fval = strtof(str, &end); /* (yes locale-dependent) */
  } else {
    dval = strtod(str, &end); /* (yes locale-dependent) */
  }
  ret = (end != str && *end == '\0');
  if (ret) {
    /* only save to caller-supplied variables if consumed everything */
    if (singlePrec) {
      *fvalP = fval;
    } else {
      *dvalP = dval;
    }
  }
  if (VERB) {
    fprintf(stderr, "!%s: %s %s parse %s", me, singlePrec ? "strtof" : "strtod",
            ret ? "did" : "FAILED to", singlePrec ? "float" : "double");
    if (ret) {
      if (singlePrec) {
        fprintf(stderr, " %.9g(native)\n", *fvalP);
      } else {
        fprintf(stderr, " %.17g(native)\n", *dvalP);
      }
    } else {
      fprintf(stderr, "\n");
    }
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
