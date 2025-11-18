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
 * _nrrdLoad<TA><TB>(const void *_v)
 *
 * Load value from TB*, convert to TA:
 * assign `*_v` to `const TB*`, dereference, cast value to TA, return it.
 *
 * calling a cast function pointer is undefined behavior (UB), so TeemV2 avoids this:
 * #define LOAD_DEFN(TA, TB) static TA _nrrdLoad##TA##TB(TB *v) { return (TA)(*v); }
 * #define LOAD_LIST(TA, TB) (TA (*)(const void *)) _nrrdLoad##TA##TB,
 *                           ^^^^^^^^^^^^^^^^^^^^^^ casting function pointer
 */
#define LOAD_DEFN(TA, TB)                                                               \
  static TA _nrrdLoad##TA##TB(const void *_v) {                                         \
    const TB *v = _v;                                                                   \
    return (TA)(*v);                                                                    \
  }
#define LOAD_LIST(TA, TB) _nrrdLoad##TA##TB,

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
 * _nrrdStore<TA><TB>(void *_v, <TA> j)
 *
 * Stores TA `j` in (after casting to `TB*`) `*_v`, implicitly casting value to TB.
 * Returns the result of the assignment, cast back to TA, which may not be the same as
 * the value that was passed in.
 *
 * TeemV2 seeks to avoid UB by not casting function pointers, so no longer doing:
 * #define STORE_DEF(TA, TB) \
 *    static TA _nrrdStore##TA##TB(TB *v, TA j) { return (TA)(*v = (TB)j); }
 * #define STORE_LIST(TA, TB) (TA (*)(void *, TA)) _nrrdStore##TA##TB,
 *                            ^^^^^^^^^^^^^^^^^^^ casting function pointer
 */
#define STORE_DEF(TA, TB)                                                               \
  static TA _nrrdStore##TA##TB(void *_v, TA j) {                                        \
    TB *v = _v;                                                                         \
    return (TA)(*v = (TB)j);                                                            \
  }
#define STORE_LIST(TA, TB) _nrrdStore##TA##TB,

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
 * _nrrdLookup<TA><TB>(const void *_v, size_t I)
 *
 * After assigning _v to TB *v, looks up TB v[I], and returns it cast to TA.
 *
 * pre-TeemV2 UB code:
 * #define LOOKUP_DEF(TA, TB)
 *   static TA _nrrdLookup##TA##TB(TB *v, size_t I) { return (TA)v[I]; }
 * #define LOOKUP_LIST(TA, TB) (TA (*)(const void *, size_t)) _nrrdLookup##TA##TB,
 */
#define LOOKUP_DEF(TA, TB)                                                              \
  static TA _nrrdLookup##TA##TB(const void *_v, size_t I) {                             \
    const TB *v = _v;                                                                   \
    return (TA)v[I];                                                                    \
  }
#define LOOKUP_LIST(TA, TB) _nrrdLookup##TA##TB,

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
 * _nrrdInsert<TA><TB>(void *_v, size_t I, <TA> j)
 *
 * Given TA j, casts to TB, and stores it in (after casting to TB*) _v[i].
 * Returns the result of the assignment, cast back to TA, which may not be the same as
 * the value that was passed in.
 * pre-TeemV2 UB code:
 * #define INSERT_DEF(TA, TB)
 *   static TA _nrrdInsert##TA##TB(TB *v, size_t I, TA j) { return (TA)(v[I] = (TB)j); }
 * #define INSERT_LIST(TA, TB) (TA (*)(void *, size_t, TA)) _nrrdInsert##TA##TB,
 */
#define INSERT_DEF(TA, TB)                                                              \
  static TA _nrrdInsert##TA##TB(void *_v, size_t I, TA j) {                             \
    TB *v = _v;                                                                         \
    return (TA)(v[I] = (TB)j);                                                          \
  }
