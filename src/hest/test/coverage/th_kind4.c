/*
  Teem: Tools to process and visualize scientific data and images
  Copyright (C) 2009--2026  University of Chicago
  Copyright (C) 2005--2008  Gordon Kindlmann
  Copyright (C) 1998--2004  University of Utah

  This library is free software; you can redistribute it and/or modify it under the terms
  of the GNU Lesser General Public License (LGPL) as published by the Free Software
  Foundation; either version 2.1 of the License, or (at your option) any later version.
  The terms of redistributing and/or modifying this software also include exceptions to
  the LGPL that facilitate static linking.

  This library is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
  PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public License
  along with this library; if not, see <https://www.gnu.org/licenses/>.
*/

// th_kind4.c: AUTOMATICALLY GENERATED; EDITING POINTLESS; SEE gen_kind.py
// Tests all types for kind=_KI_ hestOpts

#include <teem/hest.h>

#include "thcommon.h"

// -------------- "pos": an "other" that doesn't do its own allocation = "ostat"

static int
pos_parse(void *_ptr, const char *str, char *err) {
  double *ptr;
  int ret;

  ptr = _ptr;
  ret = sscanf(str, "%lf,%lf", ptr + 0, ptr + 1);
  if (2 != ret) {
    sprintf(err, "parsed %d (not 2) doubles", ret);
    return 1;
  }
  return 0;
}

// (usually hest-using libraries make available a const pointer to this struct available)
hestCB posCB = {2 * sizeof(double), "position", pos_parse, NULL};

