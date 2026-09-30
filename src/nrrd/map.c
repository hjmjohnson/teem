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
#include <assert.h>

static int /* Biff: 1 */
clampRoundConvert(Nrrd *nout, const Nrrd *nin, int type, int doClamp, int roundDir) {
  static const char me[] = "clampRoundConvert";
  char typeS[AIR_STRLEN_SMALL + 1];
  size_t num, size[NRRD_DIM_MAX];

  if (!(nin && nout && !nrrdCheck(nin) && !airEnumValCheck(nrrdType, type))) {
    biffAddf(NRRD, "%s: invalid args", me);
    return 1;
  }
  if (nin->type == nrrdTypeBlock || type == nrrdTypeBlock) {
    biffAddf(NRRD, "%s: can't convert to or from nrrd type %s", me,
             airEnumStr(nrrdType, nrrdTypeBlock));
    return 1;
  }
  if (nout == nin && nrrdTypeSize[type] != nrrdTypeSize[nin->type]) {
    biffAddf(NRRD, "%s: nout==nin but input,output type sizes unequal", me);
    return 1;
  }
  if (nrrdStateDisallowIntegerNonExist && !nrrdTypeIsIntegral[nin->type]
      && nrrdTypeIsIntegral[type]) {
    /* there's a risk of non-existent values getting converted to
       non-sensical integral values */
    if (nrrdHasNonExist(nin)) {
      biffAddf(NRRD,
               "%s: can't convert to integral values (%s) with "
               "non-existent values in input",
               me, airEnumStr(nrrdType, type));
      return 1;
    }
  }

  /* if we're actually converting to the same type, just do a copy */
  if (type == nin->type) {
    if (nout == nin) {
      /* nout == nin is allowed if the input and output type are
         of the same size, which will certainly be the case if the
         input and output types are identical, so there's actually
         no work to do */
    } else {
      if (nrrdCopy(nout, nin)) {
        biffAddf(NRRD, "%s: couldn't copy input to output", me);
        return 1;
      }
    }
  } else {
    /* allocate space if necessary */
    nrrdAxisInfoGet_nva(nin, nrrdAxisInfoSize, size);
    /* MUST be nrrdMaybeAlloc_nva (not nrrd Alloc_nva) because we allow
       nout==nin if type sizes match */
    if (nrrdMaybeAlloc_nva(nout, type, nin->dim, size)) {
      biffAddf(NRRD, "%s: failed to allocate output", me);
      return 1;
    }

    /* call the appropriate converter */
    num = nrrdElementNumber(nin);
    if (roundDir) {
      nrrd__CastClampRound[nout->type][nin->type](nout->data, nin->data, num, doClamp,
                                                  roundDir);
    } else if (doClamp) {
      nrrd__ClampConv[nout->type][nin->type](nout->data, nin->data, num);
    } else {
      nrrd__Conv[nout->type][nin->type](nout->data, nin->data, num);
    }
    nout->blockSize = 0;

    /* copy peripheral information */
    nrrdAxisInfoCopy(nout, nin, NULL, NRRD_AXIS_INFO_NONE);
    snprintf(typeS, sizeof(typeS), "(%s)", airEnumStr(nrrdType, nout->type));
    if (nrrdContentSet_va(nout, typeS, nin, "")) {
      biffAddf(NRRD, "%s:", me);
      return 1;
    }
    /* the min and max have probably changed if there was a conversion
       to integral values, or to a lower precision representation */
    if (nrrdBasicInfoCopy(nout, nin,
                          NRRD_BASIC_INFO_DATA_BIT /* */
                            | NRRD_BASIC_INFO_TYPE_BIT | NRRD_BASIC_INFO_BLOCKSIZE_BIT
                            | NRRD_BASIC_INFO_DIMENSION_BIT | NRRD_BASIC_INFO_CONTENT_BIT
                            | NRRD_BASIC_INFO_COMMENTS_BIT
                            | (nrrdStateKeyValuePairsPropagate
                                 ? 0
                                 : NRRD_BASIC_INFO_KEYVALUEPAIRS_BIT))) {
      biffAddf(NRRD, "%s:", me);
      return 1;
    }
  }
  return 0;
}

