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

#include "hest.h"
#include "privateHest.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <sys/ioctl.h> // for ioctl(), TIOCGWINSZ, struct winsize
#  include <unistd.h>    // for STDOUT_FILENO and friends
#endif

const int hestPresent = 42;

const char *const hest__BiffKey = "hest";

// These exist to help avoid undefined behavior: the documentation for both
// isspace and isdigit says (e.g. https://en.cppreference.com/c/string/byte/isdigit)
//    The behavior is undefined if the value of ch is
//    not representable as unsigned char and is not equal to EOF
// which is annoying (and maybe there is locale dependence?),
// so we create strchr-based replacements.
int /* Biff: nope */
hest__isspace(int ch) {
  // actually, side-step UB of isspace()
  return !!strchr(AIR_WHITESPACE, ch);
}
int /* Biff: nope */
hest__isdigit(int ch) {
  // actually, side-step UB of isdigit()
  return !!strchr("0123456789", ch);
}
int /* Biff: nope */
hest__isprint(int ch) {
  // we bite the bullet here
  if (ch != AIR_INT(AIR_UCHAR(ch))) {
    // includes ch==EOF since EOF is negative
    return AIR_FALSE;
  }
  // else ch is representable as an unsigned char
  return isprint(ch);
}

/* The 2026 revisit decided that counting the two kinds of options is important
   enough to capture in utility functions */
uint /* Biff: nope */
hest__flagOptNum(const hestOpt *hopt) {
  uint optNum = hopt->arrLen;
  uint ret = 0;
  for (uint opi = 0; opi < optNum; opi++) {
    if (hopt[opi].flag) {
      ret += 1;
    }
  }
  return ret;
}

uint /* Biff: nope */
hest__unflagOptNum(const hestOpt *hopt) {
  uint optNum = hopt->arrLen;
  uint ret = 0;
  for (uint opi = 0; opi < optNum; opi++) {
    if (!hopt[opi].flag) {
      ret += 1;
    }
  }
  return ret;
}

// see note in hest.h about why airType things are here, renamed as hest
/* clang-format off */
const char
hest__TypeStr[MY_HEST_TYPE_MAX+1][AIR_STRLEN_SMALL+1] = {
  "(unknown)",
  "bool",
  "short",
  "unsigned short",
  "int",
  "unsigned int",
  "long int",
  "unsigned long int",
  "size_t",
  "float",
  "double",
  "char",
  "string",
  "enum",
  "other",
};

const size_t
hest__TypeSize[MY_HEST_TYPE_MAX+1] = {
  0,
  sizeof(int),
  sizeof(short),
  sizeof(unsigned short),
  sizeof(int),
  sizeof(unsigned int),
  sizeof(long int),
  sizeof(unsigned long int),
  sizeof(size_t),
  sizeof(float),
  sizeof(double),
  sizeof(char),
  sizeof(char*),
  sizeof(int),
  0   /* we don't know anything about size of type "other" */
};
/* clang-format on */

/* ... Not bothering with Biff: annotations for these parser functions ... */

/* now (in 2025) that we've done all this work to preserve the command-line argv[]
tokens, and to properly tokenize default strings and response files, we should stop using
the airParseStrT functions that internally use airStrtok(): we have exactly one token to
parse. These functions thus return non-zero in case of error, instead of returning the
number of parsed values. See also comment about hest__P(arse)Pair in privateHest.h */
static int
parseSingleB(void *_out, const char *str, hest__PPair *hpp) {
  if (!(_out && str && hpp)) return 1;
  int *out = (int *)_out;
  *out = airEnumVal(airBool, str);
  int ret = airEnumUnknown(airBool) /* which is -1 */ == *out;
  if (ret) {
    snprintf(hpp->err, AIR_STRLEN_HUGE + 1, "couldn't parse \"%s\" (from %s) as %s", str,
             airEnumStr(hestSource, hpp->hopt->source), airBool->name);
  } else {
    hpp->err[0] = '\0';
  }
  return ret;
}

#define PARSE_1_ARGS void *out, const char *str, hest__PPair *hpp
#define PARSE_1_BODY(typstr, format)                                                    \
  if (!(out && str && hpp)) return 1;                                                   \
  int ret = (1 != airSingleSscanf(str, format, out));                                   \
  if (ret) {                                                                            \
    snprintf(hpp->err, AIR_STRLEN_HUGE + 1, "couldn't parse \"%s\" (from %s) as %s",    \
             str, airEnumStr(hestSource, hpp->hopt->source), typstr);                   \
  } else {                                                                              \
    hpp->err[0] = '\0';                                                                 \
  }                                                                                     \
  return ret

