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

/* Enable the POSIX-2008 thread-safe locale API (newlocale/strtod_l) where
   available; must precede the first system-header include (air.h pulls in
   <stdlib.h>).  TEEM_HAS_STRTOD_L selects a locale-independent numeric parse
   in airSingleSscanf(); platforms lacking it keep the historical sscanf(). */
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__)                   \
  || defined(__OpenBSD__) || defined(__DragonFly__)
#  include <xlocale.h>
#  define TEEM_HAS_STRTOD_L 1
#elif defined(__linux__)
/* __GLIBC__ comes from <features.h>, not a compiler predefine, so it is unset
   at this pre-include point; key off __linux__ (glibc, and musl >= 1.2.1,
   provide newlocale/strtod_l under _GNU_SOURCE). */
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE 1
#  endif
#  include <locale.h>
#  define TEEM_HAS_STRTOD_L 1
#elif defined(_MSC_VER)
#  include <locale.h>
#  include <intrin.h>
#  define TEEM_HAS_STRTOD_L_WIN 1
#endif

#include "air.h"
#include "privateAir.h"

#include <errno.h>

#if defined(TEEM_HAS_STRTOD_L)
#  if TEEM_PTHREAD
#    include <pthread.h>
#  endif
/* Cached locale for locale-independent strtod_l; created once.  Only
   LC_NUMERIC is "C": the other categories follow the global locale so a
   uselocale() swap does not change ctype or collation behavior. */
static locale_t myCLocale = (locale_t)0;
static void
myCLocaleInit(void) {
  locale_t base = duplocale(LC_GLOBAL_LOCALE);
  myCLocale = newlocale(LC_NUMERIC_MASK, "C", base);
  if (!myCLocale && base) {
    freelocale(base);
  }
}
static locale_t
myCLocaleGet(void) {
#  if TEEM_PTHREAD
  static pthread_once_t once = PTHREAD_ONCE_INIT;
  pthread_once(&once, myCLocaleInit);
#  else
  /* without pthreads a first-use race can only create a second, identical "C"
     locale object, which is harmless */
  if (!myCLocale) {
    myCLocaleInit();
  }
#  endif
  return myCLocale;
}
#elif defined(TEEM_HAS_STRTOD_L_WIN)
/* Cached "C" locale published exactly once via an atomic pointer CAS, so
   concurrent first use is race-free: the losing thread frees its duplicate. */
static void *volatile myCLocale = NULL;
static _locale_t
myCLocaleGet(void) {
  _locale_t loc = (_locale_t)myCLocale;
  if (!loc) {
    _locale_t created = _create_locale(LC_NUMERIC, "C");
    void *prev = _InterlockedCompareExchangePointer(&myCLocale, created, NULL);
    if (prev) {
      _free_locale(created);
      loc = (_locale_t)prev;
    } else {
      loc = created;
    }
  }
  return loc;
}
#endif

/* Cached "C" LC_NUMERIC locale as an opaque pointer (locale_t or _locale_t);
   NULL when the platform lacks the thread-safe locale API or creation failed.
   Shared with airSinglePrintf (miscAir.c) via privateAir.h. */
void *
air__CLocale(void) {
#if defined(TEEM_HAS_STRTOD_L) || defined(TEEM_HAS_STRTOD_L_WIN)
  return (void *)myCLocaleGet();
#else
  return NULL;
#endif
}

/* clang-format off */
static const char *
boolStr[] = {
  "(unknown bool)",
  "false",
  "true"
};

static const char *
boolDesc[] = {
  "unknown boolean",
  "false",
  "true"
};

static const int
boolVal[] = {
  -1,
  AIR_FALSE,
  AIR_TRUE
};

static const char *
boolStrEqv[] = {
  "0", "no", "n", "false", "f", "off", "nope",
  "1", "yes", "y", "true", "t", "on", "yea",
  ""
};

