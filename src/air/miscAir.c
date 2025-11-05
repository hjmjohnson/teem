/*
  Teem: Tools to process and visualize scientific data and images
  Copyright (C) 2009--2025  University of Chicago
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
/* timer functions */
#ifdef _WIN32
#  include <io.h>
#  include <fcntl.h>
#  include <time.h>
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <sys/time.h>
#endif

/*
******** airTeemVersion
******** airTeemReleaseDone
******** airTeemReleaseDate
**
** updated with each release to contain a string representation of
** the Teem version number and release date.  Originated in version 1.5;
** use of TEEM_VERSION #defines started in 1.9
*/
const char *const airTeemVersion = TEEM_VERSION_STRING;
const int airTeemReleaseDone = AIR_FALSE;
const char *const airTeemReleaseDate = "hopefully 2025";

/*
******** airTeemVersionSprint
**
** uniform way of printing information about the Teem version
*/
void
airTeemVersionSprint(char buff[AIR_STRLEN_LARGE + 1]) {
  snprintf(buff, AIR_STRLEN_LARGE + 1, "Teem version %s, %s%s%s", airTeemVersion,
           airTeemReleaseDone ? "released on " : "", airTeemReleaseDate,
           airTeemReleaseDone ? "" : " (not yet released)");
  return;
}

double
_airSanityHelper(double val) {
  return val * val * val;
}

/*
******** airNull()
**
** returns NULL
*/
void *
airNull(void) {

  return NULL;
}

/*
******** airSetNull
**
** dereferences and sets to NULL, returns NULL
*/
void *
airSetNull(void **ptrP) {

  if (ptrP) {
    *ptrP = NULL;
  }
  return NULL;
}

/*
******** airFree()
**
** to facilitate setting a newly free()'d pointer; always returns NULL.
** also makes sure that NULL is not passed to free().
*/
void *
airFree(void *ptr) {

  if (ptr) {
    free(ptr);
  }
  return NULL;
}

/*
******** airFopen()
**
** encapsulates that idea that "-" OR "-=" is either standard in or standard
** out, and does McRosopht stuff required to make piping work. Handling "-="
** is a convenience for implementing NrrdIoState->declineStdioOnTTY, with
** the semantics (not handled here) that "-=" means "read/write from stdin/
** stdout, even when it IS a terminal". But this is currently only supported
** in full Teem, not the minimal NrrdIO library.
**
** Does no error checking.  If fopen fails, then C' errno and strerror are
** left untouched for the caller to access.
*/
FILE *
airFopen(const char *name, FILE *std, const char *mode) {
  FILE *ret;

  if (!strcmp(name, "-")
      /* ---- BEGIN non-NrrdIO */
      || !strcmp(name, "-=")
      /* ---- END non-NrrdIO */
  ) {
    ret = std;
#ifdef _WIN32
    if (strchr(mode, 'b')) {
      _setmode(_fileno(ret), _O_BINARY);
    }
#endif
  } else {
    ret = fopen(name, mode);
  }
  return ret;
}

/*
******** airFclose()
**
** just to facilitate setting a newly fclose()'d file pointer to NULL
** also makes sure that NULL is not passed to fclose(), and won't close
** stdin, stdout, or stderr (its up to the user to open these correctly)
*/
FILE *
airFclose(FILE *file) {

  if (file) {
    if (!(stdin == file || stdout == file || stderr == file)) {
      fclose(file);
    }
  }
  return NULL;
}

/* helper function for airSinglePrintf:
   shift characters between clo and chi (inclusive) up by 2 */
static void
shift2(char *clo, char *chi) {
  while (chi >= clo) {
    *(chi + 2) = *chi;
    chi--;
  }
  return;
}

