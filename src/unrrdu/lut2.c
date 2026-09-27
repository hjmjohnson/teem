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

#define INFO "Map nrrd through a bivariate lookup table"
static const char *unu_lut2InfoL
  = (INFO " (itself represented as a nrrd). The lookup table "
          "can be 2D, in which case the output "
          "has the same dimension as the input, or 3D, in which case "
          "the output has one more dimension than the input, and each "
          "pair of values is mapped to a scanline (along axis 0) from the "
          "lookup table.  In any case, axis 0 of the input must have length "
          "two.\n " MY_UNRRDU_DILRR_INFO("lut2") /* */
     "* Uses nrrdApply2DLut");

static int
unu_lut2Main(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  Nrrd *nlut;
  hestOptAdd_1_Other(&hopt, "m,map", "lut", &nlut, NULL,
                     "lookup table to map input nrrd through", nrrdHestNrrdNoTTY);
  int rescale[2];
  uint rsclOptIdx
    = hestOptAdd_2_Bool(&hopt, "r,rescale", "bool bool", rescale, "false false",
                        "rescale one or both of the input values from the "
                        "input range to the lut domain.  The lut domain is either "
                        "explicitly defined by the axis min,max along axis 0 or 1, "
                        "or, it is implicitly defined as zero to the length of that "
                        "axis minus one.");
  double min[2], max[2];
  hestOptAdd_2_Double(&hopt, "min,minimum", "min0 min1", min, "nan nan",
                      "Low ends of input range. Defaults to lowest values "
                      "found in input nrrd.  Explicitly setting this is useful "
                      "only with rescaling (\"-r\")");
  hestOptAdd_2_Double(&hopt, "max,maximum", "max0 max1", max, "nan nan",
                      "High end of input range. Defaults to highest values "
                      "found in input nrrd.  Explicitly setting this is useful "
                      "only with rescaling (\"-r\")");
  int blind8BitRange;
  hestOptAdd_1_Bool(&hopt, "blind8", "bool", &blind8BitRange,
                    nrrdStateBlind8BitRange ? "true" : "false",
                    "Whether to know the range of 8-bit data blindly "
                    "(uchar is always [0,255], signed char is [-128,127]). "
                    "Explicitly setting this is useful only with rescaling (\"-r\")");
  int typeOut;
  hestOptAdd_1_Other(&hopt, "t,type", "type", &typeOut, "default",
                     "specify the type (\"int\", \"float\", etc.) of the "
                     "output nrrd. "
                     "By default (not using this option), the output type "
                     "is the lut's type.",
                     &unrrduHestMaybeTypeCB);
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_lut2InfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  NrrdRange *range[2] = {NULL, NULL};

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);

  if (!(nin->dim > 1 && 2 == nin->axis[0].size)) {
    char stmp[AIR_STRLEN_SMALL + 1];
    fprintf(stderr,
            "%s: input nrrd dim must be > 1, and axis[0].size "
            "must be 2 (not %s)\n",
            me, airSprintSize_t(stmp, nin->axis[0].size));
    airMopError(mop);
    return 1;
  }
  uint mapAxis = nlut->dim - 2;
  if (!(0 == mapAxis || 1 == mapAxis)) {
    fprintf(stderr, "%s: dimension of lut should be 2 or 3, not %u", me, nlut->dim);
    airMopError(mop);
    return 1;
  }

  unrrduUnuGetenvDefaultImplicitLutRmapRescale();
  Nrrd *ntmp[2];
  /* NOTE: comment in lut.c describes logic for how rescale is set */
  for (uint rai = 0; rai <= 1; rai++) {
    if (!hestSourceUser(hopt[rsclOptIdx].source)) {
      if (!(AIR_EXISTS(nlut->axis[mapAxis + rai].min)
            && AIR_EXISTS(nlut->axis[mapAxis + rai].max))) {
        rescale[rai] = unrrduUnuDefaultImplicitLutRmapRescale;
      }
    }
    if (rescale[rai]) {
      ntmp[rai] = nrrdNew();
      airMopAdd(mop, ntmp[rai], nrrdNuke_mop, airMopAlways);
      if (nrrdSlice(ntmp[rai], nin, 0, rai)) {
        airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
        fprintf(stderr, "%s: trouble slicing input value %u:\n%s", me, rai, err);
        airMopError(mop);
        return 1;
      }
      range[rai] = nrrdRangeNew(min[rai], max[rai]);
      airMopAdd(mop, range[rai], nrrdRangeNix_mop, airMopAlways);
      nrrdRangeSafeSet(range[rai], ntmp[rai], blind8BitRange);
    }
  }
  if (nrrdTypeDefault == typeOut) {
    typeOut = nlut->type;
  }
  if (nrrdApply2DLut(nout, nin, 0, range[0], range[1], nlut, typeOut, rescale[0],
                     rescale[1])) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble applying 2-D LUT:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  MY_HISTORY(nout, nin);
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(lut2, INFO);
