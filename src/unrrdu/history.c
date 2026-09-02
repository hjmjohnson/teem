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

#define INFO "Show unu command history (if known) that made an array"
static const char *unu_historyInfoL
  = (INFO
     ". When the environment variable " UNRRDU_UNU_HISTORY_TRACE_ENV " "
     "is set (to what doesn't matter), unu makes an effort to trace the history "
     "of the unu commands used to generate a nrrd by saving information in "
     "key/value pairs starting with \"" MY_HISTORY_KEY "_\". This could be of "
     "interest in settings where unu is being used to do some work on given "
     "real-world data, but its main purpose is to facilitate reproducibility "
     "in settings where unu (and only unu) is used to generate synthetic/test "
     "data from scratch.  Because there is non-trivial work in maintaining that "
     "list of key/value pairs, this functionality is not enabled by default. "
     "See \"unu env\" for info about other consequential environment variables. "
     "The key/value pairs could be see with \"unu describe\" or "
     "(from .nrrd files) \"unu head\", but this command prints just the "
     "history, in a form that (ideally) can be redirected to a shell script.\n "
     "\n "
     "\t\t\tHere is a demo example, showing off \"-{ }-\" hest commenting of "
     "part of a command-line as well as a (hacky!) way to record the "
     "origin of data coming into stdin that did not originate with unu:\n "
     "\n "
     "\t\t\t\texport " UNRRDU_UNU_HISTORY_TRACE_ENV "=\n "
     "\t\t\t\techo 0 1 2 3 |\n "
     "\t\t\t\t\t\t\t\t(" UNRRDU_UNU_HISTORY_STDIN_ENV
     "=\"echo 0 1 2 3\" unu reshape -s 2 2) |\n "
     "\t\t\t\t\t\t\t\tunu resample -s 300 300 -k tent -c node -{ bilinear interp }- |\n "
     "\t\t\t\t\t\t\t\tunu quantize -b 8 |\n "
     "\t\t\t\t\t\t\t\tunu 2op / - 16 | unu 2op x - 16 -o bands.png\n "
     "\t\t\t\tunu describe bands.png -{ to see the underlying key/value pairs }-\n "
     "\t\t\t\tunu history bands.png\n "
     "\n "
     "The final command should show shell commands that can be copy-pasted to "
     "regenerate the bands.png data (though unfortunately not, because of "
     "the " UNRRDU_UNU_HISTORY_STDIN_ENV " hack, the same self-contained history).\n\n "
     "* Uses unrrduUnuHistoryGet");

static int /* Biff: 1 */
historyDoit(const char *me, char *fname, int commentIfNone) {
  if (!strcmp("-", fname) && isatty(STDIN_FILENO)) {
    biffAddf(me, "declining to try reading Nrrd from stdin as tty (terminal)");
    return 1;
  }

  airArray *mop = airMopNew();
  Nrrd *nrrd = nrrdNew();
  airMopAdd(mop, nrrd, nrrdNuke_mop, airMopAlways);
  NrrdIoState *nio = nrrdIoStateNew();
  airMopAdd(mop, nio, nrrdIoStateNix_mop, airMopAlways);
  nio->skipData = AIR_TRUE;
  if (nrrdLoad(nrrd, fname, nio)) {
    biffMovef(me, NRRD, "%s: trouble loading \"%s\"", me, fname);
    airMopError(mop);
    return 1;
  }
  airArray *harr = unrrduUnuHistoryGet(nrrd, fname, commentIfNone);
  airMopAdd(mop, harr, airArrayNuke_mop, airMopAlways);
  char **hist = harr->data;
  for (uint hi = 0; hi < harr->len; hi++) {
    printf("%s\n", hist[hi]);
  }

  airMopOkay(mop);
  return 0;
}

static int
unu_historyMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  hestOpt *hopt = NULL;
  int commentIfNone;
  hestOptAdd_1_Bool(&hopt, "cin,commmentifnone", "bool", &commentIfNone, "true",
                    "If there is no unu history available in the key/value pairs, "
                    "print a \"#\" shell comment saying so");
  char **inS;
  unsigned int inLen;
  hestOptAdd_Mv_String(&hopt, NULL, "nin1", 1, -1, &inS, "-", "input nrrd filename(s)",
                       &inLen);

  // (boilerplate for YES some hparm modification)
  hestParm *hparm = hestParmCopy(_hparm);
  hparm->noArgsIsNoProblem = AIR_TRUE; // thanks to nrrdHestNrrdNoTTY
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_historyInfoL);
  airArray *mop = unrrduMopNew(hopt, hparm);
  char *err;

  for (unsigned int ni = 0; ni < inLen; ni++) {
    if (inLen > 1) {
      fprintf(stdout, "# ==> %s <==\n", inS[ni]);
    }
    if (historyDoit(me, inS[ni], commentIfNone)) {
      airMopAdd(mop, err = biffGetDone(me), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble with \"%s\":\n%s", me, inS[ni], err);
      hestUsage(stderr, hopt, me, hparm, AIR_TRUE /* oneline */);
      fprintf(stderr, "\nFor more info: \"%s --help\"\n", me);
      airMopError(mop);
      return 1;
    }
    if (inLen > 1 && ni < inLen - 1) {
      fprintf(stdout, "\n");
    }
  }

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(history, INFO);