/*
 ******* airSinglePrintf
 *
 * a complete stand-in for {f|s}printf(), as long as the given format string contains
 * exactly one conversion sequence, and does use any precision modifiers.  The utility of
 * this is to standardize the printing of IEEE 754 special values:
 * NAN (any kind) -> "NaN"
 * POS_INF -> "inf"   <-- TeemV2 change: was "+inf"
 * NEG_INF -> "-inf"
 * The format string can contain other things besides just the conversion sequence:
 * airSinglePrintf(f, NULL, "This %f isn't a number\n", AIR_NAN) will be the same as:
 * fprintf(f, "This %s isn't a number\n", "NaN");
 *
 * To get fprintf behavior, pass `str` as NULL
 * to get sprintf bahavior, pass `file` as NULL
 *       and pass the allocated size of `str` as `strSize`
 *       so that we can use snprintf instead of sprintf
 *
 * Finding a complete {f|s|}printf replacement would be great, but finding one compatible
 * with our LGPL+linking exception licensing is hard.
 *
 * 2025 scrutiny: The apparent intent of the old code was that with conversion sequence
 * "%g" (in the case of printing a single-precision float) or "%lg" (for a double), then
 * the code used a different conversion sequence when necessary to ensure that upon
 * sscanf parsing, would get back to the original value. HOWEVER: some of the code
 * comments acknowledged that after going through var-args the float becomes a double
 * (so `isF`,`isD` variable names risk being delusional). But the BUG was that the
 * strategy for ensuring accuracy was broken: 0.10000000000000002 (which parses to the
 * next double after the one parsed from 0.1) would also be printed as "0.1".
 * Also, NaN values were printed as " NaN" (with needless leading space)
 *
 * So, for the TeemV2 re-write: now NaN is printed as "NaN", and now "%g" means: "I'm
 * sending in a float (and I know you'll get it as a double), but can you print it with a
 * concise string that parses back to the same float". "%lg" means the same for a double.
 * Round-trip accuracy is ensured by internally changing "%g" to "%.9g" or from "%lg" to
 * "%.17g" when sscanf shows it to be necessary.
 */
