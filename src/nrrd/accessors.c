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

#include "nrrd.h"
#include "privateNrrd.h"
#include "float.h"

/*
** making these typedefs here allows us to use one token for both
** constructing function names, and for specifying argument types
*/
typedef signed char CH;
typedef unsigned char UC;
typedef signed short SH;
typedef unsigned short US;
/* Microsoft apparently uses 'IN' as a keyword, so we changed 'IN' to 'JN'. */
typedef signed int JN;
typedef unsigned int UI;
typedef airLLong LL;
/* ui64 to double conversion is not implemented, sorry */
#if defined(_MSC_VER) && _MSC_VER < 1300
typedef airLLong UL;
#else
typedef airULLong UL;
#endif
typedef float FL;
typedef double DB;

#define MAP(F, A)                                                                       \
  F(A, CH)                                                                              \
  F(A, UC)                                                                              \
  F(A, SH)                                                                              \
  F(A, US)                                                                              \
  F(A, JN)                                                                              \
  F(A, UI)                                                                              \
  F(A, LL)                                                                              \
  F(A, UL)                                                                              \
  F(A, FL)                                                                              \
  F(A, DB)

/*
 * nload<TA><TB>(const void *_v)
 *
 * Load value from TB*, convert to TA:
 * assign `*_v` to `const TB*`, dereference, cast value to TA, return it.
 *
 * calling a cast function pointer is undefined behavior (UB), so TeemV2 avoids this:
 * #define LOAD_DEFN(TA, TB) static TA nload##TA##TB(TB *v) { return (TA)(*v); }
 * #define LOAD_LIST(TA, TB) (TA (*)(const void *)) nload##TA##TB,
 *                           ^^^^^^^^^^^^^^^^^^^^^^ casting function pointer
 */
#define LOAD_DEFN(TA, TB)                                                               \
  static TA nload##TA##TB(const void *_v) {                                             \
    const TB *v = _v;                                                                   \
    return (TA)(*v);                                                                    \
  }
#define LOAD_LIST(TA, TB) nload##TA##TB,

MAP(LOAD_DEFN, UI)
MAP(LOAD_DEFN, JN)
MAP(LOAD_DEFN, FL)
MAP(LOAD_DEFN, DB)

/* clang-format off */
unsigned int (*const nrrdUILoad[NRRD_TYPE_MAX + 1])(const void *) = {NULL, MAP(LOAD_LIST, UI) NULL};
         int (*const  nrrdILoad[NRRD_TYPE_MAX + 1])(const void *) = {NULL, MAP(LOAD_LIST, JN) NULL};
       float (*const  nrrdFLoad[NRRD_TYPE_MAX + 1])(const void *) = {NULL, MAP(LOAD_LIST, FL) NULL};
      double (*const  nrrdDLoad[NRRD_TYPE_MAX + 1])(const void *) = {NULL, MAP(LOAD_LIST, DB) NULL};
/* clang-format on */
#undef LOAD_DEFN
#undef LOAD_LIST

/*
 * nstore<TA><TB>(void *_v, <TA> j)
 *
 * Stores TA `j` in (after casting to `TB*`) `*_v`, implicitly casting value to TB.
 * Returns the result of the assignment, cast back to TA, which may not be the same as
 * the value that was passed in.
 *
 * TeemV2 seeks to avoid UB by not casting function pointers, so no longer doing:
 * #define STORE_DEF(TA, TB) \
 *    static TA nstore##TA##TB(TB *v, TA j) { return (TA)(*v = (TB)j); }
 * #define STORE_LIST(TA, TB) (TA (*)(void *, TA)) nstore##TA##TB,
 *                            ^^^^^^^^^^^^^^^^^^^ casting function pointer
 */
#define STORE_DEF(TA, TB)                                                               \
  static TA nstore##TA##TB(void *_v, TA j) {                                            \
    TB *v = _v;                                                                         \
    return (TA)(*v = (TB)j);                                                            \
  }
#define STORE_LIST(TA, TB) nstore##TA##TB,

