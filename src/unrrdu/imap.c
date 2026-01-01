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

#define INFO "Map nrrd through *irregular* univariate map (\"colormap\")"
static const char *unu_imapInfoL
  = (INFO ". A map is irregular if the control points are not evenly "
          "spaced along the domain, and hence their position must be "
          "explicitly represented in the map.  As nrrds, these maps "
          "are necessarily 2D.  Along axis 0, the first value is the "
          "location of the control point, and the remaining values "
          "give are the range of the map for that control point. "
          "The output value(s) is the result of linearly "
          "interpolating between value(s) from the map.\n "
          "* Uses nrrdApply1DIrregMap");

static int
unu_imapMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nmap;
  hestOptAdd_1_Other(&hopt, "m,map", "map", &nmap, NULL,
                     "irregular map to map input nrrd through", nrrdHestNrrdNoTTY);
  uint aclLen;
  hestOptAdd_1_UInt(&hopt, "l,length", "aclLen", &aclLen, "0",
                    "length of accelerator array, used to try to speed-up "
                    "task of finding between which pair of control points "
                    "a given value lies.  Not terribly useful for small maps "
                    "(about 10 points or less).  Use 0 to turn accelorator off. ");
  int rescale;
  hestOptAdd_Flag(&hopt, "r,rescale", &rescale,
                  "rescale the input values from the input range to the "
                  "map domain");
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
                     "specify the type (\"int\", \"float\", etc.) of the output "
                     "nrrd. By default (not using this option), the output type "
                     "is the map's type",
                     &unrrduHestMaybeTypeCB);
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_imapInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  Nrrd *nacl;
  if (aclLen) {
    nacl = nrrdNew();
    airMopAdd(mop, nacl, nrrdNuke_mop, airMopAlways);
    if (nrrd1DIrregAclGenerate(nacl, nmap, aclLen)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble generating accelerator:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
  } else {
    nacl = NULL;
  }
  NrrdRange *range = NULL;
  if (rescale) {
    range = nrrdRangeNew(min, max);
    airMopAdd(mop, range, nrrdRangeNix_mop, airMopAlways);
    nrrdRangeSafeSet(range, nin, blind8BitRange);
  }
  if (nrrdTypeDefault == typeOut) {
    typeOut = nmap->type;
  }
  /* some very non-exhaustive tests seemed to indicate that the
     accelerator does not in fact reliably speed anything up.
     This of course depends on the size of the imap (# points),
     but chances are most imaps will have only a handful of points,
     in which case the binary search in _nrrd1DIrregFindInterval()
     will finish quickly ... */
  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (nrrdApply1DIrregMap(nout, nin, range, nmap, nacl, typeOut, rescale)) {
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

MY_UNRRDU_CMD(imap, INFO);