/*
******** nrrdConvert()
**
** copies values from one type of nrrd to another, without any
** transformation, except what you get with a cast.  The point is to
** make available on Nrrds the exact same behavior as you have in C
** with casts and assignments.
*/
int /* Biff: 1 */
nrrdConvert(Nrrd *nout, const Nrrd *nin, int type) {
  static const char me[] = "nrrdConvert";

  if (clampRoundConvert(nout, nin, type, AIR_FALSE /* clamp */, 0 /* round */)) {
    biffAddf(NRRD, "%s: trouble", me);
    return 1;
  }
  return 0;
}

/*
******** nrrdClampConvert()
**
** same as nrrdConvert, but with clamping to output value representation range
** so as to avoid wrap-around artifacts
**
** HEY: WARNING: may have loss of data when processing long long
** (either signed or unsigned)
*/
int /* Biff: 1 */
nrrdClampConvert(Nrrd *nout, const Nrrd *nin, int type) {
  static const char me[] = "nrrdClampConvert";

  if (clampRoundConvert(nout, nin, type, AIR_TRUE /* clamp */, 0 /* round */)) {
    biffAddf(NRRD, "%s: trouble", me);
    return 1;
  }
  return 0;
}

/*
******** nrrdCastClampRound()
**
** For (maybe) doing rounding to integer, then (maybe) clamping
** and then casting.  The maybe-ness of rounding and clamping
** in this function is inconsistent with the certainty of clamping
** nrrdClampConvert, so this function name may be regretted.
**
** NOTE! Rounding is not performed when outType is for float
** or double!  That logic is implemented here.
**
** And warning same as above:
** HEY: WARNING: may have loss of data when processing long long
** (either signed or unsigned)
*/
int /* Biff: 1 */
nrrdCastClampRound(Nrrd *nout, const Nrrd *nin, int outType, int doClamp, int roundDir) {
  static const char me[] = "nrrdCastClampRound";

  if (clampRoundConvert(nout, nin, outType, doClamp,
                        nrrdTypeIsIntegral[outType] ? roundDir : 0)) {
    biffAddf(NRRD, "%s: trouble", me);
    return 1;
  }
  return 0;
}

