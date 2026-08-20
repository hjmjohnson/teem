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
**   position 0: A[2] ints   — fixed (kind 3), unflagged, required
**   position 1: B[] strings — variadic (kind 5), unflagged, required (min=1)
**   position 2: C int       — fixed (kind 2), unflagged, required
**
** Concrete example with argv "10 20 p q r 99":
**   pass 1 forward:  A = {10, 20}       (consumes first 2 args)
**   pass 2 backward: C = 99             (consumes last arg)
**   variadic fill:   B = {"p","q","r"}  (the 3 middle args)
**
** Printed output:
**   A0=<int>   A1=<int>   A.src=...
**   B.len=<N>
**   B0=<str>   B1=<str>   ...         (N lines, one per string)
**   B.src=...
**   C=<int>    C.src=...
*/

#include <stdio.h> /* for snprintf */
#include "thcommon.h"

int
main(int argc, const char **argv) {
  TH_SANITY;
  hestOpt *opt = NULL;
  hestParm *hparm = hestParmNew();
  th_getenvInt(&(hparm->verbose), "TH_VERBOSE");
  hparm->respectDashDashHelp = AIR_TRUE;

  // export TH_FLAGVAR=true to flag the variadic option(s)
  int FLAGVAR = AIR_FALSE; // by default, do not flag (how this file started)
  th_getenvBool(&FLAGVAR, "TH_FLAGVAR");
  // export TH_FLAGFIX=true to flag the non-variadic=fixed option(s)
  int FLAGFIX = AIR_FALSE; // by default, do not flag (how this file started)
  th_getenvBool(&FLAGFIX, "TH_FLAGFIX");

  int A[2];
  hestOptAdd_2_Int(&opt, FLAGFIX ? "A" : NULL, "A0 A1", A, NULL, "two leading ints");

  char **B = NULL;
  unsigned int sawB = 0;

  int eGot;
  // export TH_VMIN=0 to trigger a particular hest failure
  int VMIN = 1; // by default, min # args is 1
  th_getenvInt(&VMIN, "TH_VMIN");
  hestOptAdd_Nv_String(&opt, FLAGVAR ? "B" : NULL, "Bvar", VMIN, -1, &B, NULL,
                       "variable # of middle strings", &sawB);

  int C = 0;
  hestOptAdd_1_Int(&opt, FLAGFIX ? "C" : NULL, "C", &C, NULL, "trailing int");

  // export TH_TWOVAR (but not TH_FLAGVAR) to trigger a different particular hest failure
  int TWOVAR = AIR_FALSE; // by default, do not use extra unflagged variadic
  th_getenvBool(&TWOVAR, "TH_TWOVAR");
  char **D = NULL;
  unsigned int sawD = 0;
  if (TWOVAR) {
    hestOptAdd_Nv_String(&opt, FLAGVAR ? "D" : NULL, "Dvar", 1, -1, &D, NULL,
                         "one-or-more end strings", &sawD);
  }

  TH_PARSE(opt, info, hparm, argc - 1, argv + 1);

  THPRINT_INT("A0", A[0]);
  THPRINT_INT("A1", A[1]);
  THPRINT_SOURCE("A", opt, 0);

  THPRINT_LEN("B", sawB);
  for (unsigned int ii = 0; ii < sawB; ii++) {
    char key[32];
    snprintf(key, sizeof(key), "B%u", ii);
    THPRINT_STR(key, B[ii]);
  }
  THPRINT_SOURCE("B", opt, 1);

  THPRINT_INT("C", C);
  THPRINT_SOURCE("C", opt, 2);

  TH_CLEANUP(opt, hparm);
  return 0;
}
