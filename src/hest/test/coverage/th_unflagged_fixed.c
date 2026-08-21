/*
This file was first written by an LLM so it is not copyrightable; subsequent human
tweaks do not change that. Link with source, assuming GLK's account:
https://claude.ai/chat/753238c4-c4d1-4241-824a-c1a310956286
*/

static const char *info
  = ("unflagged fixed positional options. "
     "Without any variadic option, unflagged options are assigned strictly in "
     "definition order, consuming args left-to-right.  This is the simple case.");
/*
** Options (all unflagged, all required — NULL default):
**   <A0> <A1>   two ints   (kind 3, unflagged)
**   <B>         one string (kind 2, unflagged)
**   <C>         one float  (kind 2, unflagged)
**
** For argv "10 20 hello 3.5":
**   A = {10, 20}   (first two args)
**   B = "hello"    (third arg)
**   C = 3.5        (fourth arg)
**
** Printed output:
**   A0=<int>   A1=<int>   A.src=...
**   B=<string> B.src=...
**   C=<float>  C.src=...
*/

#include "thcommon.h"

int
main(int argc, const char **argv) {
  TH_SANITY;
  hestOpt *opt = NULL;
  hestParm *hparm = hestParmNew();
  /* noArgsIsNoProblem stays false: all options are required anyway
     (GLK: actually that's moot because it only matters for hestParseOrExit) */
  hparm->respectDashDashHelp = AIR_TRUE;

  int A[2];
  hestOptAdd_2_Int(&opt, NULL, "A0 A1", A, NULL, "two required ints");
  char *B = NULL;
  hestOptAdd_1_String(&opt, NULL, "B", &B, NULL, "required string");
  float C = 0.0f;
  hestOptAdd_1_Float(&opt, NULL, "C", &C, NULL, "required float");

  TH_PARSE(opt, info, hparm, argc - 1, argv + 1);

  THPRINT_INT("A0", A[0]);
  THPRINT_INT("A1", A[1]);
  THPRINT_SOURCE("A", opt, 0);
  THPRINT_STR("B", B);
  THPRINT_SOURCE("B", opt, 1);
  THPRINT_FLOAT("C", C);
  THPRINT_SOURCE("C", opt, 2);

  TH_CLEANUP(opt, hparm);
  return 0;
}