static const int
boolValEqv[] = {
  AIR_FALSE, AIR_FALSE, AIR_FALSE, AIR_FALSE, AIR_FALSE, AIR_FALSE, AIR_FALSE,
  AIR_TRUE, AIR_TRUE, AIR_TRUE, AIR_TRUE, AIR_TRUE, AIR_TRUE, AIR_TRUE
};

static const airEnum
bool_ae = {
  "boolean",
  2,
  boolStr,
  boolVal,
  boolDesc,
  boolStrEqv,
  boolValEqv,
  AIR_FALSE
};

const airEnum *const airBool = &bool_ae;
/* clang-format on */

/* NOTE: see info in air.h about TeemV2 changes to all things airType and airParse */

int
airSingleSscanf(const char *str, const char *fmt, void *ptr) {
  char *tmp;
  double val;
  int ret;

  if (!(str && fmt && ptr)) {
    return EOF;
  }
  if (!strcmp(fmt, "%e") || !strcmp(fmt, "%f") || !strcmp(fmt, "%g")
      || !strcmp(fmt, "%le") || !strcmp(fmt, "%lf") || !strcmp(fmt, "%lg")) {
    tmp = airStrdup(str);
    if (!tmp) {
      return EOF;
    }
    airToLower(tmp);
    if (strstr(tmp, "nan")) {
      val = (double)AIR_NAN;
    }
    /* ---- BEGIN non-NrrdIO */
    else if (strstr(tmp, "-pi")) {
      val = -AIR_PI;
    } else if (strstr(tmp, "pi")) {
      val = AIR_PI;
    }
    /* ---- END non-NrrdIO */
    else if (strstr(tmp, "-inf")) {
      val = (double)AIR_NEG_INF;
    } else if (strstr(tmp, "inf")) {
      val = (double)AIR_POS_INF;
    } else {
      /* nothing special matched; parse one floating-point value with a
         locale-independent "C" parse so the '.' decimal separator is honored
         regardless of the process/thread LC_NUMERIC setting. */
#if defined(TEEM_HAS_STRTOD_L) || defined(TEEM_HAS_STRTOD_L_WIN)
      char *endptr = NULL;
      double dval = 0.0;
#  if defined(TEEM_HAS_STRTOD_L)
      locale_t cloc = myCLocaleGet();
#  else
      _locale_t cloc = myCLocaleGet();
#  endif
      /* a null "C" locale means we cannot parse locale-independently:
         fail the parse rather than risk a silently wrong value */
      ret = 0;
      if (cloc) {
        errno = 0;
#  if defined(TEEM_HAS_STRTOD_L)
        dval = strtod_l(str, &endptr, cloc);
#  else
        dval = _strtod_l(str, &endptr, cloc);
#  endif
        /* accept a conversion unless it overflowed (ERANGE with +/-HUGE_VAL);
           ERANGE underflow is a valid subnormal or zero */
        if (endptr != str
            && !(ERANGE == errno && (HUGE_VAL == dval || -HUGE_VAL == dval))) {
          ret = 1;
        }
      }
      /* in double range but overflows the requested float: AIR_FLOAT would
         narrow to infinity, which is the same silent wrong value that the
         ERANGE test above rejects */
      if (ret && fmt[1] != 'l' && (dval > FLT_MAX || dval < -FLT_MAX)) {
        ret = 0;
      }
      if (ret) {
        if (fmt[1] == 'l') {
          *((double *)(ptr)) = dval;
        } else {
          *((float *)(ptr)) = AIR_FLOAT(dval);
        }
      }
      free(tmp);
      return ret;
#else
      /* fallback: historical locale-dependent behavior, unchanged */
      ret = sscanf(str, fmt, ptr);
      free(tmp);
      return ret;
#endif
    }
    /* else we matched "nan", "-inf", or "inf"; now set val accordingly */
    if (fmt[1] == 'l') {
      /* we were given a double pointer */
      *((double *)(ptr)) = val;
    } else {
      /* we were given a float pointer */
      *((float *)(ptr)) = AIR_FLOAT(val);
    }
    free(tmp);
    return 1;
  } else if (!strcmp(fmt, "%zu")) {
    /* its a size_t */
    size_t tsz = 0;        /* tmp size_t */
    const char *chh = str; /* char here */
    int prsd = AIR_FALSE;  /* we parsed/converted something */
    while (*chh) {
      int dig;
      dig = AIR_INT(*chh - '0'); /* dig could be positive or negative */
      if (AIR_IN_CL(0, dig, 9)) {
        size_t nsz = 10 * tsz + AIR_CAST(size_t, dig);
        if (nsz >= tsz) {
          /* no overflow has happened */
          tsz = nsz;
        } else {
          /* new value nsz got smaller than previous tsz: overflowed */
          tsz = AIR_SIZE_T(-1);
        }
        prsd = AIR_TRUE;
      } else if (
        !isspace(*chh) /* it is not whitespace (it might be '\0') */
        || prsd /* else it is whitespace and we've already parsed something */) {
        break;
      }
      chh++;
    }
    if (prsd) {
      /* there were characters that were processed as digits */
      *((size_t *)(ptr)) = tsz;
      ret = 1;
    } else {
      /* no parsing/conversion happened: return either 0 or EOF */
      ret = *chh ? 0 : EOF;
    }
    return ret;
  } else {
    /* not a float, double, or size_t, let sscanf handle it */
    return sscanf(str, fmt, ptr);
  }
}