MAP(STORE_DEF, UI)
MAP(STORE_DEF, JN)
MAP(STORE_DEF, FL)
MAP(STORE_DEF, DB)

/* clang-format off */
unsigned int (*const nrrdUIStore[NRRD_TYPE_MAX + 1])(void *, unsigned int) = {NULL, MAP(STORE_LIST, UI) NULL};
         int (*const  nrrdIStore[NRRD_TYPE_MAX + 1])(void *, int)          = {NULL, MAP(STORE_LIST, JN) NULL};
       float (*const  nrrdFStore[NRRD_TYPE_MAX + 1])(void *, float)        = {NULL, MAP(STORE_LIST, FL) NULL};
      double (*const  nrrdDStore[NRRD_TYPE_MAX + 1])(void *, double)       = {NULL, MAP(STORE_LIST, DB) NULL};
/* clang-format on */
#undef STORE_DEF
#undef STORE_LIST

/*
 * nlookup<TA><TB>(const void *_v, size_t I)
 *
 * After assigning _v to TB *v, looks up TB v[I], and returns it cast to TA.
 *
 * pre-TeemV2 UB code:
 * #define LOOKUP_DEF(TA, TB)
 *   static TA nlookup##TA##TB(TB *v, size_t I) { return (TA)v[I]; }
 * #define LOOKUP_LIST(TA, TB) (TA (*)(const void *, size_t)) nlookup##TA##TB,
 */
#define LOOKUP_DEF(TA, TB)                                                              \
  static TA nlookup##TA##TB(const void *_v, size_t I) {                                 \
    const TB *v = _v;                                                                   \
    return (TA)v[I];                                                                    \
  }
#define LOOKUP_LIST(TA, TB) nlookup##TA##TB,

MAP(LOOKUP_DEF, UI)
MAP(LOOKUP_DEF, JN)
MAP(LOOKUP_DEF, FL)
MAP(LOOKUP_DEF, DB)

/* clang-format off */
unsigned int (*const nrrdUILookup[NRRD_TYPE_MAX + 1])(const void *, size_t) = {NULL, MAP(LOOKUP_LIST, UI) NULL};
         int (*const  nrrdILookup[NRRD_TYPE_MAX + 1])(const void *, size_t) = {NULL, MAP(LOOKUP_LIST, JN) NULL};
       float (*const  nrrdFLookup[NRRD_TYPE_MAX + 1])(const void *, size_t) = {NULL, MAP(LOOKUP_LIST, FL) NULL};
      double (*const  nrrdDLookup[NRRD_TYPE_MAX + 1])(const void *, size_t) = {NULL, MAP(LOOKUP_LIST, DB) NULL};
/* clang-format on */
#undef LOOKUP_DEF
#undef LOOKUP_LIST

/*
 * ninsert<TA><TB>(void *_v, size_t I, <TA> j)
 *
 * Given TA j, casts to TB, and stores it in (after casting to TB*) _v[i].
 * Returns the result of the assignment, cast back to TA, which may not be the same as
 * the value that was passed in.
 * pre-TeemV2 UB code:
 * #define INSERT_DEF(TA, TB)
 *   static TA ninsert##TA##TB(TB *v, size_t I, TA j) { return (TA)(v[I] = (TB)j); }
 * #define INSERT_LIST(TA, TB) (TA (*)(void *, size_t, TA)) ninsert##TA##TB,
 */
#define INSERT_DEF(TA, TB)                                                              \
  static TA ninsert##TA##TB(void *_v, size_t I, TA j) {                                 \
    TB *v = _v;                                                                         \
    return (TA)(v[I] = (TB)j);                                                          \
  }
#define INSERT_LIST(TA, TB) ninsert##TA##TB,

MAP(INSERT_DEF, UI)
MAP(INSERT_DEF, JN)
MAP(INSERT_DEF, FL)
MAP(INSERT_DEF, DB)

