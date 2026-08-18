/*
This file was first written by an LLM so it is not copyrightable; subsequent human
tweaks do not change that. Link with source, assuming GLK's account:
https://claude.ai/chat/753238c4-c4d1-4241-824a-c1a310956286
*/

/*
** th_flagged_scalars.c: flagged scalar options (kind 2).
**
** This test covers the core case of one fixed parameter per option,
** all flagged, all with defaults.  It is also the vehicle for testing
** response files and -{ }- comment blocks, because those features operate
** transparently at the argv level and require no special option setup.
**
** Options (all flagged, all kind 2, all with defaults):
**   -n <int>      integer,  default 99
**   -x <double>   double,   default 1.0
**   -s <string>   string,   default "bingo"
**   -b <bool>     boolean,  default false  (printed as 0 or 1)
**   -e <enum>     airEndian, default "little"
**                 (airEndianLittle=1234, airEndianBig=4321)
**
** hparm settings enabled here:
**   responseFileEnable    = true   (for @file.txt tests)
**   respectDashBraceComments = true (already the default; explicit for clarity)
**   noArgsIsNoProblem     = true   (so empty argv tests all-defaults cleanly)
**
** Printed output:
**   n=<int>     n.src=...
**   x=<double>  x.src=...
**   s=<string>  s.src=...
**   b=<0|1>     b.src=...
**   e=<int>     e.src=...
*/

#include "thcommon.h"

int
main(int argc, const char **argv) {
  TH_SANITY;
  hestOpt *opt = NULL;
  hestParm *hparm = hestParmNew();
  hparm->noArgsIsNoProblem = AIR_TRUE;
  hparm->responseFileEnable = AIR_TRUE;
  hparm->respectDashBraceComments = AIR_TRUE; /* already the default */
  hparm->respectDashDashHelp = AIR_TRUE;

  int n = 0;
  hestOptAdd_1_Int(&opt, "n", "count", &n, "99", "an integer");
  double x = 0.0;
  hestOptAdd_1_Double(&opt, "x", "val", &x, "1.0", "a double");
  char *s = NULL;
  hestOptAdd_1_String(&opt, "s", "str", &s, "bingo", "a string");
  int b = 0;
  hestOptAdd_1_Bool(&opt, "b", "flag", &b, "false", "a boolean");
  int e = 0;
  hestOptAdd_1_Enum(&opt, "e", "end", &e, "little", "endianness", airEndian);

  TH_PARSE(opt, hparm, argc - 1, argv + 1);

  THPRINT_INT("n", n);
  THPRINT_SOURCE("n", opt, 0);
  THPRINT_DOUBLE("x", x);
  THPRINT_SOURCE("x", opt, 1);
  THPRINT_STR("s", s);
  THPRINT_SOURCE("s", opt, 2);
  THPRINT_BOOL("b", b);
  THPRINT_SOURCE("b", opt, 3);
  THPRINT_INT("e", e);
  THPRINT_SOURCE("e", opt, 4);

  TH_CLEANUP(opt, hparm);
  return 0;
}