#define INSERT_LIST(TA, TB) _nrrdInsert##TA##TB,

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
_nrrdSprintCH(char *s, size_t sz, const void *_v) {
     const CH *v = _v; return snprintf(s, sz, "%d", *v); }
static int
_nrrdSprintUC(char *s, size_t sz, const void *_v) {
     const UC *v = _v; return snprintf(s, sz, "%u", *v); }
static int
_nrrdSprintSH(char *s, size_t sz, const void *_v) {
    const SH *v = _v; return snprintf(s, sz, "%d", *v); }
static int
_nrrdSprintUS(char *s, size_t sz, const void *_v) {
     const US *v = _v; return snprintf(s, sz, "%u", *v); }
static int
_nrrdSprintIN(char *s, size_t sz, const void *_v) {
     const JN *v = _v; return snprintf(s, sz, "%d", *v); }
static int
_nrrdSprintUI(char *s, size_t sz, const void *_v) {
     const UI *v = _v; return snprintf(s, sz, "%u", *v); }
static int
_nrrdSprintLL(char *s, size_t sz, const void *_v) {
     const LL *v = _v; return snprintf(s, sz, AIR_LLONG_FMT, *v); }
static int
_nrrdSprintUL(char *s, size_t sz, const void *_v) {
     const UL *v = _v; return snprintf(s, sz, AIR_ULLONG_FMT, *v); }
