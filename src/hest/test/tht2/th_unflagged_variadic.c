/*
This file was first written by an LLM so it is not copyrightable; subsequent human
tweaks do not change that. Link with source, assuming GLK's account:
https://claude.ai/chat/753238c4-c4d1-4241-824a-c1a310956286
*/

/*
** th_unflagged_variadic.c: forward-then-backward assignment.
**
** This test targets the most subtle part of hest's unflagged-option logic:
** when one unflagged option is variadic, hestParse uses a two-pass strategy
** to assign args unambiguously:
**
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

// GLK adapted from nrrdGetenvBool in ../nrrd/defaultsNrrd.c
static void
getenvBool(int *val, const char *envVar) {
  char *envVal = getenv(envVar);
  if (!envVal) {
    // env var was not set, bye
    return;
  }
  // else env var was set
  if (!strlen(envVal)) {
    /* for bools, being merely set to empty string means "true" */
    *val = AIR_TRUE;
    return;
  }
  // else env var was set to non-empty string
  int tmp = airEnumVal(airBool, envVal);
  if (airEnumUnknown(airBool) == tmp) {
    // else env var was set but could not be parsed
    fprintf(stderr, "ERROR(%s): could not parse env[\"%s\"]=\"%s\" as int\n", __func__,
            envVar, envVal);
    exit(1); // WE ARE OUT OF HERE
  }
  *val = tmp;
  return;
}

// GLK adapted from nrrdGetenvInt in ../nrrd/defaultsNrrd.c
static void
getenvInt(int *val, const char *envVar) {
  char *envVal = getenv(envVar);
  if (!envVal) {
    // env var was not set, bye
    return;
  }
  // else env var was set: can it be parsed?
  int tmp;
  int scanned = sscanf(envVal, "%d", &tmp);
  if (1 == scanned) {
    // if could parse one, set it
    *val = tmp;
    return;
  }
  // else env var was set but could not be parsed
  fprintf(stderr, "ERROR(%s): could not parse env[\"%s\"]=\"%s\" as int\n", __func__,
          envVar, envVal);
  exit(1); // WE ARE OUT OF HERE
  return;
}

int
main(int argc, const char **argv) {
  TH_SANITY;
  hestOpt *opt = NULL;
  hestParm *hparm = hestParmNew();
  hparm->respectDashDashHelp = AIR_TRUE;

  int A[2];
  hestOptAdd_2_Int(&opt, NULL, "A0 A1", A, NULL, "two leading ints");

  char **B = NULL;
  unsigned int sawB = 0;

  int eGot;
  // export THT_VMIN=0 to trigger a particular hest failure
  int VMIN = 1; // by default, min # args is 1
  getenvInt(&VMIN, "THT_VMIN");
  hestOptAdd_Nv_String(&opt, NULL, "B", VMIN, -1, &B, NULL,
                       "variable # of middle strings", &sawB);

  int C = 0;
  hestOptAdd_1_Int(&opt, NULL, "C", &C, NULL, "trailing int");

  // export THT_TWOVAR to trigger a different particular hest failure
  int TWOVAR = AIR_FALSE; // by default, do not use extra variadic
  getenvBool(&TWOVAR, "THT_TWOVAR");
  char **D = NULL;
  unsigned int sawD = 0;
  if (TWOVAR) {
    hestOptAdd_Nv_String(&opt, NULL, "D", 1, -1, &D, NULL, "one-or-more end strings",
                         &sawD);
  }

  TH_PARSE(opt, hparm, argc - 1, argv + 1);

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
