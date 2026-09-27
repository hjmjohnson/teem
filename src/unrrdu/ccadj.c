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

#define INFO "Form adjecency matrix of connected components"
static const char *unu_ccadjInfoL
  = (INFO ".  This operates on the output of \"ccfind\".  Output is unsigned char "
          "array containing 1 at locations (I,J) and (J,I) if CCs with ids I and J are "
          "adjacent, according to the chosen style of adjacency.\n "
          "* Uses nrrdCCAdjacency");

static int
unu_ccadjMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nin;
  uint inHidx = hestOptAdd_1_Other(&hopt, "i,input", "nin", &nin, "-", "input nrrd",
                                   nrrdHestNrrdNoTTY);
  hopt[inHidx].priority -= 10;
  uint conny;
  hestOptAdd_1_UInt(&hopt, "c,connect", "connectivity", &conny, "1",
                    "what kind of connectivity to use: the number of coordinates "
                    "that vary in order to traverse the neighborhood of a given "
                    "sample.  In 2D: \"1\": 4-connected, \"2\": 8-connected");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  hestParm *hparm = hestParmCopy(_hparm);
  hparm->noArgsIsNoProblem = AIR_TRUE; // thanks to nrrdHestNrrdNoTTY
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_ccadjInfoL);
  airArray *mop = unrrduMopNew(hopt, hparm);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (nrrdCCAdjacency(nout, nin, conny)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error finding adjacencies:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  MY_HISTORY(nout, nin);
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(ccadj, INFO);
