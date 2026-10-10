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
/* clang-format off */

#include "nrrd.h"
#include "privateNrrd.h"
#include <assert.h>

/*
Summary of information about how the kernel parameter vector is set:

                                 parmNum  parm[0]   parm[1]   parm[2]
                  nrrdKernelZero    1     scale
                 nrrdKernelCheap    1     scale
                   nrrdKernelBox    1     scale
       nrrdKernelBoxSupportDebug    1     support
nrrdKernelCatmullRomSupportDebug    1     support
      nrrdKernelCos4SupportDebug    1     support
 nrrdKernelHermiteScaleSpaceFlag    0
                  nrrdKernelTent    1     scale
              nrrdKernelForwDiff    1     scale
              nrrdKernelCentDiff    1     scale
               nrrdKernelBCCubic    3     scale       B        C
            nrrdKernelCatmullRom    0
              nrrdKernelAQuartic    2     scale       A
             nrrdKernelC3Quintic    0
               nrrdKernelC4Hexic    0
  nrrdKernelC4HexicApproxInverse    0
              nrrdKernelC5Septic    0
 nrrdKernelC5SepticApproxInverse    0
              nrrdKernelGaussian    2     sigma    cut-off
      nrrdKernelDiscreteGaussian    2     sigma    cut-off
                  nrrdKernelHann    2     scale    cut-off
              nrrdKernelBlackman    2     scale    cut-off
              nrrdKernelBSpline1    0
              nrrdKernelBSpline2    0
              nrrdKernelBSpline3    0
 nrrdKernelBSpline3ApproxInverse    0
              nrrdKernelBSpline4    0
              nrrdKernelBSpline5    0
 nrrdKernelBSpline5ApproxInverse    0
              nrrdKernelBSpline6    0
              nrrdKernelBSpline7    0
 nrrdKernelBSpline7ApproxInverse    0
             nrrdKernelTMF[][][]    1       a

Note that when parm[0] is named "scale", that parameter is optional,
and the default is 1.0, when given in string form
E.g. "tent" is understood as "tent:1",
but "gauss:4" isn't complete and won't parse; while "gauss:1,4" is good
There used to be a nrrdKernelUsesScale function, but now the new 3rd
member of the NrrdKernel is parm0IsScale, which should agree with the
appearance of "scale" above
*/

/* these functions replace what had been a lot of
   identical functions for similar kernels */

static double
returnZero(const double *parm) {
  AIR_UNUSED(parm);
  return 0.0;
}

static double
returnOne(const double *parm) {
  AIR_UNUSED(parm);
  return 1.0;
}

static double
returnTwo(const double *parm) {
  AIR_UNUSED(parm);
  return 2.0;
}

static double
returnThree(const double *parm) {
  AIR_UNUSED(parm);
  return 3.0;
}

static double
returnFour(const double *parm) {
  AIR_UNUSED(parm);
  return 4.0;
}

/* ------------------------------------------------------------ */

/* learned: if you copy/paste the macros for these kernels into
** other code, you *have* to make sure that the arguments for the
** kernels that are supposed to be reals, are not passed as an
** integral type (had this problem trying to re-use BCCUBIC
** with constant B="1" and C="0" and had trouble figuring out
** why the kernel was give garbage results
*/

/* the "zero" kernel is here more as a template than for anything else
   (as well as when you need the derivative of nrrdKernelForwDiff or
   any other piece-wise constant kernels)
   In particular the support method is pretty silly. */

#define ZERO(x) 0

static double
ZeroSup(const double *parm) {
  double S;

  S = parm[0];
  return S;
}

static double
Zero1_d(double x, const double *parm) {
  double S;

  S = parm[0];
  x = AIR_ABS(x)/S;
  return ZERO(x)/S;
}

static float
Zero1_f(float x, const double *parm) {
  float S;

  S = AIR_FLOAT(parm[0]);
  x = AIR_ABS(x)/S;
  return ZERO(x)/S;
}

static void
ZeroN_d(double *f, const double *x, size_t len, const double *parm) {
  double S;
  double t;
  size_t i;

  S = parm[0];
  for (i=0; i<len; i++) {
    t = x[i]; t = AIR_ABS(t)/S;
    f[i] = ZERO(t)/S;
  }
}

static void
ZeroN_f(float *f, const float *x, size_t len, const double *parm) {
  float t, S;
  size_t i;

  S = AIR_FLOAT(parm[0]);
  for (i=0; i<len; i++) {
    t = x[i]; t = AIR_ABS(t)/S;
    f[i] = ZERO(t)/S;
  }
}

static const NrrdKernel
KernelZero = {
  "zero", 1, AIR_TRUE /* even though we evaluate to zero! */,
  ZeroSup, returnZero,
  Zero1_f, ZeroN_f, Zero1_d, ZeroN_d
};
const NrrdKernel *const
nrrdKernelZero = &KernelZero;

/* ------------------------------------------------------------ */

/* nrrdKernelFlag exists to be a flag that some logic on kernels didn't work as expected
(such as nrrdKernelDerivative can't given an answer with certainty).  It generates AIR_NAN
in whenever possible. */
static double
FlagSup(const double *parm) {
  AIR_UNUSED(parm);
  return AIR_NAN;
}

static double
Flag1_d(double x, const double *parm) {
  AIR_UNUSED(x); AIR_UNUSED(parm);
  return AIR_NAN;
}

static float
Flag1_f(float x, const double *parm) {
  AIR_UNUSED(x); AIR_UNUSED(parm);
  return AIR_NAN;
}

static void
FlagN_d(double *f, const double *x, size_t len, const double *parm) {
  size_t i;
  AIR_UNUSED(x); AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    f[i] = AIR_NAN;
  }
}

static void
FlagN_f(float *f, const float *x, size_t len, const double *parm) {
  size_t i;
  AIR_UNUSED(x); AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    f[i] = AIR_NAN;
  }
}

static const NrrdKernel
KernelFlag = {
  "flag", 1, AIR_FALSE,
  FlagSup, returnZero,
  Flag1_f, FlagN_f, Flag1_d, FlagN_d
};
const NrrdKernel *const nrrdKernelFlag = &KernelFlag;

/* ------------------------------------------------------------ */

#define BOX(x) (x > 0.5 ? 0 : (x < 0.5 ? 1 : 0.5))

static double
BoxSup(const double *parm) {
  double S;

  S = parm[0];
  /* adding the 0.5 is to ensure that weights computed within the
     support really do catch all the non-zero values */
  return S/2 + 0.5;
}

static double
Box1_d(double x, const double *parm) {
  double S;

  S = parm[0];
  x = AIR_ABS(x)/S;
  return BOX(x)/S;
}

static float
Box1_f(float x, const double *parm) {
  float S;

  S = AIR_FLOAT(parm[0]);
  x = AIR_ABS(x)/S;
  return AIR_FLOAT(BOX(x)/S);
}

static void
BoxN_d(double *f, const double *x, size_t len, const double *parm) {
  double S;
  double t;
  size_t i;

  S = parm[0];
  for (i=0; i<len; i++) {
    t = x[i]; t = AIR_ABS(t)/S;
    f[i] = BOX(t)/S;
  }
}

static void
BoxN_f(float *f, const float *x, size_t len, const double *parm) {
  float t, S;
  size_t i;

  S = AIR_FLOAT(parm[0]);
  for (i=0; i<len; i++) {
    t = x[i]; t = AIR_ABS(t)/S;
    f[i] = AIR_FLOAT(BOX(t)/S);
  }
}

static const NrrdKernel
KernelBox = {
  "box", 1, AIR_TRUE,
  BoxSup, returnOne,
  Box1_f,  BoxN_f,  Box1_d,  BoxN_d
};
const NrrdKernel *const nrrdKernelBox = &KernelBox;

/* ------------------------------------------------------------ */

/* nrrdKernelBoxSupportDebug is like nrrdKernelBox but parm[0] determines the "support"; really
it is still the same underlying function as nrrdKernelBox (only 1 from -0.5 to 0.5) but having
variable nominal support is useful for some testing */
static double
BoxSDSup(const double *parm) {

  return parm[0];
}

static double
BoxSD1_d(double x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return BOX(x);
}

static float
BoxSD1_f(float x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return AIR_FLOAT(BOX(x));
}

static void
BoxSDN_d(double *f, const double *x, size_t len, const double *parm) {
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    double t;
    t = AIR_ABS(x[i]);
    f[i] = BOX(t);
  }
}

static void
BoxSDN_f(float *f, const float *x, size_t len, const double *parm) {
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    float t;
    t = AIR_ABS(x[i]);
    f[i] = AIR_FLOAT(BOX(t));
  }
}

static const NrrdKernel
KernelBoxSupportDebug = {
  "boxsup", 1, AIR_FALSE /* the kernel itself does NOT stretch */,
  BoxSDSup, returnOne /* well only if parm[0] > 0.5 */,
  BoxSD1_f,  BoxSDN_f,  BoxSD1_d,  BoxSDN_d
};
const NrrdKernel *const
nrrdKernelBoxSupportDebug = &KernelBoxSupportDebug;

/* ------------------------------------------------------------ */
/* Like nrrdKernelBoxSupportDebug, this kernel really is only non-zero on [0.5,0.5], even if
the nominal support will be simply parm[0]. Unlike nrrdKernelBoxSupportDebug this is a very
smooth bump inside that according to cos(pi*x)^4, hence the name nrrdKernelCos4SupportDebug
*/
#define COS4(x) (x > 0.5 \
                 ? 0.0 \
                 : cos(AIR_PI*x)*cos(AIR_PI*x)*cos(AIR_PI*x)*cos(AIR_PI*x))

static double
Cos4SDInt(const double *parm) {
  AIR_UNUSED(parm);
  return 3.0/8.0;
}

static double
Cos4SDSup(const double *parm) {

  return parm[0];
}

static double
Cos4SD1_d(double x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return COS4(x);
}

static float
Cos4SD1_f(float x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return AIR_FLOAT(COS4(x));
}

static void
Cos4SDN_d(double *f, const double *x, size_t len, const double *parm) {
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    double t;
    t = AIR_ABS(x[i]);
    f[i] = COS4(t);
  }
}

static void
Cos4SDN_f(float *f, const float *x, size_t len, const double *parm) {
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    float t;
    t = AIR_ABS(x[i]);
    f[i] = AIR_FLOAT(COS4(t));
  }
}

static const NrrdKernel
KernelCos4SupportDebug = {
  "cos4sup", 1, AIR_FALSE /* the kernel itself does NOT stretch */,
  Cos4SDSup, Cos4SDInt,
  Cos4SD1_f,  Cos4SDN_f,  Cos4SD1_d,  Cos4SDN_d
};
const NrrdKernel *const
nrrdKernelCos4SupportDebug = &KernelCos4SupportDebug;

/* ------------------------------------------------------------ */

#define DCOS4(x) (x > 0.5                                               \
                  ? 0.0                                                 \
                  : -4*AIR_PI*(cos(AIR_PI*x)*cos(AIR_PI*x)*cos(AIR_PI*x) \
                               *sin(AIR_PI*x)))

static double
DCos4SDSup(const double *parm) {
  return parm[0];
}

static double
DCos4SD1_d(double x, const double *parm) {
  int sgn;
  AIR_UNUSED(parm);
  if (x < 0) { x = -x; sgn = -1; } else { sgn = 1; }
  return sgn*DCOS4(x);
}

static float
DCos4SD1_f(float x, const double *parm) {
  int sgn;
  AIR_UNUSED(parm);
  if (x < 0) { x = -x; sgn = -1; } else { sgn = 1; }
  return AIR_FLOAT(sgn*DCOS4(x));
}

static void
DCos4SDN_d(double *f, const double *x, size_t len, const double *parm) {
  int sgn;
  double t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = sgn*DCOS4(t);
  }
}

static void
DCos4SDN_f(float *f, const float *x, size_t len, const double *parm) {
  int sgn;
  float t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = AIR_FLOAT(sgn*DCOS4(t));
  }
}

static const NrrdKernel
KernelCos4SupportDebugD = {
  "cos4supD", 1, AIR_FALSE /* the kernel itself does NOT stretch */,
  DCos4SDSup, returnZero,
  DCos4SD1_f,   DCos4SDN_f,   DCos4SD1_d,   DCos4SDN_d
};
const NrrdKernel *const
nrrdKernelCos4SupportDebugD = &KernelCos4SupportDebugD;

/* ------------------------------------------------------------ */

#define DDCOS4(x) (x > 0.5                                              \
                   ? 0.0                                                \
                   : -2*AIR_PI*AIR_PI*(cos(AIR_PI*2*x) + cos(AIR_PI*4*x)))

static double
DDCos4SDSup(const double *parm) {
  return parm[0];
}

static double
DDCos4SD1_d(double x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return DDCOS4(x);
}

static float
DDCos4SD1_f(float x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return AIR_FLOAT(DDCOS4(x));
}

static void
DDCos4SDN_d(double *f, const double *x, size_t len, const double *parm) {
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    double t;
    t = AIR_ABS(x[i]);
    f[i] = DDCOS4(t);
  }
}

static void
DDCos4SDN_f(float *f, const float *x, size_t len, const double *parm) {
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    float t;
    t = AIR_ABS(x[i]);
    f[i] = AIR_FLOAT(DDCOS4(t));
  }
}

static const NrrdKernel
KernelCos4SupportDebugDD = {
  "cos4supDD", 1, AIR_FALSE /* the kernel itself does NOT stretch */,
  DDCos4SDSup, returnZero,
  DDCos4SD1_f,  DDCos4SDN_f,  DDCos4SD1_d,  DDCos4SDN_d
};
const NrrdKernel *const
nrrdKernelCos4SupportDebugDD = &KernelCos4SupportDebugDD;

/* ------------------------------------------------------------ */

#define DDDCOS4(x) (x > 0.5                                             \
                    ? 0.0                                               \
                    : 4*AIR_PI*AIR_PI*AIR_PI*(sin(2*AIR_PI*x) + 2*sin(4*AIR_PI*x)))

static double
DDDCos4SDSup(const double *parm) {
  return parm[0];
}

static double
DDDCos4SD1_d(double x, const double *parm) {
  int sgn;
  AIR_UNUSED(parm);
  if (x < 0) { x = -x; sgn = -1; } else { sgn = 1; }
  return sgn*DDDCOS4(x);
}

static float
DDDCos4SD1_f(float x, const double *parm) {
  int sgn;
  AIR_UNUSED(parm);
  if (x < 0) { x = -x; sgn = -1; } else { sgn = 1; }
  return AIR_FLOAT(sgn*DDDCOS4(x));
}

static void
DDDCos4SDN_d(double *f, const double *x, size_t len, const double *parm) {
  int sgn;
  double t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = sgn*DDDCOS4(t);
  }
}

static void
DDDCos4SDN_f(float *f, const float *x, size_t len, const double *parm) {
  int sgn;
  float t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = AIR_FLOAT(sgn*DDDCOS4(t));
  }
}

static const NrrdKernel
KernelCos4SupportDebugDDD = {
  "cos4supDDD", 1, AIR_FALSE /* the kernel itself does NOT stretch */,
  DDDCos4SDSup, returnZero,
  DDDCos4SD1_f, DDDCos4SDN_f, DDDCos4SD1_d, DDDCos4SDN_d
};
const NrrdKernel *const
nrrdKernelCos4SupportDebugDDD = &KernelCos4SupportDebugDDD;

/* ------------------------------------------------------------ */

/* The point here is that post-kernel-evaluation, we need to see
   which sample is closest to the origin, and this is one way of
   enabling that
   SO: this kernel will not usefully report its integral or support! */
#define CHEAP(x) AIR_ABS(x)

static double
CheapSup(const double *parm) {
  double S;

  S = parm[0];
  /* adding the 0.5 is to insure that weights computed within the
     support really do catch all the non-zero values */
  return S/2 + 0.5;
}

static double
Cheap1_d(double x, const double *parm) {

  return CHEAP(x)/parm[0];
}

static float
Cheap1_f(float x, const double *parm) {

  return AIR_FLOAT(CHEAP(x)/parm[0]);
}

static void
CheapN_d(double *f, const double *x, size_t len, const double *parm) {
  double t;
  size_t i;

  for (i=0; i<len; i++) {
    t = x[i];
    f[i] = CHEAP(t)/parm[0];
  }
}

