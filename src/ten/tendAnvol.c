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

#include "ten.h"
#include "privateTen.h"

#define INFO "Apply an anisotropy metric to a DT volume"
static const char *anvolInfoL
  = (INFO ".  The anisotropy value will be zero in the locations which "
          "don't meet the given confidence threshold.");

static int
anvolMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  int pret;
  hestOpt *hopt = NULL;
  char *perr, *err;
  airArray *mop;

  int aniso;
  Nrrd *nin, *nout;
  char *outS;
  float thresh;

  hestOptAdd_1_Enum(&hopt, "a", "aniso", &aniso, NULL,
                    "Which anisotropy metric to plot.  " TEN_ANISO_DESC, tenAniso);
  hestOptAdd_1_Float(&hopt, "t", "thresh", &thresh, "0.5", "confidence threshold");
  hestOptAdd_1_Other(&hopt, "i", "nin", &nin, "-", "input diffusion tensor volume",
                     nrrdHestNrrd);
  hestOptAdd_1_String(&hopt, "o", "nout", &outS, "-", "output image (floating point)");

  mop = airMopNew();
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  USAGE_PARSE(anvolInfoL);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);

  nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (tenAnisoVolume(nout, nin, aniso, thresh)) {
    airMopAdd(mop, err = biffGetDone(TEN), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble making aniso volume:\n%s\n", me, err);
    airMopError(mop);
    return 1;
  }
  if (nrrdSave(outS, nout, NULL)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble writing:\n%s\n", me, err);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  return 0;
}
MY_TEND_CMD(anvol, INFO);
