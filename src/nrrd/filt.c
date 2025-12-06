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

/* learned example of cross-architecture floating-point confusion:
In cheapMedian2D, this line:
     hist[INDEX(nin, range, lup, I + X + sx * (J + Y), bins, val)]
              += wt[I + radius] * wt[J + radius];
on x86_64 (with -O3 but no fast math) had the same wt[] factors and
  the same hist[] index and hence the same hist[idx] increment,
BUT, after three such incements the stored hist[] value was a *different* float
than after the same three increments on arm64.
This caused great confusion when comparing old unu (svn r6382)
with new TeemV2 unu with the new teem-src/tests/tzt/nrrd/cmedian.zsh
tests.  By accident the old unu binary was compiled with "-arch x86_64"
since was using the old pre-ARM make files.
About 10% of the tests failed because of this difference. */
#include "nrrd.h"
#include "privateNrrd.h"

/* HEY maybe should be unsigned? */
static uint
CM_median(const float *hist, float half) {
  float sum = 0;
  const float *hpt = hist;

  while (sum < half)
    sum += *hpt++;

  return AIR_UINT(hpt - 1 - hist);
}

static uint
CM_mode(const float *hist, uint bins) {
  float max = 0;
  uint mi = UINT_MAX;
  for (uint i = 0; i < bins; i++) {
    if (hist[i] && (!max || hist[i] > max)) {
      max = hist[i];
      mi = i;
    }
  }
  return mi;
}

#define INDEX(nin, range, lup, idxIn, bins, val)                                        \
  (val = (lup)((nin)->data, (idxIn)), airIndex((range)->min, (val), (range)->max, bins))

#define SpU(S, U) (AIR_UINT(S + AIR_INT(U)))

#if 0
static void
CM_printhist(const float *hist, uint bins, const char *desc) {
  fprintf(stderr, "%s: (%u bins)\n", desc, bins);
  for (uint i = 0; i < bins; i++) {
    if (hist[i]) {
      fprintf(stderr, "%c   %u: %.9g\n", desc[0], i, hist[i]);
    }
  }
}
#endif

static float *
CM_wtAlloc(uint radius, float wght) {
  uint diam = 2 * radius + 1;
  float *wt = AIR_CALLOC(diam, float);
  wt[radius] = 1.0;
  for (uint r = 1; r <= radius; r++) {
    wt[radius - r] = wt[radius + r] = AIR_FLOAT(pow(1.0 / wght, r));
  }
  float sum = 0.0;
  for (uint r = 0; r < diam; r++) {
    sum += wt[r];
  }
  for (uint r = 0; r < diam; r++) {
    wt[r] /= sum;
  }
  /*
  for (uint r=0; r<diam; r++) {
    fprintf(stderr, "!%s: wt[%u] = %g\n", "CM_wtAlloc", r, wt[r]);
  }
  */
  return wt;
}

