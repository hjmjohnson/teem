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

#define INFO "Ring removal for CT"
static const char *unu_deringInfoL = (INFO
                                      ". Should be considered a work-in-progress. ");

static int
unu_deringMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  hestOpt *hopt = NULL;
  double center[2];
  hestOptAdd_2_Double(&hopt, "c,center", "x y", center, NULL,
                      "center of rings, in index space of fastest two axes");
  int verbose;
  hestOptAdd_1_Int(&hopt, "v,verbose", "v", &verbose, "0", "verbosity level");
  int linterp;
  hestOptAdd_1_Bool(&hopt, "li,linterp", "bool", &linterp, "false",
                    "whether to use linear interpolation during polar transform");
  int vertSeam;
  hestOptAdd_1_Bool(&hopt, "vs,vertseam", "bool", &vertSeam, "false",
                    "whether to dering left and right sides separately "
                    "(requires an even value for -tn thetanum)");
  uint thetaNum;
  hestOptAdd_1_UInt(&hopt, "tn,thetanum", "# smpls", &thetaNum, "20",
                    "# of theta samples");
  double radScale;
  hestOptAdd_1_Double(&hopt, "rs,radscale", "scale", &radScale, "1.0",
                      "scaling on radius in polar transform");
  NrrdKernelSpec *rkspec, *tkspec;
  hestOptAdd_1_Other(&hopt, "rk,radiuskernel", "kern", &rkspec, "gauss:3,4",
                     "kernel for high-pass filtering along radial direction",
                     nrrdHestKernelSpec);
  hestOptAdd_1_Other(&hopt, "tk,thetakernel", "kern", &tkspec, "box",
                     "kernel for blurring along theta direction.", nrrdHestKernelSpec);
  double clampPerc[2];
  hestOptAdd_2_Double(&hopt, "cp,clampperc", "lo hi", clampPerc, "0.0 0.0",
                      "when clamping values as part of ring estimation, the "
                      "clamping range is set to exclude this percent of values "
                      "from the low and high end of the data range");
  Nrrd *nmask;
  hestOptAdd_1_Other(&hopt, "m,mask", "mask", &nmask, "",
                     "optional: after deringing, output undergoes a lerp, "
                     "parameterized by this array, from the background value "
                     "(via \"-b\") where mask=0 to the original deringing "
                     "output where mask=1.  This lerp is effectively the same "
                     "as a \"unu 3op lerp\", so this should either be match the "
                     "input in size, or match its slices along the slowest axis.",
                     nrrdHestNrrdNoTTY);
  double backval;
  hestOptAdd_1_Double(&hopt, "b,back", "val", &backval, "0.0",
                      "when using a mask (\"-m\"), the background value to "
                      "lerp with.");
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for YES some hparm modification)
  hestParm *hparm = hestParmCopy(_hparm);
  hparm->elideSingleOtherDefault = AIR_FALSE;
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_deringInfoL);
  airArray *mop = unrrduMopNew(hopt, hparm);
  char *err;

  if (nmask) {
    if (!(2 == nmask->dim && nrrdTypeBlock != nmask->type
          && nmask->axis[0].size == nin->axis[0].size
          && nmask->axis[1].size == nin->axis[1].size)) {
      fprintf(stderr, "%s: given mask not 2-D %u-by-%u array of scalar type", me,
              AIR_UINT(nin->axis[0].size), AIR_UINT(nin->axis[1].size));
      airMopError(mop);
      return 1;
    }
  }

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  NrrdDeringContext *drc = nrrdDeringContextNew();
  airMopAdd(mop, drc, nrrdDeringContextNix_mop, airMopAlways);
  if (nrrdDeringVerboseSet(drc, verbose) || nrrdDeringLinearInterpSet(drc, linterp)
      || nrrdDeringVerticalSeamSet(drc, vertSeam) || nrrdDeringInputSet(drc, nin)
      || nrrdDeringCenterSet(drc, center[0], center[1])
      || nrrdDeringRadiusScaleSet(drc, radScale) || nrrdDeringThetaNumSet(drc, thetaNum)
      || nrrdDeringRadialKernelSet(drc, rkspec->kernel, rkspec->parm)
      || nrrdDeringThetaKernelSet(drc, tkspec->kernel, tkspec->parm)
      || nrrdDeringClampPercSet(drc, clampPerc[0], clampPerc[1])
      || nrrdDeringExecute(drc, nout)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error deringing:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  if (nmask) {
    NrrdIter *nitout, *nitmask, *nitback;
    Nrrd *ntmp;
    nitout = nrrdIterNew();
    airMopAdd(mop, nitout, nrrdIterNix_mop, airMopAlways);
    nitmask = nrrdIterNew();
    airMopAdd(mop, nitmask, nrrdIterNix_mop, airMopAlways);
    nitback = nrrdIterNew();
    airMopAdd(mop, nitback, nrrdIterNix_mop, airMopAlways);
    nrrdIterSetValue(nitback, backval);

    ntmp = nrrdNew();
    airMopAdd(mop, ntmp, nrrdNuke_mop, airMopAlways);

    nrrdIterSetNrrd(nitout, nout);
    nrrdIterSetNrrd(nitmask, nmask);
    if (nrrdArithIterTernaryOpSelect(ntmp, nrrdTernaryOpLerp, nitmask, nitback, nitout,
                                     2)
        || nrrdCopy(nout, ntmp)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error masking:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
  }

  MY_HISTORY(nout, nin);
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD_HIDE(dering, INFO);
