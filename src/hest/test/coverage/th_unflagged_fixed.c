/*
This file was first written by an LLM so it is not copyrightable; subsequent human
tweaks do not change that. Link with source, assuming GLK's account:
https://claude.ai/chat/753238c4-c4d1-4241-824a-c1a310956286
*/

#include "thcommon.h"

// copied from privateHest.h
#define MY_HEST_TYPE_MAX 14
typedef struct {
  hestOpt *hopt;                 // what option is this for
  char err[AIR_STRLEN_HUGE + 1]; // error message can go for any type
} hest__PPair;
extern int (*const hest__ParseSingle[MY_HEST_TYPE_MAX + 1])(void *, const char *,
                                                            hest__PPair *);

static const char *info
  = ("unflagged fixed positional options. "
     "Without any variadic option, unflagged options are assigned strictly in "
     "definition order, consuming args left-to-right.  This is the simple case.");

// (GLK copied this from th_kindK.c to exercise hestOptAdd in another way)
// -------------- "pos": an "other" that doesn't do its own allocation = "ostat"

static int
pos_parse(void *_ptr, const char *str, char *err) {
  double *ptr;
  int ret;

  ptr = _ptr;
  ret = sscanf(str, "%lf,%lf", ptr + 0, ptr + 1);
  if (2 != ret) {
    if (!getenv("TH_DSOEM")) { /* Dont Sprint Other Error Message */
      sprintf(err, "parsed %d (not 2) doubles", ret);
    } else {
      fprintf(stderr, "%s: NOT sprintf(err) because of TH_DSOEM\n", __func__);
    }
    return 1;
  }
  return 0;
}

hestCB posCB = {2 * sizeof(double), "position", pos_parse, NULL};

void
pos_print(FILE *file, const double pos[2]) {
  fprintf(file, "(");
  airSinglePrintf(file, NULL, 0, "%lg", pos[0]);
  fprintf(file, ",");
  airSinglePrintf(file, NULL, 0, "%lg", pos[1]);
  fprintf(file, ")");
  fflush(file);
  return;
}

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
  unsigned int sopi = hestOptAdd_1_String(&opt, NULL, "B", &B, NULL, "required string");
  double C[2];
  // hestOptAdd_1_Float(&opt, NULL, "C", &C, NULL, "required float");
  // using old hestOptAdd for coverage
  unsigned int oopi = hestOptAdd(&opt, NULL, "C", airTypeOther, 1, 1, C, NULL,
                                 "required x,y", NULL, NULL, &posCB);

  if (getenv("TH_THEATER")) {
    printf("%s: coverage theater begins ...\n", argv[0]);
    /* "theater" because we're doing this entirely to increase coverage,
       mostly without actually checking for correctness. */
    hestOpt *hoptStr = opt + sopi;
    hestOpt *hoptOth = opt + oopi;
    hest__PPair ppair;
    ppair.hopt = hoptStr;
    for (unsigned int ti = 1; ti <= MY_HEST_TYPE_MAX; ti++) {
      double buff[10];
      // all the different ways parsing can't start
      hest__ParseSingle[ti](buff, "bogus", NULL);
      hest__ParseSingle[ti](buff, NULL, &ppair);
      hest__ParseSingle[ti](NULL, "bogus", &ppair);
    }
    // exercising the many helpful error messages in methodsHest.c:hest__OPCheck()
    // (these will dump to stderr)
    hestOptCheck(NULL, NULL);
    hoptStr->type += 100;
    hestOptCheck(opt, NULL);
    hoptStr->type -= 100;
    char *tmpStr = hoptStr->valueP;
    hoptStr->valueP = NULL;
    hestOptCheck(opt, NULL);
    hoptStr->valueP = tmpStr;
    char longflag[] = ("this is way too long to be a reasonable flag. "
                       "this is way too long to be a reasonable flag. "
                       "this is way too long to be a reasonable flag. "
                       "this is way too long to be a reasonable flag. "
                       "this is way too long to be a reasonable flag. ");
    tmpStr = hoptStr->flag;
    hoptStr->flag = longflag;
    hestOptCheck(opt, NULL);
    // clang-format off
    hoptStr->flag = airStrdup("shortflag");
    hoptStr->flag[4] += 100;             hestOptCheck(opt, NULL);
    hoptStr->flag[4] = ' ';              hestOptCheck(opt, NULL); free(hoptStr->flag);
    hoptStr->flag = airStrdup("ss,");    hestOptCheck(opt, NULL); free(hoptStr->flag);
    hoptStr->flag = airStrdup(",ll");    hestOptCheck(opt, NULL); free(hoptStr->flag);
    hoptStr->flag = airStrdup("s,l,");   hestOptCheck(opt, NULL); free(hoptStr->flag);
    hoptStr->flag = airStrdup("s,-l");   hestOptCheck(opt, NULL); free(hoptStr->flag);
    hoptStr->flag = airStrdup("s,help"); hestOptParmCheck(opt, hparm, NULL); free(hoptStr->flag);
    hoptStr->flag = airStrdup("{");      hestOptParmCheck(opt, hparm, NULL); free(hoptStr->flag);
    hoptStr->flag = tmpStr;
    // clang-format on
    printf("%s: ... end coverage theater\n", argv[0]);
  }

  TH_PARSE(opt, info, hparm, argc - 1, argv + 1);

  printf("A=[%d|%d]\n", A[0], A[1]);
  THPRINT_SOURCE("A", opt, 0);
  printf("B=%s\n", B);
  THPRINT_SOURCE("B", opt, 1);
  printf("C=");
  pos_print(stdout, C);
  printf("\n");
  THPRINT_SOURCE("C", opt, 2);

  TH_CLEANUP(opt, hparm);
  return 0;
}