static void
cheapMedian1D(Nrrd *nout, const Nrrd *nin, const NrrdRange *range, uint radius,
              float wght, uint bins, int mode, float *hist) {
  /* // for debugging non-commutativity of flipping and filtering
  char *evs = getenv("_DBI");
  uint DBI = UINT_MAX;
  if (evs) {
    uint tmpui;
    if (1 == sscanf(evs, "%u", &tmpui)) {
      DBI = tmpui;
    }
    // else fprintf(stderr, "!%s: failed to get or parse envvar DBI\n", __func__);
  }
  if (DBI != UINT_MAX) {
    fprintf(stderr, "!%s: DBI=%u\n", __func__, DBI);
  }
  */
  // static const char me[] = "cheapMedian1D";
  uint diam = 2 * radius + 1;
  double (*lup)(const void *, size_t) = nrrdDLookup[nin->type];
  size_t num = nrrdElementNumber(nin);
  int mrad = -AIR_INT(radius);
  int prad = AIR_INT(radius);
  float half;
  double val;
  uint idx;
  if (1 == wght) {
    /* uniform weighting-> can do sliding histogram optimization */
    /* initialize histogram */
    half = AIR_FLOAT(diam / 2 + 1);
    memset(hist, 0, bins * sizeof(float));
    for (uint X = 0; X < diam; X++) {
      hist[INDEX(nin, range, lup, X, bins, val)]++;
    }
    /* nrrd__CM_printhist(hist, bins, "after init"); */
    /* find median at each point using existing histogram */
    for (uint X = radius; X < num - radius; X++) {
      /* nrrd__CM_printhist(hist, bins, "----------"); */
      idx = mode ? CM_mode(hist, bins) : CM_median(hist, half);
      val = NRRD_NODE_POS(range->min, range->max, bins, idx);
      /* printf(" median idx = %d -> val = %g\n", idx, val); */
      nrrdDInsert[nout->type](nout->data, X, val);
      /* probably update histogram for next iteration */
      if (X < num - radius - 1) {
        hist[INDEX(nin, range, lup, X + radius + 1, bins, val)]++;
        hist[INDEX(nin, range, lup, X - radius, bins, val)]--;
      }
    }
  } else {
    /* non-uniform weighting --> slow and stupid */
    float *wt = CM_wtAlloc(radius, wght);
    half = 0.5;
    for (uint X = radius; X < num - radius; X++) {
      // int DB = (X == DBI);
      memset(hist, 0, bins * sizeof(float));
      for (int I = mrad; I <= prad; I++) {
        hist[INDEX(nin, range, lup, SpU(I, X), bins, val)] += wt[SpU(I, radius)];
        /*
        uint Ipr = SpU(I, radius);
        float ww = wt[Ipr];
        uint hidx = INDEX(nin, range, lup, SpU(I, X), bins, val);
        hist[hidx] += ww;
        if (DB && hidx == 49) {
          fprintf(stderr,
                  "!%s: X=%u I=%d --> Ipr=%u ww=%.9g hidx=%u hist[hidx] --> %.9g\n",
                  __func__, X, I, Ipr, ww, hidx, hist[hidx]);
        }
        */
      }
      /*
      if (DB) {
        fprintf(stderr, "!%s: hist at DBI=%u\n", __func__, DBI);
        CM_printhist(hist, bins, "before");
      }
      */
      idx = mode ? CM_mode(hist, bins) : CM_median(hist, half);
      val = NRRD_NODE_POS(range->min, range->max, bins, idx);
      /*
      if (DB) {
        fprintf(stderr, "!%s: mode=%d --> idx=%u --> val=%.17g\n", __func__, mode, idx,
                val);
      }
      */
      nrrdDInsert[nout->type](nout->data, X, val);
    }
    free(wt);
  }
}

// HEY yikes copy-pasta of cheapMedian1D
static void
cheapMedian2D(Nrrd *nout, const Nrrd *nin, const NrrdRange *range, uint radius,
              float wght, uint bins, int mode, float *hist) {
  // static const char me[] = "cheapMedian2D";
  uint diam = 2 * radius + 1;
  double (*lup)(const void *, size_t) = nrrdDLookup[nin->type];
  size_t sx = nin->axis[0].size;
  size_t sy = nin->axis[1].size;
  int mrad = -AIR_INT(radius);
  int prad = AIR_INT(radius);
  float half;
  double val;
  uint idx;
  if (1 == wght) {
    /* uniform weighting-> can do sliding histogram optimization */
    half = AIR_FLOAT(diam * diam / 2 + 1);
    for (uint Y = radius; Y < sy - radius; Y++) {
      /* initialize histogram */
      memset(hist, 0, bins * sizeof(float));
      uint X = radius;
      for (int J = mrad; J <= prad; J++) {
        for (int I = mrad; I <= prad; I++) {
          hist[INDEX(nin, range, lup, SpU(I, X) + sx * SpU(J, Y), bins, val)]++;
        }
      }
      /* nrrd__CM_printhist(hist, bins, "after init"); */
      /* find median at each point using existing histogram */
      for (X = radius; X < sx - radius; X++) {
        idx = mode ? CM_mode(hist, bins) : CM_median(hist, half);
        val = NRRD_NODE_POS(range->min, range->max, bins, idx);
        nrrdDInsert[nout->type](nout->data, X + sx * Y, val);
        /* probably update histogram for next iteration */
        if (X < sx - radius - 1) {
          for (int J = mrad; J <= prad; J++) {
            hist[INDEX(nin, range, lup, X + radius + 1 + sx * SpU(J, Y), bins, val)]++;
            hist[INDEX(nin, range, lup, X - radius + sx * SpU(J, Y), bins, val)]--;
          }
        }
      }
    }
  } else {
    /* non-uniform weighting --> slow and stupid */
    float *wt = CM_wtAlloc(radius, wght);
    half = 0.5;
    for (uint Y = radius; Y < sy - radius; Y++) {
      for (uint X = radius; X < sx - radius; X++) {
        memset(hist, 0, bins * sizeof(float));
        for (int J = mrad; J <= prad; J++) {
          for (int I = mrad; I <= prad; I++) {
            hist[INDEX(nin, range, lup, SpU(I, X) + sx * SpU(J, Y), bins, val)]
              += wt[I + prad] * wt[J + prad];
          }
        }
        idx = mode ? CM_mode(hist, bins) : CM_median(hist, half);
        val = NRRD_NODE_POS(range->min, range->max, bins, idx);
        nrrdDInsert[nout->type](nout->data, X + sx * Y, val);
      }
    }
    free(wt);
  }
}

