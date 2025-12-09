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

#include <teem/air.h>
#include <teem/hest.h>
#include <teem/nrrd.h>
#include <teem/gage.h>
#include <teem/bane.h>

typedef unsigned int uint;

#define QBERT           "qbert"
#define QBERT_HIST_BINS 1024 /* histogram size for v, g, and h */

int qbertSaveAll; /* (set by hest) can be used to save output of every stage */

/*
** This padding/resampling is to get axis[i]'s size >= sz[i], which
** is only needed if the input volume is smaller along any of the axes
** than the desired output volume.
*/
int
qbertSizeUp(Nrrd *nout, Nrrd *nin, unsigned int *sz, NrrdKernelSpec *uk) {
  static const char me[] = "qbertSizeUp";
  airArray *mop = airMopNew();
  NrrdResampleInfo *rsi = nrrdResampleInfoNew();
  airMopAdd(mop, rsi, nrrdResampleInfoNix_mop, airMopAlways);
  int need, anyneed = 0;
  if (uk) {
    for (uint axi = 0; axi <= 2; axi++) {
      need = AIR_INT(sz[axi]) - AIR_INT(nin->axis[axi].size);
      anyneed |= !!(need);
      fprintf(stderr, "%s: sz[%ud] = %u -> need = %d --> ", me, axi,
              AIR_UINT(nin->axis[axi].size), need);
      need = AIR_MAX(0, need);
      fprintf(stderr, "%d --> %s resample\n", need, need ? "WILL" : "won't");
      if (need) {
        rsi->kernel[axi] = uk->kernel;
        memcpy(rsi->parm[axi], uk->parm, uk->kernel->parmNum * sizeof(double));
        if (!AIR_EXISTS(nin->axis[axi].min)) {
          nin->axis[axi].min = 0.0;
        }
        if (!AIR_EXISTS(nin->axis[axi].max)) {
          nin->axis[axi].max = nin->axis[axi].size - 1;
        }
        rsi->min[axi] = nin->axis[axi].min;
        rsi->max[axi] = nin->axis[axi].max;
        rsi->samples[axi] = sz[axi];
        nin->axis[axi].center = nrrdCenterNode;
      } else {
        rsi->kernel[axi] = NULL;
      }
    }
    if (anyneed) {
      rsi->boundary = nrrdBoundaryBleed;
      rsi->type = nrrdTypeFloat;
      rsi->renormalize = AIR_TRUE;
      rsi->clamp = AIR_TRUE;
      fprintf(stderr, "%s: resampling ... ", me);
      fflush(stderr);
      if (nrrdSpatialResample(nout, nin, rsi)) {
        biffMovef(QBERT, NRRD, "%s: trouble upsampling", me);
        airMopError(mop);
        return 1;
      }
      fprintf(stderr, "done\n");
    }
  } else {
    ptrdiff_t padMin[3], padMax[3];
    for (uint axi = 0; axi <= 2; axi++) {
      anyneed |= !!(need = AIR_INT(sz[axi]) - AIR_INT(nin->axis[axi].size));
      fprintf(stderr, "%s: sz[%u] = %zu -> need = %d --> ", me, axi, nin->axis[axi].size,
              need);
      need = AIR_MAX(0, need);
      fprintf(stderr, "%d --> ", need);
      padMin[axi] = 0 - (int)floor(need / 2.0);
      padMax[axi] = (ptrdiff_t)(nin->axis[axi].size - 1) + (int)ceil(need / 2.0);
      fprintf(stderr, "pad indices: [%d..%d]\n", AIR_INT(padMin[axi]),
              AIR_INT(padMax[axi]));
    }
    if (anyneed) {
      fprintf(stderr, "%s: padding ... ", me);
      fflush(stderr);
      if (nrrdPad_va(nout, nin, padMin, padMax, nrrdBoundaryPad, 0.0)) {
        biffMovef(QBERT, NRRD, "%s: trouble padding", me);
        airMopError(mop);
        return 1;
      }
      fprintf(stderr, "done\n");
    }
  }
  if (!anyneed) {
    if (nrrdCopy(nout, nin)) {
      biffMovef(QBERT, NRRD, "%s: trouble copying", me);
      airMopError(mop);
      return 1;
    }
  }
  if (qbertSaveAll) {
    fprintf(stderr, "%s: saving up.nrrd\n", me);
    nrrdSave("up.nrrd", nout, NULL);
  }
  airMopOkay(mop);
  return 0;
}