static void
CheapN_f(float *f, const float *x, size_t len, const double *parm) {
  float t;
  size_t i;

  for (i=0; i<len; i++) {
    t = x[i];
    f[i] = AIR_FLOAT(CHEAP(t)/parm[0]);
  }
}

static const NrrdKernel
KernelCheap = {
  "cheap", 1, AIR_TRUE,
  CheapSup, returnOne,
  Cheap1_f,  CheapN_f,  Cheap1_d,  CheapN_d
};
const NrrdKernel *const
nrrdKernelCheap = &KernelCheap;

/* ------------------------------------------------------------ */

#define TENT(x) (x >= 1 ? 0 : 1 - x)

static double
TentSup(const double *parm) {
  double S;

  S = parm[0];
  return S;
}

static double
Tent1_d(double x, const double *parm) {
  double S;

  S = parm[0];
  x = AIR_ABS(x)/S;
  return S ? TENT(x)/S : x == 0;
}

static float
Tent1_f(float x, const double *parm) {
  float S;

  S = AIR_FLOAT(parm[0]);
  x = AIR_ABS(x)/S;
  return S ? TENT(x)/S : x == 0;
}

static void
TentN_d(double *f, const double *x, size_t len, const double *parm) {
  double S;
  double t;
  size_t i;

  S = parm[0];
  for (i=0; i<len; i++) {
    t = x[i]; t = AIR_ABS(t)/S;
    f[i] = S ? TENT(t)/S : t == 0;
  }
}

static void
TentN_f(float *f, const float *x, size_t len, const double *parm) {
  float t, S;
  size_t i;

  S = AIR_FLOAT(parm[0]);
  for (i=0; i<len; i++) {
    t = x[i]; t = AIR_ABS(t)/S;
    f[i] = S ? TENT(t)/S : t == 0;
  }
}

static const NrrdKernel
KernelTent = {
  "tent", 1, AIR_TRUE,
  TentSup, returnOne,
  Tent1_f, TentN_f, Tent1_d, TentN_d
};
const NrrdKernel *const
nrrdKernelTent = &KernelTent;

/* ------------------------------------------------------------ */

/*
** NOTE: THERE IS NOT REALLY A HERMITE KERNEL (at least not yet,
** because it takes both values and derivatives as arguments, which
** the NrrdKernel currently can't handle).  This isn't really a
** kernel, its mostly a flag (hence the name), but it also has the
** role of generating weights according to linear interpolation, which
** is useful for the eventual spline evaluation.
**
** This hack is in sinister collusion with gage, to enable Hermite
** interpolation for its stack reconstruction.
*/

static double
Hermite1_d(double x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return TENT(x);
}

static float
Hermite1_f(float x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return TENT(x);
}

static void
HermiteN_d(double *f, const double *x, size_t len, const double *parm) {
  double t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i]; t = AIR_ABS(t);
    f[i] = TENT(t);
  }
}

static void
HermiteN_f(float *f, const float *x, size_t len, const double *parm) {
  float t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i]; t = AIR_ABS(t);
    f[i] = TENT(t);
  }
}

/* HEY: should just re-use fields from nrrdKernelTent, instead
   of creating new functions */
static const NrrdKernel
KernelHermiteScaleSpaceFlag = {
  "hermiteSS",  0, AIR_FALSE,
  returnOne, returnOne,
  Hermite1_f, HermiteN_f, Hermite1_d, HermiteN_d
};
const NrrdKernel *const
nrrdKernelHermiteScaleSpaceFlag = &KernelHermiteScaleSpaceFlag;

/* ------------------------------------------------------------ */

#define FORDIF(x) (x < -1 ?  0.0f :        \
                   (x <  0 ?  1.0f :        \
                   (x <  1 ? -1.0f : 0.0f )))

static double
FDSup(const double *parm) {
  double S;

  S = parm[0];
  return S+0.0001;  /* sigh */
}

static double
FD1_d(double x, const double *parm) {
  double t, S;

  S = parm[0];
  t = x/S;
  return FORDIF(t)/(S*S);
}

static float
FD1_f(float x, const double *parm) {
  float t, S;

  S = AIR_FLOAT(parm[0]);
  t = x/S;
  return FORDIF(t)/(S*S);
}

static void
FDN_d(double *f, const double *x, size_t len, const double *parm) {
  double t, S;
  size_t i;

  S = parm[0];
  for (i=0; i<len; i++) {
    t = x[i]/S;
    f[i] = FORDIF(t)/(S*S);
  }
}

static void
FDN_f(float *f, const float *x, size_t len, const double *parm) {
  float t, S;
  size_t i;

  S = AIR_FLOAT(parm[0]);
  for (i=0; i<len; i++) {
    t = x[i]/S;
    f[i] = FORDIF(t)/(S*S);
  }
}

static const NrrdKernel
KernelFD = {
  "fordif", 1, AIR_TRUE,
  FDSup, returnZero,
  FD1_f,   FDN_f,   FD1_d,   FDN_d
};
const NrrdKernel *const
nrrdKernelForwDiff = &KernelFD;

/* ------------------------------------------------------------ */

#define CENDIF(x) (x <= -2 ?  0         :        \
                   (x <= -1 ?  0.5*x + 1 :        \
                   (x <=  1 ? -0.5*x     :        \
                   (x <=  2 ?  0.5*x - 1 : 0 ))))

static double
CDSup(const double *parm) {
  double S;

  S = parm[0];
  return 2*S;
}

static double
CD1_d(double x, const double *parm) {
  double S;

  S = parm[0];
  x /= S;
  return CENDIF(x)/(S*S);
}

static float
CD1_f(float x, const double *parm) {
  float S;

  S = AIR_FLOAT(parm[0]);
  x /= S;
  return AIR_FLOAT(CENDIF(x)/(S*S));
}

static void
CDN_d(double *f, const double *x, size_t len, const double *parm) {
  double S;
  double t;
  size_t i;

  S = parm[0];
  for (i=0; i<len; i++) {
    t = x[i]/S;
    f[i] = CENDIF(t)/(S*S);
  }
}

static void
CDN_f(float *f, const float *x, size_t len, const double *parm) {
  float t, S;
  size_t i;

  S = AIR_FLOAT(parm[0]);
  for (i=0; i<len; i++) {
    t = x[i]/S;
    f[i] = AIR_FLOAT(CENDIF(t)/(S*S));
  }
}

static const NrrdKernel
KernelCD = {
  "cendif", 1, AIR_TRUE,
  CDSup, returnZero,
  CD1_f,   CDN_f,   CD1_d,   CDN_d
};
const NrrdKernel *const
nrrdKernelCentDiff = &KernelCD;

/* ------------------------------------------------------------ */

#define BCCUBIC(x, B, C)                                     \
  (x >= 2.0 ? 0 :                                             \
  (x >= 1.0                                                   \
   ? (((-B/6 - C)*x + B + 5*C)*x -2*B - 8*C)*x + 4*B/3 + 4*C  \
   : ((2 - 3*B/2 - C)*x - 3 + 2*B + C)*x*x + 1 - B/3))

static double
BCSup(const double *parm) {
  double S;

  S = parm[0];
  return 2*S;
}

static double
BC1_d(double x, const double *parm) {
  double S;
  double B, C;

  S = parm[0]; B = parm[1]; C = parm[2];
  x = AIR_ABS(x)/S;
  return BCCUBIC(x, B, C)/S;
}

static float
BC1_f(float x, const double *parm) {
  float B, C, S;

  S = AIR_FLOAT(parm[0]);
  B = AIR_FLOAT(parm[1]);
  C = AIR_FLOAT(parm[2]);
  x = AIR_ABS(x)/S;
  return BCCUBIC(x, B, C)/S;
}

static void
BCN_d(double *f, const double *x, size_t len, const double *parm) {
  double S;
  double t, B, C;
  size_t i;

  S = parm[0]; B = parm[1]; C = parm[2];
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t)/S;
    f[i] = BCCUBIC(t, B, C)/S;
  }
}

static void
BCN_f(float *f, const float *x, size_t len, const double *parm) {
  float S, t, B, C;
  size_t i;

  S = AIR_FLOAT(parm[0]);
  B = AIR_FLOAT(parm[1]);
  C = AIR_FLOAT(parm[2]);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t)/S;
    f[i] = BCCUBIC(t, B, C)/S;
  }
}

static const NrrdKernel
KernelBC = {
  "BCcubic", 3, AIR_TRUE,
  BCSup, returnOne,
  BC1_f,   BCN_f,   BC1_d,   BCN_d
};
const NrrdKernel *const
nrrdKernelBCCubic = &KernelBC;

/* ------------------------------------------------------------ */

#define DBCCUBIC(x, B, C)                        \
   (x >= 2.0 ? 0.0 :                              \
   (x >= 1.0                                      \
    ? ((-B/2 - 3*C)*x + 2*B + 10*C)*x -2*B - 8*C  \
    : ((6 - 9*B/2 - 3*C)*x - 6 + 4*B + 2*C)*x))

static double
DBCSup(const double *parm) {
  double S;

  S = parm[0];
  return 2*S;
}

static double
DBC1_d(double x, const double *parm) {
  double S;
  double B, C;
  int sgn = 1;

  S = parm[0]; B = parm[1]; C = parm[2];
  if (x < 0) { x = -x; sgn = -1; }
  x /= S;
  return sgn*DBCCUBIC(x, B, C)/(S*S);
}

static float
DBC1_f(float x, const double *parm) {
  float B, C, S;
  int sgn = 1;

  S = AIR_FLOAT(parm[0]);
  B = AIR_FLOAT(parm[1]);
  C = AIR_FLOAT(parm[2]);
  if (x < 0) { x = -x; sgn = -1; }
  x /= S;
  return AIR_FLOAT(sgn*DBCCUBIC(x, B, C)/(S*S));
}

static void
DBCN_d(double *f, const double *x, size_t len, const double *parm) {
  double S;
  double t, B, C;
  size_t i;
  int sgn;

  S = parm[0]; B = parm[1]; C = parm[2];
  for (i=0; i<len; i++) {
    t = x[i]/S;
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = sgn*DBCCUBIC(t, B, C)/(S*S);
  }
}

static void
DBCN_f(float *f, const float *x, size_t len, const double *parm) {
  float S, t, B, C;
  int sgn;
  size_t i;

  S = AIR_FLOAT(parm[0]);
  B = AIR_FLOAT(parm[1]);
  C = AIR_FLOAT(parm[2]);
  for (i=0; i<len; i++) {
    t = x[i]/S;
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = AIR_FLOAT(sgn*DBCCUBIC(t, B, C)/(S*S));
  }
}

static const NrrdKernel
KernelDBC = {
  "BCcubicD", 3, AIR_TRUE,
  DBCSup, returnZero,
  DBC1_f,  DBCN_f,  DBC1_d,  DBCN_d
};
const NrrdKernel *const
nrrdKernelBCCubicD = &KernelDBC;

/* ------------------------------------------------------------ */

#define DDBCCUBIC(x, B, C)                    \
   (x >= 2.0 ? 0 :                             \
   (x >= 1.0                                   \
    ? (-B - 6*C)*x + 2*B + 10*C                \
    : (12 - 9*B - 6*C)*x - 6 + 4*B + 2*C  ))

static double
DDBCSup(const double *parm) {
  double S;

  S = parm[0];
  return 2*S;
}

static double
DDBC1_d(double x, const double *parm) {
  double S;
  double B, C;

  S = parm[0]; B = parm[1]; C = parm[2];
  x = AIR_ABS(x)/S;
  return DDBCCUBIC(x, B, C)/(S*S*S);
}

static float
DDBC1_f(float x, const double *parm) {
  float B, C, S;

  S = AIR_FLOAT(parm[0]);
  B = AIR_FLOAT(parm[1]);
  C = AIR_FLOAT(parm[2]);
  x = AIR_ABS(x)/S;
  return DDBCCUBIC(x, B, C)/(S*S*S);
}

static void
DDBCN_d(double *f, const double *x, size_t len, const double *parm) {
  double S;
  double t, B, C;
  size_t i;

  S = parm[0]; B = parm[1]; C = parm[2];
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t)/S;
    f[i] = DDBCCUBIC(t, B, C)/(S*S*S);
  }
}

static void
DDBCN_f(float *f, const float *x, size_t len, const double *parm) {
  float S, t, B, C;
  size_t i;

  S = AIR_FLOAT(parm[0]);
  B = AIR_FLOAT(parm[1]);
  C = AIR_FLOAT(parm[2]);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t)/S;
    f[i] = DDBCCUBIC(t, B, C)/(S*S*S);
  }
}

static const NrrdKernel
KernelDDBC = {
  "BCcubicDD", 3, AIR_TRUE,
  DDBCSup, returnZero,
  DDBC1_f, DDBCN_f, DDBC1_d, DDBCN_d
};
const NrrdKernel *const
nrrdKernelBCCubicDD = &KernelDDBC;

/* ------------------------------------------------------------ */

/* if you've got the definition already, why not use it */
#define CTMR(x) BCCUBIC(x, 0.0, 0.5)

static double
CTMR1_d(double x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return CTMR(x);
}

static float
CTMR1_f(float x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return AIR_FLOAT(CTMR(x));
}

static void
CTMRN_d(double *f, const double *x, size_t len, const double *parm) {
  double t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    f[i] = CTMR(t);
  }
}

static void
CTMRN_f(float *f, const float *x, size_t len, const double *parm) {
  float t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    f[i] = AIR_FLOAT(CTMR(t));
  }
}

static const NrrdKernel
KernelCatmullRom = {
  "catmull-rom", 0, AIR_FALSE,
  returnTwo, returnOne,
  CTMR1_f,   CTMRN_f,   CTMR1_d,   CTMRN_d
};
const NrrdKernel *const
nrrdKernelCatmullRom = &KernelCatmullRom;


static double
CtmrSDSup(const double *parm) {

  return AIR_MAX(2.0, parm[0]);
}

static const NrrdKernel
KernelCatmullRomSupportDebug = {
  "ctmrsup",  1, AIR_FALSE /* the kernel itself does NOT stretch */,
  CtmrSDSup, returnOne,
  CTMR1_f,   CTMRN_f,   CTMR1_d,   CTMRN_d
};
const NrrdKernel *const
nrrdKernelCatmullRomSupportDebug = &KernelCatmullRomSupportDebug;

/* ------------------------------------------------------------ */

/* if you've got the definition already, why not use it */
#define DCTMR(x) DBCCUBIC(x, 0.0, 0.5)

static double
DCTMR1_d(double x, const double *parm) {
  int sgn;
  AIR_UNUSED(parm);
  if (x < 0) { x = -x; sgn = -1; } else { sgn = 1; }
  return sgn*DCTMR(x);
}

static float
DCTMR1_f(float x, const double *parm) {
  int sgn;
  AIR_UNUSED(parm);
  if (x < 0) { x = -x; sgn = -1; } else { sgn = 1; }
  return AIR_FLOAT(sgn*DCTMR(x));
}

static void
DCTMRN_d(double *f, const double *x, size_t len, const double *parm) {
  int sgn;
  double t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = sgn*DCTMR(t);
  }
}

static void
DCTMRN_f(float *f, const float *x, size_t len, const double *parm) {
  int sgn;
  float t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = AIR_FLOAT(sgn*DCTMR(t));
  }
}

static const NrrdKernel
KernelCatmullRomD = {
  "catmull-romD", 0, AIR_FALSE,
  returnTwo, returnZero,
  DCTMR1_f,   DCTMRN_f,   DCTMR1_d,   DCTMRN_d
};
const NrrdKernel *const
nrrdKernelCatmullRomD = &KernelCatmullRomD;

static const NrrdKernel
KernelCatmullRomSupportDebugD = {
  "ctmrsupD", 1, AIR_FALSE /* the kernel itself does NOT stretch */,
  CtmrSDSup, returnZero,
  DCTMR1_f,   DCTMRN_f,   DCTMR1_d,   DCTMRN_d
};
const NrrdKernel *const
nrrdKernelCatmullRomSupportDebugD = &KernelCatmullRomSupportDebugD;

/* ------------------------------------------------------------ */

/* if you've got the definition already, why not use it */
#define DDCTMR(x) DDBCCUBIC(x, 0.0, 0.5)

static double
DDCTMR1_d(double x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return DDCTMR(x);
}

static float
DDCTMR1_f(float x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return AIR_FLOAT(DDCTMR(x));
}