void
pos_print(FILE *file, const double pos[2]) {
  fprintf(file, "[");
  airSinglePrintf(file, NULL, 0, "%lg", pos[0]);
  fprintf(file, ",");
  airSinglePrintf(file, NULL, 0, "%lg", pos[1]);
  fprintf(file, "]");
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
  /*
  printf("%s: ptrP = %p  ---malloc-->  ptr = *ptrP = %p\n", __func__, AIR_VOIDP(ptrP),
         AIR_VOIDP(*ptrP));
         */
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

void
combo_print(FILE *file, Combo *co) {
  fprintf(file, "\"%s\"->", co->str);
  airSinglePrintf(file, NULL, 0, "%lg", co->val);
  fflush(file);
  return;
}

int
main(int argc, const char **argv) {
  airArray *mop = airMopNew();
  hestParm *hparm = hestParmNew();
  // HEY need to figure out mopping with and without hestParseOrExit
  airMopAdd(mop, hparm, hestParmFree_mop, airMopAlways);
  hparm->respectDashDashHelp = AIR_TRUE;
  /* HEY only for hestParseOrExit but should vary this */
  hparm->noArgsIsNoProblem = AIR_TRUE;
  unsigned int ncol = 70;
  // if TH_COLUMNS is set, it will over-ride the (small!) default above
  th_getenvUInt(&ncol, "TH_COLUMNS");
  // if ioctl() fails (and testing will force that, once), use ncol
  hestParmColumnsIoctl(hparm, ncol);
  hparm->usageDefaultDisplayAvoidsWrapping = AIR_FALSE; // HEY should vary this
  hparm->verbose = 0;                                   // HEY should vary this

  hestOpt *hopt = NULL;
  /* for each of two-letter TY below:
     setenv TH_TY_FLAG (to anything, even empty string) to use a non-NULL two-flag "ty".
     setenv TH_TY_DEFAULT to the default string (else is NULL == no default) */
  int bb;
  const char *BBflag = getenv("TH_BB_FLAG");
  const char *BBdefault = getenv("TH_BB_DEFAULT");
  hestOptAdd_1v_Bool(&hopt, BBflag ? "bb" : NULL, "bool", &bb, BBdefault,
                     "exactly 1 bool");
  short sh;
  const char *SHflag = getenv("TH_SH_FLAG");
  const char *SHdefault = getenv("TH_SH_DEFAULT");
  hestOptAdd_1v_Short(&hopt, SHflag ? "sh" : NULL, "short", &sh, SHdefault,
                      "exactly 1 short");
  unsigned short uh;
  const char *UHflag = getenv("TH_UH_FLAG");
  const char *UHdefault = getenv("TH_UH_DEFAULT");
  hestOptAdd_1v_UShort(&hopt, UHflag ? "uh" : NULL, "ushort", &uh, UHdefault,
                       "exactly 1 ushort");
  int si;
  const char *SIflag = getenv("TH_SI_FLAG");
  const char *SIdefault = getenv("TH_SI_DEFAULT");
  hestOptAdd_1v_Int(&hopt, SIflag ? "si" : NULL, "int", &si, SIdefault, "exactly 1 int");
  unsigned int ui;
  const char *UIflag = getenv("TH_UI_FLAG");
  const char *UIdefault = getenv("TH_UI_DEFAULT");
  hestOptAdd_1v_UInt(&hopt, UIflag ? "ui" : NULL, "uint", &ui, UIdefault,
                     "exactly 1 uint");
  long sl;
  const char *SLflag = getenv("TH_SL_FLAG");
  const char *SLdefault = getenv("TH_SL_DEFAULT");
  hestOptAdd_1v_Long(&hopt, SLflag ? "sl" : NULL, "long", &sl, SLdefault,
                     "exactly 1 long");
  unsigned long ul;
  const char *ULflag = getenv("TH_UL_FLAG");
  const char *ULdefault = getenv("TH_UL_DEFAULT");
  hestOptAdd_1v_ULong(&hopt, ULflag ? "ul" : NULL, "ulong", &ul, ULdefault,
                      "exactly 1 ulong");
  size_t sz;
  const char *SZflag = getenv("TH_SZ_FLAG");
  const char *SZdefault = getenv("TH_SZ_DEFAULT");
  hestOptAdd_1v_Size_t(&hopt, SZflag ? "sz" : NULL, "size_t", &sz, SZdefault,
                       "exactly 1 size_t");
  float fl;
  const char *FLflag = getenv("TH_FL_FLAG");
  const char *FLdefault = getenv("TH_FL_DEFAULT");
  hestOptAdd_1v_Float(&hopt, FLflag ? "fl" : NULL, "float", &fl, FLdefault,
                      "exactly 1 float");
  double db;
  const char *DBflag = getenv("TH_DB_FLAG");
  const char *DBdefault = getenv("TH_DB_DEFAULT");
  hestOptAdd_1v_Double(&hopt, DBflag ? "db" : NULL, "double", &db, DBdefault,
                       "exactly 1 double");
  char ch;
  const char *CHflag = getenv("TH_CH_FLAG");
  const char *CHdefault = getenv("TH_CH_DEFAULT");
  hestOptAdd_1v_Char(&hopt, CHflag ? "ch" : NULL, "char", &ch, CHdefault,
                     "exactly 1 char");
  char *ss;
  const char *SSflag = getenv("TH_SS_FLAG");
  const char *SSdefault = getenv("TH_SS_DEFAULT");
  hestOptAdd_1v_String(&hopt, SSflag ? "ss" : NULL, "string", &ss, SSdefault,
                       "exactly 1 string");
  int ee;
  const char *EEflag = getenv("TH_EE_FLAG");
  const char *EEdefault = getenv("TH_EE_DEFAULT");
  hestOptAdd_1v_Enum(&hopt, EEflag ? "ee" : NULL, "enum", &ee, EEdefault,
                     "exactly 1 enum", airEndian);
  double xy[2];
  const char *XYflag = getenv("TH_XY_FLAG");
  const char *XYdefault = getenv("TH_XY_DEFAULT");
  hestOptAdd_1v_Other(&hopt, XYflag ? "xy" : NULL, "ostat", &xy, XYdefault,
                      "exactly 1 ostat", &posCB);
  Combo *co;
  const char *COflag = getenv("TH_CO_FLAG");
  const char *COdefault = getenv("TH_CO_DEFAULT");
  hestOptAdd_1v_Other(&hopt, COflag ? "co" : NULL, "odyn", &co, COdefault,
                      "exactly 1 odyn", &comboCB);

  if (hestParse(hopt, argc - 1, argv + 1, NULL, hparm)) {
    // have already fprintf'd to stderr
    hestParmFree(hparm);
    hestOptFree(hopt);
    exit(0);
  }
  if (hopt->helpWanted) {
    hestUsage(stdout, hopt, argv[0], hparm, AIR_TRUE /* oneline */);
    hestUsage(stdout, hopt, argv[0], hparm, AIR_FALSE /* oneline */);
    if (2 != hopt->helpWanted) hestGlossary(stdout, hopt, hparm);
    hestParmFree(hparm);
    hestOptFree(hopt);
    exit(0);
  }

  // print what was learned from hest
  // print(BB)
  // print(SH)
  // print(UH)
  // print(SI)
  // print(UI)
  // print(SL)
  // print(UL)
  // print(SZ)
  // print(FL)
  // print(DB)
  // print(CH)
  // print(SS)
  // print(EE)
  // print(XY)
  // print(CO)

  /* free the memory allocated by parsing ... */
  hestParseFree(hopt);
  /* ... and the other stuff */
  hopt = hestOptFree(hopt);
  hparm = hestParmFree(hparm);
  exit(0);
}