/*
** resampling to get axis[i]'s size down to exactly sz[i]
*/
int
qbertSizeDown(Nrrd *nout, Nrrd *nin, unsigned int *sz, NrrdKernelSpec *dk) {
  static const char me[] = "qbertSizeDown";
  airArray *mop = airMopNew();
  NrrdResampleInfo *rsi = nrrdResampleInfoNew();
  airMopAdd(mop, rsi, nrrdResampleInfoNix_mop, airMopAlways);
  rsi->boundary = nrrdBoundaryBleed;
  rsi->type = nrrdTypeFloat;
  rsi->renormalize = AIR_TRUE;
  int need = 0;
  for (uint axi = 0; axi <= 2; axi++) {
    if (nin->axis[axi].size > sz[axi]) {
      need = 1;
      rsi->kernel[axi] = dk->kernel;
      memcpy(rsi->parm[axi], dk->parm, dk->kernel->parmNum * sizeof(double));
      rsi->samples[axi] = sz[axi];
      if (!AIR_EXISTS(nin->axis[axi].min)) {
        nin->axis[axi].min = 0.0;
      }
      if (!AIR_EXISTS(nin->axis[axi].max)) {
        nin->axis[axi].max = nin->axis[axi].size - 1;
      }
      rsi->min[axi] = nin->axis[axi].min;
      rsi->max[axi] = nin->axis[axi].max;
      nin->axis[axi].center = nrrdCenterNode;
      fprintf(stderr, "%s: downsampling axis %u from %zu to %zu samples\n", me, axi,
              nin->axis[axi].size, rsi->samples[axi]);
    } else {
      rsi->kernel[axi] = NULL;
    }
  }
  if (need) {
    fprintf(stderr, "%s: resampling ... ", me);
    fflush(stderr);
    if (nrrdSpatialResample(nout, nin, rsi)) {
      biffMovef(QBERT, NRRD, "%s: trouble resampling", me);
      airMopError(mop);
      return 1;
    }
    fprintf(stderr, "done\n");
  } else {
    if (nrrdCopy(nout, nin)) {
      biffMovef(QBERT, NRRD, "%s: trouble copying", me);
      airMopError(mop);
      return 1;
    }
  }
  if (qbertSaveAll) {
    fprintf(stderr, "%s: saving down.nrrd\n", me);
    nrrdSave("down.nrrd", nout, NULL);
  }

  airMopOkay(mop);
  return 0;
}