int
airSinglePrintf(FILE *file, char *str, size_t strSize, const char *_fmt, ...) {
  int ret, isF, isD;
  const char *_p0, *_p1, *_p2, *_p3, *_p4, *_p5;
  va_list ap;

  va_start(ap, _fmt);

  /* look for unmodified floating point conversion sequences */
  _p0 = strstr(_fmt, "%e");
  _p1 = strstr(_fmt, "%f");
  _p2 = strstr(_fmt, "%g");
  _p3 = strstr(_fmt, "%le");
  _p4 = strstr(_fmt, "%lf");
  _p5 = strstr(_fmt, "%lg");
  isF = _p0 || _p1 || _p2;
  isD = _p3 || _p4 || _p5;
  /* the code here says `isF` and `isD` as if it means "is float" or "is double".
     It would more accurately be `is2` or `is3`, as in, "is 2-character conv. seq., or
     "is 3-character conv. seq."
     (but for TeemV2 we run with that type implication for "%g" and "%lg") */
  if (isF || isD) {
#define PRINT(F, S, C, V)                                                               \
  ((F) /* */                                                                            \
     ? fprintf((F), (C), (V))                                                           \
     : snprintf((S), strSize, (C), (V)))
    double val0;
    const char *_conv;
    unsigned int szfmt;
    char *fmt, *conv;
    int cls;
    if (isF) {
      _conv = _p0 ? _p0 : (_p1 ? _p1 : _p2);
    } else {
      _conv = _p3 ? _p3 : (_p4 ? _p4 : _p5);
    }
    /* allocate our own copy of _fmt, with length increase by the most we might need to
       add with |%g|=2 --> |%.9g|=4 or |%lg|=3 --> |%.17g|=5, namely 2 */
    szfmt = (unsigned int)strlen(_fmt) + 2 + 1;
    fmt = AIR_CALLOC(szfmt, char);
    assert(fmt);
    strncpy(fmt, _fmt, szfmt);
    conv = fmt + (_conv - _fmt); /* conv points into fmt like _conv points into _fmt */
    /* must use "double" here to consume var-arg instead of "float" because var args are
       _always_ subject to old-style C type promotions: float promotes to double */
    val0 = va_arg(ap, double);
    /* printf("!%s: hello val0 = %.17g\n", __func__, val0); */
    cls = airFPClass_d(val0);
    if (airFP_NAN == cls || airFP_POS_INF == cls || airFP_NEG_INF == cls) {
      /* we do not need to rebuild the `fmt` string in parts, just sneakily tweak it */
      if (isF) {
        /* started with a 2-char conversion sequence, use a different one */
        memcpy(conv, "%s", 2);
      } else {
        /* isD: we started with a 3-char conversion sequence, replaces it with
           another 3-char sequence for the special value string (3 or 4 chars).
           {f,s}printf would print the string correctly with plain `%s` but we're
           just filling out three characters of a conversion sequence to avoid having to
           change any other characters in `fmt`. */
        memcpy(conv, airFP_NEG_INF == cls ? "%4s" : "%3s", 3);
      }
      /* now print with the string conversion sequence just set */
      switch (cls) {
      case airFP_NAN:
        ret = PRINT(file, str, fmt, "NaN");
        break;
      case airFP_POS_INF:
        ret = PRINT(file, str, fmt, "inf");
        break;
      case airFP_NEG_INF:
        ret = PRINT(file, str, fmt, "-inf");
        break;
      }
    } else { /* not IEEE754 special value */
      if (_p2 || _p5) {
        /* 2025 Prof GLK revisiting this post-doc GLK code (svn r2879 2005-09-21)
        and comments with better floating-point experience and understanding:
           / * got "%g" or "%lg", see if it would be better to use "%f" * /
             * sprintf(buff, "%f", val);
             * sscanf(buff, "%lf", &fVal);
             * sprintf(buff, "%g", val);
             * sscanf(buff, "%lf", &gVal);
             * if (fVal != gVal) ...
             * / * using %g/%lg lost precision, use %f/%lf instead * /
        NO, and NO.
        NO#1: The relevant test for the insufficient precision of %g/%lg for the round-
           trip val-sprintf->buff-sscanf->val is whether val == gVal, NOT fVal != gVal.
        NO#2: %f/%lf do *not* ensure round-trip accuracy. The r2879 commit message
           "fixed bug in handling of loss of precision with %g/%lg" was ignorant.
        So what to do: we rescue+extend the apparent intent of this code and implement:
         - if got %g, assume value came from a (single-precision) float, and find a
           string that ensures round-trip accuracy for a float
         - if got %lg, assume value came as a double, and find a string that ensures
           round-trip accuracy for a double
         - (the `isF` and `isD` variable names indicate the original code intent)
        Teem code using this function was updated to work with this behavior.
        See new test program teem/src/air/test/asprint */
        char buff[AIR_STRLEN_SMALL + 1];
        size_t buffSize = AIR_STRLEN_SMALL + 1;
        if (_p2) /* got "%g" intended for float */ {
          float fltval1, fltval0 = (float)val0;
          snprintf(buff, buffSize, "%g", fltval0);
          sscanf(buff, "%f", &fltval1);
          if (fltval1 != fltval0) {
            /* "%g" by itself did NOT gave round-trip single-precision accuracy;
               assemble new format string with |%g|=2 --> |%.9g|=4 */
            shift2(conv + 2 /* first char after %g */,
                   fmt + strlen(fmt) - 1 /* last char in fmt */);
            memcpy(conv, "%.9g", 4);
          }
          /* else "%g" gave round-trip single-precision accuracy; use `fmt` as is */
        } else /* _p5: got "%lg% intended for double */ {
          double val1;
          snprintf(buff, buffSize, "%g", val0);
          sscanf(buff, "%lf", &val1);
          if (val1 != val0) { /* SORRY COPY PASTA */
            shift2(conv + 3 /* first char after %lg */,
                   fmt + strlen(fmt) - 1 /* last char in `fmt` */);
            memcpy(conv, "%.17g", 5);
          }
          /* else "%lg" gave round-trip double-precision accuracy; use `fmt` as is */
        }
        /* printf("!%s: fmt=|%s|\n", __func__, fmt); */
        /* end of possible fiddling `fmt` to ensure roung-trip accuracy */
      } /* else `isF || isD` but not _p2="%g" and not _p5="%lg": got non-special value
           to print with _p0="%e", _p1="%f", _p3="%le", or _p4="%lf"
           ==> can use `fmt` as is */
      ret = PRINT(file, str, fmt, val0);
    } /* end of else !special */
    free(fmt);
#undef PRINT
  } else {
    /* `!isF && !isD`: conversion sequence is either for float or double but something
       more specific like "%.17g" (which NOTE does not get the above special treatment of
       IEEE754 special values!s), or, it isn't for any kind of floating point value.
       We can use the given `_fmt` as is (no local allocation of `fmt`) */
    ret = file ? vfprintf(file, _fmt, ap) : vsnprintf(str, strSize, _fmt, ap);
  }

  va_end(ap);
  return ret;
}

