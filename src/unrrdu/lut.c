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

#include "unrrdu.h"
#include "privateUnrrdu.h"

#define INFO "Map nrrd through one univariate lookup table"
static const char *_unrrdu_lutInfoL
  = (INFO " (itself represented as a nrrd). The lookup table "
          "can be 1D, in which case the output "
          "has the same dimension as the input, or 2D, in which case "
          "the output has one more dimension than the input, and each "
          "value is mapped to a scanline (along axis 0) from the "
          "lookup table.\n " UNRRDU_DILRR_INFO("lut") /* */
     "* Uses nrrdApply1DLut");

static int
unrrdu_lutMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nlut;
  hestOptAdd_1_Other(&hopt, "m,map", "lut", &nlut, NULL,
                     "lookup table to map input nrrd through", nrrdHestNrrdNoTTY);
  /* Pre-TeemV2; "-r" was only a flag (0 parms), which meant there was no way
     for the to explicitly say, "no rescaling".  Here is where we are grateful
     for the odd "single variadic parameter" hest option kind: The user can omit
     the option (==> rescale=false), or say "-r" (==> rescale=true),
     or say "-r false" (==> rescale=false), or say "-r true" (==> rescale=true) */
  int rescale;
  uint rsclOptIdx
    = hestOptAdd_1v_Bool(&hopt, "r,rescale", "bool", &rescale, "false",
                         "rescale the input values from the input range to the "
                         "lut domain.  The lut domain is either explicitly "
                         "defined by the axis min,max along axis 0 or 1, or, it "
                         "is implicitly defined as zero to the length of that axis "
                         "minus one.");
  double min, max;
  hestOptAdd_1_Double(&hopt, "min,minimum", "value", &min, "nan",
                      "Low end of input range. Defaults to lowest value "
                      "found in input nrrd.  Explicitly setting this is useful "
                      "only with rescaling (\"-r\")");
  hestOptAdd_1_Double(&hopt, "max,maximum", "value", &max, "nan",
                      "High end of input range. Defaults to highest value "
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
                     "is the lut's type",
                     &unrrduHestMaybeTypeCB);
  Nrrd *nin;
  OPT_ADD_NIN(nin, "input nrrd");
  char *out;
  OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, _unrrdu_lutInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);

  unrrduGetenvDefaultImplicitLutRmapRescale();
  /* Pre-TeemV2: "here is a big difference between unu and nrrd: we enforce rescaling any
  time that the map domain is implicit.  This is how the pre-1.6 functionality is
  recreated."
  TeemV2: Given an implicit map domain, if user is not used "-r", then rescale is set to
  unrrduDefaultImplicitLutRmapRescale (initialized to AIR_TRUE) which can be controlled
  by envvar "UNRRDU_DEFAULT_IMPLICIT_LUT_RMAP_RESCALE".
  Also, whenever there is rescaling we pass a NrrdRange to reflect the (optional) user
  range specification, instead of letting nrrdApply1DRegMap find the input range itself
  (by passing a NULL NrrdRange). */
  if (!hestSourceUser(hopt[rsclOptIdx].source)) {
    if (!(AIR_EXISTS(nlut->axis[nlut->dim - 1].min)
          && AIR_EXISTS(nlut->axis[nlut->dim - 1].max))) {
      rescale = unrrduDefaultImplicitLutRmapRescale;
    }
  }
  NrrdRange *range = NULL;
  if (rescale) {
    range = nrrdRangeNew(min, max);
    airMopAdd(mop, range, nrrdRangeNix_mop, airMopAlways);
    nrrdRangeSafeSet(range, nin, blind8BitRange);
  }

  if (nrrdTypeDefault == typeOut) {
    typeOut = nlut->type;
  }
  if (nrrdApply1DLut(nout, nin, range, nlut, typeOut, rescale)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble applying LUT:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

UNRRDU_CMD(lut, INFO);
