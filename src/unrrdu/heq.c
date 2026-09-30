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

#define INFO "Perform histogram equalization"
static const char *unu_heqInfoL
  = (INFO ". If this seems to be doing nothing, try increasing the "
          "number of histograms bins by an order of magnitude or "
          "two (or more).  Or, use \"unu gamma\" to warp the values "
          "in the direction you know they need to go.  Either of "
          "these might work because extremely tall and narrow peaks "
          "in the equalization histogram will produce poor results.\n "
          "* Uses nrrdHistoEq");

static int
unu_heqMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  hestOpt *hopt = NULL;
  Nrrd *nin;
  uint inHidx = hestOptAdd_1_Other(&hopt, "i,input", "nin", &nin, "-", "input nrrd",
                                   nrrdHestNrrdNoTTY);
  hopt[inHidx].priority -= 10;
  uint bins;
  /* TeemV2: this option can now finally have a default, with nrrdHestNrrdNoTTY above */
  hestOptAdd_1_UInt(&hopt, "b,bin", "bins", &bins, "1024",
                    "# bins to use in histogram that is created in order to "
                    "calculate the mapping that achieves the equalization.");
  uint smart;
  hestOptAdd_1v_UInt(&hopt, "s,smart", "bins", &smart, "0",
                     "# bins in value histogram to ignore in calculating the mapping. "
                     "Bins are ignored when they get more hits than other bins, and "
                     "when the values that fall in them are constant.  This is an "
                     "effective way to prevent large regions of background value "
                     "from distorting the equalization mapping.");
  float amount;
  hestOptAdd_1_Float(&hopt, "a,amount", "amount", &amount, "1.0",
                     "extent to which the histogram equalizing mapping should be "
                     "applied; 0.0: no change, 1.0: full equalization");
  /* we want to facilitate saving out the mapping as a text file,
     but with the domain included */
  char *mapS;
  hestOptAdd_1_String(&hopt, "m,map", "filename", &mapS, "",
                      "The value mapping used to achieve histogram equalization is "
                      "represented by a univariate regular map.  By giving a filename "
                      "here, that map can be saved out and applied to other nrrds "
                      "with \"unu rmap\"");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for YES some hparm modification)
  hestParm *hparm = hestParmCopy(_hparm);
  hparm->noArgsIsNoProblem = AIR_TRUE; // thanks to nrrdHestNrrdNoTTY
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_heqInfoL);
  airArray *mop = unrrduMopNew(hopt, hparm);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  Nrrd *nmap = NULL;
  if (nrrdHistoEq(nout, nin, airStrlen(mapS) ? &nmap : NULL, bins, smart, amount)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble histogram equalizing:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  if (airStrlen(mapS)) {
    // TODO? save history of nmap
    MY_SAVE(mapS, nmap, NULL);
  }
  MY_HISTORY(nout, nin);
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(heq, INFO);
