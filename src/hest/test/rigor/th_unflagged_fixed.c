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
extern void hest__optSingleSet(hestOpt *hopt, const char *flag, const char *name,
                               int type, unsigned int min, int max, void *valueP,
                               const char *dflt, const char *info, unsigned int *sawP,
                               const airEnum *enm, const hestCB *CB);

static const char *info
  = ("unflagged fixed positional options. "
     "Without any variadic option, unflagged options are assigned strictly in "
     "definition order, consuming args left-to-right.  This is the simple case.");

static int
quietQuitTester(const char *parseErrStr, const hestOpt *errOpt) {
  return parseErrStr && errOpt;
}

// (GLK copied these from th_kindK.c to exercise hestOptAdd in another way)
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
hestCB posCB_size0 = {0, "position", pos_parse, NULL};
hestCB posCB_type0 = {2 * sizeof(double), NULL, pos_parse, NULL};
hestCB posCB_parse0 = {2 * sizeof(double), "position", NULL, NULL};

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

// -------------- "Combo": an "other" that *does* do its own allocation = "odyn"

typedef struct {
  char *str;
  double val;
} Combo;

static int
combo_parse(void *_ptr, const char *str, char *err) {
  Combo **ptrP;
  Combo *ptr;

  ptrP = _ptr;
  ptr = (*ptrP) = AIR_MALLOC(1, Combo);
  ptr->str = NULL;
  if (1 != airSingleSscanf(str, "%lf", &(ptr->val))) {
    sprintf(err, "didn't parse a double from %s", str);
    return (free(ptr), 1);
  }
  ptr->str = airStrdup(str);
  return 0;
}

Combo *
combo_free(void *_ptr) {
  Combo *ptr = _ptr;
  if (ptr) {
    free(ptr->str);
    free(ptr);
  }
  return NULL;
}
AIR_MOPPER(combo_free);

hestCB comboCB = {sizeof(Combo *), "combo", combo_parse, combo_free_mop};
hestCB posCB_destroy = {2 * sizeof(double), "position", pos_parse, combo_free_mop};

void
combo_print(FILE *file, Combo *co) {
  fprintf(file, "\"%s\"->", co->str);
  airSinglePrintf(file, NULL, 0, "%lg", co->val);
  fflush(file);
  return;
}