unsigned int
airParseStrB(int *out, const char *_s, const char *ct, unsigned int n) {
  unsigned int i;
  char *tmp, *s, *last;

  /* if we got NULL, there's nothing to do */
  if (!(out && _s && ct)) return 0;

  /* copy the input so that we don't change it */
  s = airStrdup(_s);

  /* keep calling airStrtok() until we have everything */
  for (i = 0; i < n; i++) {
    tmp = airStrtok(i ? NULL : s, ct, &last);
    if (!tmp) {
      free(s);
      return i;
    }
    out[i] = airEnumVal(airBool, tmp);
    if (airEnumUnknown(airBool) == out[i]) {
      free(s);
      return i;
    }
  }
  free(s);
  return n;
}

#define PARSE_STR_ARGS(type) type *out, const char *_s, const char *ct, unsigned int n
#define PARSE_STR_BODY(format)                                                          \
  unsigned int i;                                                                       \
  char *tmp, *s, *last;                                                                 \
                                                                                        \
  /* if we got NULL, there's nothing to do */                                           \
  if (!(out && _s && ct)) return 0;                                                     \
                                                                                        \
  /* copy the input so that we don't change it */                                       \
  s = airStrdup(_s);                                                                    \
                                                                                        \
  /* keep calling airStrtok() until we have everything */                               \
  for (i = 0; i < n; i++) {                                                             \
    tmp = airStrtok(i ? NULL : s, ct, &last);                                           \
    if (!tmp) {                                                                         \
      free(s);                                                                          \
      return i;                                                                         \
    }                                                                                   \
    if (1 != airSingleSscanf(tmp, format, out + i)) {                                   \
      free(s);                                                                          \
      return i;                                                                         \
    }                                                                                   \
  }                                                                                     \
  free(s);                                                                              \
  return n;

/*
******* airParse*()
*
* parse `n` shorts, ints, floats, doubles, or single chars, from some given string `s`;
* the `n` of them are delimited by characters in `ct`; put the results in `out`.
*
* Returns the number of things succesfully parsed- should be `n`; there's been an error
* if return is < `n`.
*
* This uses air's thread-safe strtok() replacement: airStrtok()
*/
unsigned int
airParseStrH(PARSE_STR_ARGS(short)) {
  PARSE_STR_BODY("%hd")
}

unsigned int
airParseStrUH(PARSE_STR_ARGS(unsigned short)) {
  PARSE_STR_BODY("%hu")
}

unsigned int
airParseStrI(PARSE_STR_ARGS(int)) {
  PARSE_STR_BODY("%d")
}

unsigned int
airParseStrUI(PARSE_STR_ARGS(unsigned int)) {
  PARSE_STR_BODY("%u")
}

