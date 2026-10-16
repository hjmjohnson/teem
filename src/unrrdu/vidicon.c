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

#include "unrrdu.h"
#include "privateUnrrdu.h"

#define INFO "Try to create the look of early 80s analog B+W video"
static const char *unu_vidiconInfoL
  = (INFO ". Does various things, some more justified than others.\n "
          "* (there is no single nrrd function which does all this)");

static int
unu_vidiconMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nin;
  uint inHidx = hestOptAdd_1_Other(&hopt, "i", "input", &nin, NULL,
                                   "input image. Should be grayscale PNG.",
                                   nrrdHestNrrdNoTTY);
  hopt[inHidx].priority -= 10;
  double rescale;
  hestOptAdd_1_Double(&hopt, "rs", "rescale", &rescale, "0.75",
                      "how to rescale (downsample) the image prior to processing, "
                      "just to get a better representation of the floating-point "
                      "range of image values (overcoming 8-bit quantization effects)");
  NrrdKernelSpec *rescaleKsp;
  hestOptAdd_1_Other(&hopt, "rsk", "kern", &rescaleKsp, "hann:5",
                     "kernel for rescaling.", nrrdHestKernelSpec);
  double rperc;
  hestOptAdd_1_Double(&hopt, "rsp", "percentile", &rperc, "1.5",
                      "after rescaling, the highest and lowest percentiles are mapped "
                      "to 0.0 and 255.0, just to have a uniform range of intensities "
                      "in subsequent processing. This option determines how big those "
                      "percentiles are.");
  uint vsize[2];
  hestOptAdd_2_UInt(&hopt, "vs", "sx sy", vsize, "550 525",
                    "the lowest (\"video\") resolution to which the image is "
                    "down-sampled, reflecting the limited resolution of the "
                    "vidicon tubes");
  uint vpadding[2];
  hestOptAdd_2_UInt(&hopt, "pad", "padX padY", vpadding, "10 10",
                    "at the lowest resolution, there should be this much padding "
                    "by black, to reflect the fact the signal outside the tube "
                    "(e.g. between scanlines is black)");
  NrrdKernelSpec *vdsmp[2];
  hestOptAdd_2_Other(&hopt, "vk", "kernX kernY", vdsmp, "hann:1,4 cubic:0,0.5",
                     "kernels for downsampling to video resolution; the horizontal "
                     "and vertical kernels are different",
                     nrrdHestKernelSpec);
  char *stpfx;
  hestOptAdd_1_String(&hopt, "stp", "prefix", &stpfx, "",
                      "if a string is given here, a series of images are saved, "
                      "representing the various stages of processing");
  char *out;
  hestOptAdd_1_String(&hopt, "o", "output", &out, NULL, "output nrrd");

  // (boilerplate for YES some hparm modification)
  hestParm *hparm = hestParmCopy(_hparm);
  hparm->elideSingleOtherDefault = AIR_FALSE;
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_vidiconInfoL);
  airArray *mop = unrrduMopNew(hopt, hparm);
  char *err;

  Nrrd *ntmp = nrrdNew();
  airMopAdd(mop, ntmp, nrrdNuke_mop, airMopAlways);
  NrrdRange *b8range = nrrdRangeNew(0.0, 255.0);
  airMopAdd(mop, b8range, nrrdRangeNix_mop, airMopAlways);

  if (!(2 == nin->dim && nrrdTypeBlock != nin->type)) {
    fprintf(stderr, "%s: need input as 2D grayscale image (not %u-d %s)\n", me, nin->dim,
            airEnumStr(nrrdType, nin->type));
    airMopError(mop);
    return 1;
  }
  Nrrd *nrescale = nrrdNew();
  airMopAdd(mop, nrescale, nrrdNuke_mop, airMopAlways);

  DECISEP_DECL(2);
  fprintf(stderr, "%s: rescaling by %s ... \n", me, airDtoS(Nb[0], rescale, Ds));
  NrrdResampleContext *rsmc = nrrdResampleContextNew();
  airMopAdd(mop, rsmc, nrrdResampleContextNix_mop, airMopAlways);
  if (nrrdResampleDefaultCenterSet(rsmc, nrrdCenterCell)
      || nrrdResampleInputSet(rsmc, nin)
      || nrrdResampleKernelSet(rsmc, 0, rescaleKsp->kernel, rescaleKsp->parm)
      || nrrdResampleKernelSet(rsmc, 1, rescaleKsp->kernel, rescaleKsp->parm)
      || nrrdResampleSamplesSet(rsmc, 0, AIR_SIZE_T(rescale * nin->axis[0].size))
      || nrrdResampleSamplesSet(rsmc, 1, AIR_SIZE_T(rescale * nin->axis[1].size))
      || nrrdResampleRangeFullSet(rsmc, 0) || nrrdResampleRangeFullSet(rsmc, 1)
      || nrrdResampleTypeOutSet(rsmc, nrrdTypeFloat)
      || nrrdResampleRenormalizeSet(rsmc, AIR_TRUE)
      || nrrdResampleExecute(rsmc, nrescale)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: problem rescaling:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

#define SAVE_TMP(name, nrrd)                                                            \
  if (airStrlen(stpfx)) {                                                               \
    char stname[AIR_STRLEN_SMALL + 1];                                                  \
    snprintf(stname, sizeof(stname), "%s-" #name ".png", stpfx);                        \
    if (nrrdQuantize(ntmp, nrrd, b8range, 8) || nrrdSave(stname, ntmp, 0)) {            \
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);                   \
      fprintf(stderr, "%s: problem saving %s:\n%s", me, stname, err);                   \
      airMopError(mop);                                                                 \
      return 1;                                                                         \
    }                                                                                   \
  }
  SAVE_TMP(rescale, nrescale);

  /* rescaling values to 0.0 -- 255.0 based on percentile rperc */
  {
    double sum, maxval;
    uint hi, hbins;
    size_t ii, nn;
    airArray *submop = airMopNew();
    Nrrd *nhist = nrrdNew();
    hbins = 3000;
    airMopAdd(submop, nhist, nrrdNuke_mop, airMopAlways);
    if (nrrdHisto(nhist, nrescale, NULL, NULL, hbins, nrrdTypeDouble)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble making histogram:\n%s", me, err);
      airMopError(submop);
      airMopError(mop);
      return 1;
    }
    double *hist = AIR_CAST(double *, nhist->data);
    double total = AIR_CAST(double, nrrdElementNumber(nrescale));
    double minval = AIR_NAN;
    sum = 0;
    for (hi = 0; hi < hbins; hi++) {
      sum += hist[hi];
      if (sum >= rperc * total / 100.0) {
        minval = AIR_AFFINE(0, hi, hbins - 1, nhist->axis[0].min, nhist->axis[0].max);
        break;
      }
    }
    if (hi == hbins || !AIR_EXISTS(minval)) {
      fprintf(stderr, "%s: failed to find lower %s-percentile value", me,
              airDtoS(Nb[0], rperc, Ds));
      airMopError(submop);
      airMopError(mop);
      return 1;
    }
    maxval = AIR_NAN;
    sum = 0;
    for (hi = hbins; hi; hi--) {
      sum += hist[hi - 1];
      if (sum >= rperc * total / 100.0) {
        maxval = AIR_AFFINE(0, hi - 1, hbins - 1, nhist->axis[0].min,
                            nhist->axis[0].max);
        break;
      }
    }
    if (!hi || !AIR_EXISTS(maxval)) {
      fprintf(stderr, "%s: failed to find upper %s-percentile value", me,
              airDtoS(Nb[0], rperc, Ds));
      airMopError(submop);
      airMopError(mop);
      return 1;
    }
    fprintf(stderr, "%s: min %s --> 0, max %s --> 255\n", me, airDtoS(Nb[0], minval, Ds),
            airDtoS(Nb[1], maxval, Ds));
    nn = nrrdElementNumber(nrescale);
    float *rescaled = AIR_CAST(float *, nrescale->data);
    for (ii = 0; ii < nn; ii++) {
      rescaled[ii] = AIR_FLOAT(AIR_AFFINE(minval, rescaled[ii], maxval, 0.0, 255.0));
    }
    airMopOkay(submop);
    submop = NULL;
  }

  /* padding rescaled image with black */
  uint rpadding[2];
  rpadding[0] = AIR_ROUNDUP(AIR_CAST(double, vpadding[0]) * nrescale->axis[0].size
                            / vsize[0]);
  rpadding[1] = AIR_ROUNDUP(AIR_CAST(double, vpadding[1]) * nrescale->axis[1].size
                            / vsize[1]);
  fprintf(stderr, "%s: padding in rescaled image: %u x %u\n", me, rpadding[0],
          rpadding[1]);
  Nrrd *npad = nrrdNew();
  airMopAdd(mop, npad, nrrdNuke_mop, airMopAlways);
  {
    ptrdiff_t pmin[2], pmax[2];
    pmin[0] = -AIR_CAST(ptrdiff_t, rpadding[0]);
    pmin[1] = -AIR_CAST(ptrdiff_t, rpadding[1]);
    pmax[0] = nrescale->axis[0].size + rpadding[0];
    pmax[1] = nrescale->axis[1].size + rpadding[1];
    if (nrrdPad_nva(npad, nrescale, pmin, pmax, nrrdBoundaryPad, 0.0)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: problem padding:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
  }

  /* rescaling down to "video" resolution */
  fprintf(stderr, "%s: downsampling to %u x %u\n", me,
          AIR_UINT(vsize[0] + 2 * vpadding[0]), AIR_UINT(vsize[1] + 2 * vpadding[1]));
  Nrrd *nvbase = nrrdNew();
  airMopAdd(mop, nvbase, nrrdNuke_mop, airMopAlways);
  if (nrrdResampleDefaultCenterSet(rsmc, nrrdCenterCell)
      || nrrdResampleInputSet(rsmc, npad)
      || nrrdResampleKernelSet(rsmc, 0, vdsmp[0]->kernel, vdsmp[0]->parm)
      || nrrdResampleKernelSet(rsmc, 1, vdsmp[1]->kernel, vdsmp[1]->parm)
      || nrrdResampleSamplesSet(rsmc, 0, vsize[0] + 2 * vpadding[0])
      || nrrdResampleSamplesSet(rsmc, 1, vsize[1] + 2 * vpadding[1])
      || nrrdResampleRangeFullSet(rsmc, 0) || nrrdResampleRangeFullSet(rsmc, 1)
      || nrrdResampleTypeOutSet(rsmc, nrrdTypeFloat)
      || nrrdResampleRenormalizeSet(rsmc, AIR_TRUE)
      || nrrdResampleExecute(rsmc, nvbase)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: problem downsampling to video resolution:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  /* halo, lowfilt, windowing, noise, filt, interlace, noise, fuzz, upsample */

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  nrrdCopy(nout, nvbase);

  MY_SAVE(out, nout, NULL);
  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD_HIDE(vidicon, INFO);
