/*
This file was first written by an LLM so it is not copyrightable; subsequent human
tweaks do not change that. Link with source, assuming GLK's account:
https://claude.ai/chat/753238c4-c4d1-4241-824a-c1a310956286
*/

static const char *info = "fixed multiple-parameter options (kind 3)";
/*
** Kind 3: min == max >= 2.  The flag must be followed by exactly N arguments.
**
** Options (both flagged, both with defaults):
**   -f <f0> <f1>           two floats   (kind 3, N=2), default "1.0 2.0"
**   -i <i0> <i1> <i2>     three ints   (kind 3, N=3), default "10 20 30"
**
** Printed output:
**   f0=<float>  f1=<float>  f.src=...
**   i0=<int>    i1=<int>    i2=<int>    i.src=...
*/

#include "thcommon.h"

int
main(int argc, const char **argv) {
  /* to debug how well we're doing C-locale-emulation */
  airSetLocaleFromEnvOrExit(argv[0], airStderr());
  TH_SANITY;
  hestOpt *opt = NULL;
  hestParm *hparm = hestParmNew();
  hparm->respectDashDashHelp = AIR_TRUE;
  int testThing = 0;
  th_getenvInt(&testThing, "TH_ENEFD");
  if (testThing > 1) {
    hparm->elideMultipleNonExistFloatDefault = AIR_TRUE;
  }

  // exercise long options for a change
  float f[2];
  hestOptAdd_2_Float(&opt, "f,float", "f0 f1", f, testThing ? "nan inf" : "1.0 2.0",
                     "two floats");
  int i[3];
  hestOptAdd_3_Int(&opt, "i,int", "i0 i1 i2", i, "10 20 30", "three ints");

  TH_PARSE(opt, info, hparm, argc - 1, argv + 1);

  airSinglePrintf(stdout, NULL, 0, "f=[%g;", f[0]);
  airSinglePrintf(stdout, NULL, 0, "%g;]\n", f[1]);
  THPRINT_SOURCE("f", opt, 0);
  printf("i=[%d;%d;%d;]\n", i[0], i[1], i[2]);
  THPRINT_SOURCE("i", opt, 1);

  TH_CLEANUP(opt, hparm);
  return 0;
}