unsigned int
airParseStrL(PARSE_STR_ARGS(long int)) {
  PARSE_STR_BODY("%ld")
}

unsigned int
airParseStrUL(PARSE_STR_ARGS(unsigned long int)) {
  PARSE_STR_BODY("%lu")
}

unsigned int
airParseStrZ(PARSE_STR_ARGS(size_t)) {
  PARSE_STR_BODY("%zu")
}

unsigned int
airParseStrF(PARSE_STR_ARGS(float)) {
  PARSE_STR_BODY("%f")
}

unsigned int
airParseStrD(PARSE_STR_ARGS(double)) {
  PARSE_STR_BODY("%lf")
}
#undef PARSE_STR_ARGS
#undef PARSE_STR_BODY

unsigned int
airParseStrC(char *out, const char *_s, const char *ct, unsigned int n) {
  unsigned int i;
  char *tmp, *s, *last;

  /* if we got NULL, there's nothing to do */
  if (!(out && _s && ct)) return 0;

  /* copy the input so that we don't change it */
  s = airStrdup(_s);

  /* keep calling airStrtok() until we have everything */
  for (i = 0; i < n; i++) {
    tmp = airStrtok(i ? NULL : s, ct, &last);
    if (!tmp) {
      free(s);
      return i;
    }
    out[i] = tmp[0];
  }
  free(s);
  return n;
}

/* for TeemV2: "greedy" was a dumb idea, and is gone. It used to be passed in as the last
 * var-args argument. In the case of n==1, if greedy than the whole string was returned,
 * else (!greedy) airStrtok() was called */
unsigned int
airParseStrS(char **out, const char *_s, const char *ct, unsigned int n) {
  unsigned int i;
  char *tmp, *s, *last;
  airArray *mop;

  /* if we got NULL, there's nothing to do */
  if (!(out && _s && ct)) return 0;

  mop = airMopNew();
  /* copy the input so that we don't change it */
  s = airStrdup(_s);
  airMopMem(mop, &s, airMopAlways);

  /* keep calling airStrtok() until we have everything */
  for (i = 0; i < n; i++) {
    if (n > 1) {
      tmp = airStrtok(i ? NULL : s, ct, &last);
    } else {
      tmp = s;
    }
    if (!tmp) {
      airMopError(mop);
      return i;
    }
    out[i] = airStrdup(tmp);
    if (!out[i]) {
      airMopError(mop);
      return i;
    }
    airMopMem(mop, out + i, airMopOnError);
  }
  airMopOkay(mop);
  return n;
}

unsigned int
airParseStrE(int *out,
             const char *_s,
             const char *ct,
             unsigned int n,
             const airEnum *enm) {
  unsigned int i;
  char *tmp, *s, *last;
  airArray *mop;

  /* if we got NULL, there's nothing to do */
  if (!(out && _s && ct)) {
    return 0;
  }

  mop = airMopNew();
  /* copy the input so that we don't change it */
  s = airStrdup(_s);
  airMopMem(mop, &s, airMopAlways);

  if (1 == n) {
    /* Because it should be permissible to have spaces in what is
       intended to be only a single string from an airEnum, we treat
       1==n differently, and do NOT use airStrtok to tokenize the
       input string s into spaces.  We check the whole s string */
    out[0] = airEnumVal(enm, s);
    if (airEnumUnknown(enm) == out[0]) {
      airMopError(mop);
      return 0;
    }
  } else {
    /* keep calling airStrtok() until we have everything */
    for (i = 0; i < n; i++) {
      tmp = airStrtok(i ? NULL : s, ct, &last);
      if (!tmp) {
        airMopError(mop);
        return i;
      }
      out[i] = airEnumVal(enm, tmp);
      if (airEnumUnknown(enm) == out[i]
          /* getting the unknown value is not a parse failure if the
             string was actually the string for the unknown value! */
          && strcmp(tmp, enm->str[0])) {
        airMopError(mop);
        return i;
      }
    }
  }
  airMopOkay(mop);
  return n;
}
