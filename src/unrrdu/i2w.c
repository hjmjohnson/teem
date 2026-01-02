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

#include "unrrdu.h"
#include "privateUnrrdu.h"

#define INFO "converts from 1-D index to world position"
static const char *unu_i2wInfoL
  = (INFO ", given the centering of the data (cell vs. node), "
          "the range of positions, and the number of intervals into "
          "which position has been quantized. "
          "This is a demo/utility, which does not actually operate on any nrrds. "
          "Previously available as the stand-alone idx2pos binary.\n "
          "* Uses NRRD_POS macro");

static int
unu_i2wMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  int center;
  hestOptAdd_1_Enum(&hopt, NULL, "center", &center, NULL,
                    "which centering applies to the quantized position.\n "
                    "Possibilities are:\n "
                    "\b\bo \"cell\": for histogram bins, quantized values, and "
                    "pixels-as-squares\n "
                    "\b\bo \"node\": for non-trivially interpolated "
                    "sample points",
                    nrrdCenter);
  double minPos, maxPos, size, indx;
  hestOptAdd_1_Double(&hopt, NULL, "minPos", &minPos, NULL,
                      "smallest position associated with index 0");
  hestOptAdd_1_Double(&hopt, NULL, "maxPos", &maxPos, NULL,
                      "highest position associated with highest index");
  hestOptAdd_1_Double(&hopt, NULL, "num", &size, NULL,
                      "number of intervals into which position has been quantized");
  hestOptAdd_1_Double(&hopt, NULL, "index", &indx, NULL,
                      "the input index position, to be converted to world");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_i2wInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  // char *err;

  double pos = NRRD_POS(center, minPos, maxPos, size, indx);
  airSinglePrintf(stdout, NULL, 0, "%lg", pos);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(i2w, INFO);