/*
******** airSprintSize_t
**
** sprints a single size_t to a given string, side-stepping
** non-standardized format specifier confusion with printf
*/
char *
airSprintSize_t(char _str[AIR_STRLEN_SMALL + 1], size_t val) {
  char str[AIR_STRLEN_SMALL + 1];
  unsigned int si;

  if (!_str) {
    return NULL;
  }
  si = AIR_STRLEN_SMALL;
  str[si] = '\0';
  do {
    str[--si] = AIR_CAST(char, (val % 10) + '0');
    val /= 10;
  } while (val);
  strcpy(_str, str + si);
  return _str;
}

/*
******** airSprintPtrdiff_t
**
** sprints a single ptrdiff_t to a given string, side-stepping
** non-standardized format specifier confusion with printf
*/
char *
airSprintPtrdiff_t(char _str[AIR_STRLEN_SMALL + 1], ptrdiff_t val) {
  char str[AIR_STRLEN_SMALL + 1];
  unsigned int si;
  int sign;

  if (!_str) {
    return NULL;
  }
  si = AIR_STRLEN_SMALL;
  str[si] = '\0';
  sign = (val < 0 ? -1 : 1);
  do {
    ptrdiff_t dig;
    dig = val % 10;
    str[--si] = AIR_CAST(char, dig > 0 ? dig + '0' : -dig + '0');
    val /= 10;
  } while (val);
  if (-1 == sign) {
    str[--si] = '-';
  }
  strcpy(_str, str + si);
  return _str;
}

/* ---- BEGIN non-NrrdIO */

const int airPresent = 42;

/*
** sprints a length-"len" vector "vec" of size_t values into "dst", which is
** simply assumed to be big enough to hold this.  Vector enclosed in "[]" and
** values separated by ","
*/
char *
airSprintVecSize_t(char *dst, const size_t *vec, unsigned int len) {
  char stmp[AIR_STRLEN_SMALL + 1];
  unsigned int axi;

  /* if we got NULL to sprint into, there's nothing to do but return it */
  if (!dst) {
    return dst;
  }
  strcpy(dst, "[");
  for (axi = 0; axi < len; axi++) {
    if (axi) {
      strcat(dst, ",");
    }
    airSprintSize_t(stmp, vec[axi]);
    strcat(dst, stmp);
  }
  strcat(dst, "]");
  return dst;
}

/*
******** airPrettySprintSize_t
**
** sprints a single size_t in a way that is human readable as
** bytes, kilobytes (KB), etc
*/
char *
airPrettySprintSize_t(char str[AIR_STRLEN_SMALL + 1], size_t val) {
  static const char suff[][7] = {"bytes", "KB", "MB", "GB", "TB", "PB", "EB"};
  unsigned int suffIdx, suffNum;
  double dval;

  if (!str) {
    return NULL;
  }
  suffIdx = 0;
  dval = AIR_CAST(double, val);
  suffNum = AIR_UINT(sizeof(suff) / sizeof(suff[0]));
  while (suffIdx < suffNum - 1) { /* while we can go to a bigger suffix */
    if (dval > 1024) {
      dval /= 1024;
      suffIdx++;
    } else {
      break;
    }
  }
  snprintf(str, AIR_STRLEN_SMALL + 1, "%g %s", dval, suff[suffIdx]);
  return str;
}

/*
******** airStderr, airStdout, airStdin
**
** these exist only to give uniform access to FILE *s that might be
** annoying to access in higher-level language wrappings around Teem.
*/
FILE *
airStderr(void) {
  return stderr;
}

FILE *
airStdout(void) {
  return stdout;
}

