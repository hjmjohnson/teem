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

#include <stdio.h> /* for snprintf */
#include "thcommon.h"

int
main(int argc, const char **argv) {
  TH_SANITY;
  hestOpt *hopt = NULL;
  // mopParm means using the "self-mopping" hparm for hestParseOrExit
  int selfMop = !!getenv("TH_SELFMOP");
  hestParm *hparm = selfMop ? hestParmMopNew() : hestParmNew();
  th_getenvInt(&(hparm->verbose), "TH_VERBOSE");
  // if ioctl() fails (and testing will force that, once) small number of column
  hestParmColumnsIoctl(hparm, 70);
  hparm->respectDashDashHelp = AIR_TRUE;
  hparm->responseFileEnable = AIR_TRUE;
  int RDDMS = AIR_TRUE;
  th_getenvBool(&RDDMS, "TH_RDDMS");
  hparm->respectDashDashMeansStop = RDDMS;
  int RDPME = AIR_TRUE;
  th_getenvBool(&RDPME, "TH_RDPME");
  hparm->respectDashPeriodMeansEnd = RDPME;

  // export TH_FLAG_A=true to flag the non-variadic=fixed option "A"
  int FLAG_A = AIR_FALSE; // by default, do not flag "A" (how this file started)
  th_getenvBool(&FLAG_A, "TH_FLAG_A");
  // export TH_FLAG_B=true to flag the variadic option "B"
  int FLAG_B = AIR_FALSE; // by default, do not flag "B" (how this file started)
  th_getenvBool(&FLAG_B, "TH_FLAG_B");
  int DROP_B = AIR_FALSE; // by default, do create "B" option (how this file started)
  th_getenvBool(&DROP_B, "TH_DROP_B");
  // export TH_FLAG_C=true to flag the non-variadic=fixed option "C"
  int FLAG_C = AIR_FALSE; // by default, do not flag "C" (how this file started)
  th_getenvBool(&FLAG_C, "TH_FLAG_C");
  // export TH_FLAG_D=true to flag the option "D"
  int FLAG_D = AIR_FALSE; // by default, do not flag "D"
  th_getenvBool(&FLAG_D, "TH_FLAG_D");

  /* redonkulous game just to exercise a singular/plural branch for coverage */
  int A_LEN3 = AIR_FALSE;
  th_getenvBool(&A_LEN3, "TH_A_LEN3");
  int A2val[2];
  int A3val[3];
  int *A;
  char A2name[] = "A0 A1";
  char A3name[] = "A0 A1 A2";
  char *Aname;
  unsigned int Alen;
  unsigned int (*Ahoa)(hestOpt **hoptP, const char *flag, const char *name,
                       int valueP[2], const char *dflt, const char *info);
  if (A_LEN3) {
    A = A3val;
    Alen = 3;
    Aname = A3name;
    Ahoa = hestOptAdd_3_Int;
  } else {
    A = A2val;
    Alen = 2;
    Aname = A2name;
    Ahoa = hestOptAdd_2_Int;
  }
  unsigned int Aopi = (*Ahoa)(&hopt, FLAG_A ? "A" : NULL, Aname, A, getenv("TH_A_DFLT"),
                              "some leading ints");

  // export TH_B_VMIN=0 to trigger a particular hest failure
  int B_VMIN = 1; // by default, min # args is 1
  th_getenvInt(&B_VMIN, "TH_B_VMIN");
  int B_VMAX = -1; // by default, # args is unbounded
  th_getenvInt(&B_VMAX, "TH_B_VMAX");
  if (B_VMIN == B_VMAX) {
    fprintf(stderr, "%s: NOPE cannot work with B vmin == vmax == %d\n", argv[0], B_VMIN);
    // because then it is kind 3 not kind 5, and then the value pointer passed is wrong
    exit(1);
  }
  // hestOptAdd_Mv_String(&hopt, FLAG_VARI ? "B" : NULL, "Bvar", B_VMIN, -1, &B, NULL,
  //                      "variable # of middle strings\n "
  //                      "(including a second line)",
  //                      &sawB);
  // using old hestOptAdd for coverage
  char **B = NULL;
  unsigned int sawB = 0;
  unsigned int Bopi = (unsigned int)-1;
  if (!DROP_B) {
    Bopi = hestOptAdd(&hopt, FLAG_B ? "B" : NULL, "Bvar", airTypeString, B_VMIN, B_VMAX,
                      &B, getenv("TH_B_DFLT"),
                      "variable # of middle strings\n "
                      "(including a second line)",
                      getenv("TH_NULL_SAW") ? NULL : &sawB);
  }

  float C = 0;
  unsigned int Copi = hestOptAdd_1_Float(&hopt, FLAG_C ? "C" : NULL, "C", &C,
                                         getenv("TH_C_DFLT"), "middling float");

  // export TH_TWO_VARI=true (but not TH_FLAG_VARI) to trigger a different particular
  // hest failure
  int TWO_VARI = AIR_FALSE; // by default, do not use extra unflagged variadic
  th_getenvBool(&TWO_VARI, "TH_TWO_VARI");
  int *fakeD = NULL;
  unsigned int sawD = 0;
  int D[2];
  unsigned int Dopi = (unsigned int)-1;
  if (TWO_VARI) {
    Dopi = hestOptAdd_Mv_Int(&hopt, FLAG_D ? "D" : NULL, "Dvar", 1, -1, &fakeD,
                             getenv("TH_D_DFLT"), "one-or-more final ints", &sawD);
  } else {
    Dopi = hestOptAdd_2_Int(&hopt, FLAG_D ? "D" : NULL, "D0 D1", D, getenv("TH_D_DFLT"),
                            "two final ints");
  }

  if (selfMop) {
    hestParseOrExit(hopt, argc - 1, argv + 1, hparm, "th_variadic", info);
  } else {
    TH_PARSE(hopt, info, hparm, argc - 1, argv + 1);
  }

  if (A_LEN3) {
    printf("A=[%d;%d;%d;]\n", A[0], A[1], A[2]);
  } else {
    printf("A=[%d;%d;]\n", A[0], A[1]);
  }
  THPRINT_SOURCE("A", hopt, Aopi);

  if (!DROP_B) {
    printf("B=[");
    for (unsigned int ii = 0; ii < sawB; ii++) {
      printf("%s;", B[ii]);
    }
    printf("]\n");
    /* doing this check here in C rather than creating a new convention for communicating
       it to the Python driver
       From 0-README.md:
       In the case of multiple variadic strings, a final NULL entry is added to the
       `(*vvP)[]` array to mimic the NULL-terminated structure of `argv` itself. */
    if (B[sawB] != NULL) {
      fprintf(stderr,
              "%s: OOOooooops: parsed multiple variadic strings should be capped with "
              "NULL (but B[%u]=%p)\n",
              argv[0], sawB, AIR_CVOIDP(B[sawB]));
    }
    THPRINT_SOURCE("B", hopt, Bopi);
  }
  airSinglePrintf(stdout, NULL, 0, "C=%g\n", C);
  THPRINT_SOURCE("C", hopt, Copi);

  printf("D=[%d;%d;]\n", D[0], D[1]);
  THPRINT_SOURCE("D", hopt, Dopi);

  if ((!!hopt[0].remainArgc + !!hopt[0].remainArgv) % 2) {
    fprintf(stderr, "%s: NOOOO remainArgc %u!=0  disagrees with remainArgv!=NULL\n",
            argv[0], hopt[0].remainArgc, AIR_CVOIDP(hopt[0].remainArgv));
    return 1;
  }
  if (RDDMS) {
    printf("remain=[");
    for (unsigned int ri = 0; ri < hopt[0].remainArgc; ri++) {
      printf("%s;", hopt[0].remainArgv[ri]);
    }
    if (hopt[0].remainArgc && NULL != hopt[0].remainArgv[hopt[0].remainArgc]) {
      fprintf(stderr, "%s: what the heck, remainArgc = %u but remainArgv[%u] NOT NULL\n",
              argv[0], hopt[0].remainArgc);
      return 1;
    }
    printf("]\n");
  } else {
    printf("remain=NULL\n");
  }

  TH_CLEANUP(hopt, hparm);
  return 0;
}
