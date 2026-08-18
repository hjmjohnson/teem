/*
This file was first written by an LLM so it is not copyrightable; subsequent human
tweaks do not change that. Link with source, assuming GLK's account:
https://claude.ai/chat/753238c4-c4d1-4241-824a-c1a310956286
*/

/*
** th_errors.c: expected parse failures.
**
** The option set here is simple (one flagged int, one flagged string, both
** with defaults), but the test cases in run_hest_tests.py invoke this binary
** with argv that hestParse should reject.  Exit 0 means the expected failure
** was correctly detected; exit 1 means hestParse unexpectedly succeeded.
**
** Options (both flagged, both kind 2, both with defaults):
**   -n <int>     integer, default 99
**   -s <string>  string,  default "bingo"
**
** Printed output (on expected failure):
**   error=<hestParse error message>
**
** On unexpected success: writes a message to stderr and exits 1.
*/

#include "thcommon.h"

int
main(int argc, const char **argv) {
  TH_SANITY;
  hestOpt *opt = NULL;
  hestParm *hparm = hestParmNew();
  hparm->noArgsIsNoProblem = AIR_TRUE; /* isolate: empty argv is fine here */
  hparm->respectDashDashHelp = AIR_TRUE;

  int n = 0;
  hestOptAdd_1_Int(&opt, "n", "count", &n, "99", "an integer");
  char *s = NULL;
  hestOptAdd_1_String(&opt, "s", "str", &s, "bingo", "a string");

  TH_PARSE_SHOULD_FAIL(opt, hparm, argc - 1, argv + 1);

  TH_CLEANUP_AFTER_FAIL(opt, hparm);
  return 0;
}