// clang-format off
static int parseSingleH (PARSE_1_ARGS) { PARSE_1_BODY("short",          "%hd"); }
static int parseSingleUH(PARSE_1_ARGS) { PARSE_1_BODY("unsigned short", "%hu"); }
static int parseSingleI (PARSE_1_ARGS) { PARSE_1_BODY("int",            "%d" ); }
static int parseSingleUI(PARSE_1_ARGS) { PARSE_1_BODY("unsigned int",   "%u" ); }
static int parseSingleL (PARSE_1_ARGS) { PARSE_1_BODY("long",           "%ld"); }
static int parseSingleUL(PARSE_1_ARGS) { PARSE_1_BODY("unsigned long",  "%lu"); }
static int parseSingleZ (PARSE_1_ARGS) { PARSE_1_BODY("size_t",         "%zu"); }
static int parseSingleF (PARSE_1_ARGS) { PARSE_1_BODY("float",          "%f" ); }
static int parseSingleD (PARSE_1_ARGS) { PARSE_1_BODY("double",         "%lf"); }
// clang-format on
static int
parseSingleC(void *_out, const char *str, hest__PPair *hpp) {
  if (!(_out && str && hpp)) return 1;
  size_t slen = strlen(str);
  int ret;
  if (1 != slen) {
    snprintf(hpp->err, AIR_STRLEN_HUGE + 1,
             "expected single char but got (from %s) string \"%s\" length %u",
             airEnumStr(hestSource, hpp->hopt->source), str, AIR_UINT(slen));
    ret = 1;
  } else {
    char *out = (char *)_out;
    *out = str[0];
    hpp->err[0] = '\0';
    ret = 0;
  }
  return ret;
}
static int
parseSingleS(void *_out, const char *str, hest__PPair *hpp) {
  if (!(_out && str && hpp)) return 1;
  char **out = (char **)_out;
  *out = airStrdup(str);
  int ret = !(*out); // a NULL pointer result of strdup is a problem
  if (ret) {
    snprintf(hpp->err, AIR_STRLEN_HUGE + 1, "airStrdup failed!");
  } else {
    if (!hpp->hopt->parseMop) hpp->hopt->parseMop = airMopNew();
    airMopMem(hpp->hopt->parseMop, out, airMopAlways);
    hpp->err[0] = '\0';
  }
  return ret;
}
static int
parseSingleE(void *_out, const char *str, hest__PPair *hpp) {
  if (!(_out && str && hpp)) return 1;
  int *out = (int *)_out;
  *out = airEnumVal(hpp->hopt->enm, str);
  int ret = (airEnumUnknown(hpp->hopt->enm) == *out);
  if (ret) {
    snprintf(hpp->err, AIR_STRLEN_HUGE + 1, "couldn't parse \"%s\" (from %s) as %s", str,
             airEnumStr(hestSource, hpp->hopt->source), hpp->hopt->enm->name);
  } else {
    hpp->err[0] = '\0';
  }
  return ret;
}
static int
parseSingleO(void *out, const char *str, hest__PPair *hpp) {
  if (!(out && str && hpp)) return 1;
  char myerr[AIR_STRLEN_HUGE + 1] = "";
  /* important to return this value; it has been part of hestParse() behavior
  for a long time. e.g. hestCB *nrrdHestNrrd->parse() returns 2 if nrrdLoad()
  fails with an error message mentioning "EOF", but returns 1 otherwise */
  int ret = hpp->hopt->CB->parse(out, str, myerr);
  if (ret) {
    if (strlen(myerr)) {
      // myerr as sprinted by parse(), can contain newlines
      snprintf(hpp->err, AIR_STRLEN_HUGE + 1,
               "error (ret=%d) parsing \"%s\" from %s as %s:\n%s", ret, str,
               airEnumStr(hestSource, hpp->hopt->source), hpp->hopt->CB->type,
               airStrunc(myerr, sizeof(myerr),
                         /* conservative estimate of what is generated prior to \n by
                            format string above */
                         strlen(str) + strlen(hpp->hopt->CB->type) + 100));
    } else {
      snprintf(hpp->err, AIR_STRLEN_HUGE + 1,
               "error parsing \"%s\" as %s: returned %d\n", str, hpp->hopt->CB->type,
               ret);
    }
  } else {
    if (hpp->hopt->CB->destroy) {
      /* out is the address of a void*, we manage the void* */
      if (!hpp->hopt->parseMop) hpp->hopt->parseMop = airMopNew();
      airMopAdd(hpp->hopt->parseMop, (void **)out, airSetNull_mop, airMopAlways);
      airMopAdd(hpp->hopt->parseMop, *((void **)out), hpp->hopt->CB->destroy,
                airMopAlways);
    }
  }
  return ret;
}

int (*const hest__ParseSingle[MY_HEST_TYPE_MAX + 1])(void *, const char *,
                                                     hest__PPair *) = {
  NULL,          //
  parseSingleB,  //
  parseSingleH,  //
  parseSingleUH, //
  parseSingleI,  //
  parseSingleUI, //
  parseSingleL,  //
  parseSingleUL, //
  parseSingleZ,  //
  parseSingleF,  //
  parseSingleD,  //
  parseSingleC,  //
  parseSingleS,  //
  parseSingleE,  //
  parseSingleO   //
};

#define INVERT_SCALAR(TT, ctype)                                                        \
  static void invertScalar##TT(void *_valP) {                                           \
    ctype *valP = (ctype *)_valP;                                                       \
    ctype val = *valP;                                                                  \
    *valP = (val == 0);                                                                 \
  }
INVERT_SCALAR(B, int)
INVERT_SCALAR(H, short)
INVERT_SCALAR(UH, unsigned short)
INVERT_SCALAR(I, int)
INVERT_SCALAR(UI, unsigned int)
INVERT_SCALAR(L, long)
INVERT_SCALAR(UL, unsigned long)
INVERT_SCALAR(Z, size_t)
INVERT_SCALAR(F, float)
INVERT_SCALAR(D, double)
// not: C, char
// not: S, char *
// not: E, int
// not: ?, "other"

void (*const hest__InvertScalar[MY_HEST_TYPE_MAX + 1])(void *) = {
  NULL,           //
  invertScalarB,  //
  invertScalarH,  //
  invertScalarUH, //
  invertScalarI,  //
  invertScalarUI, //
  invertScalarL,  //
  invertScalarUL, //
  invertScalarZ,  //
  invertScalarF,  //
  invertScalarD,  //
  NULL,           // not C, char
  NULL,           // not S, char *
  NULL,           // not E, int
  NULL            // not ?, "other"
};

// loving how C99 simplifies creating an airEnum at compile-time
static const airEnum source_aenum
  = {.name = "source",
     .M = 3,
     .str = (const char *[]){"(unknown_source)", // 0
                             "command-line",     // 1
                             "response-file",    // 2
                             "default"},         // 3
     .val = NULL,
     .desc = (const char *[]){"unknown source",         //
                              "argc/argv command-line", //
                              "a response file",        //
                              "default string in hestOpt"},
     .strEqv = (const char *[]){"command-line", "cmdline",   //
                                "response-file", "respfile", //
                                "default",                   //
                                ""},
     .valEqv = (const int[]){hestSourceCommandLine, hestSourceCommandLine,   //
                             hestSourceResponseFile, hestSourceResponseFile, //
                             hestSourceDefault},
     .sense = AIR_FALSE};
const airEnum *const hestSource = &source_aenum;

int /* Biff: nope */
hestSourceUser(int src) {
  return (hestSourceCommandLine == src || hestSourceResponseFile == src);
}

/* INCR is like airArray->incr: granularity with which we (linearly) reallocate the
hestOpt array. Very few uses of hest within Teem use more than 32 options. Hopefully
this avoids all the reallocations in the past action of hestOptAdd() and the like. */
#define INCR 32

// for debugging allocation and especially hestParm->selfMop usage
#define SMVERB 0

