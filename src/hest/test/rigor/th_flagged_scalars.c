/*
This file was first written by an LLM so it is not copyrightable; subsequent human
tweaks do not change that. Link with source, assuming GLK's account:
https://claude.ai/chat/753238c4-c4d1-4241-824a-c1a310956286
*/

static const char *info = "flagged scalar options (kind 2)";
/*
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
  hparm->responseFileEnable = AIR_TRUE;
  hparm->respectDashDashHelp = AIR_TRUE;
  th_getenvInt(&(hparm->verbose), "TH_VERBOSE");
  // ENEFD == elide non-exist float default
  int testThing = 0;
  th_getenvInt(&testThing, "TH_ENEFD");
  if (testThing > 1) {
    hparm->elideSingleNonExistFloatDefault = AIR_TRUE;
  }

  int n = 0;
  // hestOptAdd_1_Int(&opt, "n", "count", &n, "99", "an integer");
  // using old hestOptAdd for coverage
  hestOptAdd(&opt, "n", "count", airTypeInt, 1, 1, &n,
             // forgive the two calls to getenv
             getenv("TH_N_DFLT") ? getenv("TH_N_DFLT") : "99", "an integer");
  double x = 0.0;
  hestOptAdd_1_Double(&opt, "x", "val", &x, testThing ? "nan" : "1.0", "a double");
  char *s = NULL;
  hestOptAdd_1_String(&opt, "s", "str", &s,
                      // forgive the two calls to getenv
                      getenv("TH_S_DFLT") ? getenv("TH_S_DFLT") : "sunk cost",
                      "a string");
  int b = 0;
  hestOptAdd_1_Bool(&opt, "b", "flag", &b, "false", "a boolean");
  int e = 0;
  // hestOptAdd_1_Enum(&opt, "e", "end", &e, "little", "endianness", airEndian);
  // using old hestOptAdd for coverage
  hestOptAdd(&opt, "e", "end", airTypeEnum, 1, 1, &e, "little", "endianness", NULL,
             airEndian);

  TH_PARSE(opt, info, hparm, argc - 1, argv + 1);

  printf("n=%d\n", n);
  THPRINT_SOURCE("n", opt, 0);
  airSinglePrintf(stdout, NULL, 0, "x=%lg\n", x);
  THPRINT_SOURCE("x", opt, 1);
  printf("s=%s\n", s);
  THPRINT_SOURCE("s", opt, 2);
  printf("b=%s\n", airEnumStr(airBool, b));
  THPRINT_SOURCE("b", opt, 3);
  printf("e=%d\n", e);
  THPRINT_SOURCE("e", opt, 4);

  TH_CLEANUP(opt, hparm);
  return 0;
}