// HEY yikes copy-pasta of cheapMedian2D
static void
cheapMedian3D(Nrrd *nout, const Nrrd *nin, const NrrdRange *range, uint radius,
              float wght, uint bins, int mode, float *hist, int verberr) {
  static const char me[] = "cheapMedian3D";
  char done[13];
  uint diam = 2 * radius + 1;
  size_t sx = nin->axis[0].size;
  size_t sy = nin->axis[1].size;
  size_t sz = nin->axis[2].size;
  double (*lup)(const void *, size_t) = nrrdDLookup[nin->type];
  if (verberr) {
    fprintf(stderr, "%s: ...       ", me);
  }
  int mrad = -AIR_INT(radius);
  int prad = AIR_INT(radius);
  float half;
  double val;
  uint idx;
  if (1 == wght) {
    /* uniform weighting-> can do sliding histogram optimization */
    half = AIR_FLOAT(diam * diam * diam / 2 + 1);
    if (verberr) {
      fflush(stderr);
    }
    for (uint Z = radius; Z < sz - radius; Z++) {
      if (verberr) {
        fprintf(stderr, "%s", airDoneStr(radius, Z, sz - radius - 1, done));
        fflush(stderr);
      }
      for (uint Y = radius; Y < sy - radius; Y++) {
        /* initialize histogram */
        memset(hist, 0, bins * sizeof(float));
        uint X = radius;
        for (int K = mrad; K <= prad; K++) {
          for (int J = mrad; J <= prad; J++) {
            for (int I = mrad; I <= prad; I++) {
              hist[INDEX(nin, range, lup, SpU(I, X) + sx * (SpU(J, Y) + sy * SpU(K, Z)),
                         bins, val)]++;
            }
          }
        }
        /* find median at each point using existing histogram */
        for (X = radius; X < sx - radius; X++) {
          idx = mode ? CM_mode(hist, bins) : CM_median(hist, half);
          val = NRRD_NODE_POS(range->min, range->max, bins, idx);
          nrrdDInsert[nout->type](nout->data, X + sx * (Y + sy * Z), val);
          /* probably update histogram for next iteration */
          if (X < sx - radius - 1) {
            for (int K = mrad; K <= prad; K++) {
              for (int J = mrad; J <= prad; J++) {
                size_t offi = sx * (SpU(J, Y) + sy * SpU(K, Z));
                hist[INDEX(nin, range, lup, X + radius + 1 + offi, bins, val)]++;
                hist[INDEX(nin, range, lup, X - radius + offi, bins, val)]--;
              }
            }
          }
        }
      }
    }
  } else {
    /* non-uniform weighting --> slow and stupid */
    float *wt = CM_wtAlloc(radius, wght);
    half = 0.5;
    for (uint Z = radius; Z < sz - radius; Z++) {
      if (verberr) {
        fprintf(stderr, "%s", airDoneStr(radius, Z, sz - radius - 1, done));
        fflush(stderr);
      }
      for (uint Y = radius; Y < sy - radius; Y++) {
        for (uint X = radius; X < sx - radius; X++) {
          memset(hist, 0, bins * sizeof(float));
          for (int K = mrad; K <= prad; K++) {
            for (int J = mrad; J <= prad; J++) {
              for (int I = mrad; I <= prad; I++) {
                hist[INDEX(nin, range, lup,
                           SpU(I, X) + sx * (SpU(J, Y) + sy * SpU(K, Z)), bins, val)]
                  += wt[I + prad] * wt[J + prad] * wt[K + prad];
              }
            }
          }
          idx = mode ? CM_mode(hist, bins) : CM_median(hist, half);
          val = NRRD_NODE_POS(range->min, range->max, bins, idx);
          nrrdDInsert[nout->type](nout->data, X + sx * (Y + sy * Z), val);
        }
      }
    }
    free(wt);
  }
  if (verberr) {
    fprintf(stderr, "\b\b\b\b\b\b  done\n");
  }
}