/*
** probing to getting floating point V, G, and maybe H values
*/
int
qbertProbe(Nrrd *nout, Nrrd *nin, NrrdKernelSpec *k00, NrrdKernelSpec *k11,
           NrrdKernelSpec *k22, int doH, unsigned int *sz) {
  static const char me[] = "qbertProbe";
  doH = !!doH;
  airArray *mop = airMopNew();
  gageContext *ctx = gageContextNew();
  airMopAdd(mop, ctx, gageContextNix_mop, airMopAlways);

  nin->axis[0].center = nrrdCenterNode;
  nin->axis[1].center = nrrdCenterNode;
  nin->axis[2].center = nrrdCenterNode;
  gagePerVolume *pvl = gagePerVolumeNew(ctx, nin, gageKindScl);
  if (!pvl) {
    biffMovef(QBERT, GAGE, "%s: gage trouble", me);
    airMopError(mop);
    return 1;
  }
  gageParmSet(ctx, gageParmVerbose, 0);
  gageParmSet(ctx, gageParmRenormalize, AIR_TRUE);
  gageParmSet(ctx, gageParmCheckIntegrals, AIR_TRUE);
  int E = 0;
  if (!E) E |= gagePerVolumeAttach(ctx, pvl);
  /* about kernel setting for probing: currently, the way that probing is
     done is ONLY on grid locations, and never in between voxels.  That
     means that the kernels set below are really only used for creating
     discrete convolution masks at unit locations */
  if (!E) E |= gageKernelSet(ctx, gageKernel00, k00->kernel, k00->parm);
  if (!E) E |= gageKernelSet(ctx, gageKernel11, k11->kernel, k11->parm);
  if (!E) E |= gageKernelSet(ctx, gageKernel22, k22->kernel, k22->parm);
  if (!E) E |= gageQueryItemOn(ctx, pvl, gageSclValue);
  if (!E) E |= gageQueryItemOn(ctx, pvl, gageSclGradMag);
  if (doH) {
    if (!E) E |= gageQueryItemOn(ctx, pvl, gageScl2ndDD);
  }
  if (!E) E |= gageUpdate(ctx);
  if (E) {
    biffMovef(QBERT, GAGE, "%s: gage trouble", me);
    airMopError(mop);
    return 1;
  }
  gageParmSet(ctx, gageParmVerbose, 0);
  const double *val = gageAnswerPointer(ctx, pvl, gageSclValue);
  const double *gmag = gageAnswerPointer(ctx, pvl, gageSclGradMag);
  const double *scnd = gageAnswerPointer(ctx, pvl, gageScl2ndDD);
  if (nrrdMaybeAlloc_va(nout, nrrdTypeFloat, 4, AIR_SIZE_T(2 + doH), AIR_SIZE_T(sz[0]),
                        AIR_SIZE_T(sz[1]), AIR_SIZE_T(sz[2]))) {
    biffMovef(QBERT, NRRD, "%s: couldn't allocate floating point VG%s volume", me,
              doH ? "H" : "");
    airMopError(mop);
    return 1;
  }
  char prog[AIR_STRLEN_SMALL + 1];
  float *vghF = (float *)nout->data;
  fprintf(stderr, "%s: probing ...       ", me);
  fflush(stderr);
  for (uint kk = 0; kk < sz[2]; kk++) {
    for (uint jj = 0; jj < sz[1]; jj++) {
      if (!((jj + sz[1] * kk) % 100)) {
        fprintf(stderr, "%s", airDoneStr(0, jj + sz[1] * kk, sz[1] * sz[2], prog));
        fflush(stderr);
      }
      for (uint ii = 0; ii < sz[0]; ii++) {
        gageProbe(ctx, ii, jj, kk);
        vghF[0] = AIR_FLOAT(*val);
        vghF[1] = AIR_FLOAT(*gmag);
        if (doH) {
          vghF[2] = AIR_FLOAT(*scnd);
        }
        vghF += 2 + doH;
      }
    }
  }
  fprintf(stderr, "%s\n", airDoneStr(0, 2, 1, prog));
  if (qbertSaveAll) {
    fprintf(stderr, "%s: saving vghF.nrrd\n", me);
    nrrdSave("vghF.nrrd", nout, NULL);
  }

  airMopOkay(mop);
  return 0;
}