int
main(int argc, const char **argv) {
  TH_SANITY;
  hestOpt *hopt = NULL;
  // mopParm means using the "self-mopping" hparm for hestParseOrExit
  int selfMop = !!getenv("TH_SELFMOP");
  hestParm *hparm = selfMop ? hestParmMopNew() : hestParmNew();
  /* noArgsIsNoProblem stays false: all options are required anyway
     (GLK: actually that's moot because it only matters for hestParseOrExit) */
  hparm->respectDashDashHelp = AIR_TRUE;

  int A[2];
  hestOptAdd_2_Int(&hopt, NULL, "A0 A1", A, NULL, "two required ints");
  char *B = NULL;
  unsigned int sopi = hestOptAdd_1_String(&hopt, NULL, "B", &B, NULL, "required string");
  double C[2];
  // hestOptAdd_1_Float(&hopt, NULL, "C", &C, NULL, "required float");
  // using old hestOptAdd for coverage
  unsigned int popi = hestOptAdd(&hopt, NULL, "C", airTypeOther, 1, 1, C, NULL,
                                 "required x,y", NULL, NULL, &posCB);
  /* yes, this is a flagged option in "th_unflagged_fixed", but that is so that
  this option can exist quietly with parsing from its default, while also being
  an opportunity for more coverage testing */
  Combo *Q;
  unsigned int qopi = hestOptAdd_1_Other(&hopt, "Q", "combo", &Q, "pi",
                                         /* newline forces default onto its own line */
                                         "optional combo\n more info on new line",
                                         &comboCB);
  /* yes, a single variadic option in "th_unflagged_fixed"; same story as above */
  int vari;
  unsigned int vopi = hestOptAdd_1v_Int(&hopt, "V", "vari", &vari, "0", "sample kind=4");
  int breakPOE = 0;
  th_getenvInt(&breakPOE, "TH_BREAK_POE");
  if (1 == breakPOE) {
    hestParseOrExit(NULL, argc - 1, argv + 1, hparm, "th_unflagged_fixed", info);
  } else if (2 == breakPOE) {
    hestParseOrExit(hopt, argc - 1, NULL, hparm, "th_unflagged_fixed", info);
  } else if (3 == breakPOE) {
    hestParseOrExit(hopt, argc - 1, argv + 1, hparm, NULL, info);
  }
  if (getenv("TH_THEATER")) {
    fprintf(stderr, "%s: coverage theater begins ...\n", argv[0]);
    /* "theater" because we're doing this entirely to increase coverage,
       mostly without actually checking for correctness (but going through this
       did reveal at least one harmless bug in hest code) */
    hestOpt *hoptStr = hopt + sopi;
    hestOpt *hoptPos = hopt + popi;
    hestOpt *hoptCom = hopt + qopi;
    hest__PPair ppair;
    ppair.hopt = hoptStr;
    for (unsigned int ti = 1; ti <= MY_HEST_TYPE_MAX; ti++) {
      double buff[10];
      // all the different ways parsing can't start
      hest__ParseSingle[ti](buff, "bogus", NULL);
      hest__ParseSingle[ti](buff, NULL, &ppair);
      hest__ParseSingle[ti](NULL, "bogus", &ppair);
    }
    fprintf(stderr, "%s: ... coverage theater continues 0 ...\n", argv[0]);
    // exercising the many helpful error messages in methodsHest.c:hest__OPCheck()
    // (these will dump to stderr)
    // NULL hopt or hparm
    hestOptCheck(NULL, NULL);
    hestOptAdd(NULL, "flag", "name", airTypeInt, 1, 1, NULL, "default", "info");
    hestOptAdd_nva(NULL, "flag", "name", airTypeInt, 1, 1, NULL, "default", "info", NULL,
                   NULL, NULL);
    hest__optSingleSet(NULL, "flag", "name", airTypeInt, 1, 1, NULL, "default", "info",
                       NULL, NULL, NULL);
    hestOptFree(NULL);
    hestParmCopy(NULL);
    hestParmMopDone(NULL);
    hestParmUnmop(NULL);
    fprintf(stderr, "%s: ... coverage theater continues 1 ...\n", argv[0]);
    // clang-format off
    // invalid type
    hoptStr->type += 100;     hestOptCheck(hopt, NULL);
    hoptStr->type -= 200;     hestOptCheck(hopt, NULL);
    hoptStr->type += 100; // back to where we started
    // NULL value pointer
    char *saveStr = hoptStr->valueP;
    hoptStr->valueP = NULL;   hestOptCheck(hopt, NULL);   hoptStr->valueP = saveStr;
    // too-long flag
    saveStr = hoptStr->flag; // save flag
    char longflag[] = ("this is way too long to be a reasonable flag. "
                       "this is way too long to be a reasonable flag. "
                       "this is way too long to be a reasonable flag. "
                       "this is way too long to be a reasonable flag. "
                       "this is way too long to be a reasonable flag. ");
    hoptStr->flag = airStrdup(longflag); hestOptCheck(hopt, NULL); free(hoptStr->flag);
    // flag with non-printing character
    hoptStr->flag = airStrdup("shortflag");
    hoptStr->flag[4] += 100;             hestOptCheck(hopt, NULL);
    // flag with space character
    hoptStr->flag[4] = ' ';              hestOptCheck(hopt, NULL); free(hoptStr->flag);
    // zero-length
    hoptStr->flag = airStrdup("");       hestOptCheck(hopt, NULL); free(hoptStr->flag);
    // zero-length long form
    hoptStr->flag = airStrdup("ss,");    hestOptCheck(hopt, NULL); free(hoptStr->flag);
    // zero-length short form
    hoptStr->flag = airStrdup(",ll");    hestOptCheck(hopt, NULL); free(hoptStr->flag);
    // too many separators
    hoptStr->flag = airStrdup("s,l,");   hestOptCheck(hopt, NULL); free(hoptStr->flag);
    // long form starts with '-'
    hoptStr->flag = airStrdup("s,-l");   hestOptCheck(hopt, NULL); free(hoptStr->flag);
    // long form would act like --help
    hoptStr->flag = airStrdup("s,help"); hestOptParmCheck(hopt, hparm, NULL); free(hoptStr->flag);
    // (short) flag would look like comment
    hoptStr->flag = airStrdup("{");      hestOptParmCheck(hopt, hparm, NULL); free(hoptStr->flag);
    // (short) flag would look like -.
    hoptStr->flag = airStrdup(".");      hestOptParmCheck(hopt, hparm, NULL); free(hoptStr->flag);
    // (short) flag would look like negative number
    hoptStr->flag = airStrdup("8");      hestOptParmCheck(hopt, hparm, NULL); free(hoptStr->flag);
    // (long) flag would look like -.
    hoptStr->flag = airStrdup("f,.");    hestOptParmCheck(hopt, hparm, NULL); free(hoptStr->flag);
    // (long) flag would look like negative number
    hoptStr->flag = airStrdup("f,8");    hestOptParmCheck(hopt, hparm, NULL); free(hoptStr->flag);
    // (long) flag would look like -{
    hoptStr->flag = airStrdup("f,}");    hestOptParmCheck(hopt, hparm, NULL); free(hoptStr->flag);
    // (long) flag would look like -}
    hoptStr->flag = airStrdup("f,}");    hestOptParmCheck(hopt, hparm, NULL); /* keep it for the next thing */
    // again with own error message buffer
    char *myerr = NULL;
    hestOptParmCheck(hopt, hparm, &myerr);
    fprintf(stderr, "%s: optparmcheck fail: myerr=%p:\n%s\n", argv[0], (void*)myerr, myerr);
    myerr = airFree(myerr); free(hoptStr->flag); // cleanup bogus flag
    // restore flag
    hoptStr->flag = saveStr;
    // non-NULL sawP for something not variadic
    hoptStr->sawP = &sopi;    hestOptCheck(hopt, NULL);  hoptStr->sawP = NULL;
    // non-NULL enm for something not an enum
    hoptStr->enm = airEndian; hestOptCheck(hopt, NULL);  hoptStr->enm = NULL;
    // same but for kind=4
    hopt[vopi].enm = airEndian; hestOptCheck(hopt, NULL);  hopt[vopi].enm = NULL;
    // handling "other"
    const hestCB *cbSave = hoptPos->CB;
    // non-NULL hestCB for something not an other
    hoptStr->CB = cbSave; hestOptCheck(hopt, NULL);  hoptStr->CB = NULL;
    // busted hestCB w/ 0 size
    hoptPos->CB = &posCB_size0; hestOptCheck(hopt, NULL);
    // busted hestCB w/ NULL type
    hoptPos->CB = &posCB_type0; hestOptCheck(hopt, NULL);
    // busted hestCB w/ NULL parser
    hoptPos->CB = &posCB_parse0; hestOptCheck(hopt, NULL);
    // busted hestCB w/ unexpected destroy CB
    hoptPos->CB = &posCB_destroy; hestOptCheck(hopt, NULL);
    hoptPos->CB = cbSave; // restore original hestCB
    // hestOptCheck and hestOptParmCheck with no problems
    hestOptCheck(hopt, NULL);
    hestOptCheck(hopt, &myerr);
    hestOptParmCheck(hopt, hparm, NULL);
    hestOptParmCheck(hopt, hparm, &myerr);
    // clang-format on
    fprintf(stderr, "%s: ... end coverage theater\n\n", argv[0]);
    // if (!selfMop) hestParmUnmop(hparm); // forgot where this should go
  }
  if (selfMop) {
    if (getenv("TH_QUIET_QUIT")) {
      hparm->quietQuit = quietQuitTester;
    }
    if (4 == breakPOE) {
      // break something in hestOpt so hestParseOrExit fails
      hopt[sopi].type += 100;
    }
    hestParseOrExit(hopt, argc - 1, argv + 1, hparm, "th_unflagged_fixed", info);
  } else {
    TH_PARSE(hopt, info, hparm, argc - 1, argv + 1);
  }

  printf("A=[%d;%d;]\n", A[0], A[1]);
  THPRINT_SOURCE("A", hopt, 0);
  printf("B=%s\n", B);
  THPRINT_SOURCE("B", hopt, 1);
  printf("C=");
  pos_print(stdout, C);
  printf("\n");
  // for human use, not actually assessed by coverage testing
  printf("Q=");
  combo_print(stdout, Q);
  printf("\n");

  TH_CLEANUP(hopt, hparm);
  return 0;
}