/* clang-format off */
unsigned int (*const nrrdUIInsert[NRRD_TYPE_MAX + 1])(void *, size_t, unsigned int) = {NULL, MAP(INSERT_LIST, UI) NULL};
         int (*const  nrrdIInsert[NRRD_TYPE_MAX + 1])(void *, size_t, int)          = {NULL, MAP(INSERT_LIST, JN) NULL};
       float (*const  nrrdFInsert[NRRD_TYPE_MAX + 1])(void *, size_t, float)        = {NULL, MAP(INSERT_LIST, FL) NULL};
      double (*const  nrrdDInsert[NRRD_TYPE_MAX + 1])(void *, size_t, double)       = {NULL, MAP(INSERT_LIST, DB) NULL};
/* clang-format on */
#undef INSERT_DEF
#undef INSERT_LIST

/*
 ******** nrrdSprint
 *
 * Dereferences void *_v (after casting to specific type)
 * and snprintf()s that value into given string s,
 * returns the result of snprintf()
 */
/* clang-format off */
static int
nsprintCH(char *s, size_t sz, const void *_v) {
     const CH *v = _v; return snprintf(s, sz, "%d", *v); }
static int
nsprintUC(char *s, size_t sz, const void *_v) {
     const UC *v = _v; return snprintf(s, sz, "%u", *v); }
static int
nsprintSH(char *s, size_t sz, const void *_v) {
    const SH *v = _v; return snprintf(s, sz, "%d", *v); }
static int
nsprintUS(char *s, size_t sz, const void *_v) {
     const US *v = _v; return snprintf(s, sz, "%u", *v); }
static int
nsprintIN(char *s, size_t sz, const void *_v) {
     const JN *v = _v; return snprintf(s, sz, "%d", *v); }
static int
nsprintUI(char *s, size_t sz, const void *_v) {
     const UI *v = _v; return snprintf(s, sz, "%u", *v); }
static int
nsprintLL(char *s, size_t sz, const void *_v) {
     const LL *v = _v; return snprintf(s, sz, AIR_LLONG_FMT, *v); }
static int
nsprintUL(char *s, size_t sz, const void *_v) {
     const UL *v = _v; return snprintf(s, sz, AIR_ULLONG_FMT, *v); }
static int
nsprintFL(char *s, size_t sz, const void *_v) {
  const FL *v = _v;
  /* having %.8g instead of %.9g was a roughly 20-year old bug,
     but now using %g with TeemV2 airSinglePrintf */
  return airSinglePrintf(NULL, s, sz, "%g", *v);
}
static int
nsprintDB(char *s, size_t sz, const void *_v) {
  const DB *v = _v;
  /* not with %.17g thanks to TeemV2 airSinglePrintf */
  return airSinglePrintf(NULL, s, sz, "%lg", *v);
}
int (*const nrrdSprint[NRRD_TYPE_MAX + 1])(char *, size_t, const void *)
  = {NULL,
     nsprintCH,
     nsprintUC,
     nsprintSH,
     nsprintUS,
     nsprintIN,
     nsprintUI,
     nsprintLL,
     nsprintUL,
     nsprintFL,
     nsprintDB,
     NULL};
/* clang-format on */

/* ---- BEGIN non-NrrdIO */

/*
 ******** nrrdFprint
 *
 * Dereferences void *_v (after casting to specific type)
 * and fprintf()s that value into given file f;
 * returns the result of fprintf()
 */
/* clang-format off */
static int
nfprintCH(FILE *f, const void *_v) {
     const CH *v = _v; return fprintf(f, "%d", *v); }
static int
nfprintUC(FILE *f, const void *_v) {
     const UC *v = _v; return fprintf(f, "%u", *v); }
static int
nfprintSH(FILE *f, const void *_v) {
     const SH *v = _v; return fprintf(f, "%d", *v); }
static int
nfprintUS(FILE *f, const void *_v) {
     const US *v = _v; return fprintf(f, "%u", *v); }
static int
nfprintIN(FILE *f, const void *_v) {
     const JN *v = _v; return fprintf(f, "%d", *v); }
static int
nfprintUI(FILE *f, const void *_v) {
     const UI *v = _v; return fprintf(f, "%u", *v); }
