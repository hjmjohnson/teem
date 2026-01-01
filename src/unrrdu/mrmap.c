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

#define INFO "Map nrrd through a whole nrrd of regular univariate maps"
static const char *unu_mrmapInfoL
  = (INFO ", one map per sample in input. The \"mrmap\" nrrd has the same dimensional "
          "constraints as the \"mlut\" nrrd for \"unu mlut\".  This functionality "
          "is a generalization of \"unu 3op lerp\": it allows you to lerp through "
          "multiple nrrds, instead of just two.\n " /* */
     MY_UNRRDU_DILRR_INFO("mrmap")                  /* */
     "* Uses nrrdApplyMulti1DRegMap");

static int
unu_mrmapMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  Nrrd **_nmmap;
  uint _nmmapLen;
  hestOptAdd_Nv_Other(&hopt, "m,map", "mmap", 1, -1, &_nmmap, NULL,
                      "one nrrd of regular maps to map input nrrd through, or, "
                      "list of nrrds which contain the individual entries of the map "
                      "at each voxel, which will be joined together.",
                      &_nmmapLen, nrrdHestNrrdNoTTY);
  int rescale;
  uint rsclOptIdx
    = hestOptAdd_1v_Bool(&hopt, "r,rescale", "bool", &rescale, "false",
                         "rescale the input values from the input range to the "
                         "map domain.  The map domain is either explicitly "
                         "defined by the axis min,max along axis 0 or 1, or, it "
                         "is implicitly defined as zero to one minus the length of "
                         "that axis.");
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
  MY_OPT_ADD_NIN(nin, "input nrrd");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_mrmapInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);

  /* see comment rmap.c */

  /* by the end of this block we need to have nmmap and mapAxis */
  uint mapAxis;
  Nrrd *nmmap;
  if (1 == _nmmapLen) {
    /* we got the mmap as a single nrrd */
    nmmap = _nmmap[0];
    mapAxis = nmmap->dim - nin->dim - 1;
    /* its not our job to do real error checking ... */
    mapAxis = AIR_MIN(mapAxis, nmmap->dim - 1);
  } else {
    /* we have to join together multiple nrrds to get the mmap */
    nmmap = nrrdNew();
    airMopAdd(mop, nmmap, nrrdNuke_mop, airMopAlways);
    /* assume that mmap component nrrds are all compatible sizes,
       nrrdJoin will fail if they aren't */
    mapAxis = _nmmap[0]->dim - nin->dim;
    if (nrrdJoin(nmmap, (const Nrrd *const *)_nmmap, _nmmapLen, mapAxis, AIR_TRUE)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble joining mmap:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
    /* set these if they were given, they'll be NaN otherwise */
    nmmap->axis[mapAxis].min = min;
    nmmap->axis[mapAxis].max = max;
  }

  unrrduUnuGetenvDefaultImplicitLutRmapRescale();
  /* NOTE: comment in lut.c describes logic for how rescale is set */
  if (!hestSourceUser(hopt[rsclOptIdx].source)) {
    if (!(AIR_EXISTS(nmmap->axis[mapAxis].min)
          && AIR_EXISTS(nmmap->axis[mapAxis].max))) {
      rescale = unrrduUnuDefaultImplicitLutRmapRescale;
    }
  }
  NrrdRange *range = NULL;
  if (rescale) {
    range = nrrdRangeNew(min, max);
    airMopAdd(mop, range, nrrdRangeNix_mop, airMopAlways);
    nrrdRangeSafeSet(range, nin, blind8BitRange);
  }

  if (nrrdTypeDefault == typeOut) {
    typeOut = nmmap->type;
  }
  if (nrrdApplyMulti1DRegMap(nout, nin, range, nmmap, typeOut, rescale)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble applying map:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  MY_HISTORY(nout, nin);
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(mrmap, INFO);