/*
******** nrrdQuantize()
**
** convert values to 8, 16, or 32 bit unsigned quantities
** by mapping the value range delimited by the nrrd's min
** and max to the representable range
**
** NOTE: for the time being, this uses a "double" as the intermediate
** value holder, which may mean needless loss of precision
*/
int /* Biff: 1 */
nrrdQuantize(Nrrd *nout, const Nrrd *nin, const NrrdRange *_range, unsigned int bits) {
  static const char me[] = "nrrdQuantize", func[] = "quantize";
  double valIn, minIn, maxIn, eps;
  int type = nrrdTypeUnknown;
  size_t I, num, size[NRRD_DIM_MAX];
  unsigned char *outUC;
  unsigned short *outUS;
  unsigned int *outUI;
  airArray *mop;
  NrrdRange *range;

  if (!(nin && nout)) {
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  if (nrrdTypeBlock == nin->type) {
    biffAddf(NRRD, "%s: can't quantize type %s", me,
             airEnumStr(nrrdType, nrrdTypeBlock));
    return 1;
  }

  /* determine nrrd type from number of bits */
  switch (bits) {
  case 8:
    type = nrrdTypeUChar;
    break;
  case 16:
    type = nrrdTypeUShort;
    break;
  case 32:
    type = nrrdTypeUInt;
    break;
  default:
    biffAddf(NRRD, "%s: bits has to be 8, 16, or 32 (not %u)", me, bits);
    return 1;
  }
  if (nout == nin && nrrdTypeSize[type] != nrrdTypeSize[nin->type]) {
    biffAddf(NRRD, "%s: nout==nin but input,output type sizes unequal", me);
    return 1;
  }
  mop = airMopNew();
  if (_range) {
    range = nrrdRangeCopy(_range);
    nrrdRangeSafeSet(range, nin, nrrdBlind8BitRangeState);
  } else {
    range = nrrdRangeNewSet(nin, nrrdBlind8BitRangeState);
  }
  airMopAdd(mop, range, nrrdRangeNix_mop, airMopAlways);
  if (nrrdStateDisallowIntegerNonExist && range->hasNonExist) {
    biffAddf(NRRD,
             "%s: can't quantize non-existent values "
             "(NaN, +/-inf)",
             me);
    airMopError(mop);
    return 1;
  }

  /* allocate space if necessary */
  nrrdAxisInfoGet_nva(nin, nrrdAxisInfoSize, size);
  /* MUST be nrrdMaybeAlloc_nva (not nrrd Alloc_nva) because we allow
     nout==nin if type sizes match */
  if (nrrdMaybeAlloc_nva(nout, type, nin->dim, size)) {
    biffAddf(NRRD, "%s: failed to create output", me);
    airMopError(mop);
    return 1;
  }

  /* the skinny */
  num = nrrdElementNumber(nin);
  minIn = range->min;
  maxIn = range->max;
  eps = (minIn == maxIn ? 1.0 : 0.0);
  outUC = (unsigned char *)nout->data;
  outUS = (unsigned short *)nout->data;
  outUI = (unsigned int *)nout->data;
  switch (bits) {
  case 8:
    for (I = 0; I < num; I++) {
      valIn = nrrdDLookup[nin->type](nin->data, I);
      outUC[I] = AIR_UCHAR(airIndexClamp(minIn, valIn, maxIn + eps, 1 << 8));
    }
    break;
  case 16:
    for (I = 0; I < num; I++) {
      valIn = nrrdDLookup[nin->type](nin->data, I);
      outUS[I] = AIR_USHORT(airIndexClamp(minIn, valIn, maxIn + eps, 1 << 16));
    }
    break;
  case 32:
    for (I = 0; I < num; I++) {
      valIn = nrrdDLookup[nin->type](nin->data, I);
      outUI[I] = AIR_UINT(airIndexClampULL(minIn, valIn, maxIn + eps,
                                           AIR_ULLONG(1) << 32));
    }
    break;
  }

  /* set information in new volume */
  if (nout != nin) {
    nrrdAxisInfoCopy(nout, nin, NULL, NRRD_AXIS_INFO_NONE);
  }
  if (nrrdContentSet_va(nout, func, nin, "%d", bits)) {
    biffAddf(NRRD, "%s:", me);
    airMopError(mop);
    return 1;
  }
  if (nrrdBasicInfoCopy(nout, nin,
                        NRRD_BASIC_INFO_DATA_BIT /* */
                          | NRRD_BASIC_INFO_TYPE_BIT | NRRD_BASIC_INFO_BLOCKSIZE_BIT
                          | NRRD_BASIC_INFO_DIMENSION_BIT | NRRD_BASIC_INFO_CONTENT_BIT
                          | NRRD_BASIC_INFO_OLDMIN_BIT | NRRD_BASIC_INFO_OLDMAX_BIT
                          | NRRD_BASIC_INFO_COMMENTS_BIT
                          | (nrrdStateKeyValuePairsPropagate
                               ? 0
                               : NRRD_BASIC_INFO_KEYVALUEPAIRS_BIT))) {
    biffAddf(NRRD, "%s:", me);
    airMopError(mop);
    return 1;
  }
  nout->oldMin = minIn;
  nout->oldMax = maxIn;
  nout->blockSize = 0;

  airMopOkay(mop);
  return 0;
}

/*
** typeNumberOfValues[]
**
** only meaningful for integral values, and only correct for
** 32-bit values; tells the number of different integral values that
** can be represented by the type
**
** HEY: this used to be public, but that was stopped when it was clear
** that only the following function was using the array.  The reason
** for the array is questionable, and the implementation below
** should be re-evaluated.
*/
static const double typeNumberOfValues[NRRD_TYPE_MAX + 1] = {
  0,                           /* unknown */
  UCHAR_MAX + 1,               /* char */
  UCHAR_MAX + 1,               /* unsigned char */
  USHRT_MAX + 1,               /* short */
  USHRT_MAX + 1,               /* unsigned short */
  (double)UINT_MAX + 1,        /* int */
  (double)UINT_MAX + 1,        /* unsigned int */
  (double)NRRD_ULLONG_MAX + 1, /* long long */
  (double)NRRD_ULLONG_MAX + 1, /* unsigned long long */
  0,                           /* float */
  0,                           /* double */
  0                            /* punt */
};

/*
******** nrrdUnquantize()
**
** try to recover floating point values from a quantized nrrd,
** using the oldMin and oldMax values, if they exist.  If they
** don't exist, the output range will be 0.0 to 1.0.  However,
** because we're using NRRD_CELL_POS to recover values,
** the output values will never be exactly 0.0 to 1.0 (or oldMin
** to oldMax).  In unsigned char data, for instance, the value
** V will be mapped to:
** NRRD_CELL_POS(0.0, 1.0, 256, V) ==
** AIR_AFFINE(0, V + 0.5, 256, 0.0, 1.0) ==
** ((double)(1.0)-(0.0))*((double)(V+0.5)-(0))/((double)(256)-(0)) + (0.0) ==
** (1.0)*(V+0.5) / (256.0) + (0.0) ==
** (V+0.5)/256
** so a 0 will be mapped to 1/512 = 0.00195
*/
int /* Biff: 1 */
nrrdUnquantize(Nrrd *nout, const Nrrd *nin, int type) {
  static const char me[] = "nrrdUnquantize", func[] = "unquantize";
  float *outF;
  double *outD, minIn, numValIn, minOut, maxOut, valIn;
  size_t NN, II, size[NRRD_DIM_MAX];

  if (!(nout && nin)) {
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  if (airEnumValCheck(nrrdType, type)) {
    biffAddf(NRRD, "%s: don't recognize type %d\n", me, type);
    return 1;
  }
  if (!(type == nrrdTypeFloat || type == nrrdTypeDouble)) {
    biffAddf(NRRD, "%s: output type must be %s or %s (not %s)", me,
             airEnumStr(nrrdType, nrrdTypeFloat), airEnumStr(nrrdType, nrrdTypeDouble),
             airEnumStr(nrrdType, type));
    return 1;
  }
  if (nrrdTypeBlock == nin->type) {
    biffAddf(NRRD, "%s: can't unquantize type %s", me,
             airEnumStr(nrrdType, nrrdTypeBlock));
    return 1;
  }
  if (!nrrdTypeIsIntegral[nin->type]) {
    biffAddf(NRRD, "%s: can only unquantize integral types, not %s", me,
             airEnumStr(nrrdType, nin->type));
    return 1;
  }
  if (nout == nin && nrrdTypeSize[type] != nrrdTypeSize[nin->type]) {
    biffAddf(NRRD, "%s: nout==nin but input,output type sizes unequal", me);
    return 1;
  }

  nrrdAxisInfoGet_nva(nin, nrrdAxisInfoSize, size);
  if (nrrdMaybeAlloc_nva(nout, type, nin->dim, size)) {
    biffAddf(NRRD, "%s: failed to create output", me);
    return 1;
  }
  minIn = nrrdTypeMin[nin->type];
  numValIn = typeNumberOfValues[nin->type];
  if (AIR_EXISTS(nin->oldMin) && AIR_EXISTS(nin->oldMax)) {
    minOut = nin->oldMin;
    maxOut = nin->oldMax;
  } else {
    minOut = 0.0;
    maxOut = 1.0;
  }
  outF = (float *)nout->data;
  outD = (double *)nout->data;
  NN = nrrdElementNumber(nin);
  switch (type) {
  case nrrdTypeFloat:
    for (II = 0; II < NN; II++) {
      valIn = minIn + nrrdDLookup[nin->type](nin->data, II);
      outF[II] = AIR_FLOAT(NRRD_CELL_POS(minOut, maxOut, numValIn, valIn));
    }
    break;
  case nrrdTypeDouble:
    for (II = 0; II < NN; II++) {
      valIn = minIn + nrrdDLookup[nin->type](nin->data, II);
      outD[II] = NRRD_CELL_POS(minOut, maxOut, numValIn, valIn);
    }
    break;
  }

  /* set information in new volume */
  if (nout != nin) {
    nrrdAxisInfoCopy(nout, nin, NULL, NRRD_AXIS_INFO_NONE);
  }
  if (nrrdContentSet_va(nout, func, nin, "")) {
    biffAddf(NRRD, "%s:", me);
    return 1;
  }
  if (nrrdBasicInfoCopy(nout, nin,
                        NRRD_BASIC_INFO_DATA_BIT /* */
                          | NRRD_BASIC_INFO_TYPE_BIT | NRRD_BASIC_INFO_BLOCKSIZE_BIT
                          | NRRD_BASIC_INFO_DIMENSION_BIT | NRRD_BASIC_INFO_CONTENT_BIT
                          | NRRD_BASIC_INFO_OLDMIN_BIT | NRRD_BASIC_INFO_OLDMAX_BIT
                          | NRRD_BASIC_INFO_COMMENTS_BIT
                          | (nrrdStateKeyValuePairsPropagate
                               ? 0
                               : NRRD_BASIC_INFO_KEYVALUEPAIRS_BIT))) {
    biffAddf(NRRD, "%s:", me);
    return 1;
  }
  nout->oldMin = nout->oldMax = AIR_NAN;
  nout->blockSize = 0;
  return 0;
}

/*
** histoEqCompareSteady()
**
** used by nrrdHistoEq in smart mode to sort the "steady" array
** in _descending_ order
*/
static int
histoEqCompareSteady(const void *a_, const void *b_) {
  const uint *a = a_, *b = b_;
  return AIR_INT(*b) - AIR_INT(*a);
}

/*
 ******** nrrdHistoEq()
 *
 * performs histogram equalization on given nrrd, treating it as a big one-dimensional
 * array.  The procedure is as follows:
 * - create a histogram of nrrd (using "bins" bins)
 * - integrate the histogram, and normalize and shift this so it is a monotonically
 *   increasing function from min to max, where (min,max) is the value range in the nrrd
 * - map the values in the nrrd through the adjusted histogram integral
 *
 * If the histogram of the given nrrd is already as flat as can be, the histogram
 * integral will increase linearly, and the adjusted histogram integral should be close
 * to the identity function, so the values shouldn't change much.
 *
 * If the nhistP arg is non-NULL, then it is set to point to the histogram that was used
 * for calculation. Otherwise this histogram is deleted on return.
 *
 * This is all that is done normally, when `smart == 0`.  In smart mode (activated
 * by setting `smart` to something greater than 0), the histogram is analyzed during its
 * creation to detect if there are a few bins which keep getting hit with the same value
 * over and over.  It may be desirable to ignore these bins in the histogram integral
 * because they may not contain any useful information, and so they should not effect how
 * values are re-mapped.  The value of `smart` is the number of bins that will be
 * ignored.  For instance, use the value 1 if the problem with naive histogram
 * equalization is a large amount of background (which is exactly one fixed value).
 * NOTE that this is a hack in the sense that it is sensitive to the order in which the
 * nin values are traversed to make the histogram: constant values along the fastest axis
 * scanlines can be recognized (with `smart==1`), but not if the same array's axes are
 * swapped.
 */
int /* Biff: 1 */
nrrdHistoEq(Nrrd *nout, const Nrrd *nin, Nrrd **nmapP, unsigned int bins,
            unsigned int smart, float amount) {
  static const char me[] = "nrrdHistoEq", func[] = "heq";
  if (!(nout && nin)) {
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  if (nrrdTypeBlock == nin->type) {
    biffAddf(NRRD, "%s: can't histogram equalize type %s", me,
             airEnumStr(nrrdType, nrrdTypeBlock));
    return 1;
  }
  if (!(bins > 4)) {
    biffAddf(NRRD, "%s: need # bins > 4 (not %u)", me, bins);
    return 1;
  }
  /* we start by simply copying, because the only thing we're changing is the values
     themselves, and all peripheral information is unchanged by this value remapping */
  if (nout != nin) {
    if (nrrdCopy(nout, nin)) {
      biffAddf(NRRD, "%s:", me);
      return 1;
    }
  }
#define VERBUG 0
  airArray *mop = airMopNew();
  if (nmapP) {
    airMopAdd(mop, nmapP, airSetNull_mop, airMopOnError);
  }
  size_t num = nrrdElementNumber(nin);
  uint *hist, *steady = NULL, *respect = NULL;
  double min, max, *last = NULL;
  Nrrd *nhist = nrrdNew();
  airMopAdd(mop, nhist, nrrdNuke_mop, airMopAlways);
  if (smart <= 0) {
    if (nrrdHisto(nhist, nin, NULL, NULL, bins, nrrdTypeUInt)) {
      biffAddf(NRRD, "%s: failed to create histogram", me);
      airMopError(mop);
      return 1;
    }
    hist = (uint *)nhist->data;
    min = nhist->axis[0].min;
    max = nhist->axis[0].max;
    if (min == max) {
      /* like the "don't crash out" code below: min == max just means its
      a constant array, and trying to do HEQ on that should not be an error.
      If we didn't return here, we'd hit:
         [nrrd] nrrdApply1DRegMap:
         [nrrd] apply1DSetUp: (axis 0) domain min (0) not less than max (0)
      which would likely be confusing for the user. */
      airMopOkay(mop);
      return 0;
    }
  } else {
    /* for "smart" mode, we have to basically reimplement nrrdHisto, plus do some extra
       work while creating the histogram to look for bins incessantly hit with the exact
       same value */
    if (nrrdMaybeAlloc_va(nhist, nrrdTypeUInt, 1, AIR_SIZE_T(bins))) {
      biffAddf(NRRD, "%s: failed to allocate histogram", me);
      airMopError(mop);
      return 1;
    }
    hist = (uint *)nhist->data;
    nhist->axis[0].size = bins;
    /* allocate the respect, steady, and last arrays */
    assert(respect = AIR_MALLOC(bins, uint));
    assert(steady = AIR_MALLOC(2 * bins, uint));
    assert(last = AIR_MALLOC(bins, double));
    airMopMem(mop, &respect, airMopAlways);
    airMopMem(mop, &steady, airMopAlways);
    airMopMem(mop, &last, airMopAlways);
    if (!(respect && steady && last)) {
      biffAddf(NRRD, "%s: couldn't allocate smart arrays", me);
      airMopError(mop);
      return 1;
    }
    /* steady[0 + 2*bii] == how many times has bin bii seen the same value
                            i.e. last[bii]
       steady[1 + 2*bii] == bii (steady will be rearranged by qsort()) */
    for (uint bii = 0; bii < bins; bii++) {
      last[bii] = AIR_NAN;
      respect[bii] = 1u; // initialized to respect all bins
      steady[0 + 2 * bii] = 0;
      steady[1 + 2 * bii] = bii;
    }
    /* now create the histogram */
    NrrdRange *range = nrrdRangeNewSet(nin, nrrdBlind8BitRangeState);
    airMopAdd(mop, range, nrrdRangeNix_mop, airMopAlways);
    if (range->min == range->max) {
      /* No, don't crash out.  It's fine; HEQ should just be a no-op in this case.
       * biffAddf(NRRD,
       *          "%s: invalid min and max in nrrd.  "
       *          "Min and max are equivalent (min,max = %g).",
       *          me, range->min);
       * airMopError(mop);
       * return 1;
       */
      airMopOkay(mop);
      return 0;
    }
    min = range->min;
    max = range->max;
    for (size_t I = 0; I < num; I++) {
      double val = nrrdDLookup[nin->type](nin->data, I);
      if (AIR_EXISTS(val)) {
        uint idx = airIndex(min, val, max, bins);
        ++hist[idx];
        /* pre-TeemV2 code:
         * if (AIR_EXISTS(last[idx])) {
         *   steady[0 + 2 * idx] = (last[idx] == val ? 1 + steady[0 + 2 * idx] : 0);
         * } */
        if (last[idx] == val) { // which implies last[idx] exists
          steady[0 + 2 * idx]++;
        }
        /* no else! Pre-TeemV2, there was effectively: steady[0 + 2 * idx] = 0
           here, which meant that all memory of an incessantly hit bin is lost if
           any different value lands in the bin, which decreased the effectiveness
           of all this "smart" processing.  Still, the *first* value to hit this
           bin sets the precedent: if the first value is a value that later recurs
           exactly, then we will (as intended) ignore occasional deviations from it
           that also hit hit bin.  But we are not storing a history of values hitting
           this bin and taking the mode, so we are not magically invariant w.r.t
           reorderings of the input values: if it was a deviant who hit this bin
           first, we will not record how often a constant value hit it later. The
           pre-TeemV2 code was also not invariant w.r.t re-orderings: a periodic
           deviant would limit the steady count to that period, instead of letting
           it accumulate over the whole array. */
        last[idx] = val;
      }
    }
    if (VERBUG) {
      for (uint ti = 0; ti < bins; ti++) {
        fprintf(stderr, "%s: pre-sort [%3u]: last=%g, steady=[%u %u]\n", __func__, ti,
                last[ti], steady[0 + 2 * ti], steady[1 + 2 * ti]);
      }
    }
    /* now sort the steady array; note the "2 *" */
    qsort(steady, bins, 2 * sizeof(uint), histoEqCompareSteady);
    /* we ignore some of the bins according to "smart" arg */
    for (uint bii = 0; bii < smart; bii++) {
      if (steady[0 + 2 * bii] > num / 200) {
        /* NEW for TeemV2: it should be harmless to turn on smart mode, if no bin was
          ever hit incessantly. Old code disrespected bin `bii` even if steady[0 + 2*bii]
          was 0, which is dumb. So now (at the cost of adding a magic number) we only
          disrespect bin `bii` if steady[0 + 2*bii] is more than 0.5% of the number of
          values in the whole array (also note the other TeemV2 change above in the logic
          of how steady is computed). This only makes a significant improvement to the
          extent that `smart` is large: if it was only 1, respecting-vs-disrespecting
          one bin would be a small change. But the idea is that setting `smart=20` should
          not have an effect if there is no region of constant value. */
        respect[steady[1 + 2 * bii]] = 0;
      }
      if (VERBUG) {
        fprintf(stderr, "%s: disrespecting bin %d\n", me, steady[1 + 2 * bii]);
      }
    }
    if (VERBUG) {
      for (uint ti = 0; ti < bins; ti++) {
        fprintf(stderr, "%s: postsort [%3u]: steady=[%u %u] respect=%u\n", __func__, ti,
                steady[0 + 2 * ti], steady[1 + 2 * ti], respect[ti]);
      }
    }
  }

  Nrrd *nmap = nrrdNew();
  airMopAdd(mop, nmap, nrrdNuke_mop, nmapP ? airMopOnError : airMopAlways);
  if (nrrdMaybeAlloc_va(nmap, nrrdTypeDouble, 1, AIR_SIZE_T(bins + 1))) {
    biffAddf(NRRD, "%s: failed to create map nrrd", me);
    airMopError(mop);
    return 1;
  }
  double *ycoord = AIR_CAST(double *, nmap->data);
  nmap->axis[0].min = min;
  nmap->axis[0].max = max;

  /* integrate the histogram then normalize it */
  /* TeemV2 fixes a long-standing problem in how "smart" was implemented: the ignored
     (disrespected) bin caused a wripple in the value map. An ideal demo for "smart" is a
     linear ramp -- interrupted by a brief flat segment -- sent through "unu gamma -g 2".
     "unu heq -s 1" should be able to recover the linear ramp with its flat histogram,
     while ignoring the spiked bin because of the flat segment. But the old code actually
     ended up producing similar results with "unu heq -s 0" and "unu heq -s 1": in both
     cases the output histogram had a depression around the spike. The long-standing
     cleverness (as of svn r676 in 2002-08-16) of trying to avoid discontinuities in the
     integrated histogram (caused by smartness ignoring some bins) either didn't work
     or made things worse, so that has been removed below. Instead, the simple strategy
     is: when you are ignoring a histogram bin in the context of computing the
     histogram integral, pretend that the ignored bin as the same count as the last
     non-ignored bin.  No need for a histogram integral post-process. */
  uint respectedBinCount = 0;
  for (uint bii = 0; bii <= bins; bii++) {
    if (bii == 0) {
      ycoord[bii] = 0;
    } else {
      uint incr;
      if (!smart || /* else we are smart and */ respect[bii - 1]) {
        incr = hist[bii - 1];
      } else {
        // smart and !respect[bii - 1]
        incr = respectedBinCount;
      }
      ycoord[bii] = ycoord[bii - 1] + incr;
    }
    if (smart && bii < bins && respect[bii]) {
      // remember a respected bin count for next iter
      respectedBinCount = hist[bii];
    }
    if (VERBUG) {
      fprintf(stderr, "%s: (initial) ycoord[%u] = %g\n", me, bii, ycoord[bii]);
    }
  }
  // (removed attempted cleverness to avoid integrated histo discontinuities)
  /* rescale the histogram integration to span the original
     value range, and affect the influence of "amount" */
  for (uint bii = 0; bii <= bins; bii++) {
    ycoord[bii] = AIR_AFFINE(0.0, ycoord[bii], ycoord[bins], min, max);
    ycoord[bii] = AIR_AFFINE(0.0, amount, 1.0, AIR_AFFINE(0, bii, bins, min, max),
                             ycoord[bii]);
  }
  if (VERBUG) {
    for (uint bii = 0; bii <= bins; bii++) {
      fprintf(stderr, "%s: (rescale) ycoord[%u] = %g\n", me, bii, ycoord[bii]);
    }
  }
  if (!AIR_EXISTS(ycoord[bins])) {
    /* TeemV2 fixes a long-standing bug here: if the smartness ended up ignoring all the
       bins that any any counts, then the ycoord[] array that started as all 0.0 will go
       all NaN, and then user will be confused by:
          [nrrd] nrrdApply1DRegMap:
          [nrrd] apply1DSetUp: regular map nrrd has non-existent values
       Like with the min==max cases: just call it done and return */
    airMopOkay(mop);
    return 0;
  }

  /* map the nrrd values through the normalized histogram integral */
  if (nrrdApply1DRegMap(nout, nin, NULL, nmap, nin->type, AIR_FALSE)) {
    biffAddf(NRRD, "%s: problem remapping", me);
    airMopError(mop);
    return 1;
  }

  /* if user is interested, set pointer to map nrrd,
     otherwise it will be nixed by airMop */
  if (nmapP) {
    *nmapP = nmap;
  }

  /* fiddling with content is the only thing we'll do */
  if (nrrdContentSet_va(nout, func, nin, "%d,%d", bins, smart)) {
    biffAddf(NRRD, "%s:", me);
    airMopError(mop);
    return 1;
  }
  if (nrrdBasicInfoCopy(nout, nin,
                        NRRD_BASIC_INFO_DATA_BIT /* */
                          | NRRD_BASIC_INFO_TYPE_BIT | NRRD_BASIC_INFO_BLOCKSIZE_BIT
                          | NRRD_BASIC_INFO_DIMENSION_BIT | NRRD_BASIC_INFO_CONTENT_BIT
                          | NRRD_BASIC_INFO_COMMENTS_BIT
                          | (nrrdStateKeyValuePairsPropagate
                               ? 0
                               : NRRD_BASIC_INFO_KEYVALUEPAIRS_BIT))) {
    biffAddf(NRRD, "%s:", me);
    airMopError(mop);
    return 1;
  }
#undef VERBUG
  airMopOkay(mop);
  return 0;
}