static int
nfprintLL(FILE *f, const void *_v) {
     const LL *v = _v; return fprintf(f, AIR_LLONG_FMT, *v); }
static int
nfprintUL(FILE *f, const void *_v) {
     const UL *v = _v; return fprintf(f, AIR_ULLONG_FMT, *v); }
static int
nfprintFL(FILE *f, const void *_v) {
  const FL *v = _v;
  /* had %.8g here since r427 2002-04-29 (23-year old bug)
     now using %g with TeemV2 airSinglePrintf instead of %.9g */
  return airSinglePrintf(f, NULL, 0, "%g", (double)(*v));
}
static int
nfprintDB(FILE *f, const void *_v) {
  const DB *v = _v;
  /* not with %.17g thanks to TeemV2 airSinglePrintf */
  return airSinglePrintf(f, NULL, 0, "%lg", *v);
}

int (*const nrrdFprint[NRRD_TYPE_MAX + 1])(FILE *, const void *)
  = {NULL,
     nfprintCH,
     nfprintUC,
     nfprintSH,
     nfprintUS,
     nfprintIN,
     nfprintUI,
     nfprintLL,
     nfprintUL,
     nfprintFL,
     nfprintDB,
     NULL};
/* clang-format on */

/* about here is where Gordon admits he might have some use for C++ */

#define MMEF_ARGS(type) void *_minP, void *_maxP, int *hneP, const Nrrd *nrrd

#define MMEF_FIXED(type)                                                                \
  size_t I, N;                                                                          \
  type *minP, *maxP, a, b, min, max, *v;                                                \
                                                                                        \
  if (!(_minP && _maxP)) return;                                                        \
  /* get properly typed pointers from the void*s */                                     \
  minP = _minP;                                                                         \
  maxP = _maxP;                                                                         \
                                                                                        \
  /* all integral values exist */                                                       \
  *hneP = nrrdHasNonExistFalse;                                                         \
                                                                                        \
  /* set the local data pointer */                                                      \
  v = nrrd->data;                                                                       \
                                                                                        \
  /* get initial values */                                                              \
  N = nrrdElementNumber(nrrd);                                                          \
  min = max = v[0];                                                                     \
                                                                                        \
  /* run through array in pairs; by doing a compare on successive                       \
     elements, we can do three compares per pair instead of the naive                   \
     four.  In one very unexhaustive test on irix6.64, this resulted                    \
     in a 20% decrease in running time.  I learned this trick from                      \
     Numerical Recipes in C, long time ago, but I can't find it                         \
     anywhere in the book now ... */                                                    \
  if (N > 1) /* size_t is unsigned, so N-2 may overflow */                              \
    for (I = 0; I <= N - 2; I += 2) {                                                   \
      a = v[0 + I];                                                                     \
      b = v[1 + I];                                                                     \
      if (a < b) {                                                                      \
        min = AIR_MIN(a, min);                                                          \
        max = AIR_MAX(b, max);                                                          \
      } else {                                                                          \
        max = AIR_MAX(a, max);                                                          \
        min = AIR_MIN(b, min);                                                          \
      }                                                                                 \
    }                                                                                   \
                                                                                        \
  /* get the very last one (may be redundant) */                                        \
  a = v[N - 1];                                                                         \
  min = AIR_MIN(a, min);                                                                \
  max = AIR_MAX(a, max);                                                                \
                                                                                        \
  /* record results */                                                                  \
  *minP = min;                                                                          \
  *maxP = max;

