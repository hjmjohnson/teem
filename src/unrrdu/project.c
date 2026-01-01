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

#define INFO "Collapse scanlines to scalars along some axis"
static const char *unu_projectInfoL
  = (INFO ". The scanline is reduced to a single scalar by "
          "\"measuring\" all the values in the scanline "
          "with some measure.  The output nrrd has dimension "
          "one less than input (except when the input is itself 1-D); "
          "the output type depends on "
          "the measure in a non-trivial way, or it can be set explicitly "
          "with the \"-t\" option.  To save the overhead of multiple input data "
          "reads if projections along different axes are needed, you can give "
          "multiple axes to \"-a\" (and a matching number of output filenames "
          "to \"-o\"), as well as multiple measures to \"-m\" (and possibly a "
          "specific type to \"-t\" to permit their joining on fastest axis).\n "
          "* Uses nrrdProject, and nrrdJoin if multiple measures");

static int
unu_projectMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  uint *axis, axisLen;
  hestOptAdd_Nv_UInt(&hopt, "a,axis", "axis", 1, -1, &axis, NULL,
                     "axis or axes to project along", &axisLen);
  int *measr;
  uint measrLen;
  hestOptAdd_Nv_Enum(&hopt, "m,measure", "measr", 1, -1, &measr, NULL,
                     "How to \"measure\" a scanline, by summarizing all its values "
                     "with a single scalar. Multiple measures will be joined along "
                     "fastest axis if output, but you may need to set output type "
                     "explicitly via \"-t\" so that the join works. " NRRD_MEASURE_DESC,
                     &measrLen, nrrdMeasure);
  int type;
  hestOptAdd_1_Other(&hopt, "t,type", "type", &type, "default",
                     "type to use for output. By default (not using this option), "
                     "the output type is determined auto-magically",
                     &unrrduHestMaybeTypeCB);
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  AIR_UNUSED(inHidx); // TODO handle history; hard because of multiple outs
  char **out;
  uint outLen;
  hestOptAdd_Nv_String(&hopt, "o,output", "nout", 1, -1, &out, "-",
                       "one or more output nrrd filenames. Number of names here "
                       "has to match number of axes specified.",
                       &outLen);

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_projectInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  if (axisLen != outLen) {
    fprintf(stderr, "%s: got %u \"-a\" axes but %u \"-o\" outputs\n", me, axisLen,
            outLen);
    airMopError(mop);
    return 1;
  }

  Nrrd **nslice;
  if (measrLen > 1) {
    nslice = AIR_CALLOC(measrLen, Nrrd *);
    airMopAdd(mop, nslice, airFree, airMopAlways);
    for (uint measrIdx = 0; measrIdx < measrLen; measrIdx++) {
      nslice[measrIdx] = nrrdNew();
      airMopAdd(mop, nslice[measrIdx], nrrdNuke_mop, airMopAlways);
    }
  } else {
    nslice = NULL;
  }
  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);

  for (uint outIdx = 0; outIdx < outLen; outIdx++) {
    if (measrLen > 1) {
      /* first project into slices */
      for (uint measrIdx = 0; measrIdx < measrLen; measrIdx++) {
        if (nrrdProject(nslice[measrIdx], nin, axis[outIdx], measr[measrIdx], type)) {
          airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
          fprintf(stderr, "%s: error projecting nrrd %u/%u:\n%s", me, outIdx, measrIdx,
                  err);
          airMopError(mop);
          return 1;
        }
      }
      /* then join slices into output */
      if (nrrdJoin(nout, (const Nrrd *const *)nslice, measrLen, 0, AIR_TRUE)) {
        airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
        fprintf(stderr,
                "%s: error joining nrrd %u; will have to use \"-t\" "
                "option to make sure all projections have same type:\n%s",
                me, outIdx, err);
        airMopError(mop);
        return 1;
      }
    } else {
      if (nrrdProject(nout, nin, axis[outIdx], measr[0], type)) {
        airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
        fprintf(stderr, "%s: error projecting nrrd %u:\n%s", me, outIdx, err);
        airMopError(mop);
        return 1;
      }
    }
    MY_SAVE(out[outIdx], nout, NULL);
  }

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(project, INFO);