/*
** make histograms of v,g,h values as first step in determining the
** inclusions for the later quantization
*/
int
qbertMakeVghHists(Nrrd *nvhist, Nrrd *nghist, Nrrd *nhhist, uint *sz, uint bins,
                  Nrrd *nvghF, Nrrd *nin) {
  static const char me[] = "qbertMakeVghHists";
  uint nval = AIR_UINT(nvghF->axis[0].size);
  int doH = !!(nval == 3);
  float *vghF = (float *)nvghF->data;
  double minv, maxv, ming, maxg, minh = 0, maxh = 0;
  minv = maxv = vghF[0 + nval * 0];
  ming = maxg = vghF[1 + nval * 0];
  if (doH) {
    minh = maxh = vghF[2 + nval * 0];
  }
  for (uint axi = 0; axi < sz[0] * sz[1] * sz[2]; axi++) {
    minv = AIR_MIN(minv, vghF[0 + nval * axi]);
    maxv = AIR_MAX(maxv, vghF[0 + nval * axi]);
    ming = AIR_MIN(ming, vghF[1 + nval * axi]);
    maxg = AIR_MAX(maxg, vghF[1 + nval * axi]);
    if (doH) {
      minh = AIR_MIN(minh, vghF[2 + nval * axi]);
      maxh = AIR_MAX(maxh, vghF[2 + nval * axi]);
    }
  }
  fprintf(stderr, "%s: values: [%g .. %g] -> ", me, minv, maxv);
  /* just because we're bastards, we're going to enforce minv >= 0 for
     types that started as unsigned integral types.  Downsampling with a
     ringing kernel can have produced negative values, so this change to
     minv can actually restrict the range, in contrast to to the changes
     to ming, minh, and maxh below */
  if (nrrdTypeIsUnsigned[nin->type]) {
    minv = AIR_MAX(minv, 0.0);
  }
  fprintf(stderr, "[%g .. %g]\n", minv, maxv);
  fprintf(stderr, "%s:  grads: [%g .. %g] -> ", me, ming, maxg);
  ming = 0;
  fprintf(stderr, "[%g .. %g]\n", ming, maxg);
  if (doH) {
    fprintf(stderr, "%s: 2ndDDs: [%g .. %g] -> ", me, minh, maxh);
    if (maxh > -minh)
      minh = -maxh;
    else
      maxh = -minh;
    fprintf(stderr, "[%g .. %g]\n", minh, maxh);
  }
  fprintf(stderr, "%s: using %u-bin histograms\n", me, bins);
  int E = 0;
  if (!E) E |= nrrdMaybeAlloc_va(nvhist, nrrdTypeUInt, 1, AIR_SIZE_T(bins));
  if (!E) E |= nrrdMaybeAlloc_va(nghist, nrrdTypeUInt, 1, AIR_SIZE_T(bins));
  if (doH) {
    if (!E) E |= nrrdMaybeAlloc_va(nhhist, nrrdTypeUInt, 1, AIR_SIZE_T(bins));
  }
  if (E) {
    biffMovef(QBERT, NRRD, "%s: couldn't allocate %u %u-bin histograms", me, nval, bins);
    return 1;
  }
  nvhist->axis[0].min = minv;
  nvhist->axis[0].max = maxv;
  nghist->axis[0].min = ming;
  nghist->axis[0].max = maxg;
  uint *vhist = (uint *)nvhist->data;
  uint *ghist = (uint *)nghist->data;
  memset(vhist, 0, bins * sizeof(uint));
  memset(ghist, 0, bins * sizeof(uint));
  uint *hhist = NULL;
  if (doH) {
    nhhist->axis[0].min = minh;
    nhhist->axis[0].max = maxh;
    hhist = (uint *)nhhist->data;
    memset(hhist, 0, bins * sizeof(uint));
  }
  vghF = (float *)nvghF->data;
  for (uint ii = 0; ii < sz[0] * sz[1] * sz[2]; ii++) {
    uint vi = airIndexClamp(minv, vghF[0], maxv, bins);
    uint gi = airIndex(ming, vghF[1], maxg, bins);
    vhist[vi]++;
    ghist[gi]++;
    if (doH) {
      uint hi = airIndex(minh, vghF[2], maxh, bins);
      hhist[hi]++;
    }
    vghF += nval;
  }
  if (qbertSaveAll) {
    fprintf(stderr, "%s: saving {v,g,h}hist.nrrd\n", me);
    nrrdSave("vhist.nrrd", nvhist, NULL);
    nrrdSave("ghist.nrrd", nghist, NULL);
    if (doH) {
      nrrdSave("hhist.nrrd", nhhist, NULL);
    }
  }

  return 0;
}

/*
 * determine inclusion from histograms and create 8-bit VGH volume
 *
 * NOTE: `hfullrange` was added for TeemV2; GLK honestly forgets what it was about the
 * SGI hardware (which motivated creating this tool) that required the the 2nd derivative
 * values to be something other than occupying all the 8-bit range.  Old code behavior
 * is equivalent to hfullrange being 0 (false).
 */
