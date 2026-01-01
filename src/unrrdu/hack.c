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

#define INFO "a hack of some kind"
static const char *unu_hackInfoL
  = (INFO ". This is used as a place to put whatever one-off code "
          "you want to try, with the whatever benefits come with being a "
          "unu command.\n "
          "* (not based on any particular nrrd function)");

static int
unu_hackMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  char *what;
  hestOptAdd_1_String(&hopt, NULL, "what", &what, NULL, "what hack to do");
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_hackInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (!strcmp(what, "sdincr")) {
    unsigned int sdim, axi;
    sdim = nin->spaceDim;
    if (!sdim) {
      fprintf(stderr, "%s: need non-zero space dimension", me);
      airMopError(mop);
      return 1;
    }
    if (nrrdCopy(nout, nin)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error converting nrrd:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
    nout->spaceDim = sdim + 1;
    nout->space = nrrdSpaceUnknown;
    nout->spaceOrigin[sdim] = 0.0;
    for (axi = 0; axi < nout->dim; axi++) {
      if (nrrdSpaceVecExists(sdim, nout->axis[axi].spaceDirection)) {
        nout->axis[axi].spaceDirection[sdim] = 0.0;
      }
    }
  } else {
    fprintf(stderr, "%s: no \"%s\" hack implemented", me, what);
    airMopError(mop);
    return 1;
  }

  MY_HISTORY(nout, nin);
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD_HIDE(hack, INFO);