FILE *
airStdin(void) {
  return stdin;
}

unsigned int
airBitsSet(unsigned int vv) {
  /* http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetKernighan */
  unsigned int cc;
  for (cc = 0; vv; cc++) {
    /* wherever lowest bit is on in vv; vv-1 will have it off, and leave
       unchanged all the higher bits */
    vv &= vv - 1;
  }
  return cc;
}

/*
******** AIR_INDEX(i,x,I,L,t)
**
** READ CAREFULLY!!
**
** Utility for mapping a floating point x in given range [i,I] to the
** index of an array with L elements, AND SAVES THE INDEX INTO GIVEN
** VARIABLE t, WHICH MUST BE OF SOME INTEGER TYPE because this relies
** on the implicit cast of an assignment to truncate away the
** fractional part.  ALSO, t must be of a type large enough to hold
** ONE GREATER than L.  So you can't pass a variable of type unsigned
** char if L is 256
**
** DOES NOT DO BOUNDS CHECKING: given an x which is not inside [i,I],
** this may produce an index not inside [0,L-1] (but it won't always
** do so: the output being outside range [0,L-1] is not a reliable
** test of the input being outside range [i, I]).  The mapping is
** accomplished by dividing the range from i to I into L intervals,
** all but the last of which is half-open; the last one is closed.
** For example, the number line from 0 to 3 would be divided as
** follows for a call with i = 0, I = 4, L = 4:
**
** index:       0    1    2    3 = L-1
** intervals: [   )[   )[   )[    ]
**            |----|----|----|----|
** value:     0    1    2    3    4
**
** The main point of the diagram above is to show how I made the
** arbitrary decision to orient the half-open interval, and which
** end has the closed interval.
**
** Note that AIR_INDEX(0,3,4,4,t) and AIR_INDEX(0,4,4,4,t) both set t = 3
**
** The reason that this macro requires a argument for saving the
** result is that this is the easiest way to avoid extra conditionals.
** Otherwise, we'd have to do some check to see if x is close enough
** to I so that the generated index would be L and not L-1.  "Close
** enough" because due to precision problems you can have an x < I
** such that (x-i)/(I-i) == 1, which was a bug with the previous version
** of this macro.  It is far simpler to just do the index generation
** and then do the sneaky check to see if the index is too large by 1.
** We are relying on the fact that C _defines_ boolean true to be exactly 1.
**
** Note also that we are never explicity casting to one kind of int or
** another-- the given t can be any integral type, including long long.
*/
/*
#define AIR_INDEX(i,x,I,L,t) ( \
(t) = (L) * ((double)(x)-(i)) / ((double)(I)-(i)), \
(t) -= ((t) == (L)) )
*/
/*
******* airIndex
**
** Given a value range [min,max], a single input value val inside the range,
** and a number of intervals N spanning and evenly dividing that range,
** airIndex returns the index of the interval containing val:
**
**  input value range: min                     max
**   val somewhere in:  |-----|-----|-----|-----|
**        N intervals:  [    )[    )[    )[     ]  (for N=4)
**       output index:     0     1     2     3
**
** This can be used (as in nrrdHisto and other histogramming functions) to
** represent the range [min,max] with N samples between 0 and N-1.  Those
** samples are cell-centered, because "0" is logically located (in the
** continuous input range) in the *middle* of the first of the N intervals.
** In contrast, the *endpoints* of the N intervals (the 5 "|" in the picture
** above) form N+1 *node*-centered samples from min to max.
**
** If max < min, then "min" and "max" would be swapped in the diagram
** above and their roles are switched. This overdue fix was only added
** in version 1.11.
**
** NOTE: This does not do bounds checking; for that use airIndexClamp
*/
unsigned int
airIndex(double min, double val, double max, unsigned int N) {
  unsigned int idx;
  double mnm;

  mnm = max - min;
  if (mnm > 0) {
    idx = AIR_UINT(N * (val - min) / mnm);
    idx -= (idx == N);
  } else if (mnm < 0) {
    idx = AIR_UINT(N * (val - max) / (-mnm));
    idx -= (idx == N);
    idx = N - 1 - idx;
  } else {
    idx = 0;
  }
  return idx;
}

