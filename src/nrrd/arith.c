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

/* These two functions probably logically belong more in dye than nrrd, oh well.
   Having them here allows them to power nrrdArithSRGBGamma, which is called with
   by "unu gamma" with "-g srgb" or "-g 1/srgb". And that much color computation
   can probably stay in nrrd. TeemV2 also makes unrrdu depend on dye (like it also
   depends on moss for "unu ilk") to support color space conversions in unu */
double /* Biff: nope */
nrrdSRGBGamma(double val) {
  return val <= 0.0031308 ? 12.92 * val : 1.055 * pow(val, 1 / 2.4) - 0.055;
}
double /* Biff: nope */
nrrdSRGBGammaInverse(double val) {
  return val <= 0.04045 ? val / 12.92 : pow((val + 0.055) / 1.055, 2.4);
}

/*
******** nrrdArithGamma()
**
** map the values in a nrrd through a power function; essentially:
** val = pow(val, 1/gamma), but this is after the val has been normalized
** to be in the range of 0.0 to 1.0 (assuming that the given min and
** max really are the full range of the values in the nrrd).  Thus,
** the given min and max values are fixed points of this
** transformation.  Using a negative gamma means that after the pow()
** function has been applied, the value is inverted with respect to
** min and max (like in xv).
*/
int /* Biff: 1 */
nrrdArithGamma(Nrrd *nout, const Nrrd *nin, const NrrdRange *_range, double Gamma) {
  static const char me[] = "nrrdArithGamma", func[] = "gamma";
  double val, min, max;
  size_t I, num;
  NrrdRange *range;
  airArray *mop;
  double (*lup)(const void *, size_t);
  double (*ins)(void *, size_t, double);

  if (!(nout && nin)) {
    /* _range can be NULL */
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  if (!(AIR_EXISTS(Gamma))) {
    biffAddf(NRRD, "%s: gamma doesn't exist", me);
    return 1;
  }
  if (!(nrrdTypeBlock != nin->type && nrrdTypeBlock != nout->type)) {
    biffAddf(NRRD, "%s: can't deal with %s type", me,
             airEnumStr(nrrdType, nrrdTypeBlock));
    return 1;
  }
  if (nout != nin) {
    if (nrrdCopy(nout, nin)) {
      biffAddf(NRRD, "%s: couldn't initialize by copy to output", me);
      return 1;
    }
  }
  mop = airMopNew();
  if (_range) {
    range = nrrdRangeCopy(_range);
    nrrdRangeSafeSet(range, nin, nrrdBlind8BitRangeState);
  } else {
    range = nrrdRangeNewSet(nin, nrrdBlind8BitRangeTrue);
  }
  airMopAdd(mop, range, nrrdRangeNix_mop, airMopAlways);
  min = range->min;
  max = range->max;
  if (min == max) {
    /* this is stupid.  We want min < max to avoid making NaNs */
    max += 1;
  }
  lup = nrrdDLookup[nin->type];
  ins = nrrdDInsert[nout->type];
  Gamma = 1 / Gamma;
  num = nrrdElementNumber(nin);
  if (Gamma < 0.0) {
    Gamma = -Gamma;
    for (I = 0; I < num; I++) {
      val = lup(nin->data, I);
      val = AIR_AFFINE(min, val, max, 0.0, 1.0);
      val = pow(val, Gamma);
      val = AIR_AFFINE(1.0, val, 0.0, min, max);
      ins(nout->data, I, val);
    }
  } else {
    for (I = 0; I < num; I++) {
      val = lup(nin->data, I);
      val = AIR_AFFINE(min, val, max, 0.0, 1.0);
      val = pow(val, Gamma);
      val = AIR_AFFINE(0.0, val, 1.0, min, max);
      ins(nout->data, I, val);
    }
  }
  if (nrrdContentSet_va(nout, func, nin, "%g,%g,%g", min, max, Gamma)) {
    biffAddf(NRRD, "%s:", me);
    airMopError(mop);
    return 1;
  }
  if (nout != nin) {
    nrrdAxisInfoCopy(nout, nin, NULL, NRRD_AXIS_INFO_NONE);
  }
  /* basic info handled by nrrdCopy above */

  airMopOkay(mop);
  return 0;
}

/*
******** nrrdArithSRGBGamma()
**
** (HEY lots of copy-paste from nrrdArithGamma); map the values in a nrrd
** through either the ~2.2 gamma correction for sRGB (when "forward" is
** non-zero), or its ~0.455 gamma inverse (when "forward" is zero).  Unlike
** nrrdArithGamma, this is does not support inverting values.
*/
int /* Biff: 1 */
nrrdArithSRGBGamma(Nrrd *nout, const Nrrd *nin, const NrrdRange *_range, int forward) {
  static const char me[] = "nrrdArithSRGBGamma", func[] = "sRGBgamma";
  double val, min, max;
  size_t I, num;
  NrrdRange *range;
  airArray *mop;
  double (*lup)(const void *, size_t);
  double (*ins)(void *, size_t, double);

  if (!(nout && nin)) {
    /* _range can be NULL */
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  if (!(nrrdTypeBlock != nin->type && nrrdTypeBlock != nout->type)) {
    biffAddf(NRRD, "%s: can't deal with %s type", me,
             airEnumStr(nrrdType, nrrdTypeBlock));
    return 1;
  }
  if (nout != nin) {
    if (nrrdCopy(nout, nin)) {
      biffAddf(NRRD, "%s: couldn't initialize by copy to output", me);
      return 1;
    }
  }
  mop = airMopNew();
  if (_range) {
    range = nrrdRangeCopy(_range);
    nrrdRangeSafeSet(range, nin, nrrdBlind8BitRangeState);
  } else {
    range = nrrdRangeNewSet(nin, nrrdBlind8BitRangeTrue);
  }
  airMopAdd(mop, range, nrrdRangeNix_mop, airMopAlways);
  min = range->min;
  max = range->max;
  if (min == max) {
    /* this is stupid.  We want min < max to avoid making NaNs */
    max += 1;
  }
  lup = nrrdDLookup[nin->type];
  ins = nrrdDInsert[nout->type];
  num = nrrdElementNumber(nin);
  for (I = 0; I < num; I++) {
    val = lup(nin->data, I);
    val = AIR_AFFINE(min, val, max, 0.0, 1.0);
    if (forward) {
      val = nrrdSRGBGamma(val);
    } else {
      val = nrrdSRGBGammaInverse(val);
    }
    val = AIR_AFFINE(0.0, val, 1.0, min, max);
    ins(nout->data, I, val);
  }
  if (nrrdContentSet_va(nout, func, nin, "%g,%g,%s", min, max,
                        forward ? "forw" : "back")) {
    biffAddf(NRRD, "%s:", me);
    airMopError(mop);
    return 1;
  }
  if (nout != nin) {
    nrrdAxisInfoCopy(nout, nin, NULL, NRRD_AXIS_INFO_NONE);
  }
  /* basic info handled by nrrdCopy above */

  airMopOkay(mop);
  return 0;
}

/* ---------------------------- unary -------------- */
/* clang-format off */
static double nunaryOpNegative(double a)   {return -a;}
static double nunaryOpReciprocal(double a) {return 1.0/a;}
static double nunaryOpSin(double a)        {return sin(a);}
static double nunaryOpCos(double a)        {return cos(a);}
static double nunaryOpTan(double a)        {return tan(a);}
static double nunaryOpAsin(double a)       {return asin(a);}
static double nunaryOpAcos(double a)       {return acos(a);}
static double nunaryOpAtan(double a)       {return atan(a);}
static double nunaryOpExp(double a)        {return exp(a);}
static double nunaryOpLog(double a)        {return log(a);}
static double nunaryOpLog2(double a)       {return log(a)/0.69314718;}
static double nunaryOpLog10(double a)      {return log10(a);}
/* clang-format on */
/* This code for log1p and expm1 comes from
   http://www.plunk.org/~hatch/rightway.html which in turn references
   http://www.cs.berkeley.edu/~wkahan/Math128/Sumnfp.pdf from the
   great Kahan of IEEE 754 fame, but sadly that URL no longer works
   (though the Math128 directory is still there, as are other documents) */
static double
nunaryOpLog1p(double a) {
  double b;

  b = 1.0 + a;
  if (b == 1.0) {
    /* "a" was so close to zero that we had underflow when adding to 1,
       resulting in something that is exactly equal to 1.  So, use the
       first term of Taylor expansion of log(x+1) around 0 == x */
    return a;
  }
  /* else "a" was not so near zero; but GLK doesn't fully grasp
     the design of this expression */
  return log(b) * a / (b - 1);
}
static double
nunaryOpExpm1(double x) {
  double u;

  u = exp(x);
  if (u == 1.0) {
    /* "x" was so close to 0.0 that exp return exactly 1; subtracting
       1 from this will give a constant for a range of x's.  Instead,
       use the Taylor expansion of exp(x)-1 around 0 == x */
    return x;
  } else if (u - 1.0 == -1.0) {
    /* "x" was close enough to -inf that exp returned something so close
       to 0 that subtracting 1 resulted in exactly -1; return that */
    return -1.0;
  }
  /* else "x" was neither near 0.0 or -inf, but GLK doesn't fully grasp
     the design of this expression */
  return (u - 1.0) * x / log(u);
}
/* clang-format off */
static double nunaryOpSqrt(double a)       {return sqrt(a);}
static double nunaryOpCbrt(double a)       {return airCbrt(a);}
static double nunaryOpErf(double a)        {return airErf(a);}
static double nunaryOpNerf(double a)       {return (1+airErf(a))/2;}
static double nunaryOpCeil(double a)       {return ceil(a);}
static double nunaryOpFloor(double a)      {return floor(a);}
static double nunaryOpRoundUp(double a)    {return AIR_ROUNDUP(a);}
static double nunaryOpRoundDown(double a)  {return AIR_ROUNDDOWN(a);}
static double nunaryOpAbs(double a)        {return AIR_ABS(a);}
static double nunaryOpSgn(double a) { return (a < 0.0 ? -1 : (a > 0.0 ? 1 : 0));}
static double nunaryOpExists(double a)     {return AIR_EXISTS(a);}
/* clang-format on */
static double
nunaryOpRand(double a) {
  AIR_UNUSED(a);
  return airDrandMT();
}
static double
nunaryOpNormalRand(double a) {
  double v;
  AIR_UNUSED(a);
  airNormalRand(&v, NULL);
  return v;
}
static double
nunaryOpIf(double a) {
  return (a != 0 ? 1 : 0);
}
static double
nunaryOpZero(double a) {
  AIR_UNUSED(a);
  return 0.0;
}
static double
nunaryOpOne(double a) {
  AIR_UNUSED(a);
  return 1.0;
}
static double
nunaryOpTauOfSigma(double s) {
  return airTauOfSigma(s);
}
static double
nunaryOpSigmaOfTau(double t) {
  return airSigmaOfTau(t);
}

static double (*const nunaryOp[NRRD_UNARY_OP_MAX + 1])(double) = {
  NULL,           nunaryOpNegative,   nunaryOpReciprocal, nunaryOpSin,   nunaryOpCos,
  nunaryOpTan,    nunaryOpAsin,       nunaryOpAcos,       nunaryOpAtan,  nunaryOpExp,
  nunaryOpLog,    nunaryOpLog2,       nunaryOpLog10,      nunaryOpLog1p, nunaryOpExpm1,
  nunaryOpSqrt,   nunaryOpCbrt,       nunaryOpErf,        nunaryOpNerf,  nunaryOpCeil,
  nunaryOpFloor,  nunaryOpRoundUp,    nunaryOpRoundDown,  nunaryOpAbs,   nunaryOpSgn,
  nunaryOpExists, nunaryOpRand,       nunaryOpNormalRand, nunaryOpIf,    nunaryOpZero,
  nunaryOpOne,    nunaryOpTauOfSigma, nunaryOpSigmaOfTau};

int /* Biff: 1 */
nrrdArithUnaryOp(Nrrd *nout, int op, const Nrrd *nin) {
  static const char me[] = "nrrdArithUnaryOp";
  size_t N, I;
  int size[NRRD_DIM_MAX];
  double (*insert)(void *v, size_t I, double d), (*lookup)(const void *v, size_t I),
    (*uop)(double), val;

  if (!(nout && nin)) {
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  if (nrrdTypeBlock == nin->type) {
    biffAddf(NRRD, "%s: can't operate on type %s", me,
             airEnumStr(nrrdType, nrrdTypeBlock));
    return 1;
  }
  if (airEnumValCheck(nrrdUnaryOp, op)) {
    biffAddf(NRRD, "%s: unary op %d invalid", me, op);
    return 1;
  }
  if (nout != nin) {
    if (nrrdCopy(nout, nin)) {
      biffAddf(NRRD, "%s:", me);
      return 1;
    }
  }
  nrrdAxisInfoGet_nva(nin, nrrdAxisInfoSize, size);
  uop = nunaryOp[op];

  N = nrrdElementNumber(nin);
  lookup = nrrdDLookup[nin->type];
  insert = nrrdDInsert[nin->type];
  for (I = 0; I < N; I++) {
    val = lookup(nin->data, I);
    insert(nout->data, I, uop(val));
  }
  if (nrrdContentSet_va(nout, airEnumStr(nrrdUnaryOp, op), nin, "")) {
    biffAddf(NRRD, "%s:", me);
    return 1;
  }
  nrrdBasicInfoInit(nout,
                    NRRD_BASIC_INFO_ALL
                      ^ (NRRD_BASIC_INFO_OLDMIN_BIT | NRRD_BASIC_INFO_OLDMAX_BIT));
  return 0;
}

/* ---------------------------- binary -------------- */
/* clang-format off */
static double nbinaryOpAdd(double a, double b)       {return a + b;}
static double nbinaryOpSubtract(double a, double b)  {return a - b;}
static double nbinaryOpMultiply(double a, double b)  {return a * b;}
static double nbinaryOpDivide(double a, double b)    {return a / b;}
static double nbinaryOpPow(double a, double b)       {return pow(a,b);}
static double nbinaryOpSgnPow(double a, double b)  {return airSgnPow(a,b);}
static double nbinaryOpFlippedSgnPow(double a, double b)  {return airFlippedSgnPow(a,b);}
static double nbinaryOpMod(double a, double b)  { return AIR_MOD((int)a,(int)b);}
static double nbinaryOpFmod(double a, double b)      {return fmod(a,b);}
static double nbinaryOpAtan2(double a, double b)     {return atan2(a,b);}
static double nbinaryOpMin(double a, double b)       {return AIR_MIN(a,b);}
static double nbinaryOpMax(double a, double b)       {return AIR_MAX(a,b);}
static double nbinaryOpLT(double a, double b)        {return (a < b);}
static double nbinaryOpLTE(double a, double b)       {return (a <= b);}
static double nbinaryOpGT(double a, double b)        {return (a > b);}
static double nbinaryOpGTE(double a, double b)       {return (a >= b);}
static double nbinaryOpCompare(double a, double b) {
                                               return (a < b ? -1 : (a > b ? 1 : 0));}
static double nbinaryOpEqual(double a, double b)     {return (a == b);}
static double nbinaryOpNotEqual(double a, double b)  {return (a != b);}
static double nbinaryOpExists(double a, double b)  {return (AIR_EXISTS(a)
                                                                ? a : b);}
static double nbinaryOpIf(double a, double b)        {return (a != 0 ? a : b);}
/* clang-format on */
static double
nbinaryOpNormalRandScaleAdd(double a, double b) {
  double v;
  airNormalRand(&v, NULL);
  return a + b * v;
}
static double
nbinaryOpRicianRand(double a, double b) {
  double vr, vi, rr, ri;
  airNormalRand(&rr, &ri);
  vr = a + b * rr;
  vi = b * ri;
  return sqrt(vr * vr + vi * vi);
}

/*
  The following is lifted from GLK's UChicago CMSC 23710 SciVis class code,
  and repackaged here as a nrrdBinaryOp so it can be conveniently accessed via
  unu. In its original context, it is used for grading single-precision
  floating point ("float") values generatd by student code, by measuring
  distance to reference code, and in that setting, the type of the output
  distance value was a 32-bit unsigned int.  The simplistic limitations of
  nrrdBinaryOp, however, is that they are defined on doubles, and return
  doubles, and everything else is cast around as needed. This is a bummer, but
  it is so much more convenient to re-use unu's existing infrastructure for
  handling nrrds and constants.

  This tries to compute the distance between two floats, in units of ULPs.
  When the two floats are both finite and have the same sign, this is easy: it
  really is just the difference beween the values interpreted as ints.  This
  function also handles the messier cases of having two finite values of
  opposite sign, or one or two non-finite values.
*/
/* clamps vv to mm*7/8 with two linear ramps */
#define QLAMP(vv, mm) ((vv) < (mm) / 2 ? (vv) : (mm) / 2 + 3 * ((vv) - (mm) / 2) / 4)
static double
nbinaryOpULPDistance(double dA, double dB) {
  float A, B;
  int Anf, Bnf;
  airFloat FA, FB;
  double ret = 0; /* is always set, but compiler might still warn */
  unsigned int maxd, Ai, Bi;
  A = AIR_CAST(float, dA);
  B = AIR_CAST(float, dB);
  Anf = !AIR_EXISTS(A);
  Bnf = !AIR_EXISTS(B);
  /* biggest finite value is with expo just shy of all 1s, and frac field all
     1s: ((2^8 - 2) << 23) + 2^23 - 1 == 2139095039 > 2000000000. But, if A
     and B have this big magnitude with opposite signs, so diff can be twice
     that. Choose a round number for visual recogition. */
  maxd = 4000000000;
  switch (Anf + Bnf) {
  case 0:
    /* both values finite; computing distance is straightforward */
    FA.f = fabsf(A);
    FB.f = fabsf(B);
    Ai = FA.i;
    Bi = FB.i;
    if ((A >= 0) == (B >= 0)) {
      /* easy case: two finite values of same sign */
      unsigned int diff = Ai > Bi ? Ai - Bi : Bi - Ai;
      ret = QLAMP(diff, maxd);
    } else {
      /* harder: two finite values of different sign. The
         density of values around zero makes this a little goofy,
         hence the very adhoc /2 to lessen the difference */
      ret = QLAMP(Ai / 2, maxd / 2) + QLAMP(Bi / 2, maxd / 2);
    }
    break;
  case 1:
    /* only one non-finite value: say simply they're very different */
    ret = maxd;
    break;
  case 2:
    /* two non-finite values */
    if (airIsNaN(A) && airIsNaN(B)) {
      ret = 0; /* really, no meaningful difference between 2 NaNs */
    } else if (airIsNaN(A) || airIsNaN(B)) {
      /* one NaN and one inf, which seems like a big difference */
      ret = maxd;
    } else {
      /* two infs */
      if ((A > 0) == (B > 0)) {
        /* two infs of same sign; call them equal */
        ret = 0;
      } else {
        /* two infs of different sign: very different */
        ret = maxd;
      }
    }
    break;
  }
  return ret;
}
#undef QLAMP
/* end of code lifted from GLK's UChicago CMSC 23710 SciVis class code */

static double (*const nbinaryOp[NRRD_BINARY_OP_MAX + 1])(double, double) = {
  NULL,
  nbinaryOpAdd,
  nbinaryOpSubtract,
  nbinaryOpMultiply,
  nbinaryOpDivide,
  nbinaryOpPow,
  nbinaryOpSgnPow,
  nbinaryOpFlippedSgnPow,
  nbinaryOpMod,
  nbinaryOpFmod,
  nbinaryOpAtan2,
  nbinaryOpMin,
  nbinaryOpMax,
  nbinaryOpLT,
  nbinaryOpLTE,
  nbinaryOpGT,
  nbinaryOpGTE,
  nbinaryOpCompare,
  nbinaryOpEqual,
  nbinaryOpNotEqual,
  nbinaryOpExists,
  nbinaryOpIf,
  nbinaryOpNormalRandScaleAdd,
  nbinaryOpRicianRand,
  /* for these 3, the clamping is actually done by the caller */
  nbinaryOpAdd,      /* for nrrdBinaryOpAddClamp */
  nbinaryOpSubtract, /* for nrrdBinaryOpSubtractClamp */
  nbinaryOpMultiply, /* for nrrdBinaryOpMultiplyClamp */
  nbinaryOpULPDistance,
};

/*
******** nrrdArithBinaryOp
**
** this is a simplified version of nrrdArithIterBinaryOp, written after
** that, in a hurry, to operate directly on two nrrds, instead of with
** the NrrdIter nonsense
*/
int /* Biff: 1 */
nrrdArithBinaryOp(Nrrd *nout, int op, const Nrrd *ninA, const Nrrd *ninB) {
  static const char me[] = "nrrdArithBinaryOp";
  char *contA, *contB;
  size_t N, I, size[NRRD_DIM_MAX];
  double (*ins)(void *v, size_t I, double d), (*clmp)(double d),
    (*lupA)(const void *v, size_t I), (*lupB)(const void *v, size_t I),
    (*bop)(double a, double b), valA, valB;

  if (!(nout && !nrrdCheck(ninA) && !nrrdCheck(ninB))) {
    biffAddf(NRRD, "%s: NULL pointer or invalid args", me);
    return 1;
  }
  if (nrrdTypeBlock == ninA->type || nrrdTypeBlock == ninB->type) {
    biffAddf(NRRD, "%s: can't operate on type %s", me,
             airEnumStr(nrrdType, nrrdTypeBlock));
    return 1;
  }
  if (!nrrdSameSize(ninA, ninB, AIR_TRUE)) {
    biffAddf(NRRD, "%s: size mismatch between arguments", me);
    return 1;
  }
  if (airEnumValCheck(nrrdBinaryOp, op)) {
    biffAddf(NRRD, "%s: binary op %d invalid", me, op);
    return 1;
  }

  nrrdAxisInfoGet_nva(ninA, nrrdAxisInfoSize, size);
  if (!(nout == ninA || nout == ninB)) {
    if (nrrd__MaybeAllocMaybeZero_nva(nout, ninA->type, ninA->dim, size,
                                      AIR_FALSE /* zero when no realloc */)) {
      biffAddf(NRRD, "%s: couldn't allocate output nrrd", me);
      return 1;
    }
    if (nrrdAxisInfoCopy(nout, ninA, NULL, NRRD_AXIS_INFO_NONE)) {
      biffAddf(NRRD, "%s:", me);
      return 1;
    }
    nrrdBasicInfoCopy(nout, ninA,
                      (NRRD_BASIC_INFO_DATA_BIT | NRRD_BASIC_INFO_TYPE_BIT
                       | NRRD_BASIC_INFO_DIMENSION_BIT | NRRD_BASIC_INFO_CONTENT_BIT
                       | NRRD_BASIC_INFO_COMMENTS_BIT
                       | (nrrdStateKeyValuePairsPropagate
                            ? 0
                            : NRRD_BASIC_INFO_KEYVALUEPAIRS_BIT)));
  }
  nrrdBasicInfoInit(nout,
                    NRRD_BASIC_INFO_ALL
                      ^ (NRRD_BASIC_INFO_OLDMIN_BIT | NRRD_BASIC_INFO_OLDMAX_BIT));
  bop = nbinaryOp[op];

  N = nrrdElementNumber(ninA);
  lupA = nrrdDLookup[ninA->type];
  lupB = nrrdDLookup[ninB->type];
  ins = nrrdDInsert[nout->type];
  if (nrrdBinaryOpAddClamp == op || nrrdBinaryOpSubtractClamp == op
      || nrrdBinaryOpMultiplyClamp == op) {
    clmp = nrrdDClamp[nout->type];
  } else {
    clmp = NULL;
  }
  for (I = 0; I < N; I++) {
    double tmp;
    /* HEY: there is a loss of precision issue here with 64-bit ints */
    valA = lupA(ninA->data, I);
    valB = lupB(ninB->data, I);
    tmp = bop(valA, valB);
    if (clmp) {
      tmp = clmp(tmp);
    }
    ins(nout->data, I, tmp);
  }

  contA = nrrd__ContentGet(ninA);
  contB = nrrd__ContentGet(ninB);
  if (nrrd__ContentSet_va(nout, airEnumStr(nrrdBinaryOp, op), contA, "%s", contB)) {
    biffAddf(NRRD, "%s:", me);
    free(contA);
    free(contB);
    return 1;
  }
  free(contA);
  free(contB);
  return 0;
}

int /* Biff: 1 */
nrrdArithIterBinaryOpSelect(Nrrd *nout, int op, NrrdIter *inA, NrrdIter *inB,
                            unsigned int which) {
  static const char me[] = "nrrdArithIterBinaryOpSelect";
  char *contA, *contB;
  size_t N, I, size[NRRD_DIM_MAX];
  int type;
  double (*insert)(void *v, size_t I, double d), (*clmp)(double d),
    (*bop)(double a, double b), valA, valB;
  const Nrrd *nin;

  if (!(nout && inA && inB)) {
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  if (airEnumValCheck(nrrdBinaryOp, op)) {
    biffAddf(NRRD, "%s: binary op %d invalid", me, op);
    return 1;
  }
  if (!(0 == which || 1 == which)) {
    biffAddf(NRRD, "%s: which %u not 0 or 1", me, which);
    return 1;
  }
  nin = (0 == which ? MY_NRRD_ITER_NRRD(inA) : MY_NRRD_ITER_NRRD(inB));
  if (!nin) {
    biffAddf(NRRD, "%s: selected input %u is a fixed value", me, which);
    return 1;
  }
  type = nin->type;
  nrrdAxisInfoGet_nva(nin, nrrdAxisInfoSize, size);

  if (nrrd__MaybeAllocMaybeZero_nva(nout, type, nin->dim, size,
                                    AIR_FALSE /* zero when no realloc */)) {
    biffAddf(NRRD, "%s: couldn't allocate output nrrd", me);
    return 1;
  }
  nrrdBasicInfoCopy(nout, nin,
                    (NRRD_BASIC_INFO_DATA_BIT | NRRD_BASIC_INFO_TYPE_BIT
                     | NRRD_BASIC_INFO_DIMENSION_BIT | NRRD_BASIC_INFO_CONTENT_BIT
                     | NRRD_BASIC_INFO_COMMENTS_BIT
                     | (nrrdStateKeyValuePairsPropagate
                          ? 0
                          : NRRD_BASIC_INFO_KEYVALUEPAIRS_BIT)));
  nrrdBasicInfoInit(nout,
                    NRRD_BASIC_INFO_ALL
                      ^ (NRRD_BASIC_INFO_OLDMIN_BIT | NRRD_BASIC_INFO_OLDMAX_BIT));
  bop = nbinaryOp[op];

  /*
  fprintf(stderr, "%s: inA->left = %d, inB->left = %d\n", me,
          (int)(inA->left), (int)(inB->left));
  */
  N = nrrdElementNumber(nin);
  insert = nrrdDInsert[type];
  if (nrrdBinaryOpAddClamp == op || nrrdBinaryOpSubtractClamp == op
      || nrrdBinaryOpMultiplyClamp == op) {
    clmp = nrrdDClamp[nout->type];
  } else {
    clmp = NULL;
  }
  for (I = 0; I < N; I++) {
    double tmp;
    /* HEY: there is a loss of precision issue here with 64-bit ints */
    valA = nrrdIterValue(inA);
    valB = nrrdIterValue(inB);
    tmp = bop(valA, valB);
    if (clmp) {
      tmp = clmp(tmp);
    }
    insert(nout->data, I, tmp);
  }
  contA = nrrdIterContent(inA);
  contB = nrrdIterContent(inB);
  if (nrrd__ContentSet_va(nout, airEnumStr(nrrdBinaryOp, op), contA, "%s", contB)) {
    biffAddf(NRRD, "%s:", me);
    free(contA);
    free(contB);
    return 1;
  }
  if (nout != nin) {
    nrrdAxisInfoCopy(nout, nin, NULL, NRRD_AXIS_INFO_NONE);
  }
  free(contA);
  free(contB);
  return 0;
}

int /* Biff: 1 */
nrrdArithIterBinaryOp(Nrrd *nout, int op, NrrdIter *inA, NrrdIter *inB) {
  static const char me[] = "nrrdArithIterBinaryOp";
  unsigned int which;

  if (!(nout && inA && inB)) {
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  which = (MY_NRRD_ITER_NRRD(inA) ? 0 : (MY_NRRD_ITER_NRRD(inB) ? 1 : 2));
  if (2 == which) {
    biffAddf(NRRD, "%s: can't operate on two fixed values", me);
    return 1;
  }
  if (nrrdArithIterBinaryOpSelect(nout, op, inA, inB, which)) {
    biffAddf(NRRD, "%s: trouble", me);
    return 1;
  }
  return 0;
}

/* ---------------------------- ternary -------------- */

static double
nternaryOpAdd(double a, double b, double c) {
  return a + b + c;
}
static double
nternaryOpMultiply(double a, double b, double c) {
  return a * b * c;
}
static double
nternaryOpMin(double a, double b, double c) {
  b = AIR_MIN(b, c);
  return AIR_MIN(a, b);
}
/*
** minsmooth(x, w, M) is like min(x,M), but starting at value M-w, values
** are lowered (via erf), so that the output is asymptotic to M
*/
static double
nternaryOpMinSmooth(double x, double width, double max) {
  double tran;
  tran = max - width;
  return (tran < max /* using the function as intended */
            ? (x < tran
                 ? x
                 : airErf((x - tran) * 0.886226925452758 / (max - tran)) * (max - tran)
                     + tran)
            : AIR_MIN(x, max)); /* transition in wrong place; revert to simple max() */
}
static double
nternaryOpMax(double a, double b, double c) {
  b = AIR_MAX(b, c);
  return AIR_MAX(a, b);
}
/*
** maxsmooth(m, w, x) is like max(m,x), but starting at value m+w, values
** are raised (via erf), so that the output is asymptotic to m
*/
static double
nternaryOpMaxSmooth(double min, double width, double x) {
  double tran;
  tran = min + width;
  return (min < tran /* using the function as intended */
            ? (tran < x
                 ? x
                 : airErf((x - tran) * 0.886226925452758 / (min - tran)) * (min - tran)
                     + tran)
            : AIR_MAX(x, min)); /* transition in wrong place; revert to simple max() */
}
static double
nternaryOpLTSmooth(double a, double w, double b) {
  return AIR_AFFINE(-1.0, airErf((b - a) / w), 1.0, 0.0, 1.0);
}
static double
nternaryOpGTSmooth(double a, double w, double b) {
  return AIR_AFFINE(-1.0, airErf((a - b) / w), 1.0, 0.0, 1.0);
}
static double
nternaryOpClamp(double a, double b, double c) {
  return AIR_CLAMP(a, b, c);
}
static double
nternaryOpIfElse(double a, double b, double c) {
  return (a != 0 ? b : c);
}
static double
nternaryOpLerp(double a, double b, double c) {
  /* we do something more than the simple lerp here because
     we want to facilitate usage as something which can get around
     non-existent values (b and c as NaN or Inf) without
     getting polluted by them. */

  if (0.0 == a) {
    return b;
  } else if (1.0 == a) {
    return c;
  } else {
    return AIR_LERP(a, b, c);
  }
}
static double
nternaryOpExists(double a, double b, double c) {
  return (AIR_EXISTS(a) ? b : c);
}
static double
nternaryOpInOpen(double a, double b, double c) {
  return (AIR_IN_OP(a, b, c));
}
static double
nternaryOpInClosed(double a, double b, double c) {
  return (AIR_IN_CL(a, b, c));
}
static double
nternaryOpGaussian(double x, double mu, double sig) {
  return airGaussian(x, mu, sig);
}
static double
nternaryOpRician(double x, double mu, double sig) {
  return airRician(x, mu, sig);
}
static double (*const nternaryOp[NRRD_TERNARY_OP_MAX + 1])(double, double, double)
  = {NULL,
     nternaryOpAdd,
     nternaryOpMultiply,
     nternaryOpMin,
     nternaryOpMinSmooth,
     nternaryOpMax,
     nternaryOpMaxSmooth,
     nternaryOpLTSmooth,
     nternaryOpGTSmooth,
     nternaryOpClamp,
     nternaryOpIfElse,
     nternaryOpLerp,
     nternaryOpExists,
     nternaryOpInOpen,
     nternaryOpInClosed,
     nternaryOpGaussian,
     nternaryOpRician};

/*
******** nrrdArithTerneryOp
**
** HEY: UNTESTED UNTESTED UNTESTED UNTESTED UNTESTED UNTESTED UNTESTED
**
** this is a simplified version of nrrdArithIterTernaryOp, written after
** that, in a hurry, to operate directly on three nrrds, instead of with
** the NrrdIter nonsense
*/
int /* Biff: 1 */
nrrdArithTernaryOp(Nrrd *nout, int op, const Nrrd *ninA, const Nrrd *ninB,
                   const Nrrd *ninC) {
  static const char me[] = "nrrdArithTernaryOp";
  char *contA, *contB, *contC;
  size_t N, I, size[NRRD_DIM_MAX];
  double (*ins)(void *v, size_t I, double d), (*lupA)(const void *v, size_t I),
    (*lupB)(const void *v, size_t I), (*lupC)(const void *v, size_t I),
    (*top)(double a, double b, double c), valA, valB, valC;

  if (!(nout && !nrrdCheck(ninA) && !nrrdCheck(ninB) && !nrrdCheck(ninC))) {
    biffAddf(NRRD, "%s: NULL pointer or invalid args", me);
    return 1;
  }
  if (!(nrrdSameSize(ninA, ninB, AIR_TRUE) && nrrdSameSize(ninA, ninC, AIR_TRUE))) {
    biffAddf(NRRD, "%s: size mismatch between arguments", me);
    return 1;
  }
  if (airEnumValCheck(nrrdTernaryOp, op)) {
    biffAddf(NRRD, "%s: ternary op %d invalid", me, op);
    return 1;
  }

  nrrdAxisInfoGet_nva(ninA, nrrdAxisInfoSize, size);
  if (!(nout == ninA || nout == ninB || nout == ninC)) {
    if (nrrd__MaybeAllocMaybeZero_nva(nout, ninA->type, ninA->dim, size,
                                      AIR_FALSE /* zero when no realloc */)) {
      biffAddf(NRRD, "%s: couldn't allocate output nrrd", me);
      return 1;
    }
    if (nrrdAxisInfoCopy(nout, ninA, NULL, NRRD_AXIS_INFO_NONE)) {
      biffAddf(NRRD, "%s:", me);
      return 1;
    }
    nrrdBasicInfoCopy(nout, ninA,
                      (NRRD_BASIC_INFO_DATA_BIT | NRRD_BASIC_INFO_TYPE_BIT
                       | NRRD_BASIC_INFO_DIMENSION_BIT | NRRD_BASIC_INFO_CONTENT_BIT
                       | NRRD_BASIC_INFO_COMMENTS_BIT
                       | (nrrdStateKeyValuePairsPropagate
                            ? 0
                            : NRRD_BASIC_INFO_KEYVALUEPAIRS_BIT)));
  }
  nrrdBasicInfoInit(nout,
                    NRRD_BASIC_INFO_ALL
                      ^ (NRRD_BASIC_INFO_OLDMIN_BIT | NRRD_BASIC_INFO_OLDMAX_BIT));
  top = nternaryOp[op];

  N = nrrdElementNumber(ninA);
  lupA = nrrdDLookup[ninA->type];
  lupB = nrrdDLookup[ninB->type];
  lupC = nrrdDLookup[ninC->type];
  ins = nrrdDInsert[nout->type];
  for (I = 0; I < N; I++) {
    /* HEY: there is a loss of precision issue here with 64-bit ints */
    valA = lupA(ninA->data, I);
    valB = lupB(ninB->data, I);
    valC = lupC(ninC->data, I);
    ins(nout->data, I, top(valA, valB, valC));
  }

  contA = nrrd__ContentGet(ninA);
  contB = nrrd__ContentGet(ninB);
  contC = nrrd__ContentGet(ninC);
  if (nrrd__ContentSet_va(nout, airEnumStr(nrrdTernaryOp, op), contA, "%s,%s", contB,
                          contC)) {
    biffAddf(NRRD, "%s:", me);
    free(contA);
    free(contB);
    free(contC);
    return 1;
  }
  free(contA);
  free(contB);
  free(contC);

  return 0;
}

int /* Biff: 1 */
nrrdArithIterTernaryOpSelect(Nrrd *nout, int op, NrrdIter *inA, NrrdIter *inB,
                             NrrdIter *inC, unsigned int which) {
  static const char me[] = "nrrdArithIterTernaryOpSelect";
  char *contA, *contB, *contC;
  size_t N, I, size[NRRD_DIM_MAX];
  int type;
  double (*insert)(void *v, size_t I, double d), (*top)(double a, double b, double c),
    valA, valB, valC;
  const Nrrd *nin;

  if (!(nout && inA && inB && inC)) {
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  if (airEnumValCheck(nrrdTernaryOp, op)) {
    biffAddf(NRRD, "%s: ternary op %d invalid", me, op);
    return 1;
  }
  if (!(0 == which || 1 == which || 2 == which)) {
    biffAddf(NRRD, "%s: which %u not valid, want 0, 1, or 2", me, which);
    return 1;
  }
  nin = (0 == which ? MY_NRRD_ITER_NRRD(inA)
                    : (1 == which ? MY_NRRD_ITER_NRRD(inB) : MY_NRRD_ITER_NRRD(inC)));
  if (!nin) {
    biffAddf(NRRD, "%s: selected input %u is a fixed value", me, which);
    return 1;
  }
  type = nin->type;
  nrrdAxisInfoGet_nva(nin, nrrdAxisInfoSize, size);
  if (nrrd__MaybeAllocMaybeZero_nva(nout, type, nin->dim, size,
                                    AIR_FALSE /* zero when no realloc */)) {
    biffAddf(NRRD, "%s: couldn't allocate output nrrd", me);
    return 1;
  }
  nrrdBasicInfoCopy(nout, nin,
                    (NRRD_BASIC_INFO_DATA_BIT | NRRD_BASIC_INFO_TYPE_BIT
                     | NRRD_BASIC_INFO_DIMENSION_BIT | NRRD_BASIC_INFO_CONTENT_BIT
                     | NRRD_BASIC_INFO_COMMENTS_BIT
                     | (nrrdStateKeyValuePairsPropagate
                          ? 0
                          : NRRD_BASIC_INFO_KEYVALUEPAIRS_BIT)));
  nrrdBasicInfoInit(nout,
                    NRRD_BASIC_INFO_ALL
                      ^ (NRRD_BASIC_INFO_OLDMIN_BIT | NRRD_BASIC_INFO_OLDMAX_BIT));
  top = nternaryOp[op];

  /*
  fprintf(stderr, "%!s: inA->left = %d, inB->left = %d\n", me,
          (int)(inA->left), (int)(inB->left));
  */
  N = nrrdElementNumber(nin);
  insert = nrrdDInsert[type];
  for (I = 0; I < N; I++) {
    /* HEY: there is a loss of precision issue here with 64-bit ints */
    valA = nrrdIterValue(inA);
    valB = nrrdIterValue(inB);
    valC = nrrdIterValue(inC);
    /*
    if (!(I % 1000)) {
      fprintf(stderr, "!%s: %d: top(%g,%g,%g) = %g\n", me, (int)I,
              valA, valB, valC,
              top(valA, valB, valC));
    }
    */
    insert(nout->data, I, top(valA, valB, valC));
  }
  contA = nrrdIterContent(inA);
  contB = nrrdIterContent(inB);
  contC = nrrdIterContent(inC);
  if (nrrd__ContentSet_va(nout, airEnumStr(nrrdTernaryOp, op), contA, "%s,%s", contB,
                          contC)) {
    biffAddf(NRRD, "%s:", me);
    free(contA);
    free(contB);
    free(contC);
    return 1;
  }
  if (nout != nin) {
    nrrdAxisInfoCopy(nout, nin, NULL, NRRD_AXIS_INFO_NONE);
  }
  free(contA);
  free(contB);
  free(contC);
  return 0;
}

int /* Biff: 1 */
nrrdArithIterTernaryOp(Nrrd *nout, int op, NrrdIter *inA, NrrdIter *inB, NrrdIter *inC) {
  static const char me[] = "nrrdArithIterTernaryOp";
  unsigned int which;

  if (!(nout && inA && inB && inC)) {
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  which = (MY_NRRD_ITER_NRRD(inA)
             ? 0
             : (MY_NRRD_ITER_NRRD(inB) ? 1 : (MY_NRRD_ITER_NRRD(inC) ? 2 : 3)));
  if (3 == which) {
    biffAddf(NRRD, "%s: can't operate on 3 fixed values", me);
    return 1;
  }
  if (nrrdArithIterTernaryOpSelect(nout, op, inA, inB, inC, which)) {
    biffAddf(NRRD, "%s: trouble", me);
    return 1;
  }
  return 0;
}

int /* Biff: 1 */
nrrdArithAffine(Nrrd *nout, double minIn, const Nrrd *nin, double maxIn, double minOut,
                double maxOut, int clamp) {
  static const char me[] = "nrrdArithAffine";
  size_t I, N;
  double (*ins)(void *v, size_t I, double d), (*lup)(const void *v, size_t I), mmin,
    mmax;

  if (!nout || nrrdCheck(nin)) {
    biffAddf(NRRD, "%s: got NULL pointer or invalid input", me);
    return 1;
  }
  if (nout != nin) {
    if (nrrdCopy(nout, nin)) {
      biffAddf(NRRD, "%s: couldn't initialize output", me);
      return 1;
    }
  }
  N = nrrdElementNumber(nin);
  ins = nrrdDInsert[nout->type];
  lup = nrrdDLookup[nin->type];
  mmin = AIR_MIN(minOut, maxOut);
  mmax = AIR_MAX(minOut, maxOut);
  for (I = 0; I < N; I++) {
    double val;
    val = lup(nin->data, I);
    val = AIR_AFFINE(minIn, val, maxIn, minOut, maxOut);
    if (clamp) {
      val = AIR_CLAMP(mmin, val, mmax);
    }
    ins(nout->data, I, val);
  }
  /* HEY: it would be much better if the ordering here was the same as in
     AIR_AFFINE, but that's not easy with the way the content functions are
     now set up */
  if (nrrdContentSet_va(nout, "affine", nin, "%g,%g,%g,%g", minIn, maxIn, minOut,
                        maxOut)) {
    biffAddf(NRRD, "%s:", me);
    return 1;
  }
  return 0;
}

int /* Biff: 1 */
nrrdArithIterAffine(Nrrd *nout, NrrdIter *minIn, NrrdIter *in, NrrdIter *maxIn,
                    NrrdIter *minOut, NrrdIter *maxOut, int clamp) {
  static const char me[] = "nrrdArithIterAffine";
  double (*ins)(void *v, size_t I, double d), mini, vin, maxi, mino, maxo, vout;
  const Nrrd *nin;
  char *contA, *contB, *contC, *contD, *contE;
  size_t I, N;

  if (!(nout && minIn && in && maxIn && minOut && maxOut)) {
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  nin = (MY_NRRD_ITER_NRRD(in)
           ? MY_NRRD_ITER_NRRD(in)
           : (MY_NRRD_ITER_NRRD(minIn)
                ? MY_NRRD_ITER_NRRD(minIn)
                : (MY_NRRD_ITER_NRRD(maxIn)
                     ? MY_NRRD_ITER_NRRD(maxIn)
                     : (MY_NRRD_ITER_NRRD(minOut) ? MY_NRRD_ITER_NRRD(minOut)
                                                  : MY_NRRD_ITER_NRRD(maxOut)))));
  if (!nin) {
    biffAddf(NRRD, "%s: can't operate solely on fixed values", me);
    return 1;
  }
  if (nrrdCopy(nout, nin)) {
    biffAddf(NRRD, "%s: couldn't initialize output", me);
    return 1;
  }
  N = nrrdElementNumber(nin);
  ins = nrrdDInsert[nout->type];
  for (I = 0; I < N; I++) {
    mini = nrrdIterValue(minIn);
    vin = nrrdIterValue(in);
    maxi = nrrdIterValue(maxIn);
    mino = nrrdIterValue(minOut);
    maxo = nrrdIterValue(maxOut);
    vout = AIR_AFFINE(mini, vin, maxi, mino, maxo);
    if (clamp) {
      double mmin = AIR_MIN(mino, maxo);
      double mmax = AIR_MAX(mino, maxo);
      vout = AIR_CLAMP(mmin, vout, mmax);
    }
    ins(nout->data, I, vout);
  }
  contA = nrrdIterContent(in);
  contB = nrrdIterContent(minIn);
  contC = nrrdIterContent(maxIn);
  contD = nrrdIterContent(maxOut);
  contE = nrrdIterContent(maxOut);
  /* HEY: same annoyance about order of arguments as in function above */
  if (nrrd__ContentSet_va(nout, "affine", contA, "%s,%s,%s,%s", contB, contC, contD,
                          contE)) {
    biffAddf(NRRD, "%s:", me);
    free(contA);
    free(contB);
    free(contC);
    free(contD);
    free(contE);
    return 1;
  }
  free(contA);
  free(contB);
  free(contC);
  free(contD);
  free(contE);

  return 0;
}

unsigned int /* Biff: nope */
nrrdCRC32(const Nrrd *nin, int endian) {
  size_t nn;

  /* NULL nrrd or data */
  if (!nin || !(nin->data) || !(nn = nrrdElementSize(nin) * nrrdElementNumber(nin))
      || airEnumValCheck(airEndian, endian)) {
    return 0;
  }

  return airCRC32(AIR_CAST(const unsigned char *, nin->data), nn, nrrdElementSize(nin),
                  endian == airMyEndian() ? AIR_FALSE : AIR_TRUE);
}
