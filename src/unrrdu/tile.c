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

#define INFO "Tile slices of one axis into two other axes"
static const char *unu_tileInfoL
  = (INFO ". Tiling an array means splitting one axis into fast and slow parts, "
          "and then interleaving those parts into other (existing) axes by doing "
          "two axis merges, which combine an existing axis with part of the split "
          "axis.  This reduces the dimension by one.  The three axis arguments all "
          "identify axes in the input array as is.  This provides, for example, "
          "a simple way of viewing the 128 slices along the slow axis of a 3-D volume "
          "as a 16x8 tiled array of 2-D slices, as with \"-a 2 0 1 -s 16 8\".\n "
          "* Uses nrrdTile2D");

static int
unu_tileMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  uint axes[3];
  hestOptAdd_3_UInt(&hopt, "a,axis", "axSplit ax0 ax1", axes, NULL,
                    "axSplit is divided and merged with ax0 and ax1");
  size_t size[2];
  hestOptAdd_2_Size_t(&hopt, "s,size", "fast slow", size, NULL,
                      "fast and slow axis sizes to produce as result of splitting "
                      "the axSplit axis.");
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_tileInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (nrrdTile2D(nout, nin, axes[1], axes[2], axes[0], size[0], size[1])) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error tiling nrrd:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(tile, INFO);