/* HEY: total copy-and-paste from cheapMedian3D */
static void
cheapMedian4D(Nrrd *nout, const Nrrd *nin, const NrrdRange *range, uint radius,
              float wght, uint bins, int mode, float *hist, int verberr) {
  static const char me[] = "cheapMedian4D";
  char done[13];
  uint diam = 2 * radius + 1;
  size_t sw = nin->axis[0].size;
  size_t sx = nin->axis[1].size;
  size_t sy = nin->axis[2].size;
  size_t sz = nin->axis[3].size;
  double (*lup)(const void *, size_t) = nrrdDLookup[nin->type];
  if (verberr) {
    fprintf(stderr, "%s: ...       ", me);
  }
  int mrad = -AIR_INT(radius);
  int prad = AIR_INT(radius);
  float half;
  double val;
  uint idx;
  if (1 == wght) {
    /* uniform weighting-> can do sliding histogram optimization */
    half = AIR_FLOAT(diam * diam * diam * diam / 2 + 1);
    if (verberr) {
      fflush(stderr);
    }
    for (uint Z = radius; Z < sz - radius; Z++) {
      if (verberr) {
        fprintf(stderr, "%s", airDoneStr(radius, Z, sz - radius - 1, done));
        fflush(stderr);
      }
      for (uint Y = radius; Y < sy - radius; Y++) {
        for (uint X = radius; X < sx - radius; X++) {
          /* initialize histogram */
          memset(hist, 0, bins * sizeof(float));
          uint W = radius;
          for (int K = mrad; K <= prad; K++) {
            for (int J = mrad; J <= prad; J++) {
              for (int I = mrad; I <= prad; I++) {
                for (int H = mrad; H <= prad; H++) {
                  hist[INDEX(nin, range, lup,
                             SpU(H, W)
                               + sw * (SpU(I, X) + sx * (SpU(J, Y) + sy * SpU(K, Z))),
                             bins, val)]++;
                }
              }
            }
          }
          /* find median at each point using existing histogram */
          for (W = radius; W < sw - radius; W++) {
            idx = mode ? CM_mode(hist, bins) : CM_median(hist, half);
            val = NRRD_NODE_POS(range->min, range->max, bins, idx);
            nrrdDInsert[nout->type](nout->data, W + sw * (X + sx * (Y + sy * Z)), val);
            /* probably update histogram for next iteration */
            if (W < sw - radius - 1) {
              for (int K = mrad; K <= prad; K++) {
                for (int J = mrad; J <= prad; J++) {
                  for (int I = mrad; I <= prad; I++) {
                    size_t offi = sw * (SpU(I, X) + sx * (SpU(J, Y) + sy * SpU(K, Z)));
                    hist[INDEX(nin, range, lup, W + radius + 1 + offi, bins, val)]++;
                    hist[INDEX(nin, range, lup, W - radius + offi, bins, val)]--;
                  }
                }
              }
            }
          }
        }
      }
    }
  } else {
    /* non-uniform weighting --> slow and stupid */
    float *wt = CM_wtAlloc(radius, wght);
    half = 0.5;
    for (uint Z = radius; Z < sz - radius; Z++) {
      if (verberr) {
        fprintf(stderr, "%s", airDoneStr(radius, Z, sz - radius - 1, done));
        fflush(stderr);
      }
      for (uint Y = radius; Y < sy - radius; Y++) {
        for (uint X = radius; X < sx - radius; X++) {
          for (uint W = radius; W < sw - radius; W++) {
            memset(hist, 0, bins * sizeof(float));
            for (int K = mrad; K <= prad; K++) {
              for (int J = mrad; J <= prad; J++) {
                for (int I = mrad; I <= prad; I++) {
                  for (int H = mrad; H <= prad; H++) {
                    hist[INDEX(nin, range, lup,
                               SpU(H, W)
                                 + sw * (SpU(I, X) + sx * (SpU(J, Y) + sy * SpU(K, Z))),
                               bins, val)]
                      += wt[H + prad] * wt[I + prad] * wt[J + prad] * wt[K + prad];
                  }
                }
              }
            }
            idx = mode ? CM_mode(hist, bins) : CM_median(hist, half);
            val = NRRD_NODE_POS(range->min, range->max, bins, idx);
            nrrdDInsert[nout->type](nout->data, W + sw * (X + sx * (Y + sy * Z)), val);
          }
        }
      }
    }
    free(wt);
  }
  if (verberr) {
    fprintf(stderr, "\b\b\b\b\b\b  done\n");
  }
}