static void
DDCTMRN_d(double *f, const double *x, size_t len, const double *parm) {
  double t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    f[i] = DDCTMR(t);
  }
}

static void
DDCTMRN_f(float *f, const float *x, size_t len, const double *parm) {
  float t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    f[i] = AIR_FLOAT(DDCTMR(t));
  }
}

static const NrrdKernel
KernelCatmullRomDD = {
  "catmull-romDD", 0, AIR_FALSE,
  returnTwo, returnZero,
  DDCTMR1_f,   DDCTMRN_f,   DDCTMR1_d,   DDCTMRN_d
};
const NrrdKernel *const
nrrdKernelCatmullRomDD = &KernelCatmullRomDD;

static const NrrdKernel
KernelCatmullRomSupportDebugDD = {
  "ctmrsupDD", 1, AIR_FALSE /* the kernel itself does NOT stretch */,
  CtmrSDSup, returnZero,
  DDCTMR1_f,   DDCTMRN_f,   DDCTMR1_d,   DDCTMRN_d
};
const NrrdKernel *const
nrrdKernelCatmullRomSupportDebugDD = &KernelCatmullRomSupportDebugDD;

/* ------------------------------------------------------------ */

#define AQUARTIC(x, A) \
   (x >= 3.0 ? 0 :      \
   (x >= 2.0            \
    ? A*(-54 + x*(81 + x*(-45 + x*(11 - x)))) \
    : (x >= 1.0                               \
       ? 4 - 6*A + x*(-10 + 25*A + x*(9 - 33*A                         \
                                 + x*(-3.5 + 17*A + x*(0.5 - 3*A))))   \
       : 1 + x*x*(-3 + 6*A + x*((2.5 - 10*A) + x*(-0.5 + 4*A))))))

static double
A4Sup(const double *parm) {
  double S;

  S = parm[0];
  return 3*S;
}

static double
A41_d(double x, const double *parm) {
  double S;
  double A;

  S = parm[0]; A = parm[1];
  x = AIR_ABS(x)/S;
  return AQUARTIC(x, A)/S;
}

static float
A41_f(float x, const double *parm) {
  float A, S;

  S = AIR_FLOAT(parm[0]); A = AIR_FLOAT(parm[1]);
  x = AIR_ABS(x)/S;
  return AIR_FLOAT(AQUARTIC(x, A)/S);
}

static void
A4N_d(double *f, const double *x, size_t len, const double *parm) {
  double S;
  double t, A;
  size_t i;

  S = parm[0]; A = parm[1];
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t)/S;
    f[i] = AQUARTIC(t, A)/S;
  }
}

static void
A4N_f(float *f, const float *x, size_t len, const double *parm) {
  float S, t, A;
  size_t i;

  S = AIR_FLOAT(parm[0]); A = AIR_FLOAT(parm[1]);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t)/S;
    f[i] = AIR_FLOAT(AQUARTIC(t, A)/S);
  }
}

static const NrrdKernel
KernelA4 = {
  "Aquartic", 2, AIR_TRUE,
  A4Sup, returnOne,
  A41_f,   A4N_f,   A41_d,   A4N_d
};
const NrrdKernel *const
nrrdKernelAQuartic = &KernelA4;

/* ------------------------------------------------------------ */

#define DAQUARTIC(x, A) \
   (x >= 3.0 ? 0 :       \
   (x >= 2.0             \
    ? A*(81 + x*(-90 + x*(33 - 4*x))) \
    : (x >= 1.0                       \
       ? -10 + 25*A + x*(18 - 66*A + x*(-10.5 + 51*A + x*(2 - 12*A))) \
       : x*(-6 + 12*A + x*(7.5 - 30*A + x*(-2 + 16*A))))))

static double
DA4Sup(const double *parm) {
  double S;

  S = parm[0];
  return 3*S;
}

static double
DA41_d(double x, const double *parm) {
  double S;
  double A;
  int sgn = 1;

  S = parm[0]; A = parm[1];
  if (x < 0) { x = -x; sgn = -1; }
  x /= S;
  return sgn*DAQUARTIC(x, A)/(S*S);
}

static float
DA41_f(float x, const double *parm) {
  float A, S;
  int sgn = 1;

  S = AIR_FLOAT(parm[0]); A = AIR_FLOAT(parm[1]);
  if (x < 0) { x = -x; sgn = -1; }
  x /= S;
  return AIR_FLOAT(sgn*DAQUARTIC(x, A)/(S*S));
}

static void
DA4N_d(double *f, const double *x, size_t len, const double *parm) {
  double S;
  double t, A;
  size_t i;
  int sgn;

  S = parm[0]; A = parm[1];
  for (i=0; i<len; i++) {
    t = x[i]/S;
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = sgn*DAQUARTIC(t, A)/(S*S);
  }
}

static void
DA4N_f(float *f, const float *x, size_t len, const double *parm) {
  float S, t, A;
  size_t i;
  int sgn;

  S = AIR_FLOAT(parm[0]); A = AIR_FLOAT(parm[1]);
  for (i=0; i<len; i++) {
    t = x[i]/S;
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = AIR_FLOAT(sgn*DAQUARTIC(t, A)/(S*S));
  }
}

static const NrrdKernel
KernelDA4 = {
  "AquarticD", 2, AIR_TRUE,
  DA4Sup, returnZero,
  DA41_f,  DA4N_f,  DA41_d,  DA4N_d
};
const NrrdKernel *const
nrrdKernelAQuarticD = &KernelDA4;

/* ------------------------------------------------------------ */

#define DDAQUARTIC(x, A) \
   (x >= 3.0 ? 0 :        \
   (x >= 2.0              \
    ? A*(-90 + x*(66 - x*12)) \
    : (x >= 1.0               \
       ? 18 - 66*A + x*(-21 + 102*A + x*(6 - 36*A))   \
       : -6 + 12*A + x*(15 - 60*A + x*(-6 + 48*A)))))

static double
DDA4Sup(const double *parm) {
  double S;

  S = parm[0];
  return 3*S;
}

static double
DDA41_d(double x, const double *parm) {
  double S;
  double A;

  S = parm[0]; A = parm[1];
  x = AIR_ABS(x)/S;
  return DDAQUARTIC(x, A)/(S*S*S);
}

static float
DDA41_f(float x, const double *parm) {
  float S, A;

  S = AIR_FLOAT(parm[0]); A = AIR_FLOAT(parm[1]);
  x = AIR_ABS(x)/S;
  return DDAQUARTIC(x, A)/(S*S*S);
}

static void
DDA4N_d(double *f, const double *x, size_t len, const double *parm) {
  double S;
  double t, A;
  size_t i;

  S = parm[0]; A = parm[1];
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t)/S;
    f[i] = DDAQUARTIC(t, A)/(S*S*S);
  }
}

static void
DDA4N_f(float *f, const float *x, size_t len, const double *parm) {
  float S, t, A;
  size_t i;

  S = AIR_FLOAT(parm[0]); A = AIR_FLOAT(parm[1]);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t)/S;
    f[i] = DDAQUARTIC(t, A)/(S*S*S);
  }
}

static const NrrdKernel
KernelDDA4 = {
  "AquarticDD", 2, AIR_TRUE,
  DDA4Sup, returnZero,
  DDA41_f, DDA4N_f, DDA41_d, DDA4N_d
};
const NrrdKernel *const
nrrdKernelAQuarticDD = &KernelDDA4;

/* ------------------------------------------------------------ */

/*
** This is the unique, four-sample support, quintic, C^3 kernel, with 1st and
** 3rd derivatives zero at origin, which integrates to unity on [-2,2], which
** exactly reconstructs 0th and 1st order polynomials.  Unfortunately it does
** NOT reconstruct 2nd order polynomials, so its not very useful.  It worse
** than "one step down" from c4hexic (see below), since while its support and
** polynomial power is one less than c4hexic, it cannot reconstruct
** parabolas; c4hexic can reconstruct cubics.
**
** The same kernel is also available as nrrdKernelTMF w/ D,C,A = -1,3,2 ==
** TMF_dn_c3_2ef == "tmf:n,3,2" == nrrdKernelTMF[0][4][2], the only advantage
** here being that you have access to the first and second derivatives of
** this quintic kernel as nrrdKernelC3QuinticD and nrrdKernelC3QuinticDD.
**
** By the way, TMF_d0_c3_3ef == TMF_dn_c3_3ef == "tmf:n,3,3", which can (by
** definition) reconstruct parabolas, has four-sample support, and has
** piecewise polynomial order *seven*
*/

#define C3QUINTIC(x) \
  (x >= 2.0 ? 0 :                                             \
  (x >= 1.0                                                   \
   ? 0.8 + x*x*(-2 + x*(2 + x*(-0.75 + x*0.1)))               \
   : 0.7 + x*x*(-1 + x*x*(0.75 - x*0.3))))

static double
c3quint1_d(double x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return C3QUINTIC(x);
}

static float
c3quint1_f(float x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return AIR_FLOAT(C3QUINTIC(x));
}

static void
c3quintN_d(double *f, const double *x, size_t len, const double *parm) {
  double t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    f[i] = C3QUINTIC(t);
  }
}

static void
c3quintN_f(float *f, const float *x, size_t len, const double *parm) {
  float t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    f[i] = AIR_FLOAT(C3QUINTIC(t));
  }
}

static const NrrdKernel
c3quint = {
  "C3Quintic", 0, AIR_FALSE,
  returnTwo, returnOne,
  c3quint1_f, c3quintN_f,   c3quint1_d,   c3quintN_d
};
const NrrdKernel *const
nrrdKernelC3Quintic = &c3quint;

/* ------------------------------------------------------------ */

#define DC3QUINTIC(x) \
  (x >= 2.0 ? 0 :                                             \
  (x >= 1.0                                                   \
   ? x*(-4 + x*(6 + x*(-3 + x*0.5)))                          \
   : x*(-2 + x*x*(3 - x*1.5))))

static double
Dc3quint1_d(double x, const double *parm) {
  int sgn = 1;
  AIR_UNUSED(parm);
  if (x < 0) { x = -x; sgn = -1; }
  return sgn*DC3QUINTIC(x);
}

static float
Dc3quint1_f(float x, const double *parm) {
  int sgn = 1;
  AIR_UNUSED(parm);
  if (x < 0) { x = -x; sgn = -1; }
  return AIR_FLOAT(sgn*DC3QUINTIC(x));
}

static void
Dc3quintN_d(double *f, const double *x, size_t len, const double *parm) {
  double t;
  size_t i;
  int sgn;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = sgn*DC3QUINTIC(t);
  }
}

static void
Dc3quintN_f(float *f, const float *x, size_t len, const double *parm) {
  float t;
  size_t i;
  int sgn;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = AIR_FLOAT(sgn*DC3QUINTIC(t));
  }
}

static const NrrdKernel
KernelDC3Quintic = {
  "C3QuinticD", 0, AIR_FALSE,
  returnTwo, returnZero,
  Dc3quint1_f,  Dc3quintN_f,  Dc3quint1_d,  Dc3quintN_d
};
const NrrdKernel *const
nrrdKernelC3QuinticD = &KernelDC3Quintic;

/* ------------------------------------------------------------ */

#define DDC3QUINTIC(x) \
  (x >= 2.0 ? 0 :                                             \
  (x >= 1.0                                                   \
   ? -4 + x*(12 + x*(-9 + x*2))                               \
   : -2 + x*x*(9 - x*6)))

static double
DDc3quint1_d(double x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return DDC3QUINTIC(x);
}

static float
DDc3quint1_f(float x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return AIR_FLOAT(DDC3QUINTIC(x));
}

static void
DDc3quintN_d(double *f, const double *x, size_t len, const double *parm) {
  double t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    f[i] = DDC3QUINTIC(t);
  }
}

static void
DDc3quintN_f(float *f, const float *x, size_t len, const double *parm) {
  float t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    f[i] = AIR_FLOAT(DDC3QUINTIC(t));
  }
}

static const NrrdKernel
DDc3quint = {
  "C3QuinticDD", 0, AIR_FALSE,
  returnTwo, returnZero,
  DDc3quint1_f,   DDc3quintN_f,   DDc3quint1_d,   DDc3quintN_d
};
const NrrdKernel *const
nrrdKernelC3QuinticDD = &DDc3quint;

/* ------------------------------------------------------------ */

/*
** This is the unique, 6-sample support, hexic, C^4 kernel, with 1st and 3rd
** derivatives zero at origin, which integrates to unity on [-3,3], with 3rd
** order Taylor accuracy (errors start showing up when reconstructing 4th
** order polynomials) It doesn't interpolate, but its close, and it rings
** once.
**
** this is awfully close to, but not quite the same as, "tmf:n,3,4" ==
** TMF_dn_c3_4ef == nrrdKernelTMF[0][4][4], which is only C^3 smooth
*/

#define C4HEXIC(x) \
  (x >= 3.0 \
   ? 0 \
   : (x >= 2.0 \
      ? 1539.0/160.0 + x*(-189.0/8.0 + x*(747.0/32.0 + x*(-12.0 + x*(109.0/32.0 + x*(-61.0/120.0 + x/32.0))))) \
      : (x >= 1.0 \
         ? 3.0/160.0 + x*(35.0/8.0 + x*(-341.0/32.0 + x*(10.0 + x*(-147.0/32.0 + x*(25.0/24.0 - x*3.0/32.0))))) \
         : 69.0/80.0 + x*x*(-23.0/16.0 + x*x*(19.0/16.0 + x*(-7.0/12.0 + x/16.0)))  )))

static double
c4hex1_d(double x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return C4HEXIC(x);
}

static float
c4hex1_f(float x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return AIR_FLOAT(C4HEXIC(x));
}

static void
c4hexN_d(double *f, const double *x, size_t len, const double *parm) {
  double t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    f[i] = C4HEXIC(t);
  }
}

static void
c4hexN_f(float *f, const float *x, size_t len, const double *parm) {
  float t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    f[i] = AIR_FLOAT(C4HEXIC(t));
  }
}

static const NrrdKernel
c4hex = {
  "C4Hexic", 0, AIR_FALSE,
  returnThree, returnOne,
  c4hex1_f,   c4hexN_f,   c4hex1_d,   c4hexN_d
};
const NrrdKernel *const
nrrdKernelC4Hexic = &c4hex;

/* ------------------------------------------------------------ */

#define DC4HEXIC(x) \
  (x >= 3.0 \
   ? 0 \
   : (x >= 2.0 \
      ? -189.0/8.0 + x*(747.0/16.0 + x*(-36.0 + x*(109.0/8.0 + x*(-61.0/24.0 + x*(3.0/16.0))))) \
      : (x >= 1.0 \
         ? 35.0/8.0 + x*(-341.0/16.0 + x*(30 + x*(-147.0/8.0 + x*(125.0/24.0 + x*(-9.0/16.0))))) \
         : x*(-23.0/8.0 + x*x*(19.0/4.0 + x*(-35.0/12.0 + x*(3.0/8.0))))  )))

static double
Dc4hex1_d(double x, const double *parm) {
  int sgn = 1;
  AIR_UNUSED(parm);
  if (x < 0) { x = -x; sgn = -1; }
  return sgn*DC4HEXIC(x);
}

static float
Dc4hex1_f(float x, const double *parm) {
  int sgn = 1;
  AIR_UNUSED(parm);
  if (x < 0) { x = -x; sgn = -1; }
  return AIR_FLOAT(sgn*DC4HEXIC(x));
}

static void
Dc4hexN_d(double *f, const double *x, size_t len, const double *parm) {
  double t;
  size_t i;
  int sgn;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = sgn*DC4HEXIC(t);
  }
}

static void
Dc4hexN_f(float *f, const float *x, size_t len, const double *parm) {
  float t;
  size_t i;
  int sgn;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = AIR_FLOAT(sgn*DC4HEXIC(t));
  }
}

static const NrrdKernel
KernelDC4hexic = {
  "C4HexicD", 0, AIR_FALSE,
  returnThree, returnZero,
  Dc4hex1_f,  Dc4hexN_f,  Dc4hex1_d,  Dc4hexN_d
};
const NrrdKernel *const
nrrdKernelC4HexicD = &KernelDC4hexic;

/* ------------------------------------------------------------ */