hestParm * /* Biff: nope */
hestParmNew(void) {
  hestParm *hparm;

  hparm = AIR_CALLOC(1, hestParm);
  assert(hparm);
#if defined(SMVERB) && SMVERB
  fprintf(stderr, "%s: newly allocated hestParm %p\n", __func__, AIR_VOIDP(hparm));
#endif
  hparm->verbose = hestDefaultVerbose;
  hparm->responseFileEnable = hestDefaultResponseFileEnable;
  hparm->elideSingleEnumType = hestDefaultElideSingleEnumType;
  hparm->elideSingleOtherType = hestDefaultElideSingleOtherType;
  hparm->elideSingleNonExistFloatDefault = hestDefaultElideSingleNonExistFloatDefault;
  hparm->elideMultipleNonExistFloatDefault
    = hestDefaultElideMultipleNonExistFloatDefault;
  hparm->elideSingleEmptyStringDefault = hestDefaultElideSingleEmptyStringDefault;
  hparm->elideMultipleEmptyStringDefault = hestDefaultElideMultipleEmptyStringDefault;
  /* (for these more recent additions to the hestParm, abstaining from adding yet more
      default global variables) */
  /* It would be really nice for parm->respectDashDashHelp to default to true:
  widespread conventions say what "--help" should mean e.g. https://clig.dev/#help
  HOWEVER, the problem is with how hestParse is called and how its return value
  is interpreted as a boolean:
  - zero has meant that hestParse could set values for all the options (either
    from the command-line or from supplied defaults), and
  - non-zero has meant that there was an error parsing the command-line arguments
  But seeing and recognizing "--help" means that options have NOT had values
  set, and, that's not an error: this is outside the error/no-error binary.
  But that binary is the precedent, so we have to work within that by default.
  Now, with parm->respectDashDashHelp, upon seeing "--help", hestParse returns 0,
  and sets helpWanted in the first hestOpt, and the caller will have to know
  to check for that.  This logic is handled by hestParse but maybe in the future there
  can be a different top-level parser function that turns on parm->respectDashDashHelp
  and knows how to check the results.
  2025 rewrite (for TeemV2): respectDashDashHelp is now turned on in hestParseOrExit,
  this replaces the hestParseOrDie that has been part of hest since at least 2003's
  version 1.5 release, so it is not a new or different top-level parser function,
  but it seems like no other new top-level parser function is coming soon.
  Still, that is just for users who want the one-stop convenience of hestParseOrExit.
  Users who do not want that convenience are signing up for more work, which includes
  deciding how to respond to --help (which is non-trivial as just explained) */
  hparm->respectDashDashHelp = AIR_FALSE;
  /* With all the hand-wringing about not respecting "--help" by default, you might
  think that respecting "--" as "stop processing here" marker would also not be on
  by default. But correctly handling "--help" is subtle, and handling "--" is simple,
  and, the POSIX precedent for what "--" means is older than conventions for "--help".
  The 2026 rewrite/revisit recognized that respecting "--" in a way that matches the
  world's expectations is necessary and very over-due. */
  hparm->respectDashDashMeansStop = AIR_TRUE;
  /* The chances are (GLK hopes) very low that an existing hest-using program (or the
  human user of it) will be stumped or thwarted by "-." meaning "end of variadic parms",
  or by "-{" and "}-" creating a command-line commenting mechanism. Thus, these two are
  also on by default. */
  hparm->respectDashPeriodMeansEnd = AIR_TRUE;
  hparm->respectDashBraceComments = AIR_TRUE;
  /* these older options fall back on default global variables */
  hparm->noArgsIsNoProblem = hestDefaultNoArgsIsNoProblem;
  hparm->cleverPluralizeOtherY = hestDefaultCleverPluralizeOtherY;
  /* this mimics pre-TeemV2 behavior of always preventing line wrapping */
  hparm->usageDefaultDisplayAvoidsWrapping = AIR_TRUE;
  hparm->columns = hestDefaultColumns;
  hparm->indent = hestDefaultIndent;
  // see note in privateHest.h about the removal of these
  // hparm->responseFileFlag = hestDefaultRespFileFlag;
  // hparm->responseFileComment = hestDefaultRespFileComment;
  // hparm->varParamStopFlag = hestDefaultVarParamStopFlag;
  // hparm->multiFlagSep = hestDefaultMultiFlagSep;
  /* The point of hestParseOrExit is to be a one-stop place for getting hest stuff done,
  and that historically have been off or not done by default. Currently, that is:
      hestParmColumnsIoctl(hparm, hestDefaultColumns);
      hparm->respectDashDashHelp = AIR_TRUE;
  Turning this next field on says: "no thanks, no such conveniences wanted" */
  hparm->orExitWithoutNewerFeatures = AIR_FALSE;
  hparm->selfMop = NULL; /* handling this is subtle: see hestParmMopNew,
                            hestParmUnmop, hestParmCopy, and hestParmFree */
  hparm->quietQuit = NULL;
  return hparm;
}

/* any usage of this could just as well be replaced by `free()`
   but the printed diagnostics are good for debugging */
static hestParm * /* Biff: nope */
plainParmFree(hestParm *hparm) {
#if defined(SMVERB) && SMVERB
  fprintf(stderr, "%s(%p): hello\n", __func__, AIR_VOIDP(hparm));
#endif
  /* ignore the hparm->selfMop to avoid recursion */
  airFree(hparm);
  return NULL;
}
static AIR_MOPPER(plainParmFree); /* semicolon just for clang-format */

/*
if you are going to be passing a hestParm to hestParseOrExit, or passing it to
something that will be be calling hestParseOrExit (and unfortunately you have to know),
then you should be making the hparm via hestParmMopNew, not plain hestParmNew
*/
hestParm * /* Biff: nope */
hestParmMopNew(void) {
  hestParm *ret = hestParmNew();
  ret->selfMop = airMopNew();
  /* we choose airMopAlways but really we don't know the `when` of
     the mopping, see hestParmFree */
#if defined(SMVERB) && SMVERB
  fprintf(stderr, "%s: created %p with mop %p with plainParmFree(%p)\n", __func__,
          AIR_VOIDP(ret), AIR_VOIDP(ret->selfMop), AIR_VOIDP(ret));
#endif
  airMopAdd(ret->selfMop, ret, plainParmFree_mop, airMopAlways);
  return ret;
}

hestParm * /* Biff: nope */
hestParmCopy(const hestParm *hparm) {
  hestParm *ret = hestParmNew();
  assert(ret);
  if (hparm) {
#if defined(SMVERB) && SMVERB
    fprintf(stderr, "%s(%p): created %p\n", __func__, AIR_CVOIDP(hparm), AIR_VOIDP(ret));
#endif
    memcpy(ret, hparm, sizeof(hestParm));
    /* if hparm->selfMop was non-NULL, then we've just copied that pointer to the
       airArray mop. If hparm was in the hparm->selfMop, it still is: ret->selfMop ==
       hparm->selfMop and hestParmFree(ret) will free it. But now we have a new
       hestParm*, this `ret`. We avoid creating new memory sloppiness, by adding `ret` to
       the same mop, and thus removing the need for a separate hestParmMopCopy */
    if (ret->selfMop) {
#if defined(SMVERB) && SMVERB
      fprintf(stderr, "%s(%p): copied mop %p (len %u), adding plainParmFree(%p)\n",
              __func__, AIR_CVOIDP(hparm), AIR_VOIDP(ret->selfMop),
              AIR_UINT(ret->selfMop->len), AIR_VOIDP(ret));
#endif
      airMopAdd(ret->selfMop, ret, plainParmFree_mop, airMopAlways);
    }
  }
  return ret;
}

