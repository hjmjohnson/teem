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

#ifdef __cplusplus
extern "C" {
#endif

/* Having hest depend on biff is new for TeemV2: with the new and more modular
re-write of hestParse, not having a useful way to accumulate error messages across a
deeper call stack was getting just too annoying.
It is still the case, however, the hest users do not need to call into biff */
#include <teem/biff.h>

#include <limits.h> // for UINT_MAX
#include <assert.h>
#include <errno.h>

/* NOTE: these SN_,ASP1_ string macros copy-pasta'd to other private<Lib>.h files:
   SN_INCR: safely increments STR arg to point '\0'-termination of STR
   SN_COPY: safely copies SRC to DST, then increments DST
   SN_PRINTF: snprintf's into DST then increments DST
   ASP1_X: short version of AIR_STRLEN_X + 1 */
#define SN_INCR(STR, SIZE)                                                              \
  do {                                                                                  \
    size_t tmp_str_len_##STR = strlen(STR);                                             \
    STR += tmp_str_len_##STR;                                                           \
    SIZE -= tmp_str_len_##STR;                                                          \
  } while (0)
#define SN_COPY(DST, DST_SIZE, SRC)                                                     \
  do {                                                                                  \
    airStrcpy((DST), (DST_SIZE), (SRC));                                                \
    SN_INCR(DST, DST_SIZE);                                                             \
  } while (0)
#define SN_PRINTF(DST, DST_SIZE, FMT, ...)                                              \
  do {                                                                                  \
    snprintf((DST), (DST_SIZE), (FMT), __VA_ARGS__);                                    \
    SN_INCR(DST, DST_SIZE);                                                             \
  } while (0)
#define ASP1_S (AIR_STRLEN_SMALL + 1)
#define ASP1_M (AIR_STRLEN_MED + 1)
#define ASP1_L (AIR_STRLEN_LARGE + 1)
#define ASP1_H (AIR_STRLEN_HUGE + 1)

typedef unsigned int uint; /* uint is just more concise */

// pre-TeemV2, these used to be change-able defaults in defaultsHest.c:
//   char hestDefaultRespFileFlag = '@';
//   char hestDefaultRespFileComment = '#';
//   char hestDefaultVarParamStopFlag = '-';
//   char hestDefaultMultiFlagSep = ',';
// with corresponding fields in the hestParm defined in hest.h
//   char respFileFlag,        /* the character at the beginning of an argument
//                                indicating that this is a response file name */
//     respFileComment,        /* comment character for the response files */
//     varParamStopFlag, /* prefixed by '-' to form the flag (usually "--") that signals
//                          the end of a *flagged* variable parameter option (single or
//                          multiple). This is important to use if there is a flagged
//                          variable parameter option preceeding an unflagged variable
//                          parameter option, because otherwise how will you know where
//                          the first stops and the second begins */
//     multiFlagSep;     /* character in flag which signifies that there is a long and
//                          short version, and which separates the two.  Or, can be set
//                          to '\0' to disable this behavior entirely. */
// However, there is more confusion than utility created by allowing these to be
// changed. The actual value in giving these things names was in code legibility by
// removing magic constants, so that's the role of these #define's now.
#define MY_RESPONSE_FILE_FLAG    '@'
#define MY_RESPONSE_FILE_COMMENT '#'
/* 2026 revisit renamed this from MY_VAR_PARM_STOP_FLAG to MY_VAR_PARM_END_FLAG, and
   changed value from '-' to '.' i.e. the way to terminate variadic parms changed from
   "--" to "-.", because we decided to respect POSIX convention for "--" */
#define MY_VAR_PARM_END_FLAG     '.'
/* 2026 revisit introduced this as way to respect POSIX convention for "--" */
#define MY_STOP_PARSING_FLAG     '-'
#define MY_MULTI_FLAG_SEP        ','

/* normally this would be in the main lib.h, but hest is unusual in that its
   usage of biff is internal only; no user of the hest API would call into biff */
#define HEST hest__BiffKey

/* methodsHest.c */
extern int hest__isspace(int ch);
extern int hest__isdigit(int ch);
extern int hest__isprint(int ch);
extern uint hest__flagOptNum(const hestOpt *hopt);
extern uint hest__unflagOptNum(const hestOpt *hopt);
extern void hest__optSingleSet(hestOpt *hopt, const char *flag, const char *name,
                               int type, unsigned int min, int max, void *valueP,
                               const char *dflt, const char *info, unsigned int *sawP,
                               const airEnum *enm, const hestCB *CB);
extern void hest__optPriorityMinMax(int *minP, int *maxP, const hestOpt *hopt);

/*
 * These were moved not just from air.h (see note there) but also from public hest.h,
 * with a air --> hest renaming, since these are only used to implement hest functions.
 */
#define MY_HEST_TYPE_MAX 14
extern const char hest__TypeStr[MY_HEST_TYPE_MAX + 1][AIR_STRLEN_SMALL + 1];
extern const size_t hest__TypeSize[MY_HEST_TYPE_MAX + 1];
extern void (*const hest__InvertScalar[MY_HEST_TYPE_MAX + 1])(void *);
/* the hest__PPair (hest parse pair) struct is a 2025 idea for addressing one of the most
annoying parts of hest's code: the special-casing of how values are parsed from strings,
depending on type (scalar vs enum vs other-via-callbacks). Old code has long had nested
switch statements: first for option kind, and then for option type. Fresh scrutiny of
this code revealed how, now that there is no more airStrtok/airStrntok action because all
individual args have been saved into per-option arg vectors, the parsing problem is just
to get a single value, and, how to get a single value can be made entirely uniform across
type; via the hest__ParseSingle[] array. All the info needed here is just the hestOpt
(and `parseMop`, `enm`, and `CB` fields within it), and the `err` msg buffer that is
used to describe any parsing error, not just via hestCB.  */
typedef struct {
  hestOpt *hopt;                 // what option is this for
  char err[AIR_STRLEN_HUGE + 1]; // error message can go for any type
} hest__PPair;
extern int (*const hest__ParseSingle[MY_HEST_TYPE_MAX + 1])(void *, const char *,
                                                            hest__PPair *);
extern const char *const hest__BiffKey;
extern int hest__Max(int max);
extern int hest__OPCheck(const hestOpt *hopt, const hestParm *parm);

// argvHest.c
extern int hest__PlainWord(const char *s);

/*                    1 for more context in biff error messages,
                      0 for fewer uncovered branches in coverage testing */
#define MY_ME_VERBOSE 0
#if MY_ME_VERBOSE
/* wacky hack to use with biffAddf(HEST), for debugging, so that normal hest users,
   who haven't cranked up hestParm->verbose to debug hest itself, don't get error
   messages prefixed by a lot of weird internal hest function names.
   In a function where hestParm *hparm is defined, the biff call should look like:
     biffAddf(HEST, "%s%shit a problem", MY_ME, ... )
   Note the "%s%s" start, with NO following space.
   Or if its not hparm but some other source `verb` of a verbosity level
     biffAddf(HEST, "%s%sthis is the error", MY_MEV(verb), ... ) */
#  define MY_ME                                                                         \
    ((hparm) && (hparm)->verbose ? __func__ : ""),                                      \
      ((hparm) && (hparm)->verbose ? ": " : "")
#  define MY_MEV(V) ((V) ? __func__ : ""), ((V) ? ": " : "")
#else
// Stub versions of the above
#  define MY_ME     "", ""
#  define MY_MEV(V) "", ""
#endif

/* when displaying argv via hestArgVecSprint for error messages, what
   should we pass for final showIndex argument that enables "i:arg" labeling
   (making this a #define instead of a variable seems to increase branch coverage)
                0 : no labels
                1 : yes labels */
#define MY_SHAI 0

#ifdef __cplusplus
}
#endif