#define MMEF_FLOAT(type)                                                                \
  size_t I, N;                                                                          \
  type *minP, *maxP, a, min, max, *v;                                                   \
                                                                                        \
  if (!(_minP && _maxP)) return;                                                        \
  /* get properly typed pointers from the void*s */                                     \
  minP = _minP;                                                                         \
  maxP = _maxP;                                                                         \
                                                                                        \
  /* this may be over-written below */                                                  \
  *hneP = nrrdHasNonExistFalse;                                                         \
                                                                                        \
  /* set the local data pointer */                                                      \
  N = nrrdElementNumber(nrrd);                                                          \
  v = nrrd->data;                                                                       \
                                                                                        \
  /* we have to explicitly search for the first non-NaN value */                        \
  max = min = AIR_NAN;                                                                  \
  for (I = 0; I < N; I++) {                                                             \
    a = v[I];                                                                           \
    if (AIR_EXISTS(a)) {                                                                \
      min = max = a;                                                                    \
      break;                                                                            \
    } else {                                                                            \
      *hneP = nrrdHasNonExistTrue;                                                      \
    }                                                                                   \
  }                                                                                     \
  if (I == N) {                                                                         \
    /* oh dear, there were NO existent values */                                        \
    min = max = AIR_NAN;                                                                \
    *hneP = nrrdHasNonExistOnly;                                                        \
  } else {                                                                              \
    /* there was at least one existent value; we continue searching,                    \
       still checking AIR_EXISTS at each value */                                       \
    for (I = I + 1; I < N; I++) {                                                       \
      a = v[I];                                                                         \
      if (AIR_EXISTS(a)) {                                                              \
        if (a < min) {                                                                  \
          min = a;                                                                      \
        } else {                                                                        \
          if (a > max) {                                                                \
            max = a;                                                                    \
          }                                                                             \
        }                                                                               \
      } else {                                                                          \
        *hneP = nrrdHasNonExistTrue;                                                    \
      }                                                                                 \
    }                                                                                   \
  }                                                                                     \
  *minP = min;                                                                          \
  *maxP = max;

/* clang-format off */
static void nMinMaxExactFindCH(MMEF_ARGS(CH)) { MMEF_FIXED(CH) }
static void nMinMaxExactFindUC(MMEF_ARGS(UC)) { MMEF_FIXED(UC) }
static void nMinMaxExactFindSH(MMEF_ARGS(SH)) { MMEF_FIXED(SH) }
static void nMinMaxExactFindUS(MMEF_ARGS(US)) { MMEF_FIXED(US) }
static void nMinMaxExactFindIN(MMEF_ARGS(JN)) { MMEF_FIXED(JN) }
static void nMinMaxExactFindUI(MMEF_ARGS(UI)) { MMEF_FIXED(UI) }
static void nMinMaxExactFindLL(MMEF_ARGS(LL)) { MMEF_FIXED(LL) }
static void nMinMaxExactFindUL(MMEF_ARGS(UL)) { MMEF_FIXED(UL) }
static void nMinMaxExactFindFL(MMEF_ARGS(FL)) { MMEF_FLOAT(FL) }
static void nMinMaxExactFindDB(MMEF_ARGS(DB)) { MMEF_FLOAT(DB) }
/* clang-format on */
#undef MMEF_ARGS
#undef MMEF_FIXED
#undef MMEF_FLOAT

/*
 ******** nrrdMinMaxExactFind[]
 *
 * the role of these is to allow finding the EXACT min and max of a nrrd,
 * so that one does not have to rely on the potentially lossy storage
 * of the min and max values in range->min and range->max, which are doubles.
 *
 * These also sets *hneP, using a value from the nrrdHasNonExist* enum
 */
void (*const nrrdMinMaxExactFind[NRRD_TYPE_MAX + 1])(void *minP, void *maxP, int *hneP,
                                                     const Nrrd *)
  = {NULL,
     nMinMaxExactFindCH,
     nMinMaxExactFindUC,
     nMinMaxExactFindSH,
     nMinMaxExactFindUS,
     nMinMaxExactFindIN,
     nMinMaxExactFindUI,
     nMinMaxExactFindLL,
     nMinMaxExactFindUL,
     nMinMaxExactFindFL,
     nMinMaxExactFindDB,
     NULL};

/*
 ******** nrrdValCompare[]
 *
 * the sort of compare you'd give to qsort() to sort in ascending order:
 * return < 0 if A < B,
 *          0 if A == B,
 *        > 0 if A > B
 * The non-trivial part of this is that for floating-point values, we
 * dictate that all non-existent values are smaller than all existent
 * values, regardless of their actual values (so +infinity < -42).  This
 * is to make sure that we have comparison that won't confuse qsort(),
 * which underlies nrrd__MeasureMedian(), and to make it easier to separate
 * existent from non-existent values.
 */
