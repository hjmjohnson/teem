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

#include "limn.h"
#include "privateLimn.h"

#define INFO "Extract the vertex array"
static const char *myinfo = (INFO ". ");

static int
lpu_vertsMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  limnPolyData *pld;
  hestOptAdd_1_Other(&hopt, NULL, "input", &pld, NULL, "input polydata filename",
                     limnHestPolyDataLMPD);
  char *out;
  hestOptAdd_1_String(&hopt, NULL, "output", &out, NULL, "output nrrd filename");

  hestParseOrExit(hopt, argc, argv, hparm, me, myinfo);
  airArray *mop = airMopNew();
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNix_mop, airMopAlways); /* NOT nrrdNuke */
  if (nrrdWrap_va(nout, pld->xyzw, nrrdTypeFloat, 2, 4, pld->xyzwNum)
      || nrrdSave(out, nout, NULL)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble:%s", me, err);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  return 0;
}

const unrrduCmd limnPu_vertsCmd = {"verts", INFO, lpu_vertsMain, AIR_FALSE};
