/*
This file was first written by an LLM so it is not copyrightable; subsequent human
tweaks do not change that. Link with source, assuming GLK's account:
https://claude.ai/chat/753238c4-c4d1-4241-824a-c1a310956286
*/

static const char *info = "stand-alone flag (kind 1)";
/*
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
  hparm->respectDashDashHelp = AIR_TRUE;
  if (getenv("TH_THEATER")) {
    printf("%s: coverage theater begins ...\n", argv[0]);
    /* "theater" because we're doing this entirely to increase coverage,
       mostly without actually checking for correctness. */
    printf("%s: hestOptNum(NULL) = %u\n", argv[0], hestOptNum(NULL));
    hestParmFree(hestParmCopy(hparm) /* i.e. not checking correctness of copy */);
    printf("%s: hestSourceUser(hestSourceCommandLine) = %d\n", argv[0],
           hestSourceUser(hestSourceCommandLine));
    printf("%s: hestSourceUser(hestSourceResponseFile) = %d\n", argv[0],
           hestSourceUser(hestSourceResponseFile));
    printf("%s: hestSourceUser(hestSourceDefault) = %d\n", argv[0],
           hestSourceUser(hestSourceDefault));
    printf("%s: hestArgcArgvSprint(NULL) = %p\n", argv[0],
           (void *)hestArgcArgvSprint(42, NULL, AIR_FALSE));
    printf("%s: hestArgVecSprint(NULL) = %p\n", argv[0],
           (void *)hestArgVecSprint(NULL, AIR_FALSE));
    for (int showIndex = 0; showIndex <= 1; showIndex++) {
      char *str = hestArgcArgvSprint(argc, argv, showIndex);
      printf("%s: argc,argv (showIndex=%d) = %s\n", argv[0], showIndex, str);
      free(str);
    }
    hestParseFree(NULL);
    hestArgVecReset(NULL);
    hestArgVecNix(NULL);
    hestInfo(stdout, NULL /* const char *argv0 */, NULL /* const char *info */,
             NULL /* const hestParm *_hparm */);
    hestInfo(stdout, NULL, "info", NULL);
    hestUsage(stdout, opt /* but its NULL */, argv[0], hparm, AIR_FALSE /* oneline */);
    hestUsage(stdout, opt /* but its NULL */, argv[0], NULL, AIR_FALSE /* oneline */);
    hestGlossary(stdout, opt, hparm);
    hestGlossary(stdout, opt, NULL);
    printf("%s: ... end coverage theater\n", argv[0]);
  }

  int v = 0;
  hestOptAdd_Flag(&opt, getenv("TH_MISSINGFLAG") ? NULL : "v", &v, "be verbose");

  TH_PARSE(opt, info, hparm, argc - 1, argv + 1);

  printf("v=%d\n", v);
  THPRINT_SOURCE("v", opt, 0);

  TH_CLEANUP(opt, hparm);
  return 0;
}
