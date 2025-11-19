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

#include "bane.h"
#include "privateBane.h"

#define INFO_INFO "Project histogram volume for opacity function generation"
static const char *infoInfoL
  = (INFO_INFO ". This distills the histogram volume down to the information required "
               "to create either 1-D or 2-D opacity functions.");

static int /* Biff: 1 */
infoMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  int measr;
  hestOptAdd_1_Enum(&hopt, "m", "measr", &measr, "mean",
                    "How to project along the 2nd derivative axis.  Possibilities "
                    "include:\n "
                    "\b\bo \"mean\": average value\n "
                    "\b\bo \"median\": value at 50th percentile\n "
                    "\b\bo \"mode\": most common value\n "
                    "\b\bo \"min\", \"max\": probably not useful",
                    baneGkmsMeasr);
  int one;
  hestOptAdd_Flag(&hopt, "one", &one,
                  "Create 1-dimensional info file; default is 2-dimensional");
  Nrrd *hvol;
  hestOptAdd_1_Other(&hopt, "i", "hvolIn", &hvol, NULL,
                     "input histogram volume (from \"gkms hvol\")", nrrdHestNrrd);
  char *outS;
  hestOptAdd_1_String(&hopt, "o", "infoOut", &outS, NULL,
                      "output info file, used by \"gkms pvg\" and \"gkms opac\"");

  hestParseOrExit(hopt, argc, argv, hparm, me, infoInfoL);
  airArray *mop = airMopNew();
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (baneOpacInfo(nout, hvol, one ? 1 : 2, measr)) {
    biffAddf(BANE, "%s: trouble distilling histogram info", me);
    airMopError(mop);
    return 1;
  }

  if (nrrdSave(outS, nout, NULL)) {
    biffMovef(BANE, NRRD, "%s: trouble saving info file", me);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  return 0;
}
const unrrduCmd baneGkms_infoCmd = {"info", INFO_INFO, infoMain, AIR_FALSE};