#define DDC4HEXIC(x) \
  (x >= 3.0 \
   ? 0 \
   : (x >= 2.0 \
      ? 747.0/16.0 + x*(-72.0 + x*(327.0/8.0 + x*(-61.0/6.0 + x*15.0/16.0))) \
      : (x >= 1.0 \
         ? -341.0/16.0 + x*(60 + x*(-441.0/8.0 + x*(125.0/6.0 - x*45.0/16.0))) \
         : -23.0/8.0 + x*x*(57.0/4.0 + x*(-35.0/3.0 + x*(15.0/8.0)))  )))

static double
DDc4hex1_d(double x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return DDC4HEXIC(x);
}

static float
DDc4hex1_f(float x, const double *parm) {
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  return AIR_FLOAT(DDC4HEXIC(x));
}

static void
DDc4hexN_d(double *f, const double *x, size_t len, const double *parm) {
  double t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    f[i] = DDC4HEXIC(t);
  }
}

static void
DDc4hexN_f(float *f, const float *x, size_t len, const double *parm) {
  float t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    f[i] = AIR_FLOAT(DDC4HEXIC(t));
  }
}

static const NrrdKernel
DDc4hex = {
  "C4HexicDD", 0, AIR_FALSE,
  returnThree, returnZero,
  DDc4hex1_f,   DDc4hexN_f,   DDc4hex1_d,   DDc4hexN_d
};
const NrrdKernel *const
nrrdKernelC4HexicDD = &DDc4hex;

/* ------------------------------------------------------------ */

#define DDDC4HEXIC(x)                                                  \
  (x >= 3.0                                                             \
   ? 0                                                                  \
   : (x >= 2.0                                                          \
      ? -72.0 + x*(327.0/4.0 + x*(-61.0/2.0 + 15*x/4))                  \
      : (x >= 1.0                                                       \
         ? 60 + x*(-441.0/4.0 + x*(125.0/2.0 - 45*x/4))                 \
         : x*(57.0/2.0  + x*(-35 + 15*x/2))                             \
         )))

static double
DDDc4hex1_d(double x, const double *parm) {
  int sgn = 1;
  AIR_UNUSED(parm);
  if (x < 0) { x = -x; sgn = -1; }
  return sgn*DDDC4HEXIC(x);
}

static float
DDDc4hex1_f(float x, const double *parm) {
  int sgn = 1;
  AIR_UNUSED(parm);
  if (x < 0) { x = -x; sgn = -1; }
  return AIR_FLOAT(sgn*DDDC4HEXIC(x));
}

static void
DDDc4hexN_d(double *f, const double *x, size_t len, const double *parm) {
  double t;
  size_t i;
  int sgn;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = sgn*DDDC4HEXIC(t);
  }
}

static void
DDDc4hexN_f(float *f, const float *x, size_t len, const double *parm) {
  float t;
  size_t i;
  int sgn;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = AIR_FLOAT(sgn*DDDC4HEXIC(t));
  }
}

static const NrrdKernel
KernelDDDC4hexic = {
  "C4HexicDDD", 0, AIR_FALSE,
  returnThree, returnZero,
  DDDc4hex1_f,  DDDc4hexN_f,  DDDc4hex1_d,  DDDc4hexN_d
};
const NrrdKernel *const
nrrdKernelC4HexicDDD = &KernelDDDC4hexic;


/* ------------- approximate inverse of c4h ------------------------- */
/* see comments around "_bspl3_ANI" in bsplKernel.c */

static double
c4hex_ANI_kvals[12] = {
  1.1906949847244948336,
  -0.13537708971729194940,
  0.047024535491780434571,
  -0.0088462060502312555095,
  0.0022443498051487024049,
  -0.00048639680369511914410,
  0.00011421848629250278186,
  -0.000025727759438407893986,
  5.9204264168395963454e-6,
  -1.3468552403175349134e-6,
  3.0637649910394681441e-7,
  -5.5762487950611026674e-8};

static double
c4hex_ANI_sup(const double *parm) {
  AIR_UNUSED(parm);
  return 12.5;
}

#define C4HEX_ANI(ret, tmp, x)                  \
  tmp = AIR_UINT(x+0.5);          \
  if (tmp < 12) {                               \
    ret = c4hex_ANI_kvals[tmp];                \
  } else {                                      \
    ret = 0.0;                                  \
  }

static double
c4hex_ANI_1d(double x, const double *parm) {
  double ax, r; unsigned int tmp;
  AIR_UNUSED(parm);
  ax = AIR_ABS(x);
  C4HEX_ANI(r, tmp, ax);
  return r;
}

static float
c4hex_ANI_1f(float x, const double *parm) {
  double ax, r; unsigned int tmp;
  AIR_UNUSED(parm);
  ax = AIR_ABS(x);
  C4HEX_ANI(r, tmp, ax);
  return AIR_FLOAT(r);
}

static void
c4hex_ANI_Nd(double *f, const double *x, size_t len, const double *parm) {
  double ax, r; unsigned int tmp;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    ax = x[i]; ax = AIR_ABS(ax);
    C4HEX_ANI(r, tmp, ax);
    f[i] = r;
  }
}

static void
c4hex_ANI_Nf(float *f, const float *x, size_t len, const double *parm) {
  double ax, r; unsigned int tmp;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    ax = x[i]; ax = AIR_ABS(ax);
    C4HEX_ANI(r, tmp, ax);
    f[i] = AIR_FLOAT(r);
  }
}

static const NrrdKernel
KernelC4HexicApproxInverse = {
  "C4HexicAI", 0, AIR_FALSE,
  c4hex_ANI_sup, returnOne,
  c4hex_ANI_1f, c4hex_ANI_Nf,
  c4hex_ANI_1d, c4hex_ANI_Nd
};
const NrrdKernel *const
nrrdKernelC4HexicApproxInverse = &KernelC4HexicApproxInverse;

/* ------------------------- c5septic ------------------------------ */

/*
** This is the unique, 8-sample support, C^5 kernel, piecewise order-7
** with 4th order Taylor accuracy (errors start showing up when
** reconstructing 5th order polynomials).  Coincidentally, it has zero
** 1st and 3rd deriv at the origin, and it integrates to unity on
** [-4,4]. It doesn't interpolate, but its close; it rings twice.  */

#define C5SEPT0(x) (0.9379776601998824 + x*x*(-1.654320987654321 + x*x*(1.073045267489712 + x*x*(-0.44997427983539096 + x*0.13978909465020575))))
#define C5SEPT1(x) (0.04651675485008818 + x*(-0.7377829218106996 + x*(0.9699074074074074 + x*(0.18531378600823045 + x*(-0.7839506172839507 + x*(0.2357253086419753 + x*(0.12021604938271604 - x*0.054552469135802466)))))))
#define C5SEPT2(x) (-0.01860670194003527 + x*(0.14022633744855967 + x*(-0.16296296296296298 + x*(-0.09825102880658436 + x*(0.28858024691358025 + x*(-0.18858024691358025 + x*(0.04405864197530864 - x*0.0013631687242798354)))))))
#define C5SEPT3(x) (0.003101116990005879 + x*(-0.014223251028806585 + x*(0.02021604938271605 + x*(0.003729423868312757 + x*(-0.0411522633744856 + x*(0.04714506172839506 + x*(-0.023199588477366254 + x*0.004383450911228689)))))))
#define C5SEPT(i, x)                           \
  (0 == i ? C5SEPT0(x)                         \
   : (1 == i ? C5SEPT1(x)                      \
      : (2 == i ? C5SEPT2(x)                   \
         : (3 == i ? C5SEPT3(x)                \
            : 0.0))))

static double
c5sept1_d(double x, const double *parm) {
  unsigned int xi;
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  xi = AIR_UINT(x);
  x -= xi;
  return C5SEPT(xi, x);
}

static float
c5sept1_f(float x, const double *parm) {
  unsigned int xi;
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  xi = AIR_UINT(x);
  x -= AIR_FLOAT(xi);
  return AIR_FLOAT(C5SEPT(xi, x));
}

static void
c5septN_d(double *f, const double *x, size_t len, const double *parm) {
  unsigned int ti;
  double t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    ti = AIR_UINT(t);
    t -= ti;
    f[i] = C5SEPT(ti, t);
  }
}

static void
c5septN_f(float *f, const float *x, size_t len, const double *parm) {
  unsigned int ti;
  float t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    ti = AIR_UINT(t);
    t -= AIR_FLOAT(ti);
    f[i] = AIR_FLOAT(C5SEPT(ti, t));
  }
}

static const NrrdKernel
c5sept = {
  "C5Septic", 0, AIR_FALSE,
  returnFour, returnOne,
  c5sept1_f,   c5septN_f,   c5sept1_d,   c5septN_d
};
const NrrdKernel *const
nrrdKernelC5Septic = &c5sept;

#define DC5SEPT0(x) (x*(-3.308641975308642 + x*x*(4.292181069958848 + x*x*(-2.6998456790123457 + x*0.9785236625514403))))
#define DC5SEPT1(x) (-0.7377829218106996 + x*(1.9398148148148149 + x*(0.5559413580246914 + x*(-3.1358024691358026 + x*(1.1786265432098766 + x*(0.7212962962962963 - x*0.3818672839506173))))))
#define DC5SEPT2(x) (0.14022633744855967 + x*(-0.32592592592592595 + x*(-0.29475308641975306 + x*(1.154320987654321 + x*(-0.9429012345679012 + x*(0.26435185185185184 - x*0.009542181069958848))))))
#define DC5SEPT3(x) (-0.014223251028806585 + x*(0.0404320987654321 + x*(0.011188271604938271 + x*(-0.1646090534979424 + x*(0.2357253086419753 + x*(-0.13919753086419753 + x*0.03068415637860082))))))
#define DC5SEPT(i, x)                           \
  (0 == i ? DC5SEPT0(x)                         \
   : (1 == i ? DC5SEPT1(x)                      \
      : (2 == i ? DC5SEPT2(x)                   \
         : (3 == i ? DC5SEPT3(x)                \
            : 0.0))))

static double
dc5sept1_d(double x, const double *parm) {
  unsigned int xi;
  int sgn = 1;
  AIR_UNUSED(parm);
  if (x < 0) { x = -x; sgn = -1; }
  xi = AIR_UINT(x);
  x -= xi;
  return sgn*DC5SEPT(xi, x);
}

static float
dc5sept1_f(float x, const double *parm) {
  unsigned int xi;
  int sgn = 1;
  AIR_UNUSED(parm);
  if (x < 0) { x = -x; sgn = -1; }
  xi = AIR_UINT(x);
  x -= AIR_FLOAT(xi);
  return AIR_FLOAT(sgn*DC5SEPT(xi, x));
}

static void
dc5septN_d(double *f, const double *x, size_t len, const double *parm) {
  unsigned int ti;
  double t;
  size_t i;
  int sgn;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    ti = AIR_UINT(t);
    t -= ti;
    f[i] = sgn*DC5SEPT(ti, t);
  }
}

static void
dc5septN_f(float *f, const float *x, size_t len, const double *parm) {
  unsigned int ti;
  float t;
  size_t i;
  int sgn = 1;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    ti = AIR_UINT(t);
    t -= AIR_FLOAT(ti);
    f[i] = AIR_FLOAT(sgn*DC5SEPT(ti, t));
  }
}

static const NrrdKernel
dc5sept = {
  "C5SepticD", 0, AIR_FALSE,
  returnFour, returnZero,
  dc5sept1_f,   dc5septN_f,   dc5sept1_d,   dc5septN_d
};
const NrrdKernel *const
nrrdKernelC5SepticD = &dc5sept;

#define DDC5SEPT0(x) (-3.308641975308642 + x*x*(12.876543209876543 + x*x*(-13.499228395061728 + x*5.871141975308642)))
#define DDC5SEPT1(x) (1.9398148148148149 + x*(1.1118827160493827 + x*(-9.407407407407407 + x*(4.714506172839506 + x*(3.6064814814814814 - x*2.2912037037037036)))))
#define DDC5SEPT2(x) (-0.32592592592592595 + x*(-0.5895061728395061 + x*(3.462962962962963 + x*(-3.771604938271605 + x*(1.3217592592592593 - x*0.05725308641975309)))))
#define DDC5SEPT3(x) (0.0404320987654321 + x*(0.022376543209876542 + x*(-0.49382716049382713 + x*(0.9429012345679012 + x*(-0.6959876543209876 + x*0.18410493827160493)))))
#define DDC5SEPT(i, x)                           \
  (0 == i ? DDC5SEPT0(x)                         \
   : (1 == i ? DDC5SEPT1(x)                      \
      : (2 == i ? DDC5SEPT2(x)                   \
         : (3 == i ? DDC5SEPT3(x)                \
            : 0.0))))

static double
ddc5sept1_d(double x, const double *parm) {
  unsigned int xi;
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  xi = AIR_UINT(x);
  x -= xi;
  return DDC5SEPT(xi, x);
}

static float
ddc5sept1_f(float x, const double *parm) {
  unsigned int xi;
  AIR_UNUSED(parm);
  x = AIR_ABS(x);
  xi = AIR_UINT(x);
  x -= AIR_FLOAT(xi);
  return AIR_FLOAT(DDC5SEPT(xi, x));
}

static void
ddc5septN_d(double *f, const double *x, size_t len, const double *parm) {
  unsigned int ti;
  double t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    ti = AIR_UINT(t);
    t -= ti;
    f[i] = DDC5SEPT(ti, t);
  }
}

static void
ddc5septN_f(float *f, const float *x, size_t len, const double *parm) {
  unsigned int ti;
  float t;
  size_t i;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    ti = AIR_UINT(t);
    t -= AIR_FLOAT(ti);
    f[i] = AIR_FLOAT(DDC5SEPT(ti, t));
  }
}

static const NrrdKernel
ddc5sept = {
  "C5SepticDD", 0, AIR_FALSE,
  returnFour, returnZero,
  ddc5sept1_f,   ddc5septN_f,   ddc5sept1_d,   ddc5septN_d
};
const NrrdKernel *const
nrrdKernelC5SepticDD = &ddc5sept;

#define DDDC5SEPT0(x) (x*(25.75308641975309 + x*x*(-53.99691358024691 + x*29.35570987654321)))
#define DDDC5SEPT1(x) (1.111882716049383 + x*(-18.81481481481481 + x*(14.14351851851852 + x*(14.42592592592593 - x*11.45601851851852))))
#define DDDC5SEPT2(x) (-0.5895061728395062 + x*(6.925925925925926 + x*(-11.31481481481481 + x*(5.287037037037037 - x*0.2862654320987654))))
#define DDDC5SEPT3(x) (0.02237654320987654 + x*(-0.9876543209876543 + x*(2.828703703703704 + x*(-2.783950617283951 + x*0.9205246913580247))))
#define DDDC5SEPT(i, x)                           \
  (0 == i ? DDDC5SEPT0(x)                         \
   : (1 == i ? DDDC5SEPT1(x)                      \
      : (2 == i ? DDDC5SEPT2(x)                   \
         : (3 == i ? DDDC5SEPT3(x)                \
            : 0.0))))

static double
dddc5sept1_d(double x, const double *parm) {
  unsigned int xi;
  int sgn = 1;
  AIR_UNUSED(parm);
  if (x < 0) { x = -x; sgn = -1; }
  xi = AIR_UINT(x);
  x -= xi;
  return sgn*DDDC5SEPT(xi, x);
}

static float
dddc5sept1_f(float x, const double *parm) {
  unsigned int xi;
  int sgn = 1;
  AIR_UNUSED(parm);
  if (x < 0) { x = -x; sgn = -1; }
  xi = AIR_UINT(x);
  x -= AIR_FLOAT(xi);
  return AIR_FLOAT(sgn*DDDC5SEPT(xi, x));
}

static void
dddc5septN_d(double *f, const double *x, size_t len, const double *parm) {
  unsigned int ti;
  double t;
  size_t i;
  int sgn;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    ti = AIR_UINT(t);
    t -= ti;
    f[i] = sgn*DDDC5SEPT(ti, t);
  }
}

static void
dddc5septN_f(float *f, const float *x, size_t len, const double *parm) {
  unsigned int ti;
  float t;
  size_t i;
  int sgn = 1;
  AIR_UNUSED(parm);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    ti = AIR_UINT(t);
    t -= AIR_FLOAT(ti);
    f[i] = AIR_FLOAT(sgn*DDDC5SEPT(ti, t));
  }
}

