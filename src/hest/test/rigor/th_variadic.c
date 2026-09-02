/*
This file was first written by an LLM so it is not copyrightable; subsequent human
tweaks do not change that. Link with source, assuming GLK's account:
https://claude.ai/chat/753238c4-c4d1-4241-824a-c1a310956286
*/

static const char *info
  = ("forward-then-backward assignment. "
     "This test targets the most subtle part of hest's unflagged-option logic: "
     "when one unflagged option is variadic, hestParse uses a two-pass strategy "
     "to assign args unambiguously.  By the env-var hack, this also tests "
     "flagged variadic options, which Claude missed testing when it generated all "
     "this.");

/*
** (the following is just about *unflagged* variadic options)
**   Pass 1 (forward): fixed options that appear BEFORE the variadic in
**     definition order consume args from the beginning of the command line.
**   Pass 2 (backward): fixed options that appear AFTER the variadic in
**     definition order consume args from the end of the command line.
**   Variadic fill: the single unflagged variadic option gets the middle.
**
** Definition order (critical — determines which pass each option participates in):
**   position 0: A[2] ints   — fixed (kind 3)
**   position 1: B[] strings — variadic (kind 5)
**   position 2: C int       — fixed (kind 2)
**   position 3: D[2] ints   - fixed (kind 3)
*/

#include <stdio.h> /* for snprintf */
#include "thcommon.h"

int
main(int argc, const char **argv) {
  TH_SANITY;
  hestOpt *hopt = NULL;
  // mopParm means using the "self-mopping" hparm for hestParseOrExit
  int selfMop = !!getenv("TH_SELFMOP");
  hestParm *hparm = selfMop ? hestParmMopNew() : hestParmNew();
  th_getenvInt(&(hparm->verbose), "TH_VERBOSE");
  // if ioctl() fails (and testing will force that, once) small number of column
  hestParmColumnsIoctl(hparm, 70);
  hparm->respectDashDashHelp = AIR_TRUE;

  // export TH_FLAG_A=true to flag the non-variadic=fixed option "A"
  int FLAG_A = AIR_FALSE; // by default, do not flag "A" (how this file started)
  th_getenvBool(&FLAG_A, "TH_FLAG_A");
  // export TH_FLAG_B=true to flag the variadic option "B"
  int FLAG_B = AIR_FALSE; // by default, do not flag "B" (how this file started)
  th_getenvBool(&FLAG_B, "TH_FLAG_B");
  // export TH_FLAG_C=true to flag the non-variadic=fixed option "C"
  int FLAG_C = AIR_FALSE; // by default, do not flag "C" (how this file started)
  th_getenvBool(&FLAG_C, "TH_FLAG_C");
  // export TH_FLAG_D=true to flag the option "D"
  int FLAG_D = AIR_FALSE; // by default, do not flag "D"
  th_getenvBool(&FLAG_D, "TH_FLAG_D");

  int A[2];
  hestOptAdd_2_Int(&hopt, FLAG_A ? "A" : NULL, "A0 A1", A, getenv("TH_A_DFLT"),
                   "two leading ints");

  char **B = NULL;
  unsigned int sawB = 0;

  int eGot;
  // export TH_B_VMIN=0 to trigger a particular hest failure
  int B_VMIN = 1; // by default, min # args is 1
  th_getenvInt(&B_VMIN, "TH_B_VMIN");
  // hestOptAdd_Mv_String(&hopt, FLAG_VARI ? "B" : NULL, "Bvar", B_VMIN, -1, &B, NULL,
  //                      "variable # of middle strings\n "
  //                      "(including a second line)",
  //                      &sawB);
  // using old hestOptAdd just for coverage
  hestOptAdd(&hopt, FLAG_B ? "B" : NULL, "Bvar", airTypeString, B_VMIN, -1, &B,
             getenv("TH_B_DFLT"),
             "variable # of middle strings\n "
             "(including a second line)",
             getenv("TH_NULL_SAW") ? NULL : &sawB);

  int C = 0;
  hestOptAdd_1_Int(&hopt, FLAG_C ? "C" : NULL, "C", &C, getenv("TH_C_DFLT"),
                   "trailing int");

  // export TH_TWO_VARI=true (but not TH_FLAG_VARI) to trigger a different particular
  // hest failure
  int TWO_VARI = AIR_FALSE; // by default, do not use extra unflagged variadic
  th_getenvBool(&TWO_VARI, "TH_TWO_VARI");
  int **fakeD = NULL;
  unsigned int sawD = 0;
  int D[2];
  if (TWO_VARI) {
    hestOptAdd_Mv_Int(&hopt, FLAG_D ? "D" : NULL, "Dvar", 1, -1, &fakeD,
                      getenv("TH_D_DFLT"), "one-or-more final ints", &sawD);
  } else {
    hestOptAdd_2_Int(&hopt, FLAG_D ? "D" : NULL, "D0 D1", D, getenv("TH_D_DFLT"),
                     "two final ints");
  }

  if (selfMop) {
    hestParseOrExit(hopt, argc - 1, argv + 1, hparm, "th_variadic", info);
  } else {
    TH_PARSE(hopt, info, hparm, argc - 1, argv + 1);
  }

  printf("A=[%d;%d;]\n", A[0], A[1]);
  THPRINT_SOURCE("A", hopt, 0);

  printf("B=[");
  for (unsigned int ii = 0; ii < sawB; ii++) {
    printf("%s;", B[ii]);
  }
  printf("]\n");
  THPRINT_SOURCE("B", hopt, 1);

  printf("C=%d\n", C);
  THPRINT_SOURCE("C", hopt, 2);

  printf("D=[%d;%d;]\n", D[0], D[1]);
  THPRINT_SOURCE("D", hopt, 3);

  TH_CLEANUP(hopt, hparm);
  return 0;
}
