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

// th_kind3_n4.c: AUTOMATICALLY GENERATED; EDITING POINTLESS; SEE gen_kind.py
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
  hparm->respectDashDashHelp = AIR_TRUE; // HEY should vary this
  /* HEY only for hestParseOrExit but should vary this */
  hparm->noArgsIsNoProblem = AIR_TRUE;
  hestParmColumnsIoctl(hparm, 80);                      // (to exercise next line)
  hparm->usageDefaultDisplayAvoidsWrapping = AIR_FALSE; // HEY should vary this
  hparm->verbose = 0;                                   // HEY should vary this

  hestOpt *hopt = NULL;
  /* for each of two-letter TY below:
     setenv TH_TY_FLAG (to anything, even empty string) to use a non-NULL two-flag "ty".
     setenv TH_TY_DEFAULT to the default string (else is NULL == no default) */
  int bb[4];
  const char *BBflag = getenv("TH_BB_FLAG");
  const char *BBdefault = getenv("TH_BB_DEFAULT");
  hestOptAdd_4_Bool(&hopt, BBflag ? "bb" : NULL, "bool0 bool1 bool2 bool3", bb,
                    BBdefault, "exactly 4 bool");
  short sh[4];
  const char *SHflag = getenv("TH_SH_FLAG");
  const char *SHdefault = getenv("TH_SH_DEFAULT");
  hestOptAdd_4_Short(&hopt, SHflag ? "sh" : NULL, "short0 short1 short2 short3", sh,
                     SHdefault, "exactly 4 short");
  unsigned short uh[4];
  const char *UHflag = getenv("TH_UH_FLAG");
  const char *UHdefault = getenv("TH_UH_DEFAULT");
  hestOptAdd_4_UShort(&hopt, UHflag ? "uh" : NULL, "ushort0 ushort1 ushort2 ushort3", uh,
                      UHdefault, "exactly 4 ushort");
  int si[4];
  const char *SIflag = getenv("TH_SI_FLAG");
  const char *SIdefault = getenv("TH_SI_DEFAULT");
  hestOptAdd_4_Int(&hopt, SIflag ? "si" : NULL, "int0 int1 int2 int3", si, SIdefault,
                   "exactly 4 int");
  unsigned int ui[4];
  const char *UIflag = getenv("TH_UI_FLAG");
  const char *UIdefault = getenv("TH_UI_DEFAULT");
  hestOptAdd_4_UInt(&hopt, UIflag ? "ui" : NULL, "uint0 uint1 uint2 uint3", ui,
                    UIdefault, "exactly 4 uint");
  long sl[4];
  const char *SLflag = getenv("TH_SL_FLAG");
  const char *SLdefault = getenv("TH_SL_DEFAULT");
  hestOptAdd_4_Long(&hopt, SLflag ? "sl" : NULL, "long0 long1 long2 long3", sl,
                    SLdefault, "exactly 4 long");
  unsigned long ul[4];
  const char *ULflag = getenv("TH_UL_FLAG");
  const char *ULdefault = getenv("TH_UL_DEFAULT");
  hestOptAdd_4_ULong(&hopt, ULflag ? "ul" : NULL, "ulong0 ulong1 ulong2 ulong3", ul,
                     ULdefault, "exactly 4 ulong");
  size_t sz[4];
  const char *SZflag = getenv("TH_SZ_FLAG");
  const char *SZdefault = getenv("TH_SZ_DEFAULT");
  hestOptAdd_4_Size_t(&hopt, SZflag ? "sz" : NULL, "size_t0 size_t1 size_t2 size_t3", sz,
                      SZdefault, "exactly 4 size_t");
  float fl[4];
  const char *FLflag = getenv("TH_FL_FLAG");
  const char *FLdefault = getenv("TH_FL_DEFAULT");
  hestOptAdd_4_Float(&hopt, FLflag ? "fl" : NULL, "float0 float1 float2 float3", fl,
                     FLdefault, "exactly 4 float");
  double db[4];
  const char *DBflag = getenv("TH_DB_FLAG");
  const char *DBdefault = getenv("TH_DB_DEFAULT");
  hestOptAdd_4_Double(&hopt, DBflag ? "db" : NULL, "double0 double1 double2 double3", db,
                      DBdefault, "exactly 4 double");
  char ch[4];
  const char *CHflag = getenv("TH_CH_FLAG");
  const char *CHdefault = getenv("TH_CH_DEFAULT");
  hestOptAdd_4_Char(&hopt, CHflag ? "ch" : NULL, "char0 char1 char2 char3", ch,
                    CHdefault, "exactly 4 char");
  char *ss[4];
  const char *SSflag = getenv("TH_SS_FLAG");
  const char *SSdefault = getenv("TH_SS_DEFAULT");
  hestOptAdd_4_String(&hopt, SSflag ? "ss" : NULL, "string0 string1 string2 string3", ss,
                      SSdefault, "exactly 4 string");
  int ee[4];
  const char *EEflag = getenv("TH_EE_FLAG");
  const char *EEdefault = getenv("TH_EE_DEFAULT");
  hestOptAdd_4_Enum(&hopt, EEflag ? "ee" : NULL, "enum0 enum1 enum2 enum3", ee,
                    EEdefault, "exactly 4 enum", airEndian);
  double xy[2][4];
  const char *XYflag = getenv("TH_XY_FLAG");
  const char *XYdefault = getenv("TH_XY_DEFAULT");
  hestOptAdd_4_Other(&hopt, XYflag ? "xy" : NULL, "ostat0 ostat1 ostat2 ostat3", xy,
                     XYdefault, "exactly 4 ostat", &posCB);
  Combo *co[4];
  const char *COflag = getenv("TH_CO_FLAG");
  const char *COdefault = getenv("TH_CO_DEFAULT");
  hestOptAdd_4_Other(&hopt, COflag ? "co" : NULL, "odyn0 odyn1 odyn2 odyn3", co,
                     COdefault, "exactly 4 odyn", &comboCB);

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
