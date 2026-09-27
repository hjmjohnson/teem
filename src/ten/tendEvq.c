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

#define INFO "Quantize directions of diffusion"
static const char *evqInfoL
  = (INFO
     ". Because VTK doesn't do multi-dimensional colormaps (circa year 2000), we "
     "have to quantize directions of diffusion (usually just the principal eigenvector) "
     "in order to create the usual XYZ<->RGB coloring.  Because eigenvector directions "
     "are poorly defined in regions of low anisotropy, the length of the vector "
     "(pre-quantization) is modulated by anisotropy, requiring the selection of some "
     "anisotropy metric.");

static int
evqMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nin;
  uint inHidx = hestOptAdd_1_Other(&hopt, "i", "nin", &nin, "-",
                                   "input diffusion tensor volume", nrrdHestNrrd);
  hopt[inHidx].priority -= 10;
  uint which;
  hestOptAdd_1_UInt(&hopt, "c", "evec index", &which, "0",
                    "Which eigenvector should be quantized: \"0\" for the "
                    "direction of fastest diffusion (eigenvector associated "
                    "with largest eigenvalue), \"1\" or \"2\" for other two "
                    "eigenvectors (associated with middle and smallest eigenvalue)");
  int aniso;
  hestOptAdd_1_Enum(&hopt, "a", "aniso", &aniso, NULL,
                    "Scale the eigenvector with this anisotropy metric. " TEN_ANISO_DESC,
                    tenAniso);
  int dontScaleByAniso;
  hestOptAdd_Flag(&hopt, "ns", &dontScaleByAniso,
                  "Don't attenuate the color by anisotropy.  By default (not "
                  "using this option), regions with low or no anisotropy are "
                  "very dark colors or black");
  char *outS;
  hestOptAdd_1_String(&hopt, "o", "nout", &outS, "-", "output image (floating point)");

  airArray *mop = airMopNew();
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  int pret;
  char *perr, *err;
  USAGE_PARSE(evqInfoL);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (tenEvqVolume(nout, nin, which, aniso, !dontScaleByAniso)) {
    airMopAdd(mop, err = biffGetDone(TEN), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble quantizing eigenvectors:\n%s\n", me, err);
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
MY_TEND_CMD(evq, INFO);
