/*
This file was first written by an LLM so it is not copyrightable; subsequent human
tweaks do not change that. Link with source, assuming GLK's account:
https://claude.ai/chat/753238c4-c4d1-4241-824a-c1a310956286
*/

/*
** th_fixed_multi.c: fixed multiple-parameter options (kind 3).
**
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
  TH_SANITY;
  hestOpt *opt = NULL;
  hestParm *hparm = hestParmNew();
  hparm->noArgsIsNoProblem = AIR_TRUE;
  hparm->respectDashDashHelp = AIR_TRUE;

  float f[2];
  hestOptAdd_2_Float(&opt, "f", "f0 f1", f, "1.0 2.0", "two floats");
  int i[3];
  hestOptAdd_3_Int(&opt, "i", "i0 i1 i2", i, "10 20 30", "three ints");

  TH_PARSE(opt, hparm, argc - 1, argv + 1);

  THPRINT_FLOAT("f0", f[0]);
  THPRINT_FLOAT("f1", f[1]);
  THPRINT_SOURCE("f", opt, 0);
  THPRINT_INT("i0", i[0]);
  THPRINT_INT("i1", i[1]);
  THPRINT_INT("i2", i[2]);
  THPRINT_SOURCE("i", opt, 1);

  TH_CLEANUP(opt, hparm);
  return 0;
}