unsigned int
airIndexClamp(double min, double val, double max, unsigned int N) {
  unsigned int idx;
  double mnm;

  mnm = max - min;
  if (mnm > 0) {
    val = AIR_MAX(min, val);
    idx = AIR_UINT(N * (val - min) / mnm);
    idx = AIR_MIN(idx, N - 1);
  } else if (mnm < 0) {
    val = AIR_MAX(max, val);
    idx = AIR_UINT(N * (val - max) / (-mnm));
    idx = AIR_MIN(idx, N - 1);
    idx = N - 1 - idx;
  } else {
    idx = 0;
  }
  return idx;
}

airULLong
airIndexULL(double min, double val, double max, airULLong N) {
  airULLong idx;
  if (min == max) {
    return 0;
  }
  if (min > max) {
    return N - 1 - airIndexULL(max, val, min, N);
  }
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MINGW32__)
  /* compile error on Win32-vs60: "error C2520: conversion from
     unsigned __int64 to double not implemented, use signed __int64 */
  airLLong sidx;
  sidx = AIR_CAST(airLLong, AIR_CAST(double, N) * (val - min) / (max - min));
  idx = AIR_CAST(airULLong, sidx);
#else
  idx = AIR_CAST(airULLong, AIR_CAST(double, N) * (val - min) / (max - min));
#endif
  idx -= (idx == N);
  return idx;
}

airULLong
airIndexClampULL(double min, double val, double max, airULLong N) {
  airULLong idx;
  if (min == max) {
    return 0;
  }
  if (min > max) {
    return N - 1 - airIndexULL(max, val, min, N);
  }
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MINGW32__)
  airLLong sidx;
  val = AIR_MAX(min, val); /* see note in airIndexClamp */
  sidx = AIR_CAST(airLLong, AIR_CAST(double, N) * (val - min) / (max - min));
  idx = AIR_CAST(airULLong, sidx);
#else
  val = AIR_MAX(min, val); /* see note in airIndexClamp */
  idx = AIR_CAST(airULLong, AIR_CAST(double, N) * (val - min) / (max - min));
#endif
  idx = AIR_MIN(idx, N - 1);
  return idx;
}

/*
 ****** airDoneStr()
 *
 * dinky little utility for generating progress messages of the form
 * "  1.9%" or " 35.3%" or  "100.0%"
 *
 * The message will ALWAYS be six characters, and will ALWAYS be preceeded by six
 * backspaces.  Thus, you pass in a string to snprintf() into,
 * and it had better be allocated for at least 6+6+1 = 13 chars.
 */
char *
airDoneStr(double start, double here, double end, char str[13]) {
  size_t strSize = 13;
  int perc = 0;

  if (str) {
    if (end != start)
      perc = (int)(1000 * (here - start) / (end - start) + 0.5);
    else
      perc = 1000;
    if (perc < 0) {
      snprintf(str, strSize, "\b\b\b\b\b\b ---- ");
    } else if (perc < 1000) {
      snprintf(str, strSize, "\b\b\b\b\b\b% 3d.%d%%", perc / 10, perc % 10);
    } else if (perc == 1000) {
      /* the "% 3d" formatting sequence should have taken care
         of this, but whatever */
      snprintf(str, strSize, "\b\b\b\b\b\b100.0%%");
    } else {
      snprintf(str, strSize, "\b\b\b\b\b\b done.");
    }
  }

  return str;
}

/*
******** airTime()
**
** returns current time in seconds (with millisecond resolution only
** when not on Windows) as a double.  From "man gettimeofday": The
** time is expressed in seconds and microseconds since midnight (0
** hour), January 1, 1970.
*/
double
airTime(void) {
#ifdef _WIN32
  /* Use high-resolution system time; FILETIME is 100-ns ticks since Jan 1, 1601 UTC */
  FILETIME ft;
  ULARGE_INTEGER uli;
  const unsigned long long EPOCH_DIFF_100NS = 116444736000000000ULL; /* 1601->1970 */
  GetSystemTimeAsFileTime(&ft);
  uli.LowPart = ft.dwLowDateTime;
  uli.HighPart = ft.dwHighDateTime;
  unsigned long long unix100ns = (uli.QuadPart >= EPOCH_DIFF_100NS)
                                   ? (uli.QuadPart - EPOCH_DIFF_100NS)
                                   : 0ULL;
  return (double)unix100ns / 10000000.0; /* convert 100ns to seconds */
#else
  double sec, usec;
  struct timeval tv;

  gettimeofday(&tv, NULL);
  sec = AIR_CAST(double, tv.tv_sec);
  usec = AIR_CAST(double, tv.tv_usec);
  return sec + usec * 1.0e-6;
#endif
}