static int
_nrrdSprintFL(char *s, size_t sz, const void *_v) {
  const FL *v = _v;
  /* having %.8g instead of %.9g was a roughly 20-year old bug,
     but now using %g with TeemV2 airSinglePrintf */
  return airSinglePrintf(NULL, s, sz, "%g", *v);
}
static int
_nrrdSprintDB(char *s, size_t sz, const void *_v) {
  const DB *v = _v;
  /* not with %.17g thanks to TeemV2 airSinglePrintf */
  return airSinglePrintf(NULL, s, sz, "%lg", *v);
}
int (*const nrrdSprint[NRRD_TYPE_MAX + 1])(char *, size_t, const void *)
  = {NULL,
     _nrrdSprintCH,
     _nrrdSprintUC,
     _nrrdSprintSH,
     _nrrdSprintUS,
     _nrrdSprintIN,
     _nrrdSprintUI,
     _nrrdSprintLL,
     _nrrdSprintUL,
     _nrrdSprintFL,
     _nrrdSprintDB,
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
_nrrdFprintCH(FILE *f, const void *_v) {
     const CH *v = _v; return fprintf(f, "%d", *v); }
static int
_nrrdFprintUC(FILE *f, const void *_v) {
     const UC *v = _v; return fprintf(f, "%u", *v); }
static int
_nrrdFprintSH(FILE *f, const void *_v) {
     const SH *v = _v; return fprintf(f, "%d", *v); }
static int
_nrrdFprintUS(FILE *f, const void *_v) {
     const US *v = _v; return fprintf(f, "%u", *v); }
static int
_nrrdFprintIN(FILE *f, const void *_v) {
     const JN *v = _v; return fprintf(f, "%d", *v); }
static int
_nrrdFprintUI(FILE *f, const void *_v) {
     const UI *v = _v; return fprintf(f, "%u", *v); }
static int
_nrrdFprintLL(FILE *f, const void *_v) {
     const LL *v = _v; return fprintf(f, AIR_LLONG_FMT, *v); }
static int
_nrrdFprintUL(FILE *f, const void *_v) {
     const UL *v = _v; return fprintf(f, AIR_ULLONG_FMT, *v); }
static int
_nrrdFprintFL(FILE *f, const void *_v) {
  const FL *v = _v;
  /* had %.8g here since r427 2002-04-29 (23-year old bug)
     now using %g with TeemV2 airSinglePrintf instead of %.9g */
  return airSinglePrintf(f, NULL, 0, "%g", (double)(*v));
}
static int
_nrrdFprintDB(FILE *f, const void *_v) {
  const DB *v = _v;
  /* not with %.17g thanks to TeemV2 airSinglePrintf */
  return airSinglePrintf(f, NULL, 0, "%lg", *v);
}

int (*const nrrdFprint[NRRD_TYPE_MAX + 1])(FILE *, const void *)
  = {NULL,
     _nrrdFprintCH,
     _nrrdFprintUC,
     _nrrdFprintSH,
     _nrrdFprintUS,
     _nrrdFprintIN,
     _nrrdFprintUI,
     _nrrdFprintLL,
     _nrrdFprintUL,
     _nrrdFprintFL,
     _nrrdFprintDB,
     NULL};
/* clang-format on */

/* about here is where Gordon admits he might have some use for C++ */

#define _MMEF_ARGS(type) void *_minP, void *_maxP, int *hneP, const Nrrd *nrrd

#define _MMEF_FIXED(type)                                                               \
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

#define _MMEF_FLOAT(type)                                                               \
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
static void _nrrdMinMaxExactFindCH(_MMEF_ARGS(CH)) { _MMEF_FIXED(CH) }
static void _nrrdMinMaxExactFindUC(_MMEF_ARGS(UC)) { _MMEF_FIXED(UC) }
static void _nrrdMinMaxExactFindSH(_MMEF_ARGS(SH)) { _MMEF_FIXED(SH) }
static void _nrrdMinMaxExactFindUS(_MMEF_ARGS(US)) { _MMEF_FIXED(US) }
static void _nrrdMinMaxExactFindIN(_MMEF_ARGS(JN)) { _MMEF_FIXED(JN) }
static void _nrrdMinMaxExactFindUI(_MMEF_ARGS(UI)) { _MMEF_FIXED(UI) }
static void _nrrdMinMaxExactFindLL(_MMEF_ARGS(LL)) { _MMEF_FIXED(LL) }
static void _nrrdMinMaxExactFindUL(_MMEF_ARGS(UL)) { _MMEF_FIXED(UL) }
static void _nrrdMinMaxExactFindFL(_MMEF_ARGS(FL)) { _MMEF_FLOAT(FL) }
static void _nrrdMinMaxExactFindDB(_MMEF_ARGS(DB)) { _MMEF_FLOAT(DB) }
/* clang-format on */
#undef _MMEF_ARGS
#undef _MMEF_FIXED
#undef _MMEF_FLOAG

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
     _nrrdMinMaxExactFindCH,
     _nrrdMinMaxExactFindUC,
     _nrrdMinMaxExactFindSH,
     _nrrdMinMaxExactFindUS,
     _nrrdMinMaxExactFindIN,
     _nrrdMinMaxExactFindUI,
     _nrrdMinMaxExactFindLL,
     _nrrdMinMaxExactFindUL,
     _nrrdMinMaxExactFindFL,
     _nrrdMinMaxExactFindDB,
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
 * which underlies _nrrdMeasureMedian(), and to make it easier to separate
 * existent from non-existent values.
 */
#define _ASC_COMP(AA, BB) ((AA) < (BB) ? -1 : ((AA) > (BB) ? 1 : 0))
#define _DEC_COMP(AA, BB) (-(_ASC_COMP(AA, BB)))
#define _VC_FIXED(COMP, type)                                                           \
  const type *A = _A, *B = _B;                                                          \
  return COMP(*A, *B)
#define _VC_FLOAT(COMP, type)                                                           \
  const type *A = _A, *B = _B;                                                          \
  int exA = AIR_EXISTS(*A), exB = AIR_EXISTS(*B);                                       \
  int ret;                                                                              \
  switch (exA + exB) {                                                                  \
  case 2:                                                                               \
    ret = COMP(*A, *B);                                                                 \
    break;                                                                              \
  case 1:                                                                               \
    ret = COMP(exA, exB);                                                               \
    break;                                                                              \
  case 0:                                                                               \
  default:                                                                              \
    ret = 0;                                                                            \
  }                                                                                     \
  return ret

/* clang-format off */
static int _nrrdValCompareCH(const void *_A, const void *_B) { _VC_FIXED(_ASC_COMP, CH); }
static int _nrrdValCompareUC(const void *_A, const void *_B) { _VC_FIXED(_ASC_COMP, UC); }
static int _nrrdValCompareSH(const void *_A, const void *_B) { _VC_FIXED(_ASC_COMP, SH); }
static int _nrrdValCompareUS(const void *_A, const void *_B) { _VC_FIXED(_ASC_COMP, US); }
static int _nrrdValCompareIN(const void *_A, const void *_B) { _VC_FIXED(_ASC_COMP, JN); }
static int _nrrdValCompareUI(const void *_A, const void *_B) { _VC_FIXED(_ASC_COMP, UI); }
static int _nrrdValCompareLL(const void *_A, const void *_B) { _VC_FIXED(_ASC_COMP, LL); }
static int _nrrdValCompareUL(const void *_A, const void *_B) { _VC_FIXED(_ASC_COMP, UL); }
static int _nrrdValCompareFL(const void *_A, const void *_B) { _VC_FLOAT(_ASC_COMP, FL); }
static int _nrrdValCompareDB(const void *_A, const void *_B) { _VC_FLOAT(_ASC_COMP, DB); }
int (*const nrrdValCompare[NRRD_TYPE_MAX + 1])(const void *, const void *)
  = {NULL,
     _nrrdValCompareCH,
     _nrrdValCompareUC,
     _nrrdValCompareSH,
     _nrrdValCompareUS,
     _nrrdValCompareIN,
     _nrrdValCompareUI,
     _nrrdValCompareLL,
     _nrrdValCompareUL,
     _nrrdValCompareFL,
     _nrrdValCompareDB,
     NULL};

/*
 * ...Inv: for descending order
 */
static int _nrrdValCompareInvCH(const void *_A, const void *_B) { _VC_FIXED(_DEC_COMP, CH); }
static int _nrrdValCompareInvUC(const void *_A, const void *_B) { _VC_FIXED(_DEC_COMP, UC); }
static int _nrrdValCompareInvSH(const void *_A, const void *_B) { _VC_FIXED(_DEC_COMP, SH); }
static int _nrrdValCompareInvUS(const void *_A, const void *_B) { _VC_FIXED(_DEC_COMP, US); }
static int _nrrdValCompareInvIN(const void *_A, const void *_B) { _VC_FIXED(_DEC_COMP, JN); }
static int _nrrdValCompareInvUI(const void *_A, const void *_B) { _VC_FIXED(_DEC_COMP, UI); }
static int _nrrdValCompareInvLL(const void *_A, const void *_B) { _VC_FIXED(_DEC_COMP, LL); }
static int _nrrdValCompareInvUL(const void *_A, const void *_B) { _VC_FIXED(_DEC_COMP, UL); }
static int _nrrdValCompareInvFL(const void *_A, const void *_B) { _VC_FLOAT(_DEC_COMP, FL); }
static int _nrrdValCompareInvDB(const void *_A, const void *_B) { _VC_FLOAT(_DEC_COMP, DB); }
int (*const nrrdValCompareInv[NRRD_TYPE_MAX + 1])(const void *, const void *)
  = {NULL,
     _nrrdValCompareInvCH,
     _nrrdValCompareInvUC,
     _nrrdValCompareInvSH,
     _nrrdValCompareInvUS,
     _nrrdValCompareInvIN,
     _nrrdValCompareInvUI,
     _nrrdValCompareInvLL,
     _nrrdValCompareInvUL,
     _nrrdValCompareInvFL,
     _nrrdValCompareInvDB,
     NULL};
/* clang-format on */
#undef _ASC_COMP
#undef _DEC_COMP
#undef _VC_FIXED
#undef _VC_FLOAT

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