int
qbertMakeVgh(Nrrd *nvgh, Nrrd *nvhist, Nrrd *nghist, Nrrd *nhhist, unsigned int *sz,
             float *perc, Nrrd *nvghF, int hfullrange) {
  static const char me[] = "qbertMakeVgh";
  uint nval = AIR_UINT(nvghF->axis[0].size);
  int doH = !!(nval == 3);
  double minv = nvhist->axis[0].min;
  double maxv = nvhist->axis[0].max;
  double ming = nghist->axis[0].min;
  double maxg = nghist->axis[0].max;
  uint *vhist = (uint *)nvhist->data;
  uint *ghist = (uint *)nghist->data;
  uint *hhist = NULL;
  double minh = 0, maxh = 0;
  if (doH) {
    minh = nhhist->axis[0].min;
    maxh = nhhist->axis[0].max;
    hhist = (uint *)nhhist->data;
  }
  int toLose = (int)(perc[0] * sz[0] * sz[1] * sz[2] / 100);
  int lose = toLose;
  uint bins = AIR_UINT(nvhist->axis[0].size);
  uint bi = bins - 1;
  while (lose > 0) {
    /* HEY: we're nibbling only from top, even though for signed
       value types, there could be a tail at low negative values (had
       this problem with some CT data) */
    lose -= vhist[bi--];
  }
  maxv = AIR_AFFINE(0, bi, bins - 1, minv, maxv);

  lose = toLose;
  bins = AIR_UINT(nghist->axis[0].size);
  bi = bins - 1;
  while (lose > 0) {
    /* nibble from top */
    lose -= ghist[bi--];
  }
  maxg = AIR_AFFINE(0, bi, bins - 1, ming, maxg);

  if (doH) {
    lose = toLose;
    bins = AIR_UINT(nhhist->axis[0].size);
    bi = 0;
    while (lose > 0) {
      /* nibble from top and bottom at equal rates */
      lose -= hhist[bi] + hhist[bins - 1 - bi];
      bi++;
    }
    minh = AIR_AFFINE(0, bi, bins - 1, minh, maxh);
    maxh = -minh;
  }

  fprintf(stderr, "%s: new values (ignored %5d): [%g .. %g]\n", me,
          (int)(perc[0] * sz[0] * sz[1] * sz[2] / 100), minv, maxv);
  fprintf(stderr, "%s: new  grads (ignored %5d): [%g .. %g]\n", me,
          (int)(perc[1] * sz[0] * sz[1] * sz[2] / 100), ming, maxg);
  if (doH) {
    fprintf(stderr, "%s: new 2ndDDs (ignored %5d): [%g .. %g]\n", me,
            (int)(perc[2] * sz[0] * sz[1] * sz[2] / 100), minh, maxh);
    if (!hfullrange) {
      fprintf(stderr, "%s: putting 2ndDD in range 1 to 169 (0.0 -> 85)\n", me);
    }
  }

  if (nrrdMaybeAlloc_va(nvgh, nrrdTypeUChar, 4, AIR_SIZE_T(nval), AIR_SIZE_T(sz[0]),
                        AIR_SIZE_T(sz[1]), AIR_SIZE_T(sz[2]))) {
    biffMovef(QBERT, NRRD, "%s: couldn't allocate 8-bit VG%s volume", me,
              doH ? "H" : "");
    return 1;
  }
  unsigned char *vgh = (unsigned char *)nvgh->data;
  float *vghF = (float *)nvghF->data;
  for (uint ii = 0; ii < sz[0] * sz[1] * sz[2]; ii++) {
    uint vi = airIndex(minv, vghF[0], maxv, 254);
    vgh[0] = AIR_UCHAR(AIR_CLAMP(1, vi + 1, 254));
    uint gi = airIndex(ming, vghF[1], maxg, 254);
    vgh[1] = AIR_UCHAR(AIR_CLAMP(1, gi + 1, 254));
    if (doH) {
      if (hfullrange) {
        uint hi = airIndex(minh, vghF[2], maxh, 254);
        vgh[2] = AIR_UCHAR(AIR_CLAMP(1, hi + 1, 254));
      } else {
        uint hi = airIndex(minh, vghF[2], maxh, 168);
        vgh[2] = AIR_UCHAR(AIR_CLAMP(1, hi + 1, 169));
      }
    }
    vgh += nval;
    vghF += nval;
  }
  char cmt[AIR_STRLEN_LARGE + 1];
  if (doH) {
    snprintf(cmt, sizeof(cmt), "exclusions (v g h): %g %g %g", perc[0], perc[1],
             perc[2]);
  } else {
    snprintf(cmt, sizeof(cmt), "exclusions (v g): %g %g", perc[0], perc[1]);
  }
  nrrdCommentAdd(nvgh, cmt);
  snprintf(cmt, sizeof(cmt), "minv: %g", minv);
  nrrdCommentAdd(nvgh, cmt);
  snprintf(cmt, sizeof(cmt), "maxv: %g", maxv);
  nrrdCommentAdd(nvgh, cmt);
  snprintf(cmt, sizeof(cmt), "ming: %g", ming);
  nrrdCommentAdd(nvgh, cmt);
  snprintf(cmt, sizeof(cmt), "maxg: %g", maxg);
  nrrdCommentAdd(nvgh, cmt);
  if (doH) {
    snprintf(cmt, sizeof(cmt), "minh: %g", minh);
    nrrdCommentAdd(nvgh, cmt);
    snprintf(cmt, sizeof(cmt), "maxh: %g", maxh);
    nrrdCommentAdd(nvgh, cmt);
  }
  nrrdAxisInfoSet_va(nvgh, nrrdAxisInfoCenter, nrrdCenterUnknown, nrrdCenterNode,
                     nrrdCenterNode, nrrdCenterNode);

  return 0;
}