static const NrrdKernel
dddc5sept = {
  "C5SepticDDD", 0, AIR_FALSE,
  returnFour, returnZero,
  dddc5sept1_f,   dddc5septN_f,   dddc5sept1_d,   dddc5septN_d
};
const NrrdKernel *const
nrrdKernelC5SepticDDD = &dddc5sept;

/* note that this implies a much more accurate inverse than is given
   for the splines or other kernels; this is a consequence of GLK
   re-purposing the Mathematica expressions for the bpsln7 inverse,
   which is unfortunate: c5septic is nearly interpolating, so far
   fewer terms would suffice */
#define C5SEPT_AI_LEN 26
static double
c5sept_ANI_kvals[C5SEPT_AI_LEN] = {
  1.072662863909143543451743,
  -0.05572032001443187610952953,
  0.02453993146603215267432700,
  -0.005922375635530229254855750,
  0.0009781882769025851448681918,
  -0.0002499281491108793120774480,
  0.00005196973116762945530292666,
  -0.00001090007030248955413371701,
  2.425581976693179040189747e-6,
  -5.143328756144314306529358e-7,
  1.109572595055083858393193e-7,
  -2.400323559797703961855318e-8,
  5.151959978856239469272136e-9,
  -1.111431289951609447815300e-9,
  2.394624249806782312051293e-10,
  -5.155654818408366273965675e-11,
  1.111091879440261302739584e-11,
  -2.393303168472914213194646e-12,
  5.155527554392687415035171e-13,
  -1.110707782883835600110634e-13,
  2.392644268109899456937863e-14,
  -5.154377464414088544322752e-15,
  1.110376957658466603291262e-15,
  -2.391459139266885907929865e-16,
  5.137528165538909945741180e-17,
  -9.024576392408067896802608e-18};

static double
c5sept_ANI_sup(const double *parm) {
  AIR_UNUSED(parm);
  return C5SEPT_AI_LEN + 0.5;
}

static double
c5sept_ANI_int(const double *parm) {
  AIR_UNUSED(parm);
  return 1.0;
}

#define C5SEPT_ANI(ret, tmp, x)                  \
  tmp = AIR_UINT(x+0.5);           \
  if (tmp < C5SEPT_AI_LEN) {                     \
    ret = c5sept_ANI_kvals[tmp];                \
  } else {                                       \
    ret = 0.0;                                   \
  }

static double
c5sept_ANI_1d(double x, const double *parm) {
  double ax, r; unsigned int tmp;
  AIR_UNUSED(parm);

  ax = AIR_ABS(x);
  C5SEPT_ANI(r, tmp, ax);
  return r;
}

static float
c5sept_ANI_1f(float x, const double *parm) {
  double ax, r; unsigned int tmp;
  AIR_UNUSED(parm);

  ax = AIR_ABS(x);
  C5SEPT_ANI(r, tmp, ax);
  return AIR_FLOAT(r);
}

static void
c5sept_ANI_Nd(double *f, const double *x, size_t len, const double *parm) {
  double ax, r; unsigned int tmp;
  size_t i;
  AIR_UNUSED(parm);

  for (i=0; i<len; i++) {
    ax = x[i]; ax = AIR_ABS(ax);
    C5SEPT_ANI(r, tmp, ax);
    f[i] = r;
  }
}

static void
c5sept_ANI_Nf(float *f, const float *x, size_t len, const double *parm) {
  double ax, r; unsigned int tmp;
  size_t i;
  AIR_UNUSED(parm);

  for (i=0; i<len; i++) {
    ax = x[i]; ax = AIR_ABS(ax);
    C5SEPT_ANI(r, tmp, ax);
    f[i] = AIR_FLOAT(r);
  }
}

static const NrrdKernel
KernelC5SepticApproxInverse = {
  "C5SepticAI", 0, AIR_FALSE,
  c5sept_ANI_sup, c5sept_ANI_int,
  c5sept_ANI_1f, c5sept_ANI_Nf,
  c5sept_ANI_1d, c5sept_ANI_Nd
};
const NrrdKernel *const
nrrdKernelC5SepticApproxInverse = &KernelC5SepticApproxInverse;

/* ------------------------------------------------------------ */

#define GAUSS(x, sig, cut) ( \
   x >= sig*cut ? 0           \
   : exp(-x*x/(2.0*sig*sig))/(sig*2.50662827463100050241))

static double
GInt(const double *parm) {
  double cut;

  cut = parm[1];
  return airErf(cut/sqrt(2.0));
}

static double
GSup(const double *parm) {
  double sig, cut;

  sig = parm[0];
  cut = parm[1];
  return sig*cut;
}

static double
G1_d(double x, const double *parm) {
  double sig, cut;

  sig = parm[0];
  cut = parm[1];
  x = AIR_ABS(x);
  return GAUSS(x, sig, cut);
}

static float
G1_f(float x, const double *parm) {
  float sig, cut;

  sig = AIR_FLOAT(parm[0]);
  cut = AIR_FLOAT(parm[1]);
  x = AIR_ABS(x);
  return AIR_FLOAT(GAUSS(x, sig, cut));
}

static void
GN_d(double *f, const double *x, size_t len, const double *parm) {
  double sig, cut, t;
  size_t i;

  sig = parm[0];
  cut = parm[1];
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    f[i] = GAUSS(t, sig, cut);
  }
}

static void
GN_f(float *f, const float *x, size_t len, const double *parm) {
  float sig, cut, t;
  size_t i;

  sig = AIR_FLOAT(parm[0]);
  cut = AIR_FLOAT(parm[1]);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    f[i] = AIR_FLOAT(GAUSS(t, sig, cut));
  }
}

static const NrrdKernel
KernelG = {
  "gauss", 2, AIR_FALSE /* (parm0IsScale) Nov 18 '25: this was AIR_TRUE,
                           but the intent of parm0IsScale is to make the
                           scaling parameter optional when it is 1.
                           That is true of "tent" but a Gaussian always
                           needs to say its sigma */,
  GSup,  GInt,
  G1_f,   GN_f,   G1_d,   GN_d
};
const NrrdKernel *const
nrrdKernelGaussian = &KernelG;

/* ------------------------------------------------------------ */

#define DISCRETEGAUSS(xx, sig, abscut)                         \
  (sig > 0                                                      \
   ? (xx > abscut                                               \
      ? 0                                                       \
      : airBesselInExpScaled(AIR_INT(xx + 0.5), sig*sig))       \
   : xx <= 0.5)

/* the last line used to be AIR_MAX(0.5, (ret)), but the problem was
   that even for reasonable cutoffs, like sigma=6, there would be a
   sudden change in the non-zero values at the edge of the kernel with
   slowly increasing sigma. The real solution is to have a smarter way
   of determining where to cut-off this particular kernel, the
   discrete gaussian, when the values are at the low level one would
   expect with "sigma=6" when talking about a continuous Gaussian */
#define DGABSCUT(ret, sig, cut) \
  (ret) = 0.5 + ceil((sig)*(cut));  \
  (ret) = AIR_MAX(2.5, (ret))

static double
DiscGaussianSup(const double *parm) {
  double ret;

  DGABSCUT(ret, parm[0], parm[1]);
  return ret;
}

/* the functions are out of their usual definition order because we
   use the function evaluation to determine the integral, rather than
   trying to do it analytically */

static double
DiscGaussian1_d(double xx, const double *parm) {
  double sig, cut;

  sig = parm[0];
  DGABSCUT(cut, sig, parm[1]);
  xx = AIR_ABS(xx);
  return DISCRETEGAUSS(xx, sig, cut);
}

static double
DiscGaussianInt(const double *parm) {
  double sum, cut;
  int ii, supp;

  DGABSCUT(cut, parm[0], parm[1]);
  supp = AIR_INT(cut);
  sum = 0.0;
  for (ii=-supp; ii<=supp; ii++) {
    sum += DiscGaussian1_d(ii, parm);
  }
  return sum;
}

static float
DiscGaussian1_f(float xx, const double *parm) {
  double sig, cut;

  sig = parm[0];
  DGABSCUT(cut, sig, parm[1]);
  xx = AIR_ABS(xx);
  return AIR_FLOAT(DISCRETEGAUSS(xx, sig, cut));
}

static void
DiscGaussianN_d(double *f, const double *x,
                  size_t len, const double *parm) {
  double sig, cut, tt;
  size_t ii;

  sig = parm[0];
  DGABSCUT(cut, sig, parm[1]);
  for (ii=0; ii<len; ii++) {
    tt = AIR_ABS(x[ii]);
    f[ii] = DISCRETEGAUSS(tt, sig, cut);
  }
}

static void
DiscGaussianN_f(float *f, const float *x, size_t len, const double *parm) {
  double sig, cut, tt;
  size_t ii;

  sig = parm[0];
  DGABSCUT(cut, sig, parm[1]);
  for (ii=0; ii<len; ii++) {
    tt = AIR_ABS(x[ii]);
    f[ii] = AIR_FLOAT(DISCRETEGAUSS(tt, sig, cut));
  }
}

static const NrrdKernel
KernelDiscreteGaussian = {
  "discretegauss", 2, AIR_FALSE /* (parm0IsScale) see note with nrrdKernelGaussian above */,
  DiscGaussianSup,  DiscGaussianInt,
  DiscGaussian1_f, DiscGaussianN_f,
  DiscGaussian1_d, DiscGaussianN_d
};
const NrrdKernel *const
nrrdKernelDiscreteGaussian = &KernelDiscreteGaussian;

/* The current implementation of nrrdKernelDiscreteGaussian, with the current
   implementation of airBesselInExpScaled, has problems for large
   sigma. Until those are fixed, this is a suggested limit on how big sigma
   (kparm[0]) can be and still have an accurate blurring kernel.  This
   problem can be avoided completely by doing the blurring in frequency
   space, which is implemented in teem/src/gage/stackBlur.c's
   _stackBlurDiscreteGaussFFT(), although that has its own problem: in places
   where a signal really should zero, the FFT can produce some very
   low-amplitude noise (and hence new extrema) */
const double nrrdKernelDiscreteGaussianGoodSigmaMax = 6.0;

/* ------------------------------------------------------------ */

#define DGAUSS(x, sig, cut) (                                               \
   x >= sig*cut ? 0                                                          \
   : -exp(-x*x/(2.0*sig*sig))*x/(sig*sig*sig*2.50662827463100050241))

static double
DGInt(const double *parm) {
  AIR_UNUSED(parm);
  return 0;
}

static double
DGSup(const double *parm) {
  double sig, cut;

  sig = parm[0];
  cut = parm[1];
  return sig*cut;
}

static double
DG1_d(double x, const double *parm) {
  double sig, cut;
  int sgn = 1;

  sig = parm[0];
  cut = parm[1];
  if (x < 0) { x = -x; sgn = -1; }
  return sgn*DGAUSS(x, sig, cut);
}

static float
DG1_f(float x, const double *parm) {
  float sig, cut;
  int sgn = 1;

  sig = AIR_FLOAT(parm[0]);
  cut = AIR_FLOAT(parm[1]);
  if (x < 0) { x = -x; sgn = -1; }
  return AIR_FLOAT(sgn*DGAUSS(x, sig, cut));
}

static void
DGN_d(double *f, const double *x, size_t len, const double *parm) {
  double sig, cut, t;
  size_t i;
  int sgn;

  sig = parm[0];
  cut = parm[1];
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = sgn*DGAUSS(t, sig, cut);
  }
}

static void
DGN_f(float *f, const float *x, size_t len, const double *parm) {
  float sig, cut, t;
  size_t i;
  int sgn;

  sig = AIR_FLOAT(parm[0]);
  cut = AIR_FLOAT(parm[1]);
  for (i=0; i<len; i++) {
    t = x[i];
    if (t < 0) { t = -t; sgn = -1; } else { sgn = 1; }
    f[i] = AIR_FLOAT(sgn*DGAUSS(t, sig, cut));
  }
}

static const NrrdKernel
KernelDG = {
  "gaussD", 2, AIR_TRUE,
  DGSup,  DGInt,
  DG1_f,   DGN_f,   DG1_d,   DGN_d
};
const NrrdKernel *const
nrrdKernelGaussianD = &KernelDG;

/* ------------------------------------------------------------ */

#define DDGAUSS(x, sig, cut) ( \
   x >= sig*cut ? 0             \
   : exp(-x*x/(2.0*sig*sig))*(x*x-sig*sig) /       \
     (sig*sig*sig*sig*sig*2.50662827463100050241))

static double
DDGInt(const double *parm) {
  double sig, cut;

  sig = parm[0];
  cut = parm[1];
  return -0.79788456080286535587*cut*exp(-cut*cut/2)/(sig*sig);
}

static double
DDGSup(const double *parm) {
  double sig, cut;

  sig = parm[0];
  cut = parm[1];
  return sig*cut;
}

static double
DDG1_d(double x, const double *parm) {
  double sig, cut;

  sig = parm[0];
  cut = parm[1];
  x = AIR_ABS(x);
  return DDGAUSS(x, sig, cut);
}

static float
DDG1_f(float x, const double *parm) {
  float sig, cut;

  sig = AIR_FLOAT(parm[0]);
  cut = AIR_FLOAT(parm[1]);
  x = AIR_ABS(x);
  return AIR_FLOAT(DDGAUSS(x, sig, cut));
}

static void
DDGN_d(double *f, const double *x, size_t len, const double *parm) {
  double sig, cut, t;
  size_t i;

  sig = parm[0];
  cut = parm[1];
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    f[i] = DDGAUSS(t, sig, cut);
  }
}

static void
DDGN_f(float *f, const float *x, size_t len, const double *parm) {
  float sig, cut, t;
  size_t i;

  sig = AIR_FLOAT(parm[0]);
  cut = AIR_FLOAT(parm[1]);
  for (i=0; i<len; i++) {
    t = x[i];
    t = AIR_ABS(t);
    f[i] = AIR_FLOAT(DDGAUSS(t, sig, cut));
  }
}

static const NrrdKernel
KernelDDG = {
  "gaussDD", 2, AIR_TRUE,
  DDGSup,  DDGInt,
  DDG1_f,   DDGN_f,   DDG1_d,   DDGN_d
};
const NrrdKernel *const
nrrdKernelGaussianDD = &KernelDDG;

/* ------------------------------------------------------------ */

