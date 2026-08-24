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

// th_kind5.c: AUTOMATICALLY GENERATED; EDITING POINTLESS; SEE gen_kind.py
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
  int *bb;
  unsigned int bbNum;
  const char *BBflag = getenv("TH_BB_FLAG");
  const char *BBdefault = getenv("TH_BB_DEFAULT");
  unsigned int BBvmin = 1;
  th_getenvUInt(&BBvmin, "TH_BB_VMIN");
  int BBvmax = 3;
  th_getenvInt(&BBvmax, "TH_BB_VMAX");
  hestOptAdd_Nv_Bool(&hopt, BBflag ? "bb" : NULL, "bool", BBvmin, BBvmax, &bb, BBdefault,
                     "some number of bool", &bbNum);
  short *sh;
  unsigned int shNum;
  const char *SHflag = getenv("TH_SH_FLAG");
  const char *SHdefault = getenv("TH_SH_DEFAULT");
  unsigned int SHvmin = 1;
  th_getenvUInt(&SHvmin, "TH_SH_VMIN");
  int SHvmax = 3;
  th_getenvInt(&SHvmax, "TH_SH_VMAX");
  hestOptAdd_Nv_Short(&hopt, SHflag ? "sh" : NULL, "short", SHvmin, SHvmax, &sh,
                      SHdefault, "some number of short", &shNum);
  unsigned short *uh;
  unsigned int uhNum;
  const char *UHflag = getenv("TH_UH_FLAG");
  const char *UHdefault = getenv("TH_UH_DEFAULT");
  unsigned int UHvmin = 1;
  th_getenvUInt(&UHvmin, "TH_UH_VMIN");
  int UHvmax = 3;
  th_getenvInt(&UHvmax, "TH_UH_VMAX");
  hestOptAdd_Nv_UShort(&hopt, UHflag ? "uh" : NULL, "ushort", UHvmin, UHvmax, &uh,
                       UHdefault, "some number of ushort", &uhNum);
  int *si;
  unsigned int siNum;
  const char *SIflag = getenv("TH_SI_FLAG");
  const char *SIdefault = getenv("TH_SI_DEFAULT");
  unsigned int SIvmin = 1;
  th_getenvUInt(&SIvmin, "TH_SI_VMIN");
  int SIvmax = 3;
  th_getenvInt(&SIvmax, "TH_SI_VMAX");
  hestOptAdd_Nv_Int(&hopt, SIflag ? "si" : NULL, "int", SIvmin, SIvmax, &si, SIdefault,
                    "some number of int", &siNum);
  unsigned int *ui;
  unsigned int uiNum;
  const char *UIflag = getenv("TH_UI_FLAG");
  const char *UIdefault = getenv("TH_UI_DEFAULT");
  unsigned int UIvmin = 1;
  th_getenvUInt(&UIvmin, "TH_UI_VMIN");
  int UIvmax = 3;
  th_getenvInt(&UIvmax, "TH_UI_VMAX");
  hestOptAdd_Nv_UInt(&hopt, UIflag ? "ui" : NULL, "uint", UIvmin, UIvmax, &ui, UIdefault,
                     "some number of uint", &uiNum);
  long *sl;
  unsigned int slNum;
  const char *SLflag = getenv("TH_SL_FLAG");
  const char *SLdefault = getenv("TH_SL_DEFAULT");
  unsigned int SLvmin = 1;
  th_getenvUInt(&SLvmin, "TH_SL_VMIN");
  int SLvmax = 3;
  th_getenvInt(&SLvmax, "TH_SL_VMAX");
  hestOptAdd_Nv_Long(&hopt, SLflag ? "sl" : NULL, "long", SLvmin, SLvmax, &sl, SLdefault,
                     "some number of long", &slNum);
  unsigned long *ul;
  unsigned int ulNum;
  const char *ULflag = getenv("TH_UL_FLAG");
  const char *ULdefault = getenv("TH_UL_DEFAULT");
  unsigned int ULvmin = 1;
  th_getenvUInt(&ULvmin, "TH_UL_VMIN");
  int ULvmax = 3;
  th_getenvInt(&ULvmax, "TH_UL_VMAX");
  hestOptAdd_Nv_ULong(&hopt, ULflag ? "ul" : NULL, "ulong", ULvmin, ULvmax, &ul,
                      ULdefault, "some number of ulong", &ulNum);
  size_t *sz;
  unsigned int szNum;
  const char *SZflag = getenv("TH_SZ_FLAG");
  const char *SZdefault = getenv("TH_SZ_DEFAULT");
  unsigned int SZvmin = 1;
  th_getenvUInt(&SZvmin, "TH_SZ_VMIN");
  int SZvmax = 3;
  th_getenvInt(&SZvmax, "TH_SZ_VMAX");
  hestOptAdd_Nv_Size_t(&hopt, SZflag ? "sz" : NULL, "size_t", SZvmin, SZvmax, &sz,
                       SZdefault, "some number of size_t", &szNum);
  float *fl;
  unsigned int flNum;
  const char *FLflag = getenv("TH_FL_FLAG");
  const char *FLdefault = getenv("TH_FL_DEFAULT");
  unsigned int FLvmin = 1;
  th_getenvUInt(&FLvmin, "TH_FL_VMIN");
  int FLvmax = 3;
  th_getenvInt(&FLvmax, "TH_FL_VMAX");
  hestOptAdd_Nv_Float(&hopt, FLflag ? "fl" : NULL, "float", FLvmin, FLvmax, &fl,
                      FLdefault, "some number of float", &flNum);
  double *db;
  unsigned int dbNum;
  const char *DBflag = getenv("TH_DB_FLAG");
  const char *DBdefault = getenv("TH_DB_DEFAULT");
  unsigned int DBvmin = 1;
  th_getenvUInt(&DBvmin, "TH_DB_VMIN");
  int DBvmax = 3;
  th_getenvInt(&DBvmax, "TH_DB_VMAX");
  hestOptAdd_Nv_Double(&hopt, DBflag ? "db" : NULL, "double", DBvmin, DBvmax, &db,
                       DBdefault, "some number of double", &dbNum);
  char *ch;
  unsigned int chNum;
  const char *CHflag = getenv("TH_CH_FLAG");
  const char *CHdefault = getenv("TH_CH_DEFAULT");
  unsigned int CHvmin = 1;
  th_getenvUInt(&CHvmin, "TH_CH_VMIN");
  int CHvmax = 3;
  th_getenvInt(&CHvmax, "TH_CH_VMAX");
  hestOptAdd_Nv_Char(&hopt, CHflag ? "ch" : NULL, "char", CHvmin, CHvmax, &ch, CHdefault,
                     "some number of char", &chNum);
  char **ss;
  unsigned int ssNum;
  const char *SSflag = getenv("TH_SS_FLAG");
  const char *SSdefault = getenv("TH_SS_DEFAULT");
  unsigned int SSvmin = 1;
  th_getenvUInt(&SSvmin, "TH_SS_VMIN");
  int SSvmax = 3;
  th_getenvInt(&SSvmax, "TH_SS_VMAX");
  hestOptAdd_Nv_String(&hopt, SSflag ? "ss" : NULL, "string", SSvmin, SSvmax, &ss,
                       SSdefault, "some number of string", &ssNum);
  int *ee;
  unsigned int eeNum;
  const char *EEflag = getenv("TH_EE_FLAG");
  const char *EEdefault = getenv("TH_EE_DEFAULT");
  unsigned int EEvmin = 1;
  th_getenvUInt(&EEvmin, "TH_EE_VMIN");
  int EEvmax = 3;
  th_getenvInt(&EEvmax, "TH_EE_VMAX");
  hestOptAdd_Nv_Enum(&hopt, EEflag ? "ee" : NULL, "enum", EEvmin, EEvmax, &ee, EEdefault,
                     "some number of enum", &eeNum, airEndian);
  double *xy;
  unsigned int xyNum;
  const char *XYflag = getenv("TH_XY_FLAG");
  const char *XYdefault = getenv("TH_XY_DEFAULT");
  unsigned int XYvmin = 1;
  th_getenvUInt(&XYvmin, "TH_XY_VMIN");
  int XYvmax = 3;
  th_getenvInt(&XYvmax, "TH_XY_VMAX");
  hestOptAdd_Nv_Other(&hopt, XYflag ? "xy" : NULL, "ostat", XYvmin, XYvmax, &xy,
                      XYdefault, "some number of ostat", &xyNum, &posCB);
  Combo **co;
  unsigned int coNum;
  const char *COflag = getenv("TH_CO_FLAG");
  const char *COdefault = getenv("TH_CO_DEFAULT");
  unsigned int COvmin = 1;
  th_getenvUInt(&COvmin, "TH_CO_VMIN");
  int COvmax = 3;
  th_getenvInt(&COvmax, "TH_CO_VMAX");
  hestOptAdd_Nv_Other(&hopt, COflag ? "co" : NULL, "odyn", COvmin, COvmax, &co,
                      COdefault, "some number of odyn", &coNum, &comboCB);

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
