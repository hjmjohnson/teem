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

#define INFO "Map nrrd through one *regular* univariate map (\"colormap\")"
static const char *_unrrdu_rmapInfoL
  = (INFO ". A map is regular if the control points are evenly "
          "spaced along the domain, and hence their position isn't "
          "explicitly represented in the map; the axis min, axis "
          "max, and number of points determine their location. "
          "The map can be a 1D nrrd (for \"grayscale\"), "
          "in which case the "
          "output has the same dimension as the input, "
          "or a 2D nrrd (for \"color\"), in which case "
          "the output has one more dimension than the input.  In "
          "either case, the output is the result of linearly "
          "interpolating between map points, either scalar values "
          "(\"grayscale\"), or scanlines along axis 0 "
          "(\"color\").\n " UNRRDU_DILRR_INFO("rmap") /* */
     "* Uses nrrdApply1DRegMap");

static int
unrrdu_rmapMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nmap;
  hestOptAdd_1_Other(&hopt, "m,map", "map", &nmap, NULL,
                     "regular map to map input nrrd through", nrrdHestNrrdNoTTY);
  int rescale;
  uint rsclOptIdx
    = hestOptAdd_1v_Bool(&hopt, "r,rescale", "bool", &rescale, "false",
                         "rescale the input values from the input range to the "
                         "map domain.  The map domain is either explicitly "
                         "defined by the axis min,max along axis 0 or 1, or, it "
                         "is implicitly defined as zero to the length of "
                         "that axis minus one.");
  double min, max;
  hestOptAdd_1_Double(&hopt, "min,minimum", "value", &min, "nan",
                      "Low end of input range. Defaults to lowest value "
                      "found in input nrrd.  Explicitly setting this is useful "
                      "only with rescaling (\"-r\") or if the map domain is only "
                      "implicitly defined");
  hestOptAdd_1_Double(&hopt, "max,maximum", "value", &max, "nan",
                      "High end of input range. Defaults to highest value "
                      "found in input nrrd.  Explicitly setting this is useful "
                      "only with rescaling (\"-r\") or if the map domain is only "
                      "implicitly defined");
  int blind8BitRange;
  hestOptAdd_1_Bool(&hopt, "blind8", "bool", &blind8BitRange,
                    nrrdStateBlind8BitRange ? "true" : "false",
                    "Whether to know the range of 8-bit data blindly "
                    "(uchar is always [0,255], signed char is [-128,127]). "
                    "Explicitly setting this is useful "
                    "only with rescaling (\"-r\") or if the map domain is only "
                    "implicitly defined");
  int typeOut;
  hestOptAdd_1_Other(&hopt, "t,type", "type", &typeOut, "default",
                     "specify the type (\"int\", \"float\", etc.) of the "
                     "output nrrd. "
                     "By default (not using this option), the output type "
                     "is the map's type",
                     &unrrduHestMaybeTypeCB);
  Nrrd *nin;
  OPT_ADD_NIN(nin, "input nrrd");
  char *out;
  OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, _unrrdu_rmapInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  unrrduGetenvDefaultImplicitLutRmapRescale();
  /* NOTE: comment in lut.c describes logic for how rescale is set */
  if (!hestSourceUser(hopt[rsclOptIdx].source)) {
    if (!(AIR_EXISTS(nmap->axis[nmap->dim - 1].min)
          && AIR_EXISTS(nmap->axis[nmap->dim - 1].max))) {
      rescale = unrrduDefaultImplicitLutRmapRescale;
    }
  }
  NrrdRange *range = NULL;
  if (rescale) {
    range = nrrdRangeNew(min, max);
    airMopAdd(mop, range, (airMopper)nrrdRangeNix, airMopAlways);
    nrrdRangeSafeSet(range, nin, blind8BitRange);
  }

  if (nrrdTypeDefault == typeOut) {
    typeOut = nmap->type;
  }
  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, (airMopper)nrrdNuke, airMopAlways);
  if (nrrdApply1DRegMap(nout, nin, range, nmap, typeOut, rescale)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble applying map:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

UNRRDU_CMD(rmap, INFO);
