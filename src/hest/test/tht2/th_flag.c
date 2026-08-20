/*
This file was first written by an LLM so it is not copyrightable; subsequent human
tweaks do not change that. Link with source, assuming GLK's account:
https://claude.ai/chat/753238c4-c4d1-4241-824a-c1a310956286
*/

/*
** th_flag.c: stand-alone flag (kind 1).
**
** Options:
**   -v        stand-alone flag (kind 1); no parameters
**
** Printed output:
**   v=0 or v=1         (0 if absent, 1 if -v appeared on command line)
**   v.src=...          (default | cmdline)
**
** Expected test cases (in run_hest_tests.py):
**   (no args)   → v=0, v.src=default
**   -v          → v=1, v.src=cmdline
*/

#include "thcommon.h"

int
main(int argc, const char **argv) {
  TH_SANITY;
  hestOpt *opt = NULL;
  hestParm *hparm = hestParmNew();
  hparm->noArgsIsNoProblem = AIR_TRUE; /* allow empty argv */
  hparm->respectDashDashHelp = AIR_TRUE;
  hestParmColumnsIoctl(hparm, 80); // just for coverage

  int v = 0;
  hestOptAdd_Flag(&opt, "v", &v, "be verbose");

  TH_PARSE(opt, hparm, argc - 1, argv + 1);

  THPRINT_BOOL("v", v);
  THPRINT_SOURCE("v", opt, 0);

  TH_CLEANUP(opt, hparm);
  return 0;
}
