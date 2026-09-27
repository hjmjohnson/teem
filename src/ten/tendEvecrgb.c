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

#define INFO "Make an RGB volume from an eigenvector and an anisotropy"
static const char *evecrgbInfoL = (INFO ". ");

static int
evecrgbMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;

  airArray *mop = airMopNew();
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  tenEvecRGBParm *rgbp = tenEvecRGBParmNew();
  airMopAdd(mop, rgbp, tenEvecRGBParmNix_mop, airMopAlways);

  Nrrd *nin;
  uint inHidx = hestOptAdd_1_Other(&hopt, "i", "nin", &nin, "-",
                                   "input diffusion tensor volume", nrrdHestNrrd);
  hopt[inHidx].priority -= 10;
  hestOptAdd_1_UInt(&hopt, "c", "evec index", &(rgbp->which), NULL,
                    "which eigenvector will be colored. \"0\" for the "
                    "principal, \"1\" for the middle, \"2\" for the minor");
  hestOptAdd_1_Enum(&hopt, "a", "aniso", &(rgbp->aniso), NULL,
                    "Which anisotropy to use for modulating the saturation "
                    "of the colors.  " TEN_ANISO_DESC,
                    tenAniso);
  hestOptAdd_1_Double(&hopt, "t", "thresh", &(rgbp->confThresh), "0.5",
                      "confidence threshold");
  hestOptAdd_1_Double(&hopt, "bg", "background", &(rgbp->bgGray), "0",
                      "gray level to use for voxels who's confidence is zero ");
  hestOptAdd_1_Double(&hopt, "gr", "gray", &(rgbp->isoGray), "0",
                      "the gray level to desaturate towards as anisotropy "
                      "decreases (while confidence remains 1.0)");
  hestOptAdd_1_Double(&hopt, "gam", "gamma", &(rgbp->gamma), "1",
                      "gamma to use on color components");
  char *outS;
  hestOptAdd_1_String(&hopt, "o", "nout", &outS, "-", "output image (floating point)");

  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  int pret;
  char *perr, *err;
  USAGE_PARSE(evecrgbInfoL);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (tenEvecRGB(nout, nin, rgbp)) {
    airMopAdd(mop, err = biffGetDone(TEN), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble doing colormapping:\n%s\n", me, err);
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
MY_TEND_CMD(evecrgb, INFO);
