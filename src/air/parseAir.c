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

/*
******** airSingleParse
TeemV2 API CHANGE: airSingleSscanf renamed airSingleParse
This has always described itself as a stand-in for sscanf, as long as there is exactly
one conversion sequence in `fmt`, but that was always wildly aspirational: it was never
close to handling otherise general format strings, nor did it need to be so, for how it
is used within Teem. Thus the "f" of "Sscanf" had to go, and at that point it a simpler
rename made sense, hence airSingleParse, and the second arg changed from `fmt` to `cnv`.

While airSinglePrintf as been changed to only do special floating-point handling with %g
and %lg (no longer %e %le %f %lf), here we keep acting on all six of these conversion
sequences, because on the scanning/parsing side, the difference never mattered anyway.
The variety of conversion sequences with sscanf exists for parity with printf, but sscanf
only cares whether it is saving to float or double.

ALSO: BEHAVIOR CHANGE For TeemV2 as of Oct 2026 revisit: when being asked to parse a
float (via %e/%f/%g) or double (via %le/%lf/%lg), this is just a wrapper around airStoF
or airStoD, but with the expense of re-learning the decimal separator each time its
called, which means that:
 (1) noticing special values is no longer via (case-insensitive) SUBSTRING SEARCH
     for "nan" (which meant "banana" parsed to NaN), "-inf", or "inf", but rather:
     A (case-insensitive) MATCH with "nan", "-inf", or "inf".
 (2) This function is exactly as locale-compensating as are airStoF/airStoD.
     All non-'.'-decimal-seperator cleverness pushed down into those functions.
IF YOU HAVE BEEN USING airSingleSscanf->airSingleParse to parse double or floats,
YOU SHOULD PROBABLY USE airStoF/airStoD directly, after a single call to
airDecimalSeparatorLearn, so that that cost is amortized not repeatedly incurred!
The value of parsing a size_t in C89 code is still here and possibly useful.
v . v . v . v . v . v . v . v . v . v . v . v . v . v . v . v . v . v . v . v . v . v
Use within Teem of airSingleParse for FP vals should be minimized or eliminated.
^ ' ^ ' ^ ' ^ ' ^ ' ^ ' ^ ' ^ ' ^ ' ^ ' ^ ' ^ ' ^ ' ^ ' ^ ' ^ ' ^ ' ^ ' ^ ' ^ ' ^ ' ^
*/
int
airSingleParse(const char *str, const char *cnv, void *ptr) {
  int ret;

  if (!(str && cnv && ptr)) {
    return EOF;
  }
  if (!strcmp(cnv, "%e") || !strcmp(cnv, "%f") || !strcmp(cnv, "%g")
      || !strcmp(cnv, "%le") || !strcmp(cnv, "%lf") || !strcmp(cnv, "%lg")) {
    char decibuff[AIR_STRLEN_SMALL + 1];
    const char *decisep = airDecimalSeparatorLearn(decibuff);
    if (cnv[1] == 'l') {
      /* `ptr` points to a double */
      ret = airStoD(ptr, str, decisep);
    } else {
      /* `ptr` points to a float */
      ret = airStoF(ptr, str, decisep);
    }
    return ret;
  } else if (!strcmp(cnv, "%zu")) {
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
    return sscanf(str, cnv, ptr); /* as part of airSingleParse */
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

/* With the TeemV2 addition of decisep, wanted to keep the PARSE_STR_BODY macro
mostly as is, but also be able to tweak it to call airStoF or aairStoD with a passed
decisep. This is a bit like a switch statement inside the pre-processor.
The same trick is used in frustratingly similar code in nrrd/parseNrrd.c */
#define CONC(A, B)                  A##_##B
#define PFUNC_OTH(FORMAT, DECISEP)  airSingleParse(word, (FORMAT), out + i)
#define PFUNC_FLT(FORMAT, DECISEP)  airStoF(out + i, word, (DECISEP))
#define PFUNC_DBL(FORMAT, DECISEP)  airStoD(out + i, word, (DECISEP))
#define PFUNC(WUT, FORMAT, DECISEP) CONC(PFUNC, WUT)(FORMAT, DECISEP)

#define PARSE_STR_ARGS(TYPE) TYPE *out, const char *_s, const char *ct, unsigned int n
#define PARSE_STR_BODY(WUT, FORMAT, DECISEP)                                            \
  unsigned int i;                                                                       \
  char *word, *s, *last;                                                                \
                                                                                        \
  /* if we got NULL, there's nothing to do */                                           \
  if (!(out && _s && ct)) return 0;                                                     \
                                                                                        \
  /* copy the input so that we don't change it */                                       \
  s = airStrdup(_s);                                                                    \
                                                                                        \
  /* keep calling airStrtok() until we have everything */                               \
  for (i = 0; i < n; i++) {                                                             \
    int pret;                                                                           \
    word = airStrtok(i ? NULL : s, ct, &last);                                          \
    if (!word) {                                                                        \
      free(s);                                                                          \
      return i;                                                                         \
    }                                                                                   \
    pret = PFUNC(WUT, FORMAT, DECISEP);                                                 \
    if (1 != pret) {                                                                    \
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
  PARSE_STR_BODY(OTH, "%hd", NULL)
}

unsigned int
airParseStrUH(PARSE_STR_ARGS(unsigned short)) {
  PARSE_STR_BODY(OTH, "%hu", NULL)
}

unsigned int
airParseStrI(PARSE_STR_ARGS(int)) {
  PARSE_STR_BODY(OTH, "%d", NULL)
}

unsigned int
airParseStrUI(PARSE_STR_ARGS(unsigned int)) {
  PARSE_STR_BODY(OTH, "%u", NULL)
}

unsigned int
airParseStrL(PARSE_STR_ARGS(long int)) {
  PARSE_STR_BODY(OTH, "%ld", NULL)
}

unsigned int
airParseStrUL(PARSE_STR_ARGS(unsigned long int)) {
  PARSE_STR_BODY(OTH, "%lu", NULL)
}

unsigned int
airParseStrZ(PARSE_STR_ARGS(size_t)) {
  PARSE_STR_BODY(OTH, "%zu", NULL)
}

unsigned int
airParseStrF(PARSE_STR_ARGS(float)) {
  /* will use airSingleParse which wastefully re-learns decisep */
  PARSE_STR_BODY(OTH, "%f", NULL)
}

unsigned int
airParseStrD(PARSE_STR_ARGS(double)) {
  /* will use airSingleParse which wastefully re-learns decisep */
  PARSE_STR_BODY(OTH, "%lf", NULL)
}

/* For TeemV2: the non-decisep-aware functions above stay as is,
   but we also make variants that can re-use an already learned decisep. */
unsigned int
airParseStrFdecisep(float *out,
                    const char *_s,
                    const char *ct,
                    const char *decisep,
                    unsigned int n) {
  PARSE_STR_BODY(FLT, "%f", decisep)
}

unsigned int
airParseStrDdecisep(double *out,
                    const char *_s,
                    const char *ct,
                    const char *decisep,
                    unsigned int n) {
  PARSE_STR_BODY(DBL, "%lf", decisep)
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