static const NrrdKernel *
KernelStrToKern(char *str) {

  if (!strcmp("zero", str))       return nrrdKernelZero;
  if (!strcmp("box", str))        return nrrdKernelBox;
  if (!strcmp("boxsup", str))     return nrrdKernelBoxSupportDebug;
  if (!strcmp("cos4sup", str))    return nrrdKernelCos4SupportDebug;
  if (!strcmp("cos4supd", str))   return nrrdKernelCos4SupportDebugD;
  if (!strcmp("cos4supdd", str))  return nrrdKernelCos4SupportDebugDD;
  if (!strcmp("cos4supddd", str)) return nrrdKernelCos4SupportDebugDDD;
  if (!strcmp("cheap", str))      return nrrdKernelCheap;
  if (!strcmp("hermiteflag", str))    return nrrdKernelHermiteScaleSpaceFlag;
  if (!strcmp("hermite", str))    return nrrdKernelHermiteScaleSpaceFlag;
  if (!strcmp("hermitess", str))    return nrrdKernelHermiteScaleSpaceFlag;
  if (!strcmp("herm", str))       return nrrdKernelHermiteScaleSpaceFlag;
  if (!strcmp("tent", str))       return nrrdKernelTent;
  if (!strcmp("tentd", str))      return nrrdKernelForwDiff;
  if (!strcmp("forwdiff", str))   return nrrdKernelForwDiff;
  if (!strcmp("fordif", str))     return nrrdKernelForwDiff;
  if (!strcmp("centdiff", str))   return nrrdKernelCentDiff;
  if (!strcmp("cendif", str))     return nrrdKernelCentDiff;
  if (!strcmp("bccubic", str))    return nrrdKernelBCCubic;
  if (!strcmp("cubic", str))      return nrrdKernelBCCubic;
  if (!strcmp("bccubicd", str))   return nrrdKernelBCCubicD;
  if (!strcmp("cubicd", str))     return nrrdKernelBCCubicD;
  if (!strcmp("bccubicdd", str))  return nrrdKernelBCCubicDD;
  if (!strcmp("cubicdd", str))    return nrrdKernelBCCubicDD;
  if (!strcmp("ctmr", str))       return nrrdKernelCatmullRom;
  if (!strcmp("catmull-rom", str)) return nrrdKernelCatmullRom;
  if (!strcmp("ctmrd", str))       return nrrdKernelCatmullRomD;
  if (!strcmp("catmull-romd", str)) return nrrdKernelCatmullRomD;
  if (!strcmp("ctmrdd", str))     return nrrdKernelCatmullRomDD;
  if (!strcmp("catmull-romdd", str)) return nrrdKernelCatmullRomDD;
  if (!strcmp("ctmrsup", str))    return nrrdKernelCatmullRomSupportDebug;
  if (!strcmp("ctmrsupd", str))   return nrrdKernelCatmullRomSupportDebugD;
  if (!strcmp("ctmrsupdd", str))   return nrrdKernelCatmullRomSupportDebugDD;
  if (!strcmp("aquartic", str))   return nrrdKernelAQuartic;
  if (!strcmp("quartic", str))    return nrrdKernelAQuartic;
  if (!strcmp("aquarticd", str))  return nrrdKernelAQuarticD;
  if (!strcmp("quarticd", str))   return nrrdKernelAQuarticD;
  if (!strcmp("aquarticdd", str)) return nrrdKernelAQuarticDD;
  if (!strcmp("quarticdd", str))  return nrrdKernelAQuarticDD;
  if (!strcmp("c3quintic", str))  return nrrdKernelC3Quintic;
  if (!strcmp("c3q", str))        return nrrdKernelC3Quintic;
  if (!strcmp("c3quinticd", str)) return nrrdKernelC3QuinticD;
  if (!strcmp("c3qd", str))       return nrrdKernelC3QuinticD;
  if (!strcmp("c3quinticdd", str)) return nrrdKernelC3QuinticDD;
  if (!strcmp("c3qdd", str))      return nrrdKernelC3QuinticDD;
  if (!strcmp("c4hexic", str))    return nrrdKernelC4Hexic;
  if (!strcmp("c4hai", str))      return nrrdKernelC4HexicApproxInverse;
  if (!strcmp("c4hexicai", str))  return nrrdKernelC4HexicApproxInverse;
  if (!strcmp("c4h", str))        return nrrdKernelC4Hexic;
  if (!strcmp("c4hexicd", str))   return nrrdKernelC4HexicD;
  if (!strcmp("c4hd", str))       return nrrdKernelC4HexicD;
  if (!strcmp("c4hexicdd", str))  return nrrdKernelC4HexicDD;
  if (!strcmp("c4hdd", str))      return nrrdKernelC4HexicDD;
  if (!strcmp("c4hexicddd", str)) return nrrdKernelC4HexicDDD;
  if (!strcmp("c4hddd", str))     return nrrdKernelC4HexicDDD;
  if (!strcmp("c5septic", str))   return nrrdKernelC5Septic;
  if (!strcmp("c5septicd", str))  return nrrdKernelC5SepticD;
  if (!strcmp("c5septicdd", str)) return nrrdKernelC5SepticDD;
  if (!strcmp("c5septicddd", str))return nrrdKernelC5SepticDDD;
  if (!strcmp("c5septicai", str)) return nrrdKernelC5SepticApproxInverse;
  if (!strcmp("gaussian", str))   return nrrdKernelGaussian;
  if (!strcmp("gauss", str))      return nrrdKernelGaussian;
  if (!strcmp("gaussiand", str))  return nrrdKernelGaussianD;
  if (!strcmp("gaussd", str))     return nrrdKernelGaussianD;
  if (!strcmp("gd", str))         return nrrdKernelGaussianD;
  if (!strcmp("gaussiandd", str)) return nrrdKernelGaussianDD;
  if (!strcmp("gaussdd", str))    return nrrdKernelGaussianDD;
  if (!strcmp("gdd", str))        return nrrdKernelGaussianDD;
  if (!strcmp("ds", str))         return nrrdKernelDiscreteGaussian;
  if (!strcmp("dscale", str))     return nrrdKernelDiscreteGaussian;
  if (!strcmp("dg", str))         return nrrdKernelDiscreteGaussian;
  if (!strcmp("dgauss", str))     return nrrdKernelDiscreteGaussian;
  if (!strcmp("dgaussian", str))  return nrrdKernelDiscreteGaussian;
  if (!strcmp("discretegauss", str))  return nrrdKernelDiscreteGaussian;
  if (!strcmp("hann", str))       return nrrdKernelHann;
  if (!strcmp("hannd", str))      return nrrdKernelHannD;
  if (!strcmp("hanndd", str))     return nrrdKernelHannDD;
  if (!strcmp("bkmn", str))       return nrrdKernelBlackman;
  if (!strcmp("black", str))      return nrrdKernelBlackman;
  if (!strcmp("blackman", str))   return nrrdKernelBlackman;
  if (!strcmp("bkmnd", str))      return nrrdKernelBlackmanD;
  if (!strcmp("blackd", str))     return nrrdKernelBlackmanD;
  if (!strcmp("blackmand", str))  return nrrdKernelBlackmanD;
  if (!strcmp("bkmndd", str))     return nrrdKernelBlackmanDD;
  if (!strcmp("blackdd", str))    return nrrdKernelBlackmanDD;
  if (!strcmp("blackmandd", str)) return nrrdKernelBlackmanDD;
  if (!strcmp("bspl1", str))      return nrrdKernelBSpline1;
  if (!strcmp("bspln1", str))     return nrrdKernelBSpline1;
  if (!strcmp("bspl1d", str))     return nrrdKernelBSpline1D;
  if (!strcmp("bspln1d", str))    return nrrdKernelBSpline1D;
  if (!strcmp("bspl2", str))      return nrrdKernelBSpline2;
  if (!strcmp("bspln2", str))     return nrrdKernelBSpline2;
  if (!strcmp("bspl2d", str))     return nrrdKernelBSpline2D;
  if (!strcmp("bspln2d", str))    return nrrdKernelBSpline2D;
  if (!strcmp("bspl2dd", str))    return nrrdKernelBSpline2DD;
  if (!strcmp("bspln2dd", str))   return nrrdKernelBSpline2DD;
  if (!strcmp("bspl3", str))      return nrrdKernelBSpline3;
  if (!strcmp("bspln3", str))     return nrrdKernelBSpline3;
  if (!strcmp("bspl3ai", str))    return nrrdKernelBSpline3ApproxInverse;
  if (!strcmp("bspln3ai", str))   return nrrdKernelBSpline3ApproxInverse;
  if (!strcmp("bspl3d", str))     return nrrdKernelBSpline3D;
  if (!strcmp("bspln3d", str))    return nrrdKernelBSpline3D;
  if (!strcmp("bspl3dd", str))    return nrrdKernelBSpline3DD;
  if (!strcmp("bspln3dd", str))   return nrrdKernelBSpline3DD;
  if (!strcmp("bspl3ddd", str))   return nrrdKernelBSpline3DDD;
  if (!strcmp("bspln3ddd", str))  return nrrdKernelBSpline3DDD;
  if (!strcmp("bspl4", str))      return nrrdKernelBSpline4;
  if (!strcmp("bspln4", str))     return nrrdKernelBSpline4;
  if (!strcmp("bspl4d", str))     return nrrdKernelBSpline4D;
  if (!strcmp("bspln4d", str))    return nrrdKernelBSpline4D;
  if (!strcmp("bspl4dd", str))    return nrrdKernelBSpline4DD;
  if (!strcmp("bspln4dd", str))   return nrrdKernelBSpline4DD;
  if (!strcmp("bspl4ddd", str))   return nrrdKernelBSpline4DDD;
  if (!strcmp("bspln4ddd", str))  return nrrdKernelBSpline4DDD;
  if (!strcmp("bspl5", str))      return nrrdKernelBSpline5;
  if (!strcmp("bspln5", str))     return nrrdKernelBSpline5;
  if (!strcmp("bspl5ai", str))    return nrrdKernelBSpline5ApproxInverse;
  if (!strcmp("bspln5ai", str))   return nrrdKernelBSpline5ApproxInverse;
  if (!strcmp("bspl5d", str))     return nrrdKernelBSpline5D;
  if (!strcmp("bspln5d", str))    return nrrdKernelBSpline5D;
  if (!strcmp("bspl5dd", str))    return nrrdKernelBSpline5DD;
  if (!strcmp("bspln5dd", str))   return nrrdKernelBSpline5DD;
  if (!strcmp("bspl5ddd", str))   return nrrdKernelBSpline5DDD;
  if (!strcmp("bspln5ddd", str))  return nrrdKernelBSpline5DDD;
  if (!strcmp("bspl6", str))      return nrrdKernelBSpline6;
  if (!strcmp("bspln6", str))     return nrrdKernelBSpline6;
  if (!strcmp("bspl6d", str))     return nrrdKernelBSpline6D;
  if (!strcmp("bspln6d", str))    return nrrdKernelBSpline6D;
  if (!strcmp("bspl6dd", str))    return nrrdKernelBSpline6DD;
  if (!strcmp("bspln6dd", str))   return nrrdKernelBSpline6DD;
  if (!strcmp("bspl6ddd", str))   return nrrdKernelBSpline6DDD;
  if (!strcmp("bspln6ddd", str))  return nrrdKernelBSpline6DDD;
  if (!strcmp("bspl7", str))      return nrrdKernelBSpline7;
  if (!strcmp("bspln7", str))     return nrrdKernelBSpline7;
  if (!strcmp("bspl7ai", str))    return nrrdKernelBSpline7ApproxInverse;
  if (!strcmp("bspln7ai", str))   return nrrdKernelBSpline7ApproxInverse;
  if (!strcmp("bspl7d", str))     return nrrdKernelBSpline7D;
  if (!strcmp("bspln7d", str))    return nrrdKernelBSpline7D;
  if (!strcmp("bspl7dd", str))    return nrrdKernelBSpline7DD;
  if (!strcmp("bspln7dd", str))   return nrrdKernelBSpline7DD;
  if (!strcmp("bspl7ddd", str))   return nrrdKernelBSpline7DDD;
  if (!strcmp("bspln7ddd", str))  return nrrdKernelBSpline7DDD;
  return NULL;
}
/* clang-format on */

/* this returns a number between -1 and max;
   it does NOT do the increment-by-one;
   it does NOT do range checking */
static int /* Biff: 1 */
KernelParseTMFInt(int *val, char *str) {
  static const char me[] = "KernelParseTMFInt";

  if (!strcmp("n", str)) {
    *val = -1;
  } else {
    if (1 != sscanf(str, "%d", val)) {
      biffAddf(NRRD, "%s: couldn't parse \"%s\" as int", me, str);
      return 1;
    }
  }
  return 0;
}