/* should be used after hestParmCopy() of a self-mopping hparm */
void
hestParmUnmop(hestParm *hparm) {
#if defined(SMVERB) && SMVERB
  fprintf(stderr, "%s(%p): hello\n", __func__, AIR_VOIDP(hparm));
#endif
  if (hparm && hparm->selfMop && hparm->selfMop->len > 1) {
    /* This is how to check that this is plausibly the result of a hestParmCopy of a
       self-mopping hestParm. airMopSub can be safely called even when the
       (pointer,callback) pair is not actually in the mop */
#if defined(SMVERB) && SMVERB
    fprintf(stderr, "%s(%p): (mop len %u) --> airMopSub(%p, %p, plainParmFree)\n",
            __func__, AIR_VOIDP(hparm), AIR_UINT(hparm->selfMop->len),
            AIR_VOIDP(hparm->selfMop), AIR_VOIDP(hparm));
#endif
    airMopSub(hparm->selfMop, hparm, plainParmFree_mop);
    /* btw: airMopSub does not actually shorten the mop; it just turns
       the removed entry into a no-op, which is harmless */
    hparm->selfMop = NULL;
    /* and with that are no longer self-mopping; so a subsequent call to hestParmFree
       will call plainParmFree */
  }
  return;
}

/*
hestParmFree, by itself, without additional args, cannot disambiguate between
the caller being in two different situations
(1) I'm about to call exit() and need to clean up everything, versus,
(2) I'm about to return and need to clean up what I locally allocated.
==> hestParseOrExit cannot simply call hestParmFree on both cases and have things
work, and, we shouldn't attempt internal cleverness towards that end.
==> for case (1), hestParseOrExit needs to first call hestParmUnmop,
and can then call hestParmFree.
*/
hestParm * /* Biff: nope */
hestParmFree(hestParm *hparm) {
#if defined(SMVERB) && SMVERB
  fprintf(stderr, "%s(%p): hello\n", __func__, AIR_VOIDP(hparm));
#endif
  if (hparm) {
    if (hparm->selfMop) {
#if defined(SMVERB) && SMVERB
      fprintf(stderr, "%s(%p): mopping mop %p (len %u)\n", __func__, AIR_VOIDP(hparm),
              AIR_VOIDP(hparm->selfMop), AIR_UINT(hparm->selfMop->len));
#endif
      /* if hparm->selfMop->len > 1 (and hparm->selfMop has not been meddled with)
         then we are the result of a hestParmCopy. If we were to just
         `airMopOkay(hparm->selfMop)` then we'd also end up freeing the (self-mopping)
         parm that we are a copy of, confusing */
      /* we could free up hparm->selfMop and then airFree(hparm), but it would
         be easier to just use the mop as intended. We are going to assume
         that the caller created this via hestParmMopNew */
      airMopOkay(hparm->selfMop);
    } else {
      plainParmFree(hparm);
    }
  }
  return NULL;
}
AIR_MOPPER(hestParmFree)

const hestParm * /* Biff: nope */
hestParmMopDone(const hestParm *hparm) {
  if (hparm && hparm->selfMop) {
    /* if this in in fact properly self-mopping running mop will clean up the same
    `hparm` that we can't call hestParmFree on because for us `hparm` is const. It seems
    like maybe we should have a special 2nd arg to airMopDone to mean "everything": to
    call callback on pointer even if it was added only airMopOnError or airMopOnOkay. But
    not a problem as long rest of hest code here uses airMopAlways */
    airMopDone(hparm->selfMop, AIR_FALSE /* error */);
  }
  /* else NULL hparm or NULL hparm->selfMop:
     either way, nothing for us to do here */
  return NULL;
}

/* hestParmColumnsIoctl:
Try to dynamically learn number of columns in the current terminal from ioctl(), and save
it in hparm->columns. Learning the terminal size from stdin will probably work if we're
not being piped into, else try learning it from stdout (but that won't work if we're
piping elsewhere), else try learning the terminal size from stderr.

If one of these works, and returns a reasonably large-enough value for #columns, then
then hparm->columns is set via the ioctl-generated info, and we return 0.  "large-enough"
means bigger than sanity threshold of max(20, hestDefaultColumns/2); if not above that
threshold, then hparm->columns is set to it and we return -1. Why bother with this
threshold: hest usage generation code isn't trusted to produce anything informative with
a tiny number of columns (and certainly hasn't been well-tested with that).

If ioctl() never worked, then hparm->columns gets the given nonIoctlColumns, and we
return 1 (but this 1 is not an error that needs any recovering from). */
int /* Biff: nope */
hestParmColumnsIoctl(hestParm *hparm, unsigned int nonIoctlColumns) {
#ifdef _WIN32
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  int ret = 1; /* default if all attempts fail */
  unsigned int sanemin = hestDefaultColumns / 2;
  HANDLE hands[3] = {GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE),
                     GetStdHandle(STD_ERROR_HANDLE)};
  for (int hi = 0; hi < 3; ++hi) {
    HANDLE h = hands[hi];
    if (h && h != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(h, &csbi)) {
      unsigned int cols = (unsigned int)(csbi.srWindow.Right - csbi.srWindow.Left + 1);
      /* mirror the POSIX path's conservative "- 2" margin */
      hparm->columns = (cols > 2 ? cols - 2 : cols);
      if (hparm->columns < sanemin) {
        hparm->columns = sanemin; /* ignore too-small values */
        ret = -1;
      } else {
        ret = 0; /* worked and sane */
      }
      return ret;
    }
  }
  /* none of the console handles yielded a size; fall back */
  hparm->columns = nonIoctlColumns;
  return 1;
#else
  struct winsize wsz;
  int ret;
  if (-1 != ioctl(STDIN_FILENO, TIOCGWINSZ, &wsz)
      || -1 != ioctl(STDOUT_FILENO, TIOCGWINSZ, &wsz)
      || -1 != ioctl(STDERR_FILENO, TIOCGWINSZ, &wsz)) {
    /* one of the ioctl calls worked */
    unsigned int sanemin;
    /* the "- 2" here may be the sign of a hest bug; sometimes it seems the "\" for line
    continuation (in generated usage info) causes a line wrap when it shouldn't */
    hparm->columns = wsz.ws_col - 2;
    sanemin = hestDefaultColumns / 2;
    if (hparm->columns < sanemin) {
      /* will ignore the too-small value ioctl produced */
      hparm->columns = sanemin;
      ret = -1;
    } else {
      /* ioctl didn't say something crazy; we keep it */
      ret = 0;
    }
  } else {
    hparm->columns = nonIoctlColumns;
    ret = 1;
  }
  return ret;
#endif
}

// hest__Max(-1) == INT_MAX, otherwise hest__Max(m) == m
int /* Biff: nope */
hest__Max(int max) {

  if (-1 == max) {
    max = INT_MAX;
  }
  return max;
}

/* minmaxKind determines the kind (1,2,3,4, or 5) of an opt,
   based on the min and max fields from the hestOpt */
static int /* Biff: nope */
minmaxKind(unsigned int min, int _max) {
  int ret;
  int max = hest__Max(_max);
  if (AIR_INT(min) > max) {
    /* invalid */
    ret = -1;
  } else { // else min <= max
    if (AIR_INT(min) == max) {
      if (0 == min) {
        // stand-alone flag
        ret = 1;
      } else if (1 == min) {
        // single fixed parm
        ret = 2;
      } else { // min==max >= 2
        // multiple fixed parms
        ret = 3;
      }
    } else { // else min < max
      if (0 == min && 1 == max) {
        // weirdo: single optional parameter
        ret = 4;
      } else {
        // multiple variadic parameters
        ret = 5;
      }
    }
  }
  return ret;
}

