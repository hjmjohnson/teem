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

#define INFO "Expand \"blocks\" into scanlines on axis 0"
static const char *unu_unblockInfoL
  = (INFO ". Based on the requested output type, the number of "
          "samples along axis 0 will be determined automatically. "
          "Axis N information will be bumped up to axis N+1. "
          "Underlying data is unchanged.");

static int
unu_unblockMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  int type;
  MY_OPT_ADD_TYPE(type, "type to unblock to", NULL);
  size_t blockSize;
  hestOptAdd_1_Size_t(&hopt, "bs", "blocksize", &blockSize, "0",
                      "Useful only if *output* type is also block: the size of "
                      "blocks in output nrrd");
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_unblockInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  nout->blockSize = blockSize;

  if (nrrdUnblock(nout, nin, type)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error unblocking nrrd:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  MY_HISTORY(nout, nin);
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD_HIDE(unblock, INFO);
