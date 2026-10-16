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

/*
** learned: if you have globals, such as nrrd__CC_verb, which are
** defined and declared here, but which are NOT initialized, then
** C++ apps which are linking against Teem will have problems!!!
** This was first seen on the mac.
** (much later) but these don't need to be globals!
*/
static int nrrd__CC_verb = 0;
static const int nrrd__CC_EqvIncr
  = 10000; /* HEY: this has to be big so that ccfind is not stuck constantly
 re-allocating the eqv array. This is will be one of the best places to test the new
 multiplicative reallocation strategy, planned for Teem 2 */

static int
nCCFind_1(Nrrd *nout, unsigned int *numid, const Nrrd *nin) {
  /* static const char me[] = "nCCFind_1"; */
  unsigned int sx, I, id, lval, val, *out, (*lup)(const void *, size_t);

  lup = nrrdUILookup[nin->type];
  out = AIR_CAST(unsigned int *, nout->data);
  out[0] = id = 0;
  *numid = 1;

  sx = AIR_UINT(nin->axis[0].size);
  lval = lup(nin->data, 0);
  for (I = 1; I < sx; I++) {
    val = lup(nin->data, I);
    if (lval != val) {
      id++;
      (*numid)++;
    }
    out[I] = id;
    lval = val;
  }

  return 0;
}

/*
 * visualization of layout of previously value (integral) `pvl` and CC ID `pid` cache,
 * which are incrementally updated as we sweep along faster "x" axis
 * with a little 3x3 window around the center, where we have to assign a CC ID:
 *
 *  2  3  4 --> X
 *  1  .  .
 *  .  .  .
 *  |
 *  v Y
 *
 * Note: 0 is not a valid index (into `pvl` and `pid`), so 0 means: invalid/unknown index
 * Also, constraints in how we're called ensure that `nin` is some unsigned integer type.
 */
static int
nCCFind_2(Nrrd *nout, unsigned int *numid, airArray *eqvArr, const Nrrd *nin,
          unsigned int conny) {
  static const char me[] = "nCCFind_2";
  double vl = 0,                  /* value from data at center */
    pvl[5] = {0, 0, 0, 0, 0};     /* cache of previous values */
  unsigned int id,                /* CC ID to assign at center  */
    sx, sy,                       /* for data indexing */
    pid[5] = {0, 0, 0, 0, 0},     /* cache of previous IDs */
    (*lup)(const void *, size_t), /* how to get integral data value */
    *out;                         /* output data */

  lup = nrrdUILookup[nin->type];
  /* these macro started innocently enough for the sake of brevity,
     but then expanded to include all the casting needed to silence warnings from
     -Wsign-conversion about implicit conversions across signednesses
     The macros are terrible examples of assuming various local variables,
     and yet they are used not just in this function but others in this file. */
#define LUP(D, I) (lup((D), AIR_SIZE_T(I)))
  out = AIR_CAST(unsigned int *, nout->data);
  sx = AIR_UINT(nin->axis[0].size);
  sy = AIR_UINT(nin->axis[1].size);
/* #define IOK(I, S)       (0 <= (I) && (I) < (S)) */
#define IOK(I, S)       (AIR_UINT(I) < (S))
#define VI2(XI, SX, YI) (AIR_INT(XI) + AIR_INT((SX) * (YI)))
  double noVal = 0.5; /* value different from any integer */
/* with GET_V_2(., yi-1) or GET_I_2(., yi-1) with yi==0 don't need IOK(yi, sy):
   yi-1 will wrapped around to big but still unsigned value, which always >= 0,
   hence may have some warnings about needless comparisons, but that big value
   will be >= sy, so we do want to test for that */
#define GET_V_2(XI, YI)                                                                 \
  ((IOK(XI, sx) && YI < sy) /* */                                                       \
     ? LUP(nin->data, VI2(XI, sx, YI))                                                  \
     : noVal)
#define GET_I_2(XI, YI)                                                                 \
  ((IOK(XI, sx) && YI < sy) /* */                                                       \
     ? out[VI2(XI, sx, YI)]                                                             \
     : UINT_MAX) /* ID different (fingers crossed) from any real CC ID */

  id = UINT_MAX;
  *numid = 0;
  for (unsigned int yi = 0; yi < sy; yi++) {
    for (unsigned int xi = 0; xi < sx; xi++) {
      unsigned int midx; /* index (into pvl, pid) of match with center pixel */
      if (nrrd__CC_verb) {
        fprintf(stderr, "%s(%u,%u) -----------\n", me, xi, yi);
      }
      /* clang-format off */
      if (xi == 0) { /* on left edge */
        pvl[1] = GET_V_2(-1, yi);    pid[1] = GET_I_2(-1, yi);
        pvl[2] = GET_V_2(-1, yi-1);  pid[2] = GET_I_2(-1, yi-1);
        pvl[3] = GET_V_2( 0, yi-1);  pid[3] = GET_I_2( 0, yi-1);

      } else { /* past left edge: shift known values */
        pvl[1] = vl;                 pid[1] = id;
        pvl[2] = pvl[3];             pid[2] = pid[3];
        pvl[3] = pvl[4];             pid[3] = pid[4];
      }
      pvl[4] = GET_V_2(xi+1, yi-1);  pid[4] = GET_I_2(xi+1, yi-1);
      /* clang-format on */
      vl = GET_V_2(xi, yi); /* get value at center */
      midx = 0;             /* initialize neighborhood search with: no match known */
      if (vl == pvl[1]) {   /* a value match at 1 means we re-use the ID at 1 */
        id = pid[midx = 1];
      }
#define TEST(P)                                                                         \
  if (vl == pvl[(P)]) {     /* we have a value match at P */                            \
    if (midx) {             /* but we already had a value match */                      \
      if (id != pid[(P)]) { /* so maybe learn new equivalence */                        \
        airEqvAdd(eqvArr, pid[(P)], id);                                                \
      }                                                                                 \
    } else { /* this is the first value match, re-used ID at P */                       \
      id = pid[midx = (P)];                                                             \
    }                                                                                   \
  }
      TEST(3);
      if (2 == conny) {
        TEST(2);
        TEST(4);
      }
      if (!midx) {
        /* value `vl` didn't match anything in `pvl`: create new CC ID */
        id = *numid;
        (*numid)++;
      }
      if (nrrd__CC_verb) {
        DECISEP_DECL(5);
        fprintf(stderr, "%s: pvl: %s %s %s %s (vl = %s)\n", me,
                airDtoS(Nb[0], pvl[1], Ds), airDtoS(Nb[1], pvl[2], Ds),
                airDtoS(Nb[2], pvl[3], Ds), airDtoS(Nb[3], pvl[4], Ds),
                airDtoS(Nb[4], vl, Ds));
        fprintf(stderr, "        pid: %u %u %u %u\n", pid[1], pid[2], pid[3], pid[4]);
        fprintf(stderr, "    --> midx = %u, id = %u, *numid = %u\n", midx, id, *numid);
      }
      out[xi + sx * yi] = id;
    }
  }

  return 0;
}