/* initializes all of a hestOpt, even arrAlloc and arrLen */
static void
optInit(hestOpt *hopt) {

  hopt->flag = NULL;
  hopt->name = NULL;
  hopt->type = airTypeUnknown; /* h== 0 */
  hopt->min = 0;
  hopt->max = 0;
  hopt->valueP = NULL;
  hopt->dflt = hopt->info = NULL;
  hopt->sawP = NULL;
  hopt->enm = NULL;
  hopt->CB = NULL;
  hopt->sawP = NULL;
  hopt->kind = 0;        // means that this hestOpt has not been set
  hopt->parseMop = NULL; // will create with airMopNew() only as needed
  hopt->havec = NULL;
  hopt->arrAlloc = hopt->arrLen = 0;
  hopt->source = hestSourceUnknown;
  hopt->parmStr = NULL;
  hopt->helpWanted = AIR_FALSE;
  hopt->remainArgv = NULL;
  hopt->remainArgc = 0;
}

/*
hestOptNum: returns the number of elements in the given hestOpt array

Unfortunately, unlike argv itself, there is no sense in which the hestOpt array can be
NULL-terminated, mainly because "opt" is an array of hestOpt structs, not an array of
pointers to hestOpt structs. Pre-2023, this function did clever things to detect the
terminating hestOpt, but with the introduction of arrAlloc and arrLen that is moot.
*/
unsigned int /* Biff: nope */
hestOptNum(const hestOpt *hopt) {
  return hopt ? hopt->arrLen : 0;
}

/* like airArrayNew: create an initial segment of the hestOpt array */
static void
optarrNew(hestOpt **optP) {
  unsigned int opi;
  hestOpt *ret = AIR_CALLOC(INCR, hestOpt);
  assert(ret);
  for (opi = 0; opi < INCR; opi++) {
    optInit(ret + opi);
  }
  ret->arrAlloc = INCR;
  ret->arrLen = 0;
  *optP = ret;
  return;
}

/* line airArrayLenIncr(1): increments logical length by 1,
and returns index of newly-available element */
static unsigned int /* Biff: nope */
optarrIncr(hestOpt **optP) {
  unsigned int olen, nlen;
  olen = (*optP)->arrLen; /* == index of new element */
  nlen = olen + 1;
  if (nlen > (*optP)->arrAlloc) {
    unsigned int opi;
    /* just walked off end of allocated length: reallocate */
    hestOpt *nopt = AIR_CALLOC((*optP)->arrAlloc + INCR, hestOpt);
    assert(nopt);
    memcpy(nopt, *optP, olen * sizeof(hestOpt));
    nopt->arrAlloc = (*optP)->arrAlloc + INCR;
    for (opi = olen; opi < nopt->arrAlloc; opi++) {
      optInit(nopt + opi);
    }
    free(*optP);
    *optP = nopt;
  }
  (*optP)->arrLen = nlen;
  return olen;
}

/*
hestOptSingleSet: a completely generic setter for a single hestOpt
Note that this makes no attempt at error-checking; that is all in hest__OPCheck.
*THIS* is the function that sets opt->kind.
*/
void
hestOptSingleSet(hestOpt *hopt, const char *flag, const char *name, int type,
                 unsigned int min, int max, void *valueP, const char *dflt,
                 const char *info, unsigned int *sawP, const airEnum *enm,
                 const hestCB *CB) {

  if (!hopt) return;
  hopt->flag = airStrdup(flag);
  hopt->name = airStrdup(name);
  hopt->type = type;
  hopt->min = min;
  hopt->max = max;
  hopt->valueP = valueP;
  hopt->dflt = airStrdup(dflt);
  hopt->info = airStrdup(info);
  // need to set kind now so can be used in later conditionals
  hopt->kind = minmaxKind(min, max);
  // deal with (what used to be) var args
  hopt->sawP = (5 == hopt->kind /* */
                  ? sawP
                  : NULL);
  hopt->enm = (airTypeEnum == type /* */
                 ? enm
                 : NULL);
  hopt->CB = (airTypeOther == type /* */
                ? CB
                : NULL);
  // hopt->parseMop may be added to by hestParse and the parseSingleT functions it calls
  hopt->havec = hestArgVecNew();
  // leave arrAlloc, arrLen untouched: managed by caller
  // yes, redundant with optInit()
  hopt->source = hestSourceUnknown;
  hopt->parmStr = NULL;
  hopt->helpWanted = AIR_FALSE;
  return;
}

/*
hestOptAdd_nva: A new (as of 2023) non-var-args ("_nva") version of hestOptAdd;
The per-hestOpt logic (including setting opt->kind) has now been moved to
hestOptSingleSet. The venerable var-args hestOptAdd is now a wrapper around this.
and all the 99 non-var-args fully typed hestOptAdd_* functions also call this.

Like hestOptAdd has done since 2013: returns UINT_MAX in case of error.

NOTE that we do NOT do here ANY error checking on the validity of the arguments passed,
e.g. enforcing that we have a non-NULL sawP iff this is a multi-variadic parameter
option, or that without a flag (`flag` is NULL) we must have min > 0.  All of that is
done later, in hest__OPCheck.
*/
unsigned int /* Biff: nope */
hestOptAdd_nva(hestOpt **hoptP, const char *flag, const char *name, int type,
               unsigned int min, int max, void *valueP, const char *dflt,
               const char *info, unsigned int *sawP, const airEnum *enm,
               const hestCB *CB) {
  unsigned int retIdx;

  /* NULL address of opt array: can't proceed */
  if (!hoptP) return UINT_MAX;
  /* initialize hestOpt array if necessary */
  if (!(*hoptP)) {
    optarrNew(hoptP);
  }
  /* increment logical length of hestOpt array; return index of opt being set here */
  retIdx = optarrIncr(hoptP);
  /* set all elements of the opt */
  hestOptSingleSet(*hoptP + retIdx, flag, name, type, min, max, /* */
                   valueP, dflt, info,                          /* */
                   sawP, enm, CB);
  return retIdx;
}

/*
 * hestOptAdd
 *
 * Until 2023, this was the main way of using hest: a var-args function that could do no
 * useful type-checking, and was very easy to call incorrectly, leading to inscrutable
 * errors.
 *
 * Now, thankfully, you have 99 better hestOptAdd_ functions to use instead:
 * hestOptAdd_Flag, hestOptAdd_1_T, hestOptAdd_{2,3,4,N}_T, hestOptAdd_1v_T, or
 * hestOptAdd_Nv_T for T=Bool, Short, UShort, Int, UInt, LongInt, ULongInt, Size_t,
 * Float, Double, Char, String, Enum, or Other.
 *
 * This returns the index of the option just added, so the caller can remember it and
 * thus speed up later checking the `hestOpt->source` to learn where how the option was
 * parsed. Returns UINT_MAX in case of error.
 */