#define ASC_COMP(AA, BB) ((AA) < (BB) ? -1 : ((AA) > (BB) ? 1 : 0))
#define DEC_COMP(AA, BB) (-(ASC_COMP(AA, BB)))
#define VC_FIXED(COMP, type)                                                            \
  const type *aa = _aa, *bb = _bb;                                                      \
  return COMP(*aa, *bb)
#define VC_FLOAT(COMP, type)                                                            \
  const type *aa = _aa, *bb = _bb;                                                      \
  int exaa = AIR_EXISTS(*aa), exbb = AIR_EXISTS(*bb);                                   \
  int ret;                                                                              \
  switch (exaa + exbb) {                                                                \
  case 2:                                                                               \
    ret = COMP(*aa, *bb);                                                               \
    break;                                                                              \
  case 1:                                                                               \
    ret = COMP(exaa, exbb);                                                             \
    break;                                                                              \
  case 0:                                                                               \
  default:                                                                              \
    ret = 0;                                                                            \
  }                                                                                     \
  return ret

/* clang-format off */
static int nvCompareCH(const void *_aa, const void *_bb) { VC_FIXED(ASC_COMP, CH); }
static int nvCompareUC(const void *_aa, const void *_bb) { VC_FIXED(ASC_COMP, UC); }
static int nvCompareSH(const void *_aa, const void *_bb) { VC_FIXED(ASC_COMP, SH); }
static int nvCompareUS(const void *_aa, const void *_bb) { VC_FIXED(ASC_COMP, US); }
static int nvCompareIN(const void *_aa, const void *_bb) { VC_FIXED(ASC_COMP, JN); }
static int nvCompareUI(const void *_aa, const void *_bb) { VC_FIXED(ASC_COMP, UI); }
static int nvCompareLL(const void *_aa, const void *_bb) { VC_FIXED(ASC_COMP, LL); }
static int nvCompareUL(const void *_aa, const void *_bb) { VC_FIXED(ASC_COMP, UL); }
static int nvCompareFL(const void *_aa, const void *_bb) { VC_FLOAT(ASC_COMP, FL); }
static int nvCompareDB(const void *_aa, const void *_bb) { VC_FLOAT(ASC_COMP, DB); }
int (*const nrrdValCompare[NRRD_TYPE_MAX + 1])(const void *, const void *)
  = {NULL,
     nvCompareCH,
     nvCompareUC,
     nvCompareSH,
     nvCompareUS,
     nvCompareIN,
     nvCompareUI,
     nvCompareLL,
     nvCompareUL,
     nvCompareFL,
     nvCompareDB,
     NULL};

/*
 * ...Inv: for descending order
 */
static int nvCompareInvCH(const void *_aa, const void *_bb) { VC_FIXED(DEC_COMP, CH); }
static int nvCompareInvUC(const void *_aa, const void *_bb) { VC_FIXED(DEC_COMP, UC); }
static int nvCompareInvSH(const void *_aa, const void *_bb) { VC_FIXED(DEC_COMP, SH); }
static int nvCompareInvUS(const void *_aa, const void *_bb) { VC_FIXED(DEC_COMP, US); }
static int nvCompareInvIN(const void *_aa, const void *_bb) { VC_FIXED(DEC_COMP, JN); }
static int nvCompareInvUI(const void *_aa, const void *_bb) { VC_FIXED(DEC_COMP, UI); }
static int nvCompareInvLL(const void *_aa, const void *_bb) { VC_FIXED(DEC_COMP, LL); }
static int nvCompareInvUL(const void *_aa, const void *_bb) { VC_FIXED(DEC_COMP, UL); }
static int nvCompareInvFL(const void *_aa, const void *_bb) { VC_FLOAT(DEC_COMP, FL); }
static int nvCompareInvDB(const void *_aa, const void *_bb) { VC_FLOAT(DEC_COMP, DB); }
int (*const nrrdValCompareInv[NRRD_TYPE_MAX + 1])(const void *, const void *)
  = {NULL,
     nvCompareInvCH,
     nvCompareInvUC,
     nvCompareInvSH,
     nvCompareInvUS,
     nvCompareInvIN,
     nvCompareInvUI,
     nvCompareInvLL,
     nvCompareInvUL,
     nvCompareInvFL,
     nvCompareInvDB,
     NULL};