/*
 * Visualization of 3D 3x3x3 indices into previous value `pvl` and CC ID `pid` caches:
 *
 *          5    6    7 --> X
 *        8    9   10
 *     11   12   13
 *    /
 *   v Y    |
 *          2    3    4
 *        1    .    .
 *      .    .    .
 *          |
 *          v Z            as above, 0 index not used, so it can mean unknown/unset
 */
static int
nCCFind_3(Nrrd *nout, unsigned int *numid, airArray *eqvArr, const Nrrd *nin,
          unsigned int conny) {
  /* static const char me[] = "nCCFind_3" ; */
  double pvl[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, vl = 0;
  unsigned int id, *out, (*lup)(const void *, size_t),
    pid[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  unsigned int midx, sx, sy, sz;

  lup = nrrdUILookup[nin->type];
  out = AIR_CAST(unsigned int *, nout->data);
  sx = AIR_UINT(nin->axis[0].size);
  sy = AIR_UINT(nin->axis[1].size);
  sz = AIR_UINT(nin->axis[2].size);
#define VI3(XI, SX, YI, SY, ZI) (AIR_INT(XI) + AIR_INT(SX) * VI2(YI, SY, ZI))
  double noVal = 0.5; /* value different from any integer */
/* see comment for GET_V_2,GET_I_2 above about NOT using IOK with YI;
   same applies here to both YI and ZI */
#define GET_V_3(XI, YI, ZI)                                                             \
  ((IOK(XI, sx) && YI < sy && ZI < sz) /* */                                            \
     ? LUP(nin->data, VI3(XI, sx, YI, sy, ZI))                                          \
     : noVal)
#define GET_I_3(XI, YI, ZI)                                                             \
  ((IOK(XI, sx) && YI < sy && ZI < sz) /* */                                            \
     ? out[VI3(XI, sx, YI, sy, ZI)]                                                     \
     : UINT_MAX)

  id = UINT_MAX;
  *numid = 0;
  for (unsigned int zi = 0; zi < sz; zi++) {
    for (unsigned int yi = 0; yi < sy; yi++) {
      for (unsigned int xi = 0; xi < sx; xi++) {
        /* clang-format off */
        if (xi == 0) {
          pvl[ 1] = GET_V_3(  -1,   yi,   zi);  pid[ 1] = GET_I_3(  -1,   yi,   zi);
          pvl[ 2] = GET_V_3(  -1, yi-1,   zi);  pid[ 2] = GET_I_3(  -1, yi-1,   zi);
          pvl[ 3] = GET_V_3(   0, yi-1,   zi);  pid[ 3] = GET_I_3(   0, yi-1,   zi);
          pvl[ 5] = GET_V_3(  -1, yi-1, zi-1);  pid[ 5] = GET_I_3(  -1, yi-1, zi-1);
          pvl[ 8] = GET_V_3(  -1,   yi, zi-1);  pid[ 8] = GET_I_3(  -1,   yi, zi-1);
          pvl[11] = GET_V_3(  -1, yi+1, zi-1);  pid[11] = GET_I_3(  -1, yi+1, zi-1);
          pvl[ 6] = GET_V_3(   0, yi-1, zi-1);  pid[ 6] = GET_I_3(   0, yi-1, zi-1);
          pvl[ 9] = GET_V_3(   0,   yi, zi-1);  pid[ 9] = GET_I_3(   0,   yi, zi-1);
          pvl[12] = GET_V_3(   0, yi+1, zi-1);  pid[12] = GET_I_3(   0, yi+1, zi-1);
        } else {
          pvl[ 1] = vl;                         pid[ 1] = id;
          pvl[ 2] = pvl[ 3];                    pid[ 2] = pid[ 3];
          pvl[ 3] = pvl[ 4];                    pid[ 3] = pid[ 4];
          pvl[ 5] = pvl[ 6];                    pid[ 5] = pid[ 6];
          pvl[ 8] = pvl[ 9];                    pid[ 8] = pid[ 9];
          pvl[11] = pvl[12];                    pid[11] = pid[12];
          pvl[ 6] = pvl[ 7];                    pid[ 6] = pid[ 7];
          pvl[ 9] = pvl[10];                    pid[ 9] = pid[10];
          pvl[12] = pvl[13];                    pid[12] = pid[13];
        }
        pvl[ 4]   = GET_V_3(xi+1, yi-1,   zi);  pid[ 4] = GET_I_3(xi+1, yi-1,   zi);
        pvl[ 7]   = GET_V_3(xi+1, yi-1, zi-1);  pid[ 7] = GET_I_3(xi+1, yi-1, zi-1);
        pvl[10]   = GET_V_3(xi+1,   yi, zi-1);  pid[10] = GET_I_3(xi+1,   yi, zi-1);
        pvl[13]   = GET_V_3(xi+1, yi+1, zi-1);  pid[13] = GET_I_3(xi+1, yi+1, zi-1);
        vl = GET_V_3(xi, yi, zi);
        midx = 0;
        if (vl == pvl[1]) {
          id = pid[midx=1];
        }
        TEST(3);
        TEST(9);
        if (2 <= conny) {
          TEST(2); TEST(4);
          TEST(6); TEST(8); TEST(10); TEST(12);
          if (3 == conny) {
            TEST(5); TEST(7); TEST(11); TEST(13);
          }
        }
        /* clang-format on */
        if (!midx) {
          /* didn't match anything previous */
          id = *numid;
          (*numid)++;
        }
        out[xi + sx * (yi + sy * zi)] = id;
      }
    }
  }

  return 0;
}

static int /* Biff: 1 */
nCCFind_N(Nrrd *nout, unsigned int *numid, airArray *eqvArr, const Nrrd *nin,
          unsigned int conny) {
  static const char me[] = "nCCFind_N";

  AIR_UNUSED(nout);
  AIR_UNUSED(numid);
  AIR_UNUSED(eqvArr);
  AIR_UNUSED(nin);
  AIR_UNUSED(conny);
  biffAddf(NRRD, "%s: sorry, not implemented yet", me);
  return 1;
}

/*
 ******** nrrdCCFind
 *
 * finds connected components (CCs) in given integral type nrrd `nin`, according to
 * connectivity `conny`, putting the results in output `nout` with output type `type`.
 * If `type` == nrrdTypeDefault, the type used will be the smallest that can contain the
 * CC ID values.  Otherwise, the specified type `type` will be used (we assume that it is
 * large enough to hold the CC IDs).
 *
 * `conny` == the number of coordinates that need to varied together in order to reach
 * all the samples that are to consitute the neighborhood around a sample.  For 2-D,
 * conny==1 specifies the 4 edge-connected pixels, and 2 specifies the 8 edge- and
 * corner-connected pixels.
 *
 * The caller can get a record of the values in each CC by passing a non-NULL nval, which
 * will be allocated to an array of the same type as nin, so that nval->data[I] is the
 * value in nin inside CC #I.
 */
int /* Biff: 1 */
nrrdCCFind(Nrrd *nout, Nrrd **nvalP, const Nrrd *nin, int type, unsigned int conny) {
  static const char me[] = "nrrdCCFind", func[] = "ccfind";
  Nrrd *nfpid; /* first-pass IDs */
  airArray *mop, *eqvArr;
  unsigned int *fpid, numid, numsettleid, *map, (*lup)(const void *, size_t),
    (*ins)(void *, size_t, unsigned int);
  size_t I, NN;
  void *val;

  if (!(nout && nin)) {
    /* NULL nvalP okay */
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  if (nout == nin) {
    biffAddf(NRRD, "%s: nout == nin disallowed", me);
    return 1;
  }
  if (!(nrrdTypeIsIntegral[nin->type] && nrrdTypeIsUnsigned[nin->type]
        && nrrdTypeSize[nin->type] <= 4)) {
    biffAddf(NRRD,
             "%s: can only find connected components in "
             "1, 2, or 4 byte unsigned integral values (not %s)",
             me, airEnumStr(nrrdType, nin->type));
    return 1;
  }
  if (nrrdTypeDefault != type) {
    if (!(AIR_IN_OP(nrrdTypeUnknown, type, nrrdTypeLast))) {
      biffAddf(NRRD, "%s: got invalid target type %d", me, type);
      return 1;
    }
    if (nrrdTypeBlock == type) {
      biffAddf(NRRD, "%s: cannot output CCs in type %s", me, airEnumStr(nrrdType, type));
    }
    if (!(nrrdTypeIsIntegral[type] && nrrdTypeIsUnsigned[nin->type]
          && nrrdTypeSize[type] <= 4)) {
      biffAddf(NRRD,
               "%s: can only save connected components to 1, 2, or 4 byte "
               "unsigned integral values (not %s)",
               me, airEnumStr(nrrdType, type));
      return 1;
    }
  }
  if (!(conny <= nin->dim)) {
    biffAddf(NRRD,
             "%s: connectivity value must be in [1..%u] for %u-D "
             "data (not %u)",
             me, nin->dim, nin->dim, conny);
    return 1;
  }
  if (nrrdConvert(nfpid = nrrdNew(), nin, nrrdTypeUInt)) {
    biffAddf(NRRD, "%s: couldn't allocate fpid %s array to match input size", me,
             airEnumStr(nrrdType, nrrdTypeUInt));
    return 1;
  }

  mop = airMopNew();
  airMopAdd(mop, nfpid, nrrdNuke_mop, airMopAlways);
  eqvArr = airArrayNew(NULL, NULL, 2 * sizeof(unsigned int), nrrd__CC_EqvIncr);
  airMopAdd(mop, eqvArr, airArrayNuke_mop, airMopAlways);
  int ret = 0;
  switch (nin->dim) {
  case 1:
    ret = nCCFind_1(nfpid, &numid, nin);
    break;
  case 2:
    ret = nCCFind_2(nfpid, &numid, eqvArr, nin, conny);
    break;
  case 3:
    ret = nCCFind_3(nfpid, &numid, eqvArr, nin, conny);
    break;
  default:
    ret = nCCFind_N(nfpid, &numid, eqvArr, nin, conny);
    break;
  }
  if (ret) {
    biffAddf(NRRD, "%s: initial pass failed", me);
    airMopError(mop);
    return 1;
  }

  map = AIR_MALLOC(numid, unsigned int);
  airMopAdd(mop, map, airFree, airMopAlways);
  numsettleid = airEqvMap(eqvArr, map, numid);
  /* convert fpid values to final id values */
  fpid = (unsigned int *)(nfpid->data);
  NN = nrrdElementNumber(nfpid);
  for (I = 0; I < NN; I++) {
    fpid[I] = map[fpid[I]];
  }
  if (nvalP) {
    if (!(*nvalP)) {
      *nvalP = nrrdNew();
    }
    if (nrrdMaybeAlloc_va(*nvalP, nin->type, 1, AIR_SIZE_T(numsettleid))) {
      biffAddf(NRRD, "%s: couldn't allocate output value list", me);
      airMopError(mop);
      return 1;
    }
    airMopAdd(mop, nvalP, airSetNull_mop, airMopOnError);
    airMopAdd(mop, *nvalP, nrrdNuke_mop, airMopOnError);
    val = (*nvalP)->data;
    lup = nrrdUILookup[nin->type];
    ins = nrrdUIInsert[nin->type];
    /* I'm not sure if its more work to do all the redundant assignments
       or to check whether or not to do them */
    for (I = 0; I < NN; I++) {
      ins(val, fpid[I], lup(nin->data, I));
    }
  }

  if (nrrdTypeDefault != type) {
    if (numsettleid - 1 > nrrdTypeMax[type]) {
      biffAddf(NRRD, "%s: max cc id %u is too large to fit in output type %s", me,
               numsettleid - 1, airEnumStr(nrrdType, type));
      airMopError(mop);
      return 1;
    }
  } else {
    type = (numsettleid - 1 <= nrrdTypeMax[nrrdTypeUChar]
              ? nrrdTypeUChar
              : (numsettleid - 1 <= nrrdTypeMax[nrrdTypeUShort] ? nrrdTypeUShort
                                                                : nrrdTypeUInt));
  }
  if (nrrdConvert(nout, nfpid, type)) {
    biffAddf(NRRD, "%s: trouble converting to final output", me);
    airMopError(mop);
    return 1;
  }
  if (nrrdContentSet_va(nout, func, nin, "%s,%d", airEnumStr(nrrdType, type), conny)) {
    biffAddf(NRRD, "%s:", me);
    return 1;
  }
  if (nout != nin) {
    nrrdAxisInfoCopy(nout, nin, NULL, NRRD_AXIS_INFO_NONE);
  }
  /* basic info handled by nrrdConvert */
  /* but we can invalidate a field that is definitely no longer descriptive:
     oldMin and oldMax document a scalar value range prior to quantization.
     But here we no longer have anything like scalar values; we have CC IDs.
     oldMin and oldMax can't help usefully interpret the CC IDs */
  nout->oldMin = nout->oldMax = AIR_NAN;

  airMopOkay(mop);
  return 0;
}

static int
nCCAdj_1(unsigned char *out, unsigned int numid, const Nrrd *nin) {

  AIR_UNUSED(out);
  AIR_UNUSED(numid);
  AIR_UNUSED(nin);
  return 0;
}

/* For the `Adj` adjacency functions, nin is an array of integral CC IDs */
static int
nCCAdj_2(unsigned char *out, unsigned int numid, const Nrrd *nin, unsigned int conny) {
  unsigned int (*lup)(const void *, size_t), sx, sy,
    id = UINT_MAX, pid[5] = {0, 0, 0, 0, 0},
    noVal = UINT_MAX; /* for GET_V_2: different (we hope) from any CC ID */

  lup = nrrdUILookup[nin->type];
  sx = AIR_UINT(nin->axis[0].size);
  sy = AIR_UINT(nin->axis[1].size);
  for (unsigned int yi = 0; yi < sy; yi++) {
    for (unsigned int xi = 0; xi < sx; xi++) {
      if (!xi) {
        pid[1] = GET_V_2(-1, yi);
        pid[2] = GET_V_2(-1, yi - 1);
        pid[3] = GET_V_2(0, yi - 1);
      } else {
        pid[1] = id;
        pid[2] = pid[3];
        pid[3] = pid[4];
      }
      pid[4] = GET_V_2(xi + 1, yi - 1);
      id = GET_V_2(xi, yi);
#define TADJ(P)                                                                         \
  if (pid[(P)] != UINT_MAX && id != pid[(P)]) {                                         \
    /* ID at the center != ID at P, but they are adjacent, so record that */            \
    out[id + numid * pid[(P)]] = out[pid[(P)] + numid * id] = 1;                        \
  }
      TADJ(1);
      TADJ(3);
      if (2 == conny) {
        TADJ(2);
        TADJ(4);
      }
    }
  }

  return 0;
}

static int
nCCAdj_3(unsigned char *out, unsigned int numid, const Nrrd *nin, unsigned int conny) {
  unsigned int (*lup)(const void *, size_t), sx, sy, sz,
    id = UINT_MAX, pid[14] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    noVal = UINT_MAX; /* for GET_V_2: different (we hope) from any CC ID */

  lup = nrrdUILookup[nin->type];
  sx = AIR_UINT(nin->axis[0].size);
  sy = AIR_UINT(nin->axis[1].size);
  sz = AIR_UINT(nin->axis[2].size);
  for (unsigned int zi = 0; zi < sz; zi++) {
    for (unsigned int yi = 0; yi < sy; yi++) {
      for (unsigned int xi = 0; xi < sx; xi++) {
        /* clang-format off */
        if (!xi) {
          pid[ 1] = GET_V_3(-1,   yi,   zi);
          pid[ 2] = GET_V_3(-1, yi-1,   zi);
          pid[ 3] = GET_V_3( 0, yi-1,   zi);
          pid[ 5] = GET_V_3(-1, yi-1, zi-1);
          pid[ 8] = GET_V_3(-1,   yi, zi-1);
          pid[11] = GET_V_3(-1, yi+1, zi-1);
          pid[ 6] = GET_V_3( 0, yi-1, zi-1);
          pid[ 9] = GET_V_3( 0,   yi, zi-1);
          pid[12] = GET_V_3( 0, yi+1, zi-1);
        } else {
          pid[ 1] = id;
          pid[ 2] = pid[ 3];
          pid[ 3] = pid[ 4];
          pid[ 5] = pid[ 6];
          pid[ 8] = pid[ 9];
          pid[11] = pid[12];
          pid[ 6] = pid[ 7];
          pid[ 9] = pid[10];
          pid[12] = pid[13];
        }
        pid[ 4] = GET_V_3(xi+1, yi-1,   zi);
        pid[ 7] = GET_V_3(xi+1, yi-1, zi-1);
        pid[10] = GET_V_3(xi+1,   yi, zi-1);
        pid[13] = GET_V_3(xi+1, yi+1, zi-1);
        id = AIR_UINT(GET_V_3(xi, yi, zi));
        TADJ(1);
        TADJ(3);
        TADJ(9);
        if (2 <= conny) {
          TADJ(2); TADJ(4);
          TADJ(6); TADJ(8); TADJ(10); TADJ(12);
          if (3 == conny) {
            TADJ(5); TADJ(7); TADJ(11); TADJ(13);
          }
        }
        /* clang-format on */
      }
    }
  }

  return 0;
}

static int /* Biff: 1 */
nCCAdj_N(unsigned char *out, unsigned int numid, const Nrrd *nin, unsigned int conny) {
  static const char me[] = "nCCAdj_N";

  AIR_UNUSED(out);
  AIR_UNUSED(numid);
  AIR_UNUSED(nin);
  AIR_UNUSED(conny);
  biffAddf(NRRD, "%s: sorry, not implemented", me);
  return 1;
}

int /* Biff: 1 */
nrrdCCAdjacency(Nrrd *nout, const Nrrd *nin, unsigned int conny) {
  static const char me[] = "nrrdCCAdjacency", func[] = "ccadj";
  int ret;
  unsigned int maxid;
  unsigned char *out;

  if (!(nout && nrrdCCValid(nin))) {
    biffAddf(NRRD, "%s: invalid args", me);
    return 1;
  }
  if (nout == nin) {
    biffAddf(NRRD, "%s: nout == nin disallowed", me);
    return 1;
  }
  if (!(AIR_IN_CL(1, conny, nin->dim))) {
    biffAddf(NRRD,
             "%s: connectivity value must be in [1..%u] for %u-D "
             "data (not %u)",
             me, nin->dim, nin->dim, conny);
    return 1;
  }
  maxid = nrrdCCMax(nin);
  if (nrrdMaybeAlloc_va(nout, nrrdTypeUChar, 2, AIR_SIZE_T(maxid + 1),
                        AIR_SIZE_T(maxid + 1))) {
    biffAddf(NRRD, "%s: trouble allocating output", me);
    return 1;
  }
  out = (unsigned char *)(nout->data);

  switch (nin->dim) {
  case 1:
    ret = nCCAdj_1(out, maxid + 1, nin);
    break;
  case 2:
    ret = nCCAdj_2(out, maxid + 1, nin, conny);
    break;
  case 3:
    ret = nCCAdj_3(out, maxid + 1, nin, conny);
    break;
  default:
    ret = nCCAdj_N(out, maxid + 1, nin, conny);
    break;
  }
  if (ret) {
    biffAddf(NRRD, "%s: trouble", me);
    return 1;
  }
  /* this goofiness is just so that histo-based projections
     return the sorts of values that we expect */
  nout->axis[0].center = nout->axis[1].center = nrrdCenterCell;
  nout->axis[0].min = nout->axis[1].min = -0.5;
  nout->axis[0].max = nout->axis[1].max = maxid + 0.5;
  if (nrrdContentSet_va(nout, func, nin, "%d", conny)) {
    biffAddf(NRRD, "%s:", me);
    return 1;
  }

  return 0;
}

/*
 ******** nrrdCCMerge
 *
 * Slightly-too-multi-purpose tool for merging small connected components (CCs) into
 * larger ones, according to a number of possible different constraints, as explained
 * below.
 *
 * `valDir`: (value direction) uses information about the original values in the CC to
 * constrain whether darker gets merged into brighter, or vice versa, or neither.  For
 * non-zero `valDir` values, a non-NULL _nval (from nrrdCCFind) must be passed:
 *   valDir > 0 : merge dark CCs into bright, but not vice versa
 *   valDir = 0 : merge either way, values are irrelevant
 *   valDir < 0 : merge bright CCs into dark, but not vice versa
 * When merging with multiple neighbors (maxNeighbor > 1), the value of the largest
 * neighbor is considered.
 *
 * `maxSize`: a cap on how big "small" is- CCs any larger than `maxSize` are not merged,
 * as they are deemed too significant.  Or, a `maxSize` of 0 says size is no object for
 * merging CCs.
 *
 * `maxNeighbor`: a maximum number of neighbors that a CC can have (either bigger than
 * the CC or not) if it is to be merged.  Use 1 to merge isolated islands into their
 * surrounds, 2 to merge CC with the larger of their two neighbors, etc., or 0 to allow
 * any number of neighbors.
 *
 * `conny`: passed to nrrdCCAdjacency() when determining neighbors
 *
 * In order to prevent weirdness, the merging done in one call to this function is not
 * transitive: if A is merged to B, then B will not be merged to anything else, even if
 * meets all the requirements defined by the given parameters.  This is accomplished by
 * working from the smallest CCs to the largest. Iterated calls may be needed to acheive
 * the desired effect.
 *
 * Note: the output of this is not "settled"- the CC id values are not shiftward
 * downwards to their lowest possible values, since this would needlessly invalidate the
 * nval value store.
 */
int /* Biff: 1 */
nrrdCCMerge(Nrrd *nout, const Nrrd *nin, Nrrd *_nval, int valDir, unsigned int maxSize,
            unsigned int maxNeighbor, unsigned int conny) {
  static const char me[] = "nrrdCCMerge", func[] = "ccmerge";
  const char *valcnt;
  unsigned int _i, i, j, bigi = 0, numid, *size, *sizeId, *nn, /* number of neighbors */
                                                            *val = NULL, *hit,
                         (*lup)(const void *, size_t),
                         (*ins)(void *, size_t, unsigned int);
  Nrrd *nadj, *nsize, *nval = NULL, *nnn;
  unsigned char *adj;
  unsigned int *map, *id;
  airArray *mop;
  size_t I, NN;

  mop = airMopNew();
  if (!(nout && nrrdCCValid(nin))) {
    /* _nval can be NULL */
    biffAddf(NRRD, "%s: invalid args", me);
    airMopError(mop);
    return 1;
  }
  if (valDir) {
    airMopAdd(mop, nval = nrrdNew(), nrrdNuke_mop, airMopAlways);
    if (nrrdConvert(nval, _nval, nrrdTypeUInt)) {
      biffAddf(NRRD, "%s: value-directed merging needs usable nval", me);
      airMopError(mop);
      return 1;
    }
    val = (unsigned int *)(nval->data);
  }
  if (nout != nin) {
    if (nrrdCopy(nout, nin)) {
      biffAddf(NRRD, "%s:", me);
      airMopError(mop);
      return 1;
    }
  }
  airMopAdd(mop, nadj = nrrdNew(), nrrdNuke_mop, airMopAlways);
  airMopAdd(mop, nsize = nrrdNew(), nrrdNuke_mop, airMopAlways);
  airMopAdd(mop, nnn = nrrdNew(), nrrdNuke_mop, airMopAlways);

  if (nrrdCCSize(nsize, nin)
      || nrrdCopy(nnn, nsize) /* just to allocate to right size and type */
      || nrrdCCAdjacency(nadj, nin, conny)) {
    biffAddf(NRRD, "%s:", me);
    airMopError(mop);
    return 1;
  }
  size = (unsigned int *)(nsize->data);
  adj = (unsigned char *)(nadj->data);
  nn = (unsigned int *)(nnn->data);
  numid = AIR_UINT(nsize->axis[0].size);
  for (i = 0; i < numid; i++) {
    nn[i] = 0;
    for (j = 0; j < numid; j++) {
      nn[i] += adj[j + numid * i];
    }
  }
  map = AIR_MALLOC(numid, unsigned int);
  id = AIR_MALLOC(numid, unsigned int);
  hit = AIR_MALLOC(numid, unsigned int);
  sizeId = AIR_MALLOC(2 * numid, unsigned int);
  /* we add to the mops BEFORE error checking so that anything non-NULL
     will get airFree'd, and happily airFree is a no-op on NULL */
  airMopAdd(mop, map, airFree, airMopAlways);
  airMopAdd(mop, id, airFree, airMopAlways);
  airMopAdd(mop, hit, airFree, airMopAlways);
  airMopAdd(mop, sizeId, airFree, airMopAlways);
  if (!(map && id && hit && sizeId)) {
    biffAddf(NRRD, "%s: couldn't allocate buffers", me);
    airMopError(mop);
    return 1;
  }

  /* store and sort size/id pairs */
  for (i = 0; i < numid; i++) {
    sizeId[0 + 2 * i] = size[i];
    sizeId[1 + 2 * i] = i;
  }
  qsort(sizeId, numid, 2 * sizeof(unsigned int), nrrdValCompare[nrrdTypeUInt]);
  for (i = 0; i < numid; i++) {
    id[i] = sizeId[1 + 2 * i];
  }

  /* initialize arrays */
  for (i = 0; i < numid; i++) {
    map[i] = i;
    hit[i] = AIR_FALSE;
  }
  /* _i goes through 0 to numid-1,
     i goes through the CC ids in ascending order of size */
  for (_i = 0; _i < numid; _i++) {
    i = id[_i];
    if (hit[i]) {
      continue;
    }
    if (maxSize && (size[i] > maxSize)) {
      continue;
    }
    if (maxNeighbor && (nn[i] > maxNeighbor)) {
      continue;
    }
    /* find biggest neighbor, exploiting the fact that we already
       sorted CC ids on size.  j descends through indices of id[],
       bigi goes through CC ids which are larger than CC i */
    for (j = numid - 1; j > _i; j--) {
      bigi = id[j];
      if (adj[bigi + numid * i]) break;
    }
    if (j == _i) {
      continue; /* we had no neighbors ?!?! */
    }
    if (valDir && (AIR_INT(val[bigi]) - AIR_INT(val[i])) * valDir < 0) {
      continue;
    }
    /* else all criteria for merging have been met */
    map[i] = bigi;
    hit[bigi] = AIR_TRUE;
  }
  lup = nrrdUILookup[nin->type];
  ins = nrrdUIInsert[nout->type];
  NN = nrrdElementNumber(nin);
  for (I = 0; I < NN; I++) {
    ins(nout->data, I, map[lup(nin->data, I)]);
  }

  valcnt = ((_nval && _nval->content) ? _nval->content : nrrdStateUnknownContent);
  if ((valDir
       && nrrdContentSet_va(nout, func, nin, "%c(%s),%d,%d,%d", (valDir > 0 ? '+' : '-'),
                            valcnt, maxSize, maxNeighbor, conny))
      || (!valDir
          && nrrdContentSet_va(nout, func, nin, ".,%d,%d,%d", maxSize, maxNeighbor,
                               conny))) {
    biffAddf(NRRD, "%s:", me);
    airMopError(mop);
    return 1;
  }
  /* basic info handled by nrrdCopy */
  airMopOkay(mop);
  return 0;
}

/*
 ******** nrrdCCRevalue()
 *
 * assigns the original values back to the connected components obviously, this could be
 * subsumed by nrrdApply1DLut(), but this is so special purpose that it seemed simpler to
 * code from scratch
 */
int /* Biff: 1 */
nrrdCCRevalue(Nrrd *nout, const Nrrd *nin, const Nrrd *nval) {
  static const char me[] = "nrrdCCRevalue";
  size_t I, NN;
  unsigned int (*vlup)(const void *, size_t), (*ilup)(const void *, size_t),
    (*ins)(void *, size_t, unsigned int);

  if (!(nout && nrrdCCValid(nin) && nval)) {
    biffAddf(NRRD, "%s: invalid args", me);
    return 1;
  }
  if (nrrdConvert(nout, nin, nval->type)) {
    biffAddf(NRRD, "%s: couldn't initialize output", me);
    return 1;
  }
  NN = nrrdElementNumber(nin);
  vlup = nrrdUILookup[nval->type];
  ilup = nrrdUILookup[nin->type];
  ins = nrrdUIInsert[nout->type];
  for (I = 0; I < NN; I++) {
    ins(nout->data, I, vlup(nval->data, ilup(nin->data, I)));
  }
  /* basic info handled by nrrdConvert */

  return 0;
}

int /* Biff: 1 */
nrrdCCSettle(Nrrd *nout, Nrrd **nvalP, const Nrrd *nin) {
  static const char me[] = "nrrdCCSettle", func[] = "ccsettle";
  unsigned int numid, maxid, jd, id, *map, (*lup)(const void *, size_t),
    (*ins)(void *, size_t, unsigned int);
  size_t I, NN;
  airArray *mop;

  mop = airMopNew();
  if (!(nout && nrrdCCValid(nin))) {
    /* nvalP can be NULL */
    biffAddf(NRRD, "%s: invalid args", me);
    airMopError(mop);
    return 1;
  }
  if (nrrdCopy(nout, nin)) {
    biffAddf(NRRD, "%s: initial copy failed", me);
    airMopError(mop);
    return 1;
  }
  maxid = nrrdCCMax(nin);
  lup = nrrdUILookup[nin->type];
  ins = nrrdUIInsert[nin->type];
  NN = nrrdElementNumber(nin);
  map = AIR_CALLOC(maxid + 1, unsigned int); /* we do need it zeroed out */
  if (!map) {
    biffAddf(NRRD, "%s: couldn't allocate internal LUT", me);
    airMopError(mop);
    return 1;
  }
  airMopAdd(mop, map, airFree, airMopAlways);
  for (I = 0; I < NN; I++) {
    map[lup(nin->data, I)] = 1;
  }
  numid = 0;
  for (jd = 0; jd <= maxid; jd++) {
    numid += map[jd];
  }

  if (nvalP) {
    if (!(*nvalP)) {
      *nvalP = nrrdNew();
    }
    if (nrrdMaybeAlloc_va(*nvalP, nin->type, 1, AIR_SIZE_T(numid))) {
      biffAddf(NRRD, "%s: couldn't allocate output value list", me);
      airMopError(mop);
      return 1;
    }
    airMopAdd(mop, nvalP, airSetNull_mop, airMopOnError);
    airMopAdd(mop, *nvalP, nrrdNuke_mop, airMopOnError);
  }

  id = 0;
  for (jd = 0; jd <= maxid; jd++) {
    if (map[jd]) {
      map[jd] = id;
      if (nvalP) {
        ins((*nvalP)->data, id, jd);
      }
      id++;
    }
  }
  for (I = 0; I < NN; I++) {
    ins(nout->data, I, map[lup(nin->data, I)]);
  }

  if (nrrdContentSet_va(nout, func, nin, "")) {
    biffAddf(NRRD, "%s:", me);
    airMopError(mop);
    return 1;
  }
  /* basic info handled by nrrdCopy */
  airMopOkay(mop);
  return 0;
}