unsigned int /* Biff: nope */
hestOptAdd(hestOpt **hoptP, const char *flag, const char *name, int type,
           unsigned int min, int max, void *valueP, const char *dflt, const char *info,
           ...) {
  unsigned int *sawP = NULL;
  const airEnum *enm = NULL;
  const hestCB *CB = NULL;
  va_list ap;

  if (!hoptP) return UINT_MAX;
  /* deal with var args */
  if (5 == minmaxKind(min, max)) {
    va_start(ap, info);
    sawP = va_arg(ap, unsigned int *);
    va_end(ap);
  }
  if (airTypeEnum == type) {
    va_start(ap, info);
    va_arg(ap, unsigned int *); /* skip sawP */
    enm = va_arg(ap, const airEnum *);
    va_end(ap);
  }
  if (airTypeOther == type) {
    va_start(ap, info);
    va_arg(ap, unsigned int *); /* skip sawP */
    va_arg(ap, airEnum *);      /* skip enm */
    CB = va_arg(ap, hestCB *);
    va_end(ap);
  }
  return hestOptAdd_nva(hoptP, flag, name, type, min, max, /* */
                        valueP, dflt, info,                /* */
                        sawP, enm, CB);
}

static void
optFree(hestOpt *hopt) {

  hopt->flag = (char *)airFree(hopt->flag);
  hopt->name = (char *)airFree(hopt->name);
  hopt->dflt = (char *)airFree(hopt->dflt);
  hopt->info = (char *)airFree(hopt->info);
  /* In normal usage, this branch was never taken because hestParseFree had
     already been called, which meant that hopt->parseMop was already handled,
     and is now set to NULL */
  // if (hopt->parseMop) hopt->parseMop = airMopOkay(hopt->parseMop);
  hopt->havec = hestArgVecNix(hopt->havec);
  return;
}

hestOpt * /* Biff: nope */
hestOptFree(hestOpt *hopt) {
  if (!hopt) return NULL;
  uint num = hopt->arrLen;
  for (uint opi = 0; opi < num; opi++) {
    optFree(hopt + opi);
  }
  free(hopt);
  return NULL;
}
AIR_MOPPER(hestOptFree)