/*
******** nrrdCheapMedian
**
** histogram-based median or mode filtering
** !mode: median filtering
** mode: mode filtering
*/
int /* Biff: 1 */
nrrdCheapMedian(Nrrd *_nout, const Nrrd *_nin, int pad, int mode, unsigned int radius,
                float wght, unsigned int bins, int verberr) {
  static const char me[] = "nrrdCheapMedian", func[] = "cmedian";
  if (!(_nin && _nout)) {
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  if (!(radius >= 1)) {
    biffAddf(NRRD, "%s: need radius >= 1 (got %u)", me, radius);
    return 1;
  }
  if (!(bins >= 1)) {
    biffAddf(NRRD, "%s: need bins >= 1 (got %u)", me, bins);
    return 1;
  }
  if (!(AIR_IN_CL(1, _nin->dim, 4))) {
    biffAddf(NRRD, "%s: sorry, can only handle dim 1, 2, 3 (not %u)", me, _nin->dim);
    return 1;
  }
  size_t minsize = _nin->axis[0].size;
  if (_nin->dim > 1) {
    minsize = AIR_MIN(minsize, _nin->axis[1].size);
  }
  if (_nin->dim > 2) {
    minsize = AIR_MIN(minsize, _nin->axis[2].size);
  }
  if (_nin->dim > 3) {
    minsize = AIR_MIN(minsize, _nin->axis[3].size);
  }
  if (!pad && minsize < 2 * radius + 1) {
    biffAddf(NRRD,
             "%s: minimum nrrd size (%zu) smaller than filtering "
             "window size (%u) with radius %u; must enable padding",
             me, minsize, 2 * radius + 1, radius);
    return 1;
  }
  if (_nout == _nin) {
    biffAddf(NRRD, "%s: nout==nin disallowed", me);
    return 1;
  }
  if (nrrdTypeBlock == _nin->type) {
    biffAddf(NRRD, "%s: can't filter nrrd type %s", me,
             airEnumStr(nrrdType, nrrdTypeBlock));
    return 1;
  }
  airArray *mop = airMopNew();
  Nrrd *nout, *nin;
  /* set nin based on _nin */
  airMopAdd(mop, nin = nrrdNew(), nrrdNuke_mop, airMopAlways);
  if (pad) {
    airMopAdd(mop, nout = nrrdNew(), nrrdNuke_mop, airMopAlways);
    if (nrrdSimplePad_va(nin, _nin, radius, nrrdBoundaryBleed)) {
      biffAddf(NRRD, "%s: trouble padding input", me);
      airMopError(mop);
      return 1;
    }
  } else {
    if (nrrdCopy(nin, _nin)) {
      biffAddf(NRRD, "%s: trouble copying input", me);
      airMopError(mop);
      return 1;
    }
    nout = _nout;
  }
  if (nrrdCopy(nout, nin)) {
    biffAddf(NRRD, "%s: failed to create initial copy of input", me);
    airMopError(mop);
    return 1;
  }
  NrrdRange *range = nrrdRangeNewSet(nin, nrrdBlind8BitRangeFalse);
  airMopAdd(mop, range, nrrdRangeNix_mop, airMopAlways);
  float *hist = AIR_CALLOC(bins, float);
  if (!hist) {
    biffAddf(NRRD, "%s: couldn't allocate histogram (%u bins)", me, bins);
    airMopError(mop);
    return 1;
  }
  airMopAdd(mop, hist, airFree, airMopAlways);
  if (!AIR_EXISTS(wght)) {
    wght = 1.0;
  }
  switch (nin->dim) {
  case 1:
    cheapMedian1D(nout, nin, range, radius, wght, bins, mode, hist);
    break;
  case 2:
    cheapMedian2D(nout, nin, range, radius, wght, bins, mode, hist);
    break;
  case 3:
    cheapMedian3D(nout, nin, range, radius, wght, bins, mode, hist, verberr);
    break;
  case 4:
    cheapMedian4D(nout, nin, range, radius, wght, bins, mode, hist, verberr);
    break;
  default:
    biffAddf(NRRD, "%s: sorry, %u-dimensional median unimplemented", me, nin->dim);
    airMopError(mop);
    return 1;
  }

  nrrdAxisInfoCopy(nout, nin, NULL, NRRD_AXIS_INFO_NONE);
  if (nrrdContentSet_va(nout, func, nin, "%d,%d,%g,%d", mode, radius, wght, bins)) {
    biffAddf(NRRD, "%s:", me);
    airMopError(mop);
    return 1;
  }
  /* basic info handled by nrrdCopy above */

  /* set _nout based on nout */
  if (pad) {
    if (nrrdSimpleCrop(_nout, nout, radius)) {
      biffAddf(NRRD, "%s: trouble cropping output", me);
      airMopError(mop);
      return 1;
    }
  } else {
    /* we've already set output in _nout == nout */
  }

  airMopOkay(mop);
  return 0;
}

/*
** returns intersection of parabolas c(x) = spc^2 (x - xi)^2 + yi
*/
static double
intx(double xx0, double yy0, double xx1, double yy1, double spc) {
  double ss;

  ss = spc * spc;
  return (yy1 / ss + xx1 * xx1 - (yy0 / ss + xx0 * xx0)) / (2 * (xx1 - xx0));
}

/*
** squared-distance transform of single scanline
**
** based on code published with:
** Distance Transforms of Sampled Functions
** Pedro F. Felzenszwalb and Daniel P. Huttenlocher
** Cornell Computing and Information Science TR2004-1963
**
** dd: output (pre-allocated for "len")
** ff: input function (pre-allocated for "len")
** zz: buffer (pre-allocated for "len"+1) for locations of
**     boundaries between parabolas
** vv: buffer (pre-allocated for "len") for locations of
**     parabolas in lower envelope
**
** The major modification from the published method is the inclusion
** of the "spc" parameter that gives the inter-sample spacing, so that
** the multi-dimensional version can work on non-isotropic samples.
**
** FLT_MIN and FLT_MAX are used to be consistent with the
** initialization in nrrdDistanceL2(), which uses FLT_MAX
** to be compatible with the case of using floats
*/
static void
distanceL2Sqrd1D(double *dd, const double *ff, double *zz, unsigned int *vv, size_t len,
                 double spc) {
  size_t kk, qq;

  if (!(dd && ff && zz && vv && len > 0)) {
    /* error */
    return;
  }

  kk = 0;
  vv[0] = 0;
  zz[0] = -FLT_MAX;
  zz[1] = +FLT_MAX;
  for (qq = 1; qq < len; qq++) {
    double ss;
    ss = intx(AIR_DOUBLE(qq), ff[qq], vv[kk], ff[vv[kk]], spc);
    /* while (ss <= zz[kk]) {
    ** HEY this can have kk going to -1 and into memory errors!
    */
    while (ss <= zz[kk] && kk) {
      kk--;
      ss = intx(AIR_DOUBLE(qq), ff[qq], vv[kk], ff[vv[kk]], spc);
    }
    kk++;
    vv[kk] = AIR_UINT(qq);
    zz[kk] = ss;
    zz[kk + 1] = +FLT_MAX;
  }

  kk = 0;
  for (qq = 00; qq < len; qq++) {
    double dx;
    while (zz[kk + 1] < qq) {
      kk++;
    }
    /* cast to avoid overflow weirdness on the unsigned ints */
    dx = AIR_DOUBLE(qq) - vv[kk];
    dd[qq] = spc * spc * dx * dx + ff[vv[kk]];
  }

  return;
}

static int /* Biff: 1 */
distanceL2Sqrd(Nrrd *ndist, double *spcMean) {
  static const char me[] = "distanceL2Sqrd";
  size_t sizeMax; /* max size of all axes */
  Nrrd *ntmpA, *ntmpB, *npass[NRRD_DIM_MAX + 1];
  int spcSomeExist, spcSomeNonExist;
  unsigned int di, *vv;
  double *dd, *ff, *zz;
  double spc[NRRD_DIM_MAX], vector[NRRD_SPACE_DIM_MAX];
  double (*lup)(const void *, size_t), (*ins)(void *, size_t, double);
  airArray *mop;

  if (!(nrrdTypeFloat == ndist->type || nrrdTypeDouble == ndist->type)) {
    biffAddf(NRRD, "%s: sorry, can only process type %s or %s (not %s)", me,
             airEnumStr(nrrdType, nrrdTypeFloat), airEnumStr(nrrdType, nrrdTypeDouble),
             airEnumStr(nrrdType, ndist->type));
    return 1;
  }

  spcSomeExist = AIR_FALSE;
  spcSomeNonExist = AIR_FALSE;
  for (di = 0; di < ndist->dim; di++) {
    nrrdSpacingCalculate(ndist, di, spc + di, vector);
    spcSomeExist |= AIR_EXISTS(spc[di]);
    spcSomeNonExist |= !AIR_EXISTS(spc[di]);
  }
  if (spcSomeExist && spcSomeNonExist) {
    biffAddf(NRRD, "%s: axis spacings must all exist or all non-exist", me);
    return 1;
  }
  if (!spcSomeExist) {
    for (di = 0; di < ndist->dim; di++) {
      spc[di] = 1.0;
    }
  }
  *spcMean = 0;
  for (di = 0; di < ndist->dim; di++) {
    *spcMean += spc[di];
  }
  *spcMean /= ndist->dim;

  sizeMax = 0;
  for (di = 0; di < ndist->dim; di++) {
    sizeMax = AIR_MAX(sizeMax, ndist->axis[di].size);
  }

  /* create mop and allocate tmp buffers */
  mop = airMopNew();
  ntmpA = nrrdNew();
  airMopAdd(mop, ntmpA, nrrdNuke_mop, airMopAlways);
  if (ndist->dim > 2) {
    ntmpB = nrrdNew();
    airMopAdd(mop, ntmpB, nrrdNuke_mop, airMopAlways);
  } else {
    ntmpB = NULL;
  }
  if (nrrdCopy(ntmpA, ndist) || (ndist->dim > 2 && nrrdCopy(ntmpB, ndist))) {
    biffAddf(NRRD, "%s: couldn't allocate image buffers", me);
  }
  dd = AIR_CAST(double *, calloc(sizeMax, sizeof(double)));
  ff = AIR_CAST(double *, calloc(sizeMax, sizeof(double)));
  zz = AIR_CAST(double *, calloc(sizeMax + 1, sizeof(double)));
  vv = AIR_CAST(unsigned int *, calloc(sizeMax, sizeof(unsigned int)));
  airMopAdd(mop, dd, airFree, airMopAlways);
  airMopAdd(mop, ff, airFree, airMopAlways);
  airMopAdd(mop, zz, airFree, airMopAlways);
  airMopAdd(mop, vv, airFree, airMopAlways);
  if (!(dd && ff && zz && vv)) {
    biffAddf(NRRD, "%s: couldn't allocate scanline buffers", me);
    airMopError(mop);
    return 1;
  }

  /* set up array of buffers */
  npass[0] = ndist;
  for (di = 1; di < ndist->dim; di++) {
    npass[di] = (di % 2) ? ntmpA : ntmpB;
  }
  npass[ndist->dim] = ndist;

  /* run the multiple passes */
  /* what makes the indexing here so simple is that by assuming that
     we're processing every axis, the input to any given pass can be
     logically considered a 2-D image (a list of scanlines), where the
     second axis is the merge of all input axes but the first.  With
     the rotational shuffle of axes through passes, the initial axis
     and the set of other axes swap places, so its like the 2-D image
     is being transposed.  NOTE: the Nrrds that were allocated as
     buffers are really being mis-used, in that the axis sizes and
     raster ordering of what we're storing there is *not* the same as
     told by axis[].size */
  lup = nrrdDLookup[ndist->type];
  ins = nrrdDInsert[ndist->type];
  for (di = 0; di < ndist->dim; di++) {
    size_t lineIdx, lineNum, valIdx, valNum;

    valNum = ndist->axis[di].size;
    lineNum = nrrdElementNumber(ndist) / valNum;
    for (lineIdx = 0; lineIdx < lineNum; lineIdx++) {
      /* read input scanline into ff */
      for (valIdx = 0; valIdx < valNum; valIdx++) {
        ff[valIdx] = lup(npass[di]->data, valIdx + valNum * lineIdx);
      }
      /* do the transform */
      distanceL2Sqrd1D(dd, ff, zz, vv, valNum, spc[di]);
      /* write dd to output scanline */
      for (valIdx = 0; valIdx < valNum; valIdx++) {
        ins(npass[di + 1]->data, lineIdx + lineNum * valIdx, dd[valIdx]);
      }
    }
  }

  airMopOkay(mop);
  return 0;
}

/*
** helper function for distance transforms, is called by things that want to do
** specific kinds of transforms.
*/
static int /* Biff: 1 */
distanceBase(Nrrd *nout, const Nrrd *nin, int typeOut, const int *axisDo, double thresh,
             double bias, int insideHigher) {
  static const char me[] = "distanceBase";
  size_t ii, nn;
  double (*lup)(const void *, size_t), (*ins)(void *, size_t, double);
  double spcMean;

  if (!(nout && nin)) {
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }
  if (nrrdTypeBlock == nin->type) {
    biffAddf(NRRD, "%s: need scalar type for distance transform (not %s)", me,
             airEnumStr(nrrdType, nrrdTypeBlock));
    return 1;
  }
  if (!(nrrdTypeDouble == typeOut || nrrdTypeFloat == typeOut)) {
    biffAddf(NRRD,
             "%s: sorry, can only transform to type %s or %s "
             "(not %s)",
             me, airEnumStr(nrrdType, nrrdTypeFloat),
             airEnumStr(nrrdType, nrrdTypeDouble), airEnumStr(nrrdType, typeOut));
    return 1;
  }
  if (axisDo) {
    biffAddf(NRRD, "%s: sorry, selective axis transform not implemented", me);
    return 1;
  }
  if (!AIR_EXISTS(thresh)) {
    biffAddf(NRRD, "%s: threshold (%g) doesn't exist", me, thresh);
    return 1;
  }

  if (nrrdConvert(nout, nin, typeOut)) {
    biffAddf(NRRD, "%s: couldn't allocate output", me);
    return 1;
  }
  lup = nrrdDLookup[nout->type];
  ins = nrrdDInsert[nout->type];

  nn = nrrdElementNumber(nout);
  for (ii = 0; ii < nn; ii++) {
    double val, bb;
    val = lup(nout->data, ii);
    if (insideHigher) {
      bb = bias * (val - thresh);
      ins(nout->data, ii, val > thresh ? bb * bb : FLT_MAX);
    } else {
      bb = bias * (thresh - val);
      ins(nout->data, ii, val <= thresh ? bb * bb : FLT_MAX);
    }
  }

  if (distanceL2Sqrd(nout, &spcMean)) {
    biffAddf(NRRD, "%s: trouble doing transform", me);
    return 1;
  }

  for (ii = 0; ii < nn; ii++) {
    double val;
    val = sqrt(lup(nout->data, ii));
    /* here's where the distance is tweaked downwards by half a sample width */
    ins(nout->data, ii, AIR_MAX(0, val - spcMean / 2));
  }

  return 0;
}

/*
******** nrrdDistanceL2
**
** computes euclidean (L2) distance transform of input image, after
** thresholding at "thresh".
**
** NOTE: the output of this is slightly offset from what one might
** expect; decreased by half of the average (over all axes) sample
** spacing.  The reason for this is so that when the transform is
** applied to the inverted image and negated, to create a full
** signed distance map, the transition from interior to exterior
** distance values is smooth.  Without this trick, there is a
** small little plateau at the transition.
*/
int /* Biff: 1 */
nrrdDistanceL2(Nrrd *nout, const Nrrd *nin, int typeOut, const int *axisDo,
               double thresh, int insideHigher) {
  static const char me[] = "nrrdDistanceL2";

  if (distanceBase(nout, nin, typeOut, axisDo, thresh, 0, insideHigher)) {
    biffAddf(NRRD, "%s: trouble doing distance transform", me);
    return 1;
  }
  return 0;
}

int /* Biff: 1 */
nrrdDistanceL2Biased(Nrrd *nout, const Nrrd *nin, int typeOut, const int *axisDo,
                     double thresh, double bias, int insideHigher) {
  static const char me[] = "nrrdDistanceL2Biased";

  if (distanceBase(nout, nin, typeOut, axisDo, thresh, bias, insideHigher)) {
    biffAddf(NRRD, "%s: trouble doing distance transform", me);
    return 1;
  }
  return 0;
}

int /* Biff: 1 */
nrrdDistanceL2Signed(Nrrd *nout, const Nrrd *nin, int typeOut, const int *axisDo,
                     double thresh, int insideHigher) {
  static const char me[] = "nrrdDistanceL2Signed";
  airArray *mop;
  Nrrd *ninv;

  if (!(nout && nin)) {
    biffAddf(NRRD, "%s: got NULL pointer", me);
    return 1;
  }

  mop = airMopNew();
  ninv = nrrdNew();
  airMopAdd(mop, ninv, nrrdNuke_mop, airMopAlways);

  if (nrrdDistanceL2(nout, nin, typeOut, axisDo, thresh, insideHigher)
      || nrrdDistanceL2(ninv, nin, typeOut, axisDo, thresh, !insideHigher)
      || nrrdArithUnaryOp(ninv, nrrdUnaryOpNegative, ninv)
      || nrrdArithBinaryOp(nout, nrrdBinaryOpAdd, nout, ninv)) {
    biffAddf(NRRD, "%s: trouble doing or combining transforms", me);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  return 0;
}
