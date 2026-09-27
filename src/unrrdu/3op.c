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

#define INFO "Ternary operation on three nrrds or constants"
static const char *unu_3opInfoL
  = (INFO ". Can have one, two, or three nrrds, but not zero. "
          "Use \"-\" for an operand to signify "
          "a nrrd to be read from stdin (a pipe).  Note, however, "
          "that \"-\" can probably only be used once (reliably).\n "
          "* Uses nrrdArithIterTernaryOp or (with -w) nrrdArithIterTernaryOpSelect");

static int
unu_3opMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  int op;
  hestOptAdd_1_Enum(&hopt, NULL, "operator", &op, NULL,
                    "Ternary operator. Possibilities include:\n "
                    "\b\bo \"+\", \"x\": sum or product of three values\n "
                    "\b\bo \"min\", \"max\": minimum, maximum\n "
                    "\b\bo \"min_sm\": smoothed minimum function; "
                    "min_sm(x, w, M) is like min(x,M) but for x > M-w (with w > 0) "
                    "there is a smooth transition from x to asymptotic to M\n "
                    "\b\bo \"max_sm\": smoothed maximum function; "
                    "max_sm(m, w, x) is like max(m,x) but for x < m+w (with w > m) "
                    "there is a smooth transition from x to asymptotic to m\n "
                    "\b\bo \"lt_sm\": 1st less than 3rd, smoothed by 2nd\n "
                    "\b\bo \"gt_sm\": 1st greater than 3rd, smoothed by 2nd\n "
                    "\b\bo \"clamp\": 2nd value is clamped to range between "
                    "the 1st and the 3rd\n "
                    "\b\bo \"ifelse\": if 1st value non-zero, then 2nd value, else "
                    "3rd value\n "
                    "\b\bo \"lerp\": linear interpolation between the 2nd and "
                    "3rd values, as the 1st value varies between 0.0 and 1.0, "
                    "respectively\n "
                    "\b\bo \"exists\": if the 1st value exists, use the 2nd "
                    "value, otherwise use the 3rd\n "
                    "\b\bo \"in_op\": 1 iff 2nd value is > 1st and < 3rd, "
                    "0 otherwise\n "
                    "\b\bo \"in_cl\": 1 iff 2nd value is >= 1st and <= 3rd, "
                    "0 otherwise\n "
                    "\b\bo \"gauss\": evaluate (at 1st value) Gaussian with mean=2nd "
                    "and stdv=3rd value\n "
                    "\b\bo \"rician\": evaluate (at 1st value) Rician with mean=2nd "
                    "and stdv=3rd value",
                    nrrdTernaryOp);
  NrrdIter *in1, *in2, *in3;
  uint in1Hidx = hestOptAdd_1_Other(&hopt, NULL, "in1", &in1, NULL,
                                    "First input.  Can be a single value or a nrrd.",
                                    nrrdHestIter);
  uint in2Hidx = hestOptAdd_1_Other(&hopt, NULL, "in2", &in2, NULL,
                                    "Second input.  Can be a single value or a nrrd.",
                                    nrrdHestIter);
  uint in3Hidx = hestOptAdd_1_Other(&hopt, NULL, "in3", &in3, NULL,
                                    "Third input.  Can be a single value or a nrrd.",
                                    nrrdHestIter);
  hopt[in1Hidx].priority -= 10;
  hopt[in2Hidx].priority -= 10;
  hopt[in3Hidx].priority -= 10;
  int type;
  hestOptAdd_1_Other(&hopt, "t,type", "type", &type, "default",
                     "type to convert all nrrd inputs to, prior to "
                     "doing operation.  This also determines output type. "
                     "By default (not using this option), the types of the input "
                     "nrrds are left unchanged.",
                     &unrrduHestMaybeTypeCB);
  int which;
  hestOptAdd_1_Int(&hopt, "w,which", "arg", &which, "-1",
                   "Which argument (0, 1, or 2) should be used to determine the "
                   "shape of the output nrrd. By default (not using this option), "
                   "the first non-constant argument is used. ");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_3opInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);

  /*
  fprintf(stderr, "%s: op = %d\n", me, op);
  fprintf(stderr, "%s: in1->left = %d, in2->left = %d\n", me,
          (int)(in1->left), (int)(in2->left));
  */
  if (nrrdTypeDefault != type) {
    /* they wanted to convert nrrds to some other type first */
    Nrrd *ntmp;
    int E = 0;
    if (in1->ownNrrd) {
      if (!E) E |= nrrdConvert(ntmp = nrrdNew(), in1->ownNrrd, type);
      if (!E) nrrdIterSetOwnNrrd(in1, ntmp);
    }
    if (in2->ownNrrd) {
      if (!E) E |= nrrdConvert(ntmp = nrrdNew(), in2->ownNrrd, type);
      if (!E) nrrdIterSetOwnNrrd(in2, ntmp);
    }
    if (in3->ownNrrd) {
      if (!E) E |= nrrdConvert(ntmp = nrrdNew(), in3->ownNrrd, type);
      if (!E) nrrdIterSetOwnNrrd(in3, ntmp);
    }
    if (E) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error converting input nrrd(s):\n%s", me, err);
      airMopError(mop);
      return 1;
    }
    /* this will still leave a nrrd in the NrrdIter for nrrdIterNix()
       (called by hestParseFree() called be airMopOkay()) to clear up */
  }

  /* HEY: will need to add handling of RNG seed (as in 1op and 2op)
     if there are any 3ops involving random numbers */

  if (-1 == which
        ? nrrdArithIterTernaryOp(nout, op, in1, in2, in3)
        : nrrdArithIterTernaryOpSelect(nout, op, in1, in2, in3, AIR_UINT(which))) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error doing ternary operation:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  unrrduUnuHistoryClear(nout);
  if (getenv(UNRRDU_TEEM_KVP_HISTORY_TRACE_ENV)) {
    // HEY copy-pasta from 2op.c
    // (it is a big bummer than handling history is so clunky ...)
    unrrduUnuOutNSPair onsp = {.nout = nout, .name = hopt[outHidx].parmStr};
    const Nrrd *in1Nrrd = in1->nrrd ? in1->nrrd : (in1->ownNrrd ? in1->ownNrrd : NULL);
    const Nrrd *in2Nrrd = in2->nrrd ? in2->nrrd : (in2->ownNrrd ? in2->ownNrrd : NULL);
    const Nrrd *in3Nrrd = in3->nrrd ? in3->nrrd : (in3->ownNrrd ? in3->ownNrrd : NULL);
    unrrduUnuInNSPair insp1 = {.nin = in1Nrrd, .name = hopt[in1Hidx].parmStr};
    unrrduUnuInNSPair insp2 = {.nin = in2Nrrd, .name = hopt[in2Hidx].parmStr};
    unrrduUnuInNSPair insp3 = {.nin = in3Nrrd, .name = hopt[in3Hidx].parmStr};
    int in1Stdin = !strcmp("-", insp1.name);
    int in2Stdin = !strcmp("-", insp2.name);
    int in3Stdin = !strcmp("-", insp3.name);
    int E = 0;
    if (in1Stdin + in2Stdin + in3Stdin == 0) {
      // inputs are only files or constants
      if (!E && insp1.nin) E |= unrrduUnuHistoryAdd(onsp, 0, NULL, insp1);
      if (!E && insp2.nin) E |= unrrduUnuHistoryAdd(onsp, 0, NULL, insp2);
      if (!E && insp3.nin) E |= unrrduUnuHistoryAdd(onsp, 0, NULL, insp3);
      if (!E) E |= unrrduUnuHistoryAdd(onsp, argc + 1, argv - 1, MY_NULL_INSP);
    } else if (in1Stdin + in2Stdin + in3Stdin == 1) {
      // one these three is stdin; the other two's history should come first
      unrrduUnuInNSPair inspStdin, inspOtheA, inspOtheB;
      if (in1Stdin) {
        inspStdin = insp1;
        inspOtheA = insp2;
        inspOtheB = insp3;
      } else if (in2Stdin) {
        inspStdin = insp2;
        inspOtheA = insp1;
        inspOtheB = insp3;
      } else {
        inspStdin = insp3;
        inspOtheA = insp1;
        inspOtheB = insp2;
      }
      if (!E && inspOtheA.nin) E |= unrrduUnuHistoryAdd(onsp, 0, NULL, inspOtheA);
      if (!E && inspOtheB.nin) E |= unrrduUnuHistoryAdd(onsp, 0, NULL, inspOtheB);
      if (!E) E |= unrrduUnuHistoryAdd(onsp, argc + 1, argv - 1, inspStdin);
    }
    if (E) {
      airMopAdd(mop, err = biffGetDone(UNRRDU), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble generating history:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
  }
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(3op, INFO);