int /* Biff: 1 */
nrrdKernelParse(const NrrdKernel **kernelP, double *parm, const char *str_) {
  static const char me[] = "nrrdKernelParse";
  char decibuff[ASP1_S];
  const char *decisep;
  if (!(kernelP && parm && str_)) {
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  if (!(decisep = airDecimalSeparatorLearn(decibuff))) {
    biffAddf(NRRD, "%s: couldn't learn locale's decimal separator", me);
    return 1;
  }

  /* [jorik] (if i understood this correctly) parm is always of length
  ** NRRD_KERNEL_PARM_MAXNUM. We have to clear all parameters here, since
  ** nrrdKernelSet copies all arguments into its own array later, and
  ** copying uninitialised memory is bad (it traps my memory debugger).
  */
  for (uint jj = 0; jj < NRRD_KERNEL_PARM_MAXNUM; jj++) {
    parm[jj] = 0;
  }

  airArray *mop = airMopNew();
  char *str = airStrdup(str_);
  assert(str);
  airMopAdd(mop, str, airFree, airMopAlways);
  char *_pstr = NULL, *pstr = strchr(str, ':');
  if (pstr) {
    *pstr = '\0';
    _pstr = ++pstr;
  }
  char *kstr = airStrdup(str); // just the kernel name since ':'->'\0'
  assert(kstr);
  airMopAdd(mop, kstr, airFree, airMopAlways);
  airToLower(kstr);
  /* first see if its a TMF, then try parsing it as the other stuff */
  if (kstr == strstr(kstr, "tmf")) {
    int tmfD, tmfC, tmfA;
    char *tmfStr[4] = {NULL, NULL, NULL, NULL};
    if (4 == airParseStrS(tmfStr, pstr, ",", 4)) {
      airMopAdd(mop, tmfStr[0], airFree, airMopAlways);
      airMopAdd(mop, tmfStr[1], airFree, airMopAlways);
      airMopAdd(mop, tmfStr[2], airFree, airMopAlways);
      airMopAdd(mop, tmfStr[3], airFree, airMopAlways);
      /* a TMF with a parameter: D,C,A,a */
      if (1 != airStoD(parm, tmfStr[3], decisep)) {
        biffAddf(NRRD, "%s: couldn't parse TMF parameter \"%s\" as double", me,
                 tmfStr[3]);
        airMopError(mop);
        return 1;
      }
    } else if (3 == airParseStrS(tmfStr, pstr, ",", 3)) {
      airMopAdd(mop, tmfStr[0], airFree, airMopAlways);
      airMopAdd(mop, tmfStr[1], airFree, airMopAlways);
      airMopAdd(mop, tmfStr[2], airFree, airMopAlways);
      /* a TMF without a parameter: D,C,A ==> a=0.0 */
      parm[0] = 0.0;
    } else {
      biffAddf(NRRD,
               "%s: TMF kernels require 3 arguments D, C, A "
               "in the form tmf:D,C,A",
               me);
      airMopError(mop);
      return 1;
    }
    if (KernelParseTMFInt(&tmfD, tmfStr[0]) || KernelParseTMFInt(&tmfC, tmfStr[1])
        || KernelParseTMFInt(&tmfA, tmfStr[2])) {
      biffAddf(NRRD,
               "%s: problem parsing \"%s,%s,%s\" as D,C,A "
               "for TMF kernel",
               me, tmfStr[0], tmfStr[1], tmfStr[2]);
      airMopError(mop);
      return 1;
    }
    if (!AIR_IN_CL(-1, tmfD, (int)nrrdKernelTMF_maxD)) {
      biffAddf(NRRD, "%s: derivative value %d outside range [-1,%u]", me, tmfD,
               nrrdKernelTMF_maxD);
      airMopError(mop);
      return 1;
    }
    if (!AIR_IN_CL(-1, tmfC, (int)nrrdKernelTMF_maxC)) {
      biffAddf(NRRD, "%s: continuity value %d outside range [-1,%u]", me, tmfC,
               nrrdKernelTMF_maxC);
      airMopError(mop);
      return 1;
    }
    if (!AIR_IN_CL(1, tmfA, (int)nrrdKernelTMF_maxA)) {
      biffAddf(NRRD, "%s: accuracy value %d outside range [1,%u]", me, tmfA,
               nrrdKernelTMF_maxA);
      airMopError(mop);
      return 1;
    }
    /*
    fprintf(stderr, "!%s: D,C,A = %d,%d,%d --> %d,%d,%d\n", me,
            tmfD, tmfC, tmfA, tmfD+1, tmfC+1, tmfA);
    */
    *kernelP = nrrdKernelTMF[tmfD + 1][tmfC + 1][tmfA];
  } else {
    /* its not a TMF */
    if (!(*kernelP = KernelStrToKern(kstr))) {
      biffAddf(NRRD, "%s: kernel name \"%s\" not recognized", me, kstr);
      airMopError(mop);
      return 1;
    }
    if ((*kernelP)->parmNum > NRRD_KERNEL_PARM_MAXNUM) {
      biffAddf(NRRD, "%s: kernel \"%s\" requests %u parameters > max %u", me, kstr,
               (*kernelP)->parmNum, NRRD_KERNEL_PARM_MAXNUM);
      airMopError(mop);
      return 1;
    }
    uint needParm = 0;
    if ((*kernelP)->parmNum) {
      if ((*kernelP)->parm0IsScale) {
        /* we can make do with one less than the required,
           by using the default spacing 1.0
           (note that TMF kernels are handled separately) */
        needParm = (*kernelP)->parmNum - 1;
      } else {
        /* for this kernel we need all the parameters given explicitly */
        needParm = (*kernelP)->parmNum;
      }
    }
    if (needParm && !pstr) {
      biffAddf(NRRD,
               "%s: didn't get any of %u required parameters after "
               "a (missing) colon in \"%s\"",
               me, needParm, str_);
      airMopError(mop);
      return 1;
    }
    uint haveParm;
    for (haveParm = 0; haveParm < (*kernelP)->parmNum; haveParm++) {
      if (!pstr) break;
      /* pre-C-locale-emulation, code here was:
       *     if (1 != airSingleSscanf(pstr, "%lg", parm+haveParm)) {
       * in which `pstr` is the rest of the comma-separated parms. The old
       * airSingleSscanf handed things off to sscanf, with all the locale-dependency
       * that implies.  But now for C-locale-emulation, airStoD intentionally balks
       * at inputs that contain the non-'.' decisep. So `pstr` under e.g. de_DE would
       * fail to parse even the first value. Having already made a local copy in
       * `str`, we can nul-out and then restore the comma, pre- and post-airStoD.
       */
      char *comma = strchr(pstr, ',');
      if (comma) {
        *comma = '\0';
      }
      int pret = airStoD(parm + haveParm, pstr, decisep);
      if (comma) {
        *comma = ',';
      }
      if (1 != pret) {
        biffAddf(NRRD, "%s: trouble parsing \"%s\" as parameter[%u] (in \"%s\")", me,
                 pstr, haveParm, str_);
        airMopError(mop);
        return 1;
      }
      /* If there was a later comma, `comma` points to it, and we prepare
         for the next iteration by setting pstr to comma+1 */
      if (comma) {
        pstr = comma + 1;
        if (!*pstr) {
          biffAddf(NRRD, "%s: nothing after last comma in \"%s\" (in \"%s\")", me, _pstr,
                   str_);
          airMopError(mop);
          return 1;
        }
      } else {
        pstr = NULL;
      }
    }
    /* haveParm is now the number of parameters that were parsed. */
    if (haveParm < needParm) {
      biffAddf(NRRD,
               "%s: parsed only %u of %u required doubles "
               "from \"%s\" (after ':' in \"%s\")",
               me, haveParm, needParm, _pstr, str_);
      airMopError(mop);
      return 1;
    } else if (haveParm == needParm && needParm == (*kernelP)->parmNum - 1) {
      /* shift up parsed values, and set parm[0] to default */
      for (uint jj = haveParm; jj >= 1; jj--) {
        parm[jj] = parm[jj - 1];
      }
      parm[0] = 1 /* Gone in TeemV2: nrrdDefaultKernelParm0 */;
    } else {
      if (pstr) {
        biffAddf(NRRD,
                 "%s: \"%s\" (after ':' in \"%s\") conveys "
                 "more than %u needed parameter%s",
                 me, _pstr, str_, (*kernelP)->parmNum,
                 1 == (*kernelP)->parmNum ? "" : "s");
        airMopError(mop);
        return 1;
      }
    }
  }
  /*
  fprintf(stderr, "%s: %g %g %g %g %g\n", me,
          parm[0], parm[1], parm[2], parm[3], parm[4]);
  */
  airMopOkay(mop);
  return 0;
}

int /* Biff: 1 */
nrrdKernelSpecParse(NrrdKernelSpec *ksp, const char *str) {
  static const char me[] = "nrrdKernelSpecParse";
  const NrrdKernel *kern;
  double kparm[NRRD_KERNEL_PARM_MAXNUM];

  if (!(ksp && str)) {
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  if (nrrdKernelParse(&kern, kparm, str)) {
    biffAddf(NRRD, "%s: ", me);
    return 1;
  }
  nrrdKernelSpecSet(ksp, kern, kparm);
  return 0;
}

/*
** note that the given string has to be allocated for a certain size
** which is plenty big
*/
int /* Biff: 1 */
nrrdKernelSpecSprint(char str_[AIR_STRLEN_SMALL + 1], const NrrdKernelSpec *ksp) {
  static const char me[] = "nrrdKernelSpecSprint";
  char decibuff[ASP1_S], nbuff[ASP1_S];
  const char *decisep;
  char *str = str_;
  size_t strSize = ASP1_S;

  if (!(str_ && ksp)) {
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  if (!(decisep = airDecimalSeparatorLearn(decibuff))) {
    biffAddf(NRRD, "%s: couldn't learn locale's decimal separator", me);
    return 1;
  }
  if (strstr(ksp->kernel->name, "TMF")) {
    /* these are handled differently; the identification of the
       kernel is actually packaged as kernel parameters */
    if (!(ksp->kernel->name == strstr(ksp->kernel->name, "TMF"))) {
      biffAddf(NRRD, "%s: TMF kernel name %s didn't start with TMF", me,
               ksp->kernel->name);
      return 1;
    }
    /* 0123456789012 */
    /* TMF_dX_cX_Xef */
    if (!(13 == strlen(ksp->kernel->name) && '_' == ksp->kernel->name[3]
          && '_' == ksp->kernel->name[6] && '_' == ksp->kernel->name[9])) {
      biffAddf(NRRD, "%s: sorry, expected strlen(%s) = 13 with 3 _s", me,
               ksp->kernel->name);
      return 1;
    }
    SN_PRINTF(str, strSize, "tmf:%c,%c,%c", ksp->kernel->name[5], ksp->kernel->name[8],
              ksp->kernel->name[10]);
    /* see if the single parm should be added on */
    if (0.0 != ksp->parm[0]) {
      snprintf(str, strSize, ",%s", airDtoS(nbuff, ksp->parm[0], decisep));
      SN_INCR(str, strSize);
    }
  } else {
    SN_COPY(str, strSize, ksp->kernel->name);
    uint parmNum = ksp->kernel->parmNum, p1st = 0;
    if (parmNum && ksp->kernel->parm0IsScale && ksp->parm[0] == 1) {
      // actually we'll skip printing the first scaling parameter since it is
      // default==1
      p1st++;
    }
    if (parmNum) {
      for (uint pi = p1st; pi < parmNum; pi++) {
        SN_COPY(str, strSize, pi == p1st ? ":" : ",");
        airStrcpy(str, strSize, airDtoS(nbuff, ksp->parm[pi], decisep));
        SN_INCR(str, strSize);
      }
    }
  }
  if (strSize <= 1) { // actually the SN_ macros will never let strSize go below 1
    biffAddf(NRRD,
             "%s: kernel name (len %zu) or its %u parms "
             "maxed out %u-size destination buffer",
             me, strlen(ksp->kernel->name), ksp->kernel->parmNum, ASP1_L);
    return 1;
  }
  return 0;
}

int /* Biff: 1 */
nrrdKernelSprint(char str[AIR_STRLEN_SMALL + 1], const NrrdKernel *kernel,
                 const double kparm[NRRD_KERNEL_PARM_MAXNUM]) {
  static const char me[] = "nrrdKernelSprint";
  NrrdKernelSpec ksp;

  nrrdKernelSpecSet(&ksp, kernel, kparm);
  if (nrrdKernelSpecSprint(str, &ksp)) {
    biffAddf(NRRD, "%s: trouble", me);
    return 1;
  }
  return 0;
}

/*
** This DOES make an effort to set *differ based on "ordering" (-1 or +1)
** but HEY that's very contrived; why bother?
*/
int /* Biff: 1 */
nrrdKernelCompare(const NrrdKernel *kernA, const double parmA[NRRD_KERNEL_PARM_MAXNUM],
                  const NrrdKernel *kernB, const double parmB[NRRD_KERNEL_PARM_MAXNUM],
                  int *differ, char explain[AIR_STRLEN_LARGE + 1]) {
  static const char me[] = "nrrdKernelCompare";
  unsigned int pnum, pidx;

  if (!(kernA && kernB && differ)) {
    biffAddf(NRRD, "%s: got NULL pointer (%p, %p, or %p)", me, AIR_CVOIDP(kernA),
             AIR_CVOIDP(kernB), AIR_VOIDP(differ));
    return 1;
  }
  if (kernA != kernB) {
    *differ = kernA < kernB ? -1 : 1;
    if (explain) {
      snprintf(explain, ASP1_L, "kernA %s kernB", *differ < 0 ? "<" : ">");
    }
    return 0;
  }
  pnum = kernA->parmNum;
  if (!pnum) {
    /* actually, we're done: no parms and kernels are equal */
    *differ = 0;
    return 0;
  }
  if (!(parmA && parmB)) {
    biffAddf(NRRD, "%s: kernel %s needs %u parms but got NULL parm vectors", me,
             kernA->name, pnum);
    return 1;
  }
  for (pidx = 0; pidx < pnum; pidx++) {
    if (parmA[pidx] != parmB[pidx]) {
      *differ = parmA[pidx] < parmB[pidx] ? -1 : 1;
      if (explain) {
        snprintf(explain, ASP1_L, "parmA[%u]=%f %s parmB[%u]=%f", pidx, parmA[pidx],
                 *differ < 0 ? "<" : ">", pidx, parmB[pidx]);
      }
      return 0;
    }
  }

  /* so far nothing unequal */
  *differ = 0;
  return 0;
}

/*
** This DOES NOT make an effort to set *differ based on "ordering";
*/
int /* Biff: 1 */
nrrdKernelSpecCompare(const NrrdKernelSpec *aa, const NrrdKernelSpec *bb, int *differ,
                      char explain[AIR_STRLEN_LARGE + 1]) {
  static const char me[] = "nrrdKernelSpecCompare";
  char subexplain[ASP1_L];

  if (!(differ)) {
    biffAddf(NRRD, "%s: got NULL differ", me);
    return 1;
  }
  if (!!aa != !!bb) {
    if (explain) {
      snprintf(explain, ASP1_L, "different NULL-ities of kspec itself %s != %s",
               aa ? "non-NULL" : "NULL", bb ? "non-NULL" : "NULL");
    }
    *differ = 1;
    return 0;
  }
  if (!aa) {
    /* got two NULL kernel specs ==> equal */
    *differ = 0;
    return 0;
  }
  if (!!aa->kernel != !!bb->kernel) {
    if (explain) {
      snprintf(explain, ASP1_L, "different NULL-ities of kspec->kernel %s != %s",
               aa->kernel ? "non-NULL" : "NULL", bb->kernel ? "non-NULL" : "NULL");
    }
    *differ = 1;
    return 0;
  }
  if (!aa->kernel) {
    /* both kernels NULL, can't do anything informative with parms */
    *differ = 0;
    return 0;
  }
  if (nrrdKernelCompare(aa->kernel, aa->parm, bb->kernel, bb->parm, differ,
                        subexplain)) {
    biffAddf(NRRD, "%s: trouble comparing kernels", me);
    return 1;
  }
  if (*differ) {
    if (explain) {
      snprintf(explain, ASP1_L, "kern/parm pairs differ: %s",
               airStrunc(subexplain, sizeof(subexplain), ASP1_S));
    }
    *differ = 1; /* losing ordering info (of dubious value) */
    return 0;
  }
  *differ = 0;
#undef ASL
  return 0;
}

/*
******** nrrdKernelCheck
**
** Makes sure a given kernel is behaving as expected
**
** Tests:
** nrrdKernelSprint
** nrrdKernelParse
** nrrdKernelCompare
** nrrdKernelSpecNew
** nrrdKernelSpecNix
** nrrdKernelSpecSet
** nrrdKernelSpecSprint
** nrrdKernelSpecParse
** nrrdKernelDerivative
** and also exercises all the ways of evaluating the kernel and
** makes sure they all agree, and agree with the integral kernel (if given)
** the derivative kernel (if known by nrrdKernelDerivative)
** HEY why do we need a separate parameter vector iparm? nrrdKernelDerivative is
** based on idea that the parm vec is the same for a kernel and its derivative
*/
int /* Biff: 1 */
nrrdKernelCheck(const NrrdKernel *kern, const double parm[NRRD_KERNEL_PARM_MAXNUM],
                size_t evalNum, double epsilon, unsigned int diffOkEvalMax,
                unsigned int diffOkIntglOrDerivMax, const NrrdKernel *ikern,
                const double iparm[NRRD_KERNEL_PARM_MAXNUM]) {
  static const char me[] = "nrrdKernelCheck";
  const NrrdKernel *parsedkern, *kernD;
  double parsedparm[NRRD_KERNEL_PARM_MAXNUM], supp, integral;
  char kstr[AIR_STRLEN_LARGE + 1], explain[AIR_STRLEN_LARGE + 1],
    stmp[AIR_STRLEN_SMALL + 1], kdstr[AIR_STRLEN_LARGE + 1];
  int differ;
  size_t evalIdx;
  double *dom_d, *ran_d, wee;
  float *dom_f, *ran_f;
  unsigned int diffEvalNum, diffIntglNum, diffDerivNum;
  NrrdKernelSpec *kspA, *kspB;
  airArray *mop;

  /* got tired of seeing these two lines over and over */
#define ERRR                                                                            \
  airMopError(mop);                                                                     \
  return 1

  mop = airMopNew();
  if (!kern) {
    biffAddf(NRRD, "%s: got NULL kernel", me);
    ERRR;
  }
  if (!(evalNum > 20)) {
    biffAddf(NRRD, "%s: need evalNum > 20", me);
    ERRR;
  }
  if (!(kern->support && kern->integral && kern->eval1_f && kern->evalN_f
        && kern->eval1_d && kern->evalN_d)) {
    biffAddf(NRRD, "%s: kernel has NULL fields (%d,%d,%d,%d,%d,%d)", me,
             !!(kern->support), !!(kern->integral), !!(kern->eval1_f), !!(kern->evalN_f),
             !!(kern->eval1_d), !!(kern->evalN_d));
    ERRR;
  }
  kspA = nrrdKernelSpecNew();
  airMopAdd(mop, kspA, nrrdKernelSpecNix_mop, airMopAlways);
  kspB = nrrdKernelSpecNew();
  airMopAdd(mop, kspB, nrrdKernelSpecNix_mop, airMopAlways);
  nrrdKernelSpecSet(kspA, kern, parm);
  char kspstr[AIR_STRLEN_SMALL + 1];
  if (nrrdKernelSprint(kstr, kern, parm) || nrrdKernelSpecSprint(kspstr, kspA)) {
    biffAddf(NRRD, "%s: trouble", me);
    ERRR;
  }
  if (strcmp(kstr, kspstr)) {
    biffAddf(NRRD, "%s: sprinted kernel |%s| != kspec |%s|", me, kstr, kspstr);
    ERRR;
  }
  if (nrrdKernelParse(&parsedkern, parsedparm, kstr)
      || nrrdKernelSpecParse(kspB, kstr)) {
    biffAddf(NRRD, "%s: trouble parsing |%s| back to kern/parm pair or kspec", me, kstr);
    ERRR;
  }
  if (nrrdKernelCompare(kern, parm, parsedkern, parsedparm, &differ, explain)) {
    biffAddf(NRRD, "%s: trouble comparing kern/parm pairs", me);
    ERRR;
  }
  if (differ) {
    biffAddf(NRRD, "%s: given and re-parsed kernels differ: %s", me, explain);
    ERRR;
  }
  if (nrrdKernelCompare(kspA->kernel, kspA->parm, kspB->kernel, kspB->parm, &differ,
                        explain)) {
    biffAddf(NRRD, "%s: trouble comparing kspecs", me);
    ERRR;
  }
  if (differ) {
    biffAddf(NRRD, "%s: given and re-parsed kspecs differ: %s", me, explain);
    ERRR;
  }

  supp = kern->support(parm);
  /* kernel should be non-zero outside [-supp, supp] */
  /* wee is the step between evaluation points with
     evalNum cell-centered samples in [-supp,supp] */
  wee = 2 * supp / AIR_CAST(double, evalNum);
  if ((kern->eval1_d)(supp + wee / 1000, parm) || (kern->eval1_d)(supp + wee, parm)
      || (kern->eval1_d)(supp + 10 * wee, parm)
      || (kern->eval1_d)(-supp - wee / 1000, parm) || (kern->eval1_d)(-supp - wee, parm)
      || (kern->eval1_d)(-supp - 10 * wee, parm)) {
    if (nrrdKernelCheap != kern) {
      /* the "cheap" kernel alone gets a pass on reporting its support */
      biffAddf(NRRD, "%s: kern %s is non-zero outside support %g", me, kstr, supp);
      ERRR;
    }
  }
  /* allocate domain and range for both float and double */
  dom_d = AIR_CALLOC(evalNum, double);
  airMopAdd(mop, dom_d, airFree, airMopAlways);
  ran_d = AIR_CALLOC(evalNum, double);
  airMopAdd(mop, ran_d, airFree, airMopAlways);
  dom_f = AIR_CALLOC(evalNum, float);
  airMopAdd(mop, dom_f, airFree, airMopAlways);
  ran_f = AIR_CALLOC(evalNum, float);
  airMopAdd(mop, ran_f, airFree, airMopAlways);
  if (!(dom_d && ran_d && dom_f && ran_f)) {
    biffAddf(NRRD, "%s: couldn't alloc buffers for %s values for %s", me,
             airSprintSize_t(stmp, evalNum), kstr);
    ERRR;
  }
  for (evalIdx = 0; evalIdx < evalNum; evalIdx++) {
    dom_d[evalIdx] = AIR_AFFINE(-0.5, evalIdx, AIR_CAST(double, evalNum) - 0.5, -supp,
                                supp);
    dom_f[evalIdx] = AIR_FLOAT(dom_d[evalIdx]);
  }
  /* do the vector evaluations */
  kern->evalN_f(ran_f, dom_f, evalNum, parm);
  kern->evalN_d(ran_d, dom_d, evalNum, parm);
  /*
  for (evalIdx=0; evalIdx<evalNum; evalIdx++) {
    fprintf(stderr, "%u %g --> %g\n", AIR_UINT(evalIdx),
            dom_d[evalIdx], ran_d[evalIdx]);
  }
  */
  kernD = nrrdKernelDerivative(kern);
  if (nrrdKernelFlag == kernD) {
    /* nrrdKernelDerivative is not confident about knowing derivative of kern */
    kernD = NULL;
  } else { /* set kdstr to string specification of kernD and (same) parm */
    if (nrrdKernelSprint(kdstr, kernD, parm)) {
      biffAddf(NRRD, "%s: trouble", me);
      ERRR;
    }
  }
  if (ikern) {
    const NrrdKernel *ikd;
    ikd = nrrdKernelDerivative(ikern);
    if (ikd != kern) {
      biffAddf(NRRD, "%s: got nrrdKernelDerivative(ikern=%s) = %s != %s=kern", me,
               ikern->name, ikd->name, kern->name);
      ERRR;
    }
  }
  /* compare evaluations (and maybe derivatives) and numerically xcompute
     integral */
  diffEvalNum = 0;
  diffIntglNum = 0;
  diffDerivNum = 0;
  integral = 0.0;
  for (evalIdx = 0; evalIdx < evalNum; evalIdx++) {
    double single_f, single_d;
    single_f = kern->eval1_f(dom_f[evalIdx], parm);
    single_d = kern->eval1_d(dom_d[evalIdx], parm);
    integral += single_d;
    /* single float vs vector float */
    if (nrrdKernelForwDiff == kern || nrrdKernelBCCubic == kern
        || nrrdKernelBCCubicDD == kern || nrrdKernelAQuarticDD == kern) {
      if (fabs(single_f - ran_f[evalIdx]) > 0.0) {
        biffAddf(NRRD,
                 "%s: %s (eval1_f(%.17g)=%.17g) != "
                 "(evalN_f(%.17g)=%.17g) by %.17g > %.17g",
                 me, kstr, dom_f[evalIdx], single_f, dom_f[evalIdx], ran_f[evalIdx],
                 fabs(single_f - ran_f[evalIdx]), 0.0);
        ERRR;
      }
    } else { /* for most well-behaved kernels */
      if (single_f != ran_f[evalIdx]) {
        biffAddf(NRRD,
                 "%s: %s (eval1_f(%.17g)=%.17g) != "
                 "(evalN_f(%.17g)=%.17g)",
                 me, kstr, dom_f[evalIdx], single_f, dom_f[evalIdx], ran_f[evalIdx]);
        ERRR;
      }
    }
    /* single double vs vector double */
    if (single_d != ran_d[evalIdx]) {
      biffAddf(NRRD, "%s: %s (eval1_d(%.17g)=%.17g) != (evalN_d(%.17g)=%.17g)", me, kstr,
               dom_d[evalIdx], single_d, dom_d[evalIdx], ran_d[evalIdx]);
      ERRR;
    }
    /* single float vs single double */
    if (fabs(single_f - single_d) > epsilon) {
      diffEvalNum++;
      if (diffEvalNum > diffOkEvalMax) {
        biffAddf(NRRD,
                 "%s: %s |eval1_f(%.17g)=%.17g) - (eval1_d(%.17g)=%.17g)|="
                 "%.17g  >  epsilon %.17g too many times (%u > %u)",
                 me, kstr, dom_f[evalIdx], single_f, dom_d[evalIdx], single_d,
                 fabs(single_f - single_d), epsilon, diffEvalNum, diffOkEvalMax);
        ERRR;
      }
    }
    /* check whether we're the derivative of ikern, not with an absolute per-sample test,
       but by seeing if the number of times it failed the per-sample test exceeded the
       passed threshold number. GLK does not remember exactly how or why this testing
       approach evolved; but it probably relates to how thinks get wonky at the
       boundaries of piece-wise definitions */
    if (ikern) {
      double forw, back, ndrv;
      forw = ikern->eval1_d(dom_d[evalIdx] + wee / 2, iparm);
      back = ikern->eval1_d(dom_d[evalIdx] - wee / 2, iparm);
      ndrv = (forw - back) / wee;
      if (fabs(ndrv - single_d) > epsilon) {
        diffIntglNum++;
        if (diffIntglNum > diffOkIntglOrDerivMax) {
          biffAddf(NRRD,
                   "%s: %s(%.17g) |numerical deriv of integral(%s) %.17g - %.17g|="
                   "%.17g > %.17g too many times (%u > %u)",
                   me, kstr, dom_d[evalIdx], ikern->name, ndrv, single_d,
                   fabs(ndrv - single_d), epsilon, diffIntglNum, diffOkIntglOrDerivMax);
          ERRR;
        }
      }
    }
    if (kernD) {
      /* HEY copy-pasta from above, now differentiating given kern to see if it's
         derivative is kernD */
      double forw, back, ndrv, dsingle; /* dsingle is single eval of derivative kernel */
      forw = kern->eval1_d(dom_d[evalIdx] + wee / 2, parm);
      back = kern->eval1_d(dom_d[evalIdx] - wee / 2, parm);
      ndrv = (forw - back) / wee;
      dsingle = kernD->eval1_d(dom_d[evalIdx], parm);
      if (fabs(ndrv - dsingle) > epsilon) {
        diffDerivNum++;
        if (diffDerivNum > diffOkIntglOrDerivMax) {
          biffAddf(NRRD,
                   "%s: %s(%.17g) |numerical deriv(%s) %.17g - %.17g=%s(%.17g)|="
                   "%.17g > %.17g too many times (%u > %u)",
                   me, kstr, dom_d[evalIdx], kern->name, ndrv, dsingle, kdstr,
                   dom_d[evalIdx], fabs(ndrv - dsingle), epsilon, diffDerivNum,
                   diffOkIntglOrDerivMax);
          ERRR;
        }
      }
    }
  }
  integral *= 2 * supp / (AIR_CAST(double, evalNum));
  /* the "cheap" kernel alone gets a pass on reporting its integral */
  if (nrrdKernelCheap != kern) {
    double hackeps = 10;
    /* hackeps is clearly a hack to permit the integral to have greater
       error than any single evaluation; there must be a more principled
       way to set this */
    if (fabs(integral - kern->integral(parm)) > hackeps * epsilon) {
      biffAddf(NRRD,
               "%s: %s |numerical integral %.17g - claimed %.17g| "
               "%.17g > %.17g",
               me, kstr, integral, kern->integral(parm),
               fabs(integral - kern->integral(parm)), hackeps * epsilon);
      ERRR;
    }
  }
#undef ERRR

  airMopOkay(mop);
  return 0;
}

/* nrrdKernelParm0IsScale answers the question: does the given kernel look at parm[0] to
 * determine the over-all size of the kernel?  In the early days of nrrd, all kernels did
 * this, but gradually the utility of doing that became less obvious, and for
 * simplicity's sake newer kernels were parameter-free. The kernels listed here thus tend
 * to be the older kernels in nrrd.
 *
 * For TeemV2, the kernels gained a new parm0IsScale field, and meetNrrdKernelAllCheck()
 * verified that it was set correctly w.r.t this function. Then, since this function
 * became superfluous, it was removed.

 * int / * Biff: nope * /
 * nrrdKernelParm0IsScale(const NrrdKernel *kern) {
 *   int ret;
 *
 *   if (!kern) {
 *       ret = 0;
 *   } else if (nrrdKernelZero == kern
 *              || nrrdKernelBox == kern || nrrdKernelCheap == kern
 *              || nrrdKernelTent == kern || nrrdKernelForwDiff == kern
 *              || nrrdKernelCentDiff == kern || nrrdKernelBCCubic == kern
 *              || nrrdKernelBCCubicD == kern || nrrdKernelBCCubicDD == kern
 *              || nrrdKernelAQuartic == kern || nrrdKernelAQuarticD == kern
 *              || nrrdKernelAQuarticDD == kern || nrrdKernelGaussian == kern
 *              || nrrdKernelGaussianD == kern || nrrdKernelGaussianDD == kern
 *              || nrrdKernelDiscreteGaussian == kern || nrrdKernelHann == kern
 *              || nrrdKernelHannD == kern || nrrdKernelHannDD == kern
 *              || nrrdKernelBlackman == kern || nrrdKernelBlackmanD == kern
 *              || nrrdKernelBlackmanDD == kern) {
 *     / * nrrdKernelFlag should maybe be part of this, but it's a flag, not a kernel * /
 *     ret = 1;
 *   } else {
 *     ret = 0;
 *   }
 *   return ret;
 * }
 */

/* Given a kernel, make an attempt to return its derivative,
  but if can't figure it out return nrrdKernelFlag.
  The intent is that whatever parameters the given kernel takes; they will
  also be the right parameters for the returned derivative kernel.
  NOTE: This is probably returning nrrdKernelFlag more often than necessary;
  but sorting this out may require a more careful re-assessment of what has
  really become a mess of kernels */
/* clang-format off */
const NrrdKernel * /* Biff: nope */
nrrdKernelDerivative(const NrrdKernel *kern) {
  if (!kern) return nrrdKernelFlag;
  if (nrrdKernelAQuartic == kern)     return nrrdKernelAQuarticD;
  if (nrrdKernelAQuarticD == kern)    return nrrdKernelAQuarticDD;
  if (nrrdKernelAQuarticDD == kern)   return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelBCCubic == kern)      return nrrdKernelBCCubicD;
  if (nrrdKernelBCCubicD == kern)     return nrrdKernelBCCubicDD;
  if (nrrdKernelBCCubicDD == kern)    return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelBSpline1 == kern)     return nrrdKernelBSpline1D;
  if (nrrdKernelBSpline1D == kern)    return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelBSpline2 == kern)     return nrrdKernelBSpline2D;
  if (nrrdKernelBSpline2D == kern)    return nrrdKernelBSpline2DD;
  if (nrrdKernelBSpline2DD == kern)   return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelBSpline3ApproxInverse == kern) return nrrdKernelFlag; /* !!! */
  if (nrrdKernelBSpline3 == kern)     return nrrdKernelBSpline3D;
  if (nrrdKernelBSpline3D == kern)    return nrrdKernelBSpline3DD;
  if (nrrdKernelBSpline3DD == kern)   return nrrdKernelBSpline3DDD;
  if (nrrdKernelBSpline3DDD == kern)  return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelBSpline4 == kern)     return nrrdKernelBSpline4D;
  if (nrrdKernelBSpline4D == kern)    return nrrdKernelBSpline4DD;
  if (nrrdKernelBSpline4DD == kern)   return nrrdKernelBSpline4DDD;
  if (nrrdKernelBSpline4DDD == kern)  return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelBSpline5ApproxInverse == kern) return nrrdKernelFlag; /* !!! */
  if (nrrdKernelBSpline5 == kern)     return nrrdKernelBSpline5D;
  if (nrrdKernelBSpline5D == kern)    return nrrdKernelBSpline5DD;
  if (nrrdKernelBSpline5DD == kern)   return nrrdKernelBSpline5DDD;
  if (nrrdKernelBSpline5DDD == kern)  return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelBSpline6 == kern)     return nrrdKernelBSpline6D;
  if (nrrdKernelBSpline6D == kern)    return nrrdKernelBSpline6DD;
  if (nrrdKernelBSpline6DD == kern)   return nrrdKernelBSpline6DDD;
  if (nrrdKernelBSpline6DDD == kern)  return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelBSpline7ApproxInverse == kern) return nrrdKernelFlag; /* !!! */
  if (nrrdKernelBSpline7 == kern)     return nrrdKernelBSpline7D;
  if (nrrdKernelBSpline7D == kern)    return nrrdKernelBSpline7DD;
  if (nrrdKernelBSpline7DD == kern)   return nrrdKernelBSpline7DDD;
  if (nrrdKernelBSpline7DDD == kern)  return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelBlackman == kern)     return nrrdKernelBlackmanD;
  if (nrrdKernelBlackmanD == kern)    return nrrdKernelBlackmanDD;
  if (nrrdKernelBlackmanDD == kern)   return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelBox == kern)          return nrrdKernelZero; /* really */
  if (nrrdKernelBoxSupportDebug == kern) return nrrdKernelFlag;       /* !!! */
  if (nrrdKernelC3Quintic == kern)    return nrrdKernelC3QuinticD;
  if (nrrdKernelC3QuinticD == kern)   return nrrdKernelC3QuinticDD;
  if (nrrdKernelC3QuinticDD == kern)  return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelC4HexicApproxInverse == kern) return nrrdKernelFlag;  /* !!! */
  if (nrrdKernelC4Hexic == kern)      return nrrdKernelC4HexicD;
  if (nrrdKernelC4HexicD == kern)     return nrrdKernelC4HexicDD;
  if (nrrdKernelC4HexicDD == kern)    return nrrdKernelC4HexicDDD;
  if (nrrdKernelC4HexicDDD == kern)   return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelC5SepticApproxInverse == kern) return nrrdKernelFlag; /* !!! */
  if (nrrdKernelC5Septic == kern)     return nrrdKernelC5SepticD;
  if (nrrdKernelC5SepticD == kern)    return nrrdKernelC5SepticDD;
  if (nrrdKernelC5SepticDD == kern)   return nrrdKernelC5SepticDDD;
  if (nrrdKernelC5SepticDDD == kern)  return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelCatmullRom == kern)   return nrrdKernelCatmullRomD;
  if (nrrdKernelCatmullRomD == kern)  return nrrdKernelCatmullRomDD;
  if (nrrdKernelCatmullRomDD == kern) return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelCatmullRomSupportDebug == kern)   return nrrdKernelCatmullRomSupportDebugD;
  if (nrrdKernelCatmullRomSupportDebugD == kern)  return nrrdKernelCatmullRomSupportDebugDD;
  if (nrrdKernelCatmullRomSupportDebugDD == kern) return nrrdKernelFlag; /* !!! */
  if (nrrdKernelCentDiff == kern)     return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelCheap == kern)        return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelCos4SupportDebug == kern) return nrrdKernelCos4SupportDebugD;
  if (nrrdKernelCos4SupportDebugD == kern) return nrrdKernelCos4SupportDebugDD;
  if (nrrdKernelCos4SupportDebugDD == kern) return nrrdKernelCos4SupportDebugDDD;
  if (nrrdKernelCos4SupportDebugDDD == kern) return nrrdKernelFlag;   /* !!! */
  if (nrrdKernelDiscreteGaussian == kern) return nrrdKernelFlag;      /* !!! */
  if (nrrdKernelTent == kern)         return nrrdKernelForwDiff;
  if (nrrdKernelForwDiff == kern)     return nrrdKernelZero; /* really */
  if (nrrdKernelGaussian == kern)     return nrrdKernelGaussianD;
  if (nrrdKernelGaussianD == kern)    return nrrdKernelGaussianDD;
  if (nrrdKernelGaussianDD == kern)   return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelHann == kern)         return nrrdKernelHannD;
  if (nrrdKernelHannD == kern)        return nrrdKernelHannDD;
  if (nrrdKernelHannDD == kern)       return nrrdKernelFlag;          /* !!! */
  if (nrrdKernelHermiteScaleSpaceFlag == kern) return nrrdKernelFlag; /* !!! */
  if (nrrdKernelZero == kern)         return nrrdKernelZero; /* really */
  /* else: includes nrrdKernelTMF[][][] and nrrdKernelFlag itself */
  return nrrdKernelFlag; /* !!! */
}
/* clang-format on */