/* hest__OPCheck
New biff-based container for all logic that originated in hest__OptCheck (which was the
2025 rename of what had long been called hest__Panic): the validation of the given
hestOpt array `hopt` itself (but *not* anything about the command-line or its parsing),
relative to the given hestParm `hparm` (if non-NULL).

Pre-2025, hest did not depend on biff, and this instead took a 'char *err' that somehow
magically had to be allocated for the size of any possible error message generated here.
The 2025 re-write recognized that biff is the right way to accumulate error messages, but
the use of biff is internal to hest, and (unusually for Teem) not part of the expected
use of hest's API. Thus, public functions hestOptCheck() and hestOptParmCheck(), which
are the expected way to access the functionality herein, take a `char **errP` arg into
which a message is sprintf'ed, after allocation.

The shift to using biff removed how this function used to fprintf(stderr) some messages
like "panic 0.5" which were totally uninformative.  Now, hestOptCheck() and
hestOptParmCheck(), which both call hest__OPCheck, will fprintf(stderr) the informative
biff message.

Prior to 2023 code revisit: this (when named hest__Panic, of all things) used to set the
"kind" in all the opts, but now that is more appropriately done at the time the option is
added. This particular change closes off the possibility of doing an argtable(v1)-style
static creation of a hestOpt array, alas.  Thanks to argtable(v1) for inspiring hest!
*/
int /* Biff: (private) 1 */
hest__OPCheck(const hestOpt *hopt, const hestParm *hparm) {
  if (!hopt) {
    /* we allow hparm to be NULL because sometimes we just want to check hopt! */
    biffAddf(HEST, "%s%sgot NULL hopt", MY_ME);
    return 1;
  }
  uint optNum = hopt->arrLen;
  uint ufvarNum = 0; // number of unflagged variadic-parameter options
  /* dynamically allocated memory of previous short and long flags,
     just so that we can ensure flags are not repeated */
  airArray *mop = airMopNew();
  char **shfl = AIR_CALLOC(optNum, char *);
  assert(shfl);
  airMopAdd(mop, shfl, airFree, airMopAlways);
  char **lgfl = AIR_CALLOC(optNum, char *);
  assert(lgfl);
  airMopAdd(mop, lgfl, airFree, airMopAlways);
  for (uint opi = 0; opi < optNum; opi++) {
    // always initialize flag pointers, will fill later if there is a flag
    lgfl[opi] = shfl[opi] = NULL;
    if (!(AIR_IN_OP(airTypeUnknown, hopt[opi].type, airTypeLast))) {
      biffAddf(HEST, "%s%sopt[%u].type (%d) not in valid range [%d,%d]", MY_ME, opi,
               hopt[opi].type, airTypeUnknown + 1, airTypeLast - 1);
      airMopError(mop);
      return 1;
    }
    // `kind` set by hestOptSingleSet
    if (-1 == hopt[opi].kind) {
      biffAddf(HEST, "%s%sopt[%u]'s min (%u) and max (%d) incompatible", MY_ME, opi,
               hopt[opi].min, hopt[opi].max);
      airMopError(mop);
      return 1;
    }
    if (!(hopt[opi].valueP)) {
      biffAddf(HEST, "%s%sopt[%u]'s valueP is NULL", MY_ME, opi);
      airMopError(mop);
      return 1;
    }
    if (1 == hopt[opi].kind) {
      if (!hopt[opi].flag) {
        biffAddf(HEST, "%s%sstand-alone flag opt[%u] must have a flag (!)", MY_ME, opi);
        airMopError(mop);
        return 1;
      }
      if (hopt[opi].dflt) {
        biffAddf(HEST, "%s%sstand-alone flag (opt[%u] %s) should not have a default",
                 MY_ME, opi, hopt[opi].flag);
        airMopError(mop);
        return 1;
      }
      if (hopt[opi].name) {
        biffAddf(HEST, "%s%sstand-alone flag (opt[%u] %s) should not have a name", MY_ME,
                 opi, hopt[opi].flag);
        airMopError(mop);
        return 1;
      }
    } else { // ------ end of if (1 == hopt[opi].kind)
      if (!hopt[opi].name) {
        biffAddf(HEST, "%s%sopt[%u] isn't stand-alone flag: must have \"name\"", MY_ME,
                 opi);
        airMopError(mop);
        return 1;
      }
    }
    if (hopt[opi].flag) {
      const char *flag = hopt[opi].flag;
      size_t fslen = strlen(flag);
      if (!fslen) {
        biffAddf(HEST, "%s%sopt[%u].flag is empty", MY_ME, opi);
        airMopError(mop);
        return 1;
      }
      /* This strlen constraint is leveraged by usage.c/hsetBuff() */
      if (fslen >= AIR_STRLEN_SMALL / 2) {
        biffAddf(HEST, "%s%sstrlen(opt[%u].flag) %zu is too big", MY_ME, opi, fslen);
        airMopError(mop);
        return 1;
      }
      if ('-' == flag[0]) {
        biffAddf(HEST,
                 "%s%sopt[%u].flag \"%s\" itself starts with '-', "
                 "which seems confused",
                 MY_ME, opi, flag);
        airMopError(mop);
        return 1;
      }
      if ('.' == flag[0]) {
        biffAddf(HEST,
                 "%s%sopt[%u].flag \"%s\" starts with '.', "
                 "which could be confused with \"-.\" end variadic parms flag",
                 MY_ME, opi, flag);
        airMopError(mop);
        return 1;
      }
      if (hest__isdigit(flag[0])) {
        biffAddf(HEST,
                 "%s%sopt[%u].flag \"%s\" starts with digit, "
                 "which could create confusion with a negative number",
                 MY_ME, opi, flag);
        airMopError(mop);
        return 1;
      }
      for (uint chi = 0; chi < AIR_UINT(fslen); chi++) {
        if (!hest__isprint(flag[chi])) {
          biffAddf(HEST, "%s%sopt[%u].flag \"%s\" char %u '%c' non-printing", MY_ME, opi,
                   flag, chi, flag[chi]);
          airMopError(mop);
          return 1;
        }
        if (strchr(AIR_WHITESPACE, flag[chi])) {
          biffAddf(HEST, "%s%sopt[%u].flag \"%s\" char %u '%c' is whitespace", MY_ME,
                   opi, flag, chi, flag[chi]);
          airMopError(mop);
          return 1;
        }
      }
      char *tbuff = airStrdup(flag);
      assert(tbuff);
      airMopAdd(mop, tbuff, airFree, airMopAlways);
      char *sep;
      if ((sep = strchr(tbuff, MY_MULTI_FLAG_SEP))) {
        *sep = '\0';
        // have both short `tbuff` and long `sep + 1` flags
        if (!(strlen(tbuff) && strlen(sep + 1))) {
          biffAddf(HEST,
                   "%s%seither short (\"%s\") or long (\"%s\") flag"
                   " of opt[%u] is zero length",
                   MY_ME, tbuff, sep + 1, opi);
          airMopError(mop);
          return 1;
        }
        if (strchr(sep + 1, MY_MULTI_FLAG_SEP)) {
          biffAddf(HEST,
                   "%s%sopt[%u] flag string \"%s\" has more than one instance of "
                   "short/long separation character '%c'",
                   MY_ME, opi, flag, MY_MULTI_FLAG_SEP);
          airMopError(mop);
          return 1;
        }
        if ('-' == (sep + 1)[0]) {
          biffAddf(HEST,
                   "%s%slong flag \"%s\" (part of \"%s\") of opt[%u] itself "
                   "starts with '-', which seems confused",
                   MY_ME, sep + 1, flag, opi);
          airMopError(mop);
          return 1;
        }
        if ('.' == (sep + 1)[0]) {
          biffAddf(HEST,
                   "%s%slong flag \"%s\" (part of \"%s\") of opt[%u] starts with '.', "
                   "which could be confused with \"-.\" end variadic parms flag",
                   MY_ME, sep + 1, flag, opi);
          airMopError(mop);
          return 1;
        }
        if (hest__isdigit((sep + 1)[0])) {
          biffAddf(HEST,
                   "%s%slong flag \"%s\" (part of \"%s\") of opt[%u] starts with "
                   "a digit, which could create confusion with a negative number",
                   MY_ME, sep + 1, flag, opi);
          airMopError(mop);
          return 1;
        }
        if (hparm && hparm->respectDashDashHelp && !strcmp("help", sep + 1)) {
          biffAddf(HEST,
                   "%s%slong \"--%s\" flag of opt[%u] is same as \"--help\" "
                   "that requested hparm->respectDashDashHelp handles separately",
                   MY_ME, sep + 1, opi);
          airMopError(mop);
          return 1;
        }
        // remember short flag
        shfl[opi] = airStrdup(tbuff);
        assert(shfl[opi]);
        airMopAdd(mop, shfl[opi], airFree, airMopAlways);
        // remember long flag
        lgfl[opi] = airStrdup(sep + 1);
        assert(lgfl[opi]);
        airMopAdd(mop, lgfl[opi], airFree, airMopAlways);
      } else {
        // remember (short) flag
        shfl[opi] = airStrdup(hopt[opi].flag);
        assert(shfl[opi]);
        airMopAdd(mop, shfl[opi], airFree, airMopAlways);
      }
      if (strchr(flag, '{') || strchr(flag, '}')) {
        biffAddf(HEST,
                 "%s%sopt[%u]'s flag \"%s\" contains '{' or '}' "
                 "which could be confused with -{ }- hest comment flags",
                 MY_ME, opi, flag);
        airMopError(mop);
        return 1;
      }
    } else { // ------ end of if (hopt[opi].flag)
      // hopt[opi] is unflagged
      // unflagged kind=1 is already checked above
      if (4 == hopt[opi].kind) {
        if (!hopt[opi].min) {
          biffAddf(HEST, "%s%s single variadic opt[%u] (name %s) must be flagged", MY_ME,
                   opi, hopt[opi].name);
          airMopError(mop);
          return 1;
        }
      }
      // else it's a fixed (non-variadic) unflagged option; cool
    }
    if (4 == hopt[opi].kind) { // single variadic parameter
      // above test has already ruled out unflagged kind 4
      if (!hopt[opi].dflt) {
        biffAddf(HEST,
                 "%s%sopt[%u] -%s is single variadic parameter, but "
                 "no default set",
                 MY_ME, opi, hopt[opi].flag);
        airMopError(mop);
        return 1;
      }
      if (!(strlen(hopt[opi].dflt) && hest__PlainWord(hopt[opi].dflt))) {
        biffAddf(HEST,
                 "%s%sopt[%u] -%s is single variadic parameter, but "
                 "default \"%s\" needs to be non-empty single value",
                 MY_ME, opi, hopt[opi].flag, hopt[opi].dflt);
        airMopError(mop);
        return 1;
      }
      /* pre 2025, at least some of these types were allowed for kind 4, but the
         semantics are just so weird that GLK can no longer justify supporting them.
         They therefore did not survive the re-write (sorry). */
      if (airTypeChar == hopt[opi].type || airTypeString == hopt[opi].type
          || airTypeEnum == hopt[opi].type || airTypeOther == hopt[opi].type) {
        biffAddf(HEST,
                 "%s%sopt[%u] -%s is single variadic parameter, but sorry, "
                 "type %s no longer supported",
                 MY_ME, opi, hopt[opi].flag, hest__TypeStr[hopt[opi].type]);
        airMopError(mop);
        return 1;
      }
    }
    if (5 == hopt[opi].kind && !(hopt[opi].sawP)) {
      biffAddf(HEST,
               "%s%sopt[%u] has multiple variadic parameters (min=%u,max=%d), "
               "but sawP is NULL",
               MY_ME, opi, hopt[opi].min, hopt[opi].max);
      airMopError(mop);
      return 1;
    }
    if (5 != hopt[opi].kind && hopt[opi].sawP) {
      biffAddf(HEST,
               "%s%sopt[%u] has non-NULL sawP but is not a (kind=5) "
               "multiple variadic parm option (min=%u,max=%d)",
               MY_ME, opi, hopt[opi].min, hopt[opi].max);
      airMopError(mop);
      return 1;
    }
    if (airTypeEnum == hopt[opi].type && !(hopt[opi].enm)) {
      biffAddf(HEST,
               "%s%sopt[%u] (%s) is type \"enum\", but no "
               "airEnum pointer given",
               MY_ME, opi, hopt[opi].flag ? hopt[opi].flag : "unflagged");
      airMopError(mop);
      return 1;
    }
    if (airTypeEnum != hopt[opi].type && hopt[opi].enm) {
      biffAddf(HEST,
               "%s%sopt[%u] (%s) has non-NULL airEnum pointer, but is not airTypeEnum",
               MY_ME, opi, hopt[opi].flag ? hopt[opi].flag : "unflagged");
      airMopError(mop);
      return 1;
    }
    if (airTypeOther == hopt[opi].type) {
      if (!(hopt[opi].CB)) {
        biffAddf(HEST,
                 "%s%sopt[%u] (%s) is type \"other\", but no "
                 "callbacks given",
                 MY_ME, opi, hopt[opi].flag ? hopt[opi].flag : "unflagged");
        airMopError(mop);
        return 1;
      }
      if (!(hopt[opi].CB->size > 0)) {
        biffAddf(HEST, "%s%sopt[%u]'s \"size\" (%u) invalid", MY_ME, opi,
                 (uint)(hopt[opi].CB->size));
        airMopError(mop);
        return 1;
      }
      if (!(hopt[opi].CB->type)) {
        biffAddf(HEST, "%s%sopt[%u]'s \"type\" is NULL", MY_ME, opi);
        airMopError(mop);
        return 1;
      }
      if (!(hopt[opi].CB->parse)) {
        biffAddf(HEST, "%s%sopt[%u]'s \"parse\" callback is NULL", MY_ME, opi);
        airMopError(mop);
        return 1;
      }
      if (hopt[opi].CB->destroy && (sizeof(void *) != hopt[opi].CB->size)) {
        biffAddf(HEST,
                 "%s%sopt[%u] has a \"destroy\", but size %lu isn't "
                 "sizeof(void*)",
                 MY_ME, opi, (unsigned long)(hopt[opi].CB->size));
        airMopError(mop);
        return 1;
      }
    } else { // airTypeOther != hopt[opi].type
      if (hopt[opi].CB) {
        biffAddf(HEST,
                 "%s%sopt[%u] (%s) has non-NULL callbacks, but is not airTypeOther",
                 MY_ME, opi, hopt[opi].flag ? hopt[opi].flag : "unflagged");
        airMopError(mop);
        return 1;
      }
    }
    // kind=4 : single variadic parm;  kind=5 : multiple variadic parm
    ufvarNum += (hopt[opi].kind > 3 && (!hopt[opi].flag));
    // finally, check for repeated use of flags
    for (uint opj = 0; opj < opi; opj++) {
      if (shfl[opi]) {
        if (shfl[opj] && !strcmp(shfl[opi], shfl[opj])) {
          biffAddf(HEST, "%s%s opt[%u] flag \"-%s\" repeats that of previous opt[%u]",
                   MY_ME, opi, shfl[opi], opj);
          airMopError(mop);
          return 1;
        }
        if (lgfl[opj] && !strcmp(shfl[opi], lgfl[opj])) {
          biffAddf(HEST,
                   "%s%s opt[%u] flag \"-%s\" repeats "
                   "long flag \"--%s\" of previous opt[%u]",
                   MY_ME, opi, shfl[opi], lgfl[opj], opj);
          airMopError(mop);
          return 1;
        }
      }
      if (lgfl[opi]) {
        if (lgfl[opj] && !strcmp(lgfl[opi], lgfl[opj])) {
          biffAddf(HEST,
                   "%s%s opt[%u] long flag \"--%s\" repeats that of previous opt[%u]",
                   MY_ME, opi, lgfl[opi], opj);
          airMopError(mop);
          return 1;
        }
        if (shfl[opj] && !strcmp(lgfl[opi], shfl[opj])) {
          biffAddf(HEST,
                   "%s%s opt[%u] long flag \"--%s\" repeats "
                   "short flag \"-%s\" of previous opt[%u]",
                   MY_ME, opi, lgfl[opi], shfl[opj], opj);
          airMopError(mop);
          return 1;
        }
      }
    }
  }
  if (ufvarNum > 1) {
    biffAddf(HEST, "%s%scan have at most 1 unflagged variadic options, not %u", MY_ME,
             ufvarNum);
    airMopError(mop);
    return 1;
  }
  airMopOkay(mop);
  return 0;
}

