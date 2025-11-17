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

#include "../dye.h"

/*
** hist[0]: hue vs sat
** hist[1]: hue vs val
*/
void
imageProc(Nrrd *nhproj[3], Nrrd *nhist[2], unsigned int sH, float *rgb,
          unsigned int size0, unsigned int sXY, unsigned int overSampleNum,
          float overSampleScale) {
  unsigned int xyi, hi, si, vi, oi;
  float rr, gg, bb, hh, ss, vv, *hist[2];
  double rndA, rndB;

  nrrdZeroSet(nhist[0]);
  nrrdZeroSet(nhist[1]);
  hist[0] = AIR_CAST(float *, nhist[0]->data);
  hist[1] = AIR_CAST(float *, nhist[1]->data);
  for (xyi = 0; xyi < sXY; xyi++) {
    rr = AIR_CLAMP(0, rgb[0], 255);
    gg = AIR_CLAMP(0, rgb[1], 255);
    bb = AIR_CLAMP(0, rgb[2], 255);
    rr = (float)AIR_AFFINE(-1, rr, 256, 0, 1);
    gg = (float)AIR_AFFINE(-1, gg, 256, 0, 1);
    bb = (float)AIR_AFFINE(-1, bb, 256, 0, 1);
    dyeRGBtoHSV(&hh, &ss, &vv, rr, gg, bb);
    si = airIndexClamp(0, ss, 1, sH);
    vi = airIndexClamp(0, vv, 1, sH);

#define UPDATE_HIST(rnd)                                                                \
  hi = airIndexClamp(0, hh + overSampleScale * (1 - ss) * (rnd), 1, sH);                \
  hist[0][hi + sH * si] += 1.0f / overSampleNum;                                        \
  hist[1][hi + sH * vi] += 1.0f / overSampleNum

    if (overSampleNum % 2 == 1) {
      airNormalRand(&rndA, NULL);
      UPDATE_HIST(rndA);
      overSampleNum -= 1;
    }
    for (oi = 0; oi < overSampleNum; oi += 2) {
      airNormalRand(&rndA, &rndB);
      UPDATE_HIST(rndA);
      UPDATE_HIST(rndB);
    }
    rgb += size0;
  }
  nrrdProject(nhproj[0], nhist[0], 1, nrrdMeasureHistoMean, nrrdTypeFloat);
  nrrdProject(nhproj[1], nhist[1], 1, nrrdMeasureHistoMean, nrrdTypeFloat);
  nrrdProject(nhproj[2], nhist[1], 1, nrrdMeasureSum, nrrdTypeFloat);
}

const char *mchistInfo = ("Makes a color histogram of frames. ");