int
qbertScat(Nrrd *nvgh, uint pos, unsigned int size, const char *name) {
  static const char me[] = "qbertScat";
  size_t bins[2] = {size, size};
  int clamp[2] = {AIR_FALSE, AIR_FALSE};
  airArray *mop = airMopNew();
  Nrrd *nv, *nx, *nscA, *nscB;
  airMopAdd(mop, nv = nrrdNew(), nrrdNuke_mop, airMopAlways);
  airMopAdd(mop, nx = nrrdNew(), nrrdNuke_mop, airMopAlways);
  airMopAdd(mop, nscA = nrrdNew(), nrrdNuke_mop, airMopAlways);
  airMopAdd(mop, nscB = nrrdNew(), nrrdNuke_mop, airMopAlways);
  /* HEY: this used to be nin[2], but its passing to nrrdJoin caused
     "dereferencing type-punned pointer might break strict-aliasing rules"
     warning; GLK not sure how else to fix it */
  Nrrd **nin = AIR_CALLOC(2, Nrrd *);
  airMopAdd(mop, nin, airFree, airMopAlways);
  nin[0] = nv;
  nin[1] = nx;
  int E = 0;
  if (!E) E |= nrrdSlice(nv, nvgh, 0, 0);
  if (!E) E |= nrrdSlice(nx, nvgh, 0, pos);
  if (!E)
    E |= nrrdHistoJoint(nscA, (const Nrrd *const *)nin, NULL, 2, NULL, bins,
                        nrrdTypeFloat, clamp);
  if (!E) E |= nrrdArithUnaryOp(nscB, nrrdUnaryOpLog1p, nscA);
  if (!E) E |= nrrdHistoEq(nscA, nscB, NULL, 2048, 2, 0.45f);
  NrrdRange *range = NULL;
  if (!E) {
    range = nrrdRangeNewSet(nscA, nrrdBlind8BitRangeTrue);
    airMopAdd(mop, range, nrrdRangeNix_mop, airMopAlways);
    range->max *= 0.8;
  }
  if (!E) E |= nrrdQuantize(nscB, nscA, range, 8);
  if (!E) E |= nrrdFlip(nscA, nscB, 1);
  if (!E) E |= nrrdSave(name, nscA, NULL);
  if (E) {
    biffMovef(QBERT, NRRD, "%s: trouble generating/saving scatterplot", me);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  return 0;
}

static const char qbertInfo[]
  = "Generates volume datasets friendly to circa-2002 GPU-based "
    "volume renderers. "
    "The main value of this is a means of combining the functions of "
    "resampling a dataset to a particular size, measuring first (and "
    "optionally second) derivatives, and doing some semi-intelligent "
    "quantization of the derivative values down to 8-bits (if quantization "
    "is desired).  The various up and down sampling, as well as the "
    "the VGH measurements, can be done with various nrrd kernels.  Also, "
    "histogram-equalized "
    "VG and VH scatterplots can be generated at a specified resolution.";

int
main(int argc, const char *argv[]) {
  const char *me = argv[0];
  hestOpt *hopt = NULL;
  Nrrd *nin;
  hestOptAdd_1_Other(&hopt, "i", "nin", &nin, NULL, "input volume, in nrrd format",
                     nrrdHestNrrd);
  int notdoH;
  hestOptAdd_Flag(&hopt, "vg", &notdoH,
                  "Make a 2-channel VG volume, instead of the usual (default) "
                  "3-channel VGH volume.");
  int useFloat;
  hestOptAdd_Flag(&hopt, "f", &useFloat,
                  "Keep the output volume in floating point, instead of "
                  "(by default) quantizing down to 8-bits.  The "
                  "\"-vp\", \"-gp\", and \"-hp\" options become moot.");
  uint sz[3];
  hestOptAdd_3_UInt(&hopt, "d", "dimX dimY dimZ", sz, NULL,
                    "dimensions of output volume");
  int ups;
  hestOptAdd_Flag(&hopt, "up", &ups,
                  "Instead of just padding axes up to dimensions given "
                  "with \"-d\" when original dimensions are smaller, do filtered "
                  "upsampling.");
  NrrdKernelSpec *uk, *dk, *k00, *k11, *k22;
  hestOptAdd_1_Other(&hopt, "uk", "upsample k", &uk, "cubic:0,0.5",
                     "kernel to use when doing the upsampling enabled by \"-up\"",
                     nrrdHestKernelSpec);
  hestOptAdd_1_Other(&hopt, "dk", "downsample k", &dk, "tent",
                     "kernel to use when DOWNsampling volume to fit with specified "
                     "dimensions. NOTE: ringing can be problematic here.",
                     nrrdHestKernelSpec);
  hestOptAdd_1_Other(&hopt, "k00", "kern00", &k00, "tent",
                     "kernel for gageKernel00, used to probe values (\"V\") "
                     "in the volume that has been padded/resampled to fit in the "
                     "dimensions given by \"-d\"",
                     nrrdHestKernelSpec);
  hestOptAdd_1_Other(&hopt, "k11", "kern11", &k11, "cubicd:1,0",
                     "kernel for gageKernel11, used with k00 to probe "
                     "gradient magnitudes (\"G\")",
                     nrrdHestKernelSpec);
  hestOptAdd_1_Other(&hopt, "k22", "kern22", &k22, "cubicdd:1,0",
                     "kernel for gageKernel22, used with k00,k11 to "
                     "probe Hessian-based 2nd derivatives (\"H\")",
                     nrrdHestKernelSpec);
  float perc[3];
  hestOptAdd_1_Float(&hopt, "vp", "V excl perc", perc + 0, "0.000",
                     "Percent of voxels to throw away in quantization (if doing "
                     "quantization) based their data value being too high");
  hestOptAdd_1_Float(&hopt, "gp", "G perc", perc + 1, "0.002",
                     "Like \"-vp\", but for gradient magnitudes. ");
  hestOptAdd_1_Float(&hopt, "hp", "H perc", perc + 2, "0.004",
                     "Like \"-vp\", but for Hessian-based 2nd derivatives. ");
  int hlimrange;
  hestOptAdd_Flag(&hopt, "hlr", &hlimrange,
                  "For some reason (GLK forgets!) the original version of this "
                  "code limited the quantized 2nd derivative value to [1,169] "
                  "instead of [1,254]. Use this option to recreate that old "
                  "behavior. Without it, the fuller range of 8-bit values is used. ");
  uint scat;
  hestOptAdd_1_UInt(&hopt, "scat", "scat size", &scat, "0",
                    "generate VG (and VH) scatterplots with this resolution. "
                    "Size 0 means \"no scatterplots\".  The scatterplots are "
                    "histogram equalized, quantized, and saved out as PGM images "
                    "named \"vg.pgm\" (and \"vh.pgm\").");
  hestOptAdd_Flag(&hopt, "saveall", &qbertSaveAll,
                  "save all intermediate results for debugging");
  char *outS;
  hestOptAdd_1_String(&hopt, "o", "output", &outS, NULL, "output volume in nrrd format");

  hestParm *hparm = hestParmMopNew();
  hparm->elideSingleOtherType = AIR_TRUE;
  hparm->elideSingleNonExistFloatDefault = AIR_TRUE;
  hparm->elideMultipleNonExistFloatDefault = AIR_TRUE;
  hparm->respectDashDashHelp = AIR_TRUE;
  hestParseOrExit(hopt, argc - 1, argv + 1, hparm, me, qbertInfo);
  airArray *mop = airMopNew();
  airMopAdd(mop, hparm, hestParmFree_mop, airMopAlways);
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);
  char *errS;

  if (3 != nin->dim) {
    fprintf(stderr, "%s: input nrrd is %u-dimensional, not 3\n", me, nin->dim);
    airMopError(mop);
    exit(1);
  }
  if (!AIR_EXISTS(nin->axis[0].spacing)) {
    nrrdAxisInfoSpacingSet(nin, 0);
  }
  if (!AIR_EXISTS(nin->axis[1].spacing)) {
    nrrdAxisInfoSpacingSet(nin, 1);
  }
  if (!AIR_EXISTS(nin->axis[2].spacing)) {
    nrrdAxisInfoSpacingSet(nin, 2);
  }

  Nrrd *npad = nrrdNew();
  airMopAdd(mop, npad, nrrdNuke_mop, airMopAlways);
  if (qbertSizeUp(npad, nin, sz, ups ? uk : NULL)) {
    airMopAdd(mop, errS = biffGetDone(QBERT), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble:\n%s\n", me, errS);
    airMopError(mop);
    exit(1);
  }

  Nrrd *nrsmp = nrrdNew();
  airMopAdd(mop, nrsmp, nrrdNuke_mop, airMopAlways);
  if (qbertSizeDown(nrsmp, npad, sz, dk)) {
    airMopAdd(mop, errS = biffGetDone(QBERT), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble:\n%s\n", me, errS);
    airMopError(mop);
    exit(1);
  }
  airMopSub(mop, npad, nrrdNuke_mop);
  npad = nrrdNuke(npad);

  /* this axis info is being saved so that it can be re-enstated at the end */
  double amin[4], amax[4], spacing[4];
  spacing[0] = amin[0] = amax[0] = AIR_NAN;
  nrrdAxisInfoGet_nva(nrsmp, nrrdAxisInfoSpacing, spacing + 1);
  nrrdAxisInfoGet_nva(nrsmp, nrrdAxisInfoMin, amin + 1);
  nrrdAxisInfoGet_nva(nrsmp, nrrdAxisInfoMax, amax + 1);
  /* if we had to downsample, we may have enstated axis mins and maxs where
     they didn't exist before, and those shouldn't be saved in output.  But
     we can't just copy axis mins and maxs from the original input because
     padding could have changed them.  If no axis mins and maxs existed on
     the input nrrd, these will all be nan, so they won't be saved out.
     NOTE: we're only nixing axis min/max information, not spacing. */
  for (uint axi = 0; axi <= 2; axi++) {
    if (!AIR_EXISTS(nin->axis[axi].min)) amin[1 + axi] = AIR_NAN;
    if (!AIR_EXISTS(nin->axis[axi].max)) amax[1 + axi] = AIR_NAN;
  }
  Nrrd *nvghF = nrrdNew();
  airMopAdd(mop, nvghF, nrrdNuke_mop, airMopAlways);
  if (qbertProbe(nvghF, nrsmp, k00, k11, k22, !notdoH, sz)) {
    airMopAdd(mop, errS = biffGetDone(QBERT), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble:\n%s\n", me, errS);
    airMopError(mop);
    exit(1);
  }
  airMopSub(mop, nrsmp, nrrdNuke_mop);
  nrsmp = nrrdNuke(nrsmp);
  int E;
  if (useFloat) {
    /* we're done! */
    if (scat
        && (qbertScat(nvghF, 1, scat, "vg.pgm")
            || (!notdoH && qbertScat(nvghF, 2, scat, "vh.pgm")))) {
      airMopAdd(mop, errS = biffGetDone(QBERT), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble:\n%s\n", me, errS);
      airMopError(mop);
      exit(1);
    }
    E = nrrdSave(outS, nvghF, NULL);
  } else {
    Nrrd *nvhist = nrrdNew();
    Nrrd *nghist = nrrdNew();
    Nrrd *nhhist = nrrdNew();
    airMopAdd(mop, nvhist, nrrdNuke_mop, airMopAlways);
    airMopAdd(mop, nghist, nrrdNuke_mop, airMopAlways);
    airMopAdd(mop, nhhist, nrrdNuke_mop, airMopAlways);
    if (qbertMakeVghHists(nvhist, nghist, nhhist, sz, QBERT_HIST_BINS, nvghF, nin)) {
      fprintf(stderr, "%s: trouble:\n%s\n", me, errS = biffGetDone(QBERT));
      free(errS);
      exit(1);
    }

    Nrrd *nvgh = nrrdNew();
    airMopAdd(mop, nvgh, nrrdNuke_mop, airMopAlways);
    if (qbertMakeVgh(nvgh, nvhist, nghist, nhhist, sz, perc, nvghF, !hlimrange)) {
      airMopAdd(mop, errS = biffGetDone(QBERT), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble:\n%s\n", me, errS);
      airMopError(mop);
      exit(1);
    }
    airMopSub(mop, nvghF, nrrdNuke_mop);
    nvghF = nrrdNuke(nvghF);

    if (scat
        && (qbertScat(nvgh, 1, scat, "vg.pgm")
            || (!notdoH && qbertScat(nvgh, 2, scat, "vh.pgm")))) {
      airMopAdd(mop, errS = biffGetDone(QBERT), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble:\n%s\n", me, errS);
      airMopError(mop);
      exit(1);
    }

    /* do final decoration of axes */
    nrrdAxisInfoSet_va(nvgh, nrrdAxisInfoLabel, !notdoH ? "vgh" : "vg", "x", "y", "z");
    nrrdAxisInfoSet_nva(nvgh, nrrdAxisInfoMin, amin);
    nrrdAxisInfoSet_nva(nvgh, nrrdAxisInfoMax, amax);
    nrrdAxisInfoSet_nva(nvgh, nrrdAxisInfoSpacing, spacing);
    nrrdContentSet_va(nvgh, "qbert", nin, "");

    E = nrrdSave(outS, nvgh, NULL);
  }
  if (E) {
    airMopAdd(mop, errS = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble saving output:\n%s\n", me, errS);
    airMopError(mop);
    exit(1);
  }

  /* HEY: why am I getting memory-in-use with purify? */
  airMopOkay(mop);
  exit(0);
}