/*
******** airEqvSettle()
**
** takes a mapping map[i], i in [0..len-1], and shifts the range of the
** mapping downward so that the range is a contiguous set of uints
** starting at 0.
**
** returns the number of different uints; previous version returned one
** less than this (the highest mapping output value, after the settling)
**
** honestly this doesn't necessarily have anything to do with processing
** equivalence classes, but its an operation that is nice to have in
** those cases
*/
unsigned int
airEqvSettle(unsigned int *map, unsigned int len) {
  unsigned int i, j, count, max, *hit;

  max = 0;
  for (i = 0; i < len; i++) {
    max = AIR_MAX(max, map[i]);
  }
  hit = (unsigned int *)calloc(1 + max, sizeof(unsigned int));
  for (i = 0; i < len; i++) {
    hit[map[i]] = 1;
  }
  count = 0;
  for (j = 0; j <= max; j++) {
    if (hit[j]) {
      hit[j] = count;
      count += 1;
    }
  }
  for (i = 0; i < len; i++) {
    map[i] = hit[map[i]];
  }
  free(hit);
  return count;
}

/*
******** airEqvMap
**
** takes the equivalence pairs in eqvArr, and an array of uints map of
** length len, and puts in map[i] the uint that class i's value should
** be changed to.
**
** based on numerical recipes, C edition, pg. 346
** modifications:
**  - when resolving ancestors, map to the one with the lower index.
**  - applies settling to resulting map
**
** returns the number of different class IDs
*/
unsigned int
airEqvMap(airArray *eqvArr, unsigned int *map, unsigned int len) {
  unsigned int *eqv, j, k, t, eqi;

  for (j = 0; j < len; j++) {
    map[j] = j;
  }
  eqv = (unsigned int *)(eqvArr->data);
  for (eqi = 0; eqi < eqvArr->len; eqi++) {
    j = eqv[0 + 2 * eqi];
    k = eqv[1 + 2 * eqi];
    while (map[j] != j) {
      j = map[j];
    }
    while (map[k] != k) {
      k = map[k];
    }
    if (j != k) {
      if (j < k) {
        t = j;
        j = k;
        k = t;
      }
      map[j] = k;
    }
  }
  for (j = 0; j < len; j++) {
    while (map[j] != map[map[j]]) {
      map[j] = map[map[j]];
    }
  }
  return airEqvSettle(map, len);
}

/*
******** airEqvAdd
**
** adds another equivalence (which may or may not amount to adding
** a new class; that will be determined later)
*/
void
airEqvAdd(airArray *eqvArr, unsigned int j, unsigned int k) {
  unsigned int *eqv, eqi;

  /* HEY: would it speed things up at all to enforce j < k? */
  if (eqvArr->len) {
    /* we have some equivalences, but we're only going to check against
       the last one in an effort to reduce duplicate entries */
    eqv = AIR_CAST(unsigned int *, eqvArr->data);
    eqi = eqvArr->len - 1;
    if ((eqv[0 + 2 * eqi] == j && eqv[1 + 2 * eqi] == k)
        || (eqv[0 + 2 * eqi] == k && eqv[1 + 2 * eqi] == j)) {
      /* don't add a duplicate */
      return;
    }
  }
  eqi = airArrayLenIncr(eqvArr, 1);
  eqv = AIR_CAST(unsigned int *, eqvArr->data);
  eqv[0 + 2 * eqi] = j;
  eqv[1 + 2 * eqi] = k;
  return;
}
/* ---- END non-NrrdIO */