int
main(int argc, const char *argv[]) {
  const char *me = argv[0];
  airArray *mop = airMopNew();
  hestParm *hparm = hestParmMopNew();
  airMopAdd(mop, hparm, (airMopper)hestParmFree, airMopAlways);
  hparm->responseFileEnable = AIR_TRUE;
  hestOpt *hopt = NULL;
  char **ninStr;
  unsigned int ninLen;
  hestOptAdd_Nv_String(&hopt, "i", "images", 1, -1, &ninStr, NULL,
                       "input image sequence", &ninLen);
  unsigned int sH;
  hestOptAdd_1_UInt(&hopt, "sh", "histo size", &sH, "500", "histogram size");
  NrrdKernelSpec *ksp;
  hestOptAdd_1_Other(&hopt, "k", "kern", &ksp, "tent", "kernel for upsampling images",
                     nrrdHestKernelSpec);
  float upSample;
  hestOptAdd_1_Float(&hopt, "us", "upsampling", &upSample, "1",
                     "amount of upsampling of image");
  unsigned int overSampleNum;
  hestOptAdd_1_UInt(&hopt, "osn", "# oversmp", &overSampleNum, "1",
                    "number of sample per (upsampled) pixel");
  float overSampleScale;
  hestOptAdd_1_Float(&hopt, "osc", "scaling", &overSampleScale, "1",
                     "scaling with oversampling");
  float maxSum;
  hestOptAdd_1_Float(&hopt, "ms", "max sum", &maxSum, "10",
                     "per-hue histogram summation is non-linearly and "
                     "asymptotically clamped to this maximum");
  char *outS;
  hestOptAdd_1_String(&hopt, "o", "nout", &outS, "-", "output filename");

  hestParseOrExit(hopt, argc - 1, argv + 1, hparm, me, mchistInfo);
  airMopAdd(mop, hopt, (airMopper)hestOptFree, airMopAlways);
  airMopAdd(mop, hopt, (airMopper)hestParseFree, airMopAlways);

  if (0 == overSampleNum) {
    fprintf(stderr, "%s: overSampleNum must be > 0\n", me);
    airMopError(mop);
    return 1;
  }
  Nrrd *nin0 = nrrdNew();
  char *err;
  airMopAdd(mop, nin0, (airMopper)nrrdNuke, airMopAlways);
  if (nrrdLoad(nin0, ninStr[0], NULL)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: couldn't load first image:\n%s", me, err);
    airMopError(mop);
    return 1;
  }
  if (!((3 == nin0->axis[0].size || 4 == nin0->axis[0].size) && 3 == nin0->dim
        && nrrdTypeUChar == nin0->type)) {
    fprintf(stderr,
            "%s: 1st image not 3D (3-or-4)-by-X-by-Y %s array "
            "(got %u-D %s array)\n",
            me, airEnumStr(nrrdType, nrrdTypeUChar), nin0->dim,
            airEnumStr(nrrdType, nin0->type));
    airMopError(mop);
    return 1;
  }
  NrrdResampleContext *rsmc = nrrdResampleContextNew();
  airMopAdd(mop, rsmc, (airMopper)nrrdResampleContextNix, airMopAlways);
  unsigned int size0 = AIR_UINT(nin0->axis[0].size);
  unsigned int sX = AIR_UINT(upSample * nin0->axis[1].size);
  unsigned int sY = AIR_UINT(upSample * nin0->axis[2].size);
  Nrrd *nrgb = nrrdNew();
  airMopAdd(mop, nrgb, (airMopper)nrrdNuke, airMopAlways);
  if (nrrdResampleDefaultCenterSet(rsmc, nrrdCenterCell)
      || nrrdResampleInputSet(rsmc, nin0)
      || nrrdResampleKernelSet(rsmc, 1, ksp->kernel, ksp->parm)
      || nrrdResampleKernelSet(rsmc, 2, ksp->kernel, ksp->parm)
      || nrrdResampleSamplesSet(rsmc, 1, sX) //
      || nrrdResampleSamplesSet(rsmc, 2, sY) //
      || nrrdResampleRangeFullSet(rsmc, 1)   //
      || nrrdResampleRangeFullSet(rsmc, 2)   //
      || nrrdResampleTypeOutSet(rsmc, nrrdTypeFloat)
      || nrrdResampleRenormalizeSet(rsmc, AIR_TRUE) //
      || nrrdResampleExecute(rsmc, nrgb)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error resampling slice:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  Nrrd *nhist[2];
  nhist[0] = nrrdNew();
  airMopAdd(mop, nhist[0], (airMopper)nrrdNuke, airMopAlways);
  nhist[1] = nrrdNew();
  airMopAdd(mop, nhist[1], (airMopper)nrrdNuke, airMopAlways);
  if (nrrdMaybeAlloc_va(nhist[0], nrrdTypeFloat, 2, AIR_SIZE_T(sH), AIR_SIZE_T(sH))
      || nrrdMaybeAlloc_va(nhist[1], nrrdTypeFloat, 2, AIR_SIZE_T(sH), AIR_SIZE_T(sH))) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error allocating histos:\n%s", me, err);
    airMopError(mop);
    return 1;
  }
  nhist[0]->axis[0].min = nhist[0]->axis[1].min = 0.0;
  nhist[0]->axis[0].max = nhist[0]->axis[1].max = 1.0;
  nhist[1]->axis[0].min = nhist[1]->axis[1].min = 0.0;
  nhist[1]->axis[0].max = nhist[1]->axis[1].max = 1.0;
  Nrrd *nhproj[3];
  nhproj[0] = nrrdNew();
  airMopAdd(mop, nhproj[0], (airMopper)nrrdNix, airMopAlways);
  nhproj[1] = nrrdNew();
  airMopAdd(mop, nhproj[1], (airMopper)nrrdNix, airMopAlways);
  nhproj[2] = nrrdNew();
  airMopAdd(mop, nhproj[2], (airMopper)nrrdNix, airMopAlways);

  printf("working ...       ");
  Nrrd *nin = nrrdNew();
  airMopAdd(mop, nin, (airMopper)nrrdNuke, airMopAlways);
  Nrrd *npreout = nrrdNew();
  airMopAdd(mop, npreout, (airMopper)nrrdNuke, airMopAlways);
  if (nrrdMaybeAlloc_va(npreout, nrrdTypeFloat, 3, AIR_SIZE_T(3), AIR_SIZE_T(sH),
                        AIR_SIZE_T(ninLen))) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error allocating pre-output:\n%s", me, err);
    airMopError(mop);
    return 1;
  }
  float *preout = AIR_CAST(float *, npreout->data);
  unsigned int ti; // used post-loop with airDoneStr
  char doneStr[13];
  for (ti = 0; ti < ninLen; ti++) {
    printf("%s", airDoneStr(0, ti, ninLen, doneStr));
    fflush(stdout);
    if (nrrdLoad(nin, ninStr[ti], NULL)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: couldn't load image[%u]:\n%s", me, ti, err);
      airMopError(mop);
      return 1;
    }
    if (!nrrdSameSize(nin0, nin, AIR_TRUE)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: nin[%u] not like nin[0]:\n%s", me, ti, err);
      airMopError(mop);
      return 1;
    }
    if (nrrdResampleInputSet(rsmc, nin) || nrrdResampleExecute(rsmc, nrgb)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble resampling nin[%u]:\n%s", me, ti, err);
      airMopError(mop);
      return 1;
    }
    if (nrrdWrap_va(nhproj[0], preout + 0 * sH, nrrdTypeFloat, 1, AIR_SIZE_T(sH))
        || nrrdWrap_va(nhproj[1], preout + 1 * sH, nrrdTypeFloat, 1, AIR_SIZE_T(sH))
        || nrrdWrap_va(nhproj[2], preout + 2 * sH, nrrdTypeFloat, 1, AIR_SIZE_T(sH))) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble wrapping output[%u]:\n%s", me, ti, err);
      airMopError(mop);
      return 1;
    }
    float *rgb = AIR_CAST(float *, nrgb->data);
    imageProc(nhproj, nhist, sH, rgb, size0, sX * sY, overSampleNum, overSampleScale);
    preout += 3 * sH;
  }
  printf("%s\n", airDoneStr(0, ti, ninLen, doneStr));
  fflush(stdout);

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, (airMopper)nrrdNuke, airMopAlways);
  if (nrrdMaybeAlloc_va(nout, nrrdTypeFloat, 3, AIR_SIZE_T(3), AIR_SIZE_T(sH),
                        AIR_SIZE_T(ninLen))) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error allocating output:\n%s", me, err);
    airMopError(mop);
    return 1;
  }
  float *out = AIR_CAST(float *, nout->data);
  preout = AIR_CAST(float *, npreout->data);
  for (ti = 0; ti < ninLen; ti++) {
    unsigned int hi;
    float hh, vv, ss, scl;
    for (hi = 0; hi < sH; hi++) {
      hh = (float)AIR_AFFINE(0, hi, sH, 0, 1);
      if (!preout[hi + 2 * sH]) {
        ELL_3V_SET(out + 3 * hi, 0, 0, 0);
      } else {
        ss = preout[hi + 2 * sH];
        scl = ss / (maxSum + ss);
        vv = scl * preout[hi + 1 * sH];
        dyeHSVtoRGB(out + 0 + 3 * hi, out + 1 + 3 * hi, out + 2 + 3 * hi, hh,
                    preout[hi + 0 * sH], vv);
      }
    }
    out += 3 * sH;
    preout += 3 * sH;
  }

  if (nrrdSave(outS, nout, NULL)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error saving output:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  return 0;
}