/*
 * hestOptCheck: check given hestOpt array `hopt`, using the default hestParm.
 * Puts any errors into newly allocated (caller responsible to free) `*errP`.
 */
int /* Biff: nope # biff use is internal */
hestOptCheck(const hestOpt *hopt, char **errP) {
  if (hest__OPCheck(hopt, NULL)) {
    char *err = biffGetDone(HEST);
    if (errP) {
      /* they did give a pointer address; they'll free it */
      *errP = err;
    } else {
      /* they didn't give a pointer address; we dump to stderr */
      fprintf(stderr, "%s: problem with given hestOpt array:\n%s", __func__, err);
      free(err);
    }
    return 1;
  }
  /* else, no problems */
  if (errP) *errP = NULL;
  return 0;
}

/*
 * hestOptParmCheck: check given hestOpt array `hopt` in combination with the given
 * hestParm `hparm`. Puts any errors into newly allocated (caller responsible to free)
 * `*errP`.
 * HEY copy-pasta
 */
int /* Biff: nope # biff use is internal */
hestOptParmCheck(const hestOpt *hopt, const hestParm *hparm, char **errP) {
  if (hest__OPCheck(hopt, hparm)) {
    char *err = biffGetDone(HEST);
    if (errP) {
      /* they did give a pointer address; they'll free it */
      *errP = err;
    } else {
      /* they didn't give a pointer address; we dump to stderr */
      fprintf(stderr, "%s: problem with given hestOpt array:\n%s", __func__, err);
      free(err);
    }
    return 1;
  }
  /* else, no problems */
  if (errP) *errP = NULL;
  return 0;
}
