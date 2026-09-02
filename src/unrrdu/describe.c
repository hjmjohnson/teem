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

#define INFO "Describe Nrrd struct created by reading file(s)"
static const char *unu_describeInfoL
  = (INFO ". This will actually nrrdLoad from the named file(s) (in NRRD or other "
          "format), which is useful for debugging the relationship between the "
          "file contents and the Nrrd struct created by reading it. In contrast, "
          "\"unu head\" just prints the header lines from NRRD file(s). "
          "What information this prints about the Nrrd, and how, can be controlled "
          "somewhat.\n "
          "* Uses nrrdDescribeMore");

static int /* Biff: 1 */
describeDoit(const char *me, char *fname, int skipData, int showPtrs, int elideNonExist,
             const char *sepStr, unsigned int sepWidth) {
  if (!strcmp("-", fname) && isatty(STDIN_FILENO)) {
    biffAddf(me, "declining to try reading Nrrd from stdin as tty (terminal)");
    return 1;
  }

  airArray *mop = airMopNew();
  Nrrd *nrrd = nrrdNew();
  airMopAdd(mop, nrrd, nrrdNuke_mop, airMopAlways);
  NrrdIoState *nio = NULL;
  if (skipData) {
    nio = nrrdIoStateNew();
    airMopAdd(mop, nio, nrrdIoStateNix_mop, airMopAlways);
    nio->skipData = AIR_TRUE;
  }
  if (nrrdLoad(nrrd, fname, nio)) {
    biffMovef(me, NRRD, "%s: trouble loading \"%s\"", me, fname);
    airMopError(mop);
    return 1;
  }
  nrrdDescribeMore(stdout, nrrd, NULL /* prefix */, showPtrs, elideNonExist,
                   AIR_TRUE /* elideUnknown */, sepStr, sepWidth);
  airMopOkay(mop);
  return 0;
}

static int
unu_describeMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  hestOpt *hopt = NULL;
  int skipData;
  hestOptAdd_Flag(&hopt,
                  "skip,skipdata",
                  &skipData,
                  "Do not try to read in data from NRRD files");
  unsigned int sepWidth;
  hestOptAdd_1_UInt(&hopt, "sw,sepwidth", "width", &sepWidth, "25",
                    "print separators (between info sections) of "
                    "this width");
  char *sepStr;
  hestOptAdd_1_String(&hopt, "ss,sepstr", "sep", &sepStr, "- ",
                      "if printing separators between info sections "
                      "via `-sw`, what string to repeat");
  int showPtr;
  hestOptAdd_1_Bool(&hopt, "ptr,showptr", "bool", &showPtr, "false",
                    "show actual hex values for pointers into memory");
  int elideNonExist;
  hestOptAdd_1_Bool(&hopt, "ene,elidenonexist", "bool", &elideNonExist, "true",
                    "if a Nrrd struct field is NaN meaning \"not set\" or \"unknown\", "
                    "then don't print it");
  char **inS;
  unsigned int inLen;
  hestOptAdd_Mv_String(&hopt, NULL, "nin1", 1, -1, &inS, "-", "input nrrd filename(s)",
                       &inLen);

  // (boilerplate for YES some hparm modification)
  hestParm *hparm = hestParmCopy(_hparm);
  hparm->noArgsIsNoProblem = AIR_TRUE; // thanks to nrrdHestNrrdNoTTY
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_describeInfoL);
  airArray *mop = unrrduMopNew(hopt, hparm);
  char *err;

  /* HEY "okay" logic copied from head.c */
  int okay = AIR_FALSE;
  for (unsigned int ni = 0; ni < inLen; ni++) {
    if (inLen > 1) {
      fprintf(stdout, "==> %s <==\n", inS[ni]);
    }
    if (describeDoit(me, inS[ni], skipData, showPtr, elideNonExist, sepStr, sepWidth)) {
      airMopAdd(mop, err = biffGetDone(me), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble with \"%s\":\n%s", me, inS[ni], err);
      /* continue working on the remaining files */
    } else {
      /* processed at least one file ok */
      okay = AIR_TRUE;
    }
    if (inLen > 1 && ni < inLen - 1) {
      fprintf(stdout, "\n");
    }
  }
  if (!okay) {
    /* none of the given files could be read; something is wrong */
    if (inLen > 1) {
      fprintf(stderr, "\n%s: Unable to read data from any file\n", me);
    }
    hestUsage(stderr, hopt, me, hparm, AIR_TRUE /* oneline */);
    fprintf(stderr, "\nFor more info: \"%s --help\"\n", me);
  }

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(describe, INFO);