/* clang-format on */
#undef ASC_COMP
#undef DEC_COMP
#undef VC_FIXED
#undef VC_FLOAT

/*
 ******** nrrdArrayCompare
 *
 * something like strcmp() for arrays of numeric values, except that the arrays have to
 * be equal length, and it has to do error checking.
 *
 * See comment about logic of return value above nrrdCompare()
 *
 * This is a very rare kind of nrrd function that operates on a bare arrays, not Nrrds
 */
int /* Biff: 1 */
nrrdArrayCompare(int type, const void *_valA, const void *_valB, size_t valNum,
                 double epsilon, int *differ, char explain[AIR_STRLEN_LARGE + 1]) {
  static const char me[] = "nrrdArrayCompare";
  const unsigned char *valA, *valB;
  int (*compare)(const void *, const void *);
  size_t ii, sze;
  char stmp[AIR_STRLEN_SMALL + 1];

  if (!(_valA && _valB && differ)) {
    biffAddf(NRRD, "%s: got NULL pointer (%p, %p, or %p)", me, _valA, _valB,
             AIR_VOIDP(differ));
    return 1;
  }
  if (!valNum) {
    biffAddf(NRRD, "%s: can't work with 0-length arrays", me);
    return 1;
  }
  if (!AIR_EXISTS(epsilon)) {
    biffAddf(NRRD, "%s: non-existent epsilon %g", me, epsilon);
    return 1;
  }
  if (airEnumValCheck(nrrdType, type)) {
    biffAddf(NRRD, "%s: invalid nrrd type %d", me, type);
    return 1;
  }
  if (nrrdTypeBlock == type) {
    biffAddf(NRRD, "%s: can't use type %s", me, airEnumStr(nrrdType, type));
    return 1;
  }

  if (explain) {
    explain[0] = '\0';
  }
  if (type == nrrdTypeLLong || type == nrrdTypeULLong) {
    fprintf(stderr,
            "%s: WARNING: possible erroneous comparison of "
            "%s values with %s-based comparison\n",
            me, airEnumStr(nrrdType, type), airEnumStr(nrrdType, nrrdTypeDouble));
  }
  sze = nrrdTypeSize[type];
  compare = nrrdValCompare[type];
  valA = AIR_CAST(const unsigned char *, _valA);
  valB = AIR_CAST(const unsigned char *, _valB);
  for (ii = 0; ii < valNum; ii++) {
    *differ = compare(valA + ii * sze, valB + ii * sze);
    if (*differ) {
      double aa, bb;
      /* same loss of precision as warned about above */
      aa = nrrdDLookup[type](valA, ii);
      bb = nrrdDLookup[type](valB, ii);
      if (0 == epsilon || fabs(aa - bb) > epsilon) {
        if (explain) {
          airSprintSize_t(stmp, ii);
          if (0 == epsilon) {
            snprintf(explain, AIR_STRLEN_LARGE + 1,
                     "valA[%s]=%.17g %s valB[%s]=%.17g "
                     "by %g",
                     stmp, aa, *differ < 0 ? "<" : ">", stmp, bb, fabs(aa - bb));
          } else {
            snprintf(explain, AIR_STRLEN_LARGE + 1,
                     "valA[%s]=%.17g %s valB[%s]=%.17g "
                     "by %g, more than eps %g",
                     stmp, aa, *differ < 0 ? "<" : ">", stmp, bb, fabs(aa - bb),
                     epsilon);
          }
        }
        break;
      } else {
        /* we did detect a difference, but it was not in excess of
           epsilon, so we reset *differ to 0 */
        *differ = 0;
      }
    }
  }

  return 0;
}
/* ---- END non-NrrdIO */
