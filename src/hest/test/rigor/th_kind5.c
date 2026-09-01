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
     setenv TH_TY_FLAG (to anything, even empty string) for non-NULL 2-letter flag"TY".
     setenv TH_TY_DEFAULT to the default string (else is NULL == no default) */
  int *BB;
  unsigned int BBNum;
  const char *BBflag = getenv("TH_BB_FLAG");
  const char *BBdefault = getenv("TH_BB_DEFAULT");
  unsigned int BBvmin = 1;
  th_getenvUInt(&BBvmin, "TH_BB_VMIN");
  int BBvmax = 3;
  th_getenvInt(&BBvmax, "TH_BB_VMAX");
  unsigned int BBopidx = hestOptAdd_Nv_Bool(&hopt, BBflag ? "BB" : NULL, "bool", BBvmin,
                                            BBvmax, &BB, BBdefault,
                                            "some number of bool", &BBNum);
  short *SH;
  unsigned int SHNum;
  const char *SHflag = getenv("TH_SH_FLAG");
  const char *SHdefault = getenv("TH_SH_DEFAULT");
  unsigned int SHvmin = 1;
  th_getenvUInt(&SHvmin, "TH_SH_VMIN");
  int SHvmax = 3;
  th_getenvInt(&SHvmax, "TH_SH_VMAX");
  unsigned int SHopidx = hestOptAdd_Nv_Short(&hopt, SHflag ? "SH" : NULL, "short",
                                             SHvmin, SHvmax, &SH, SHdefault,
                                             "some number of short", &SHNum);
  unsigned short *UH;
  unsigned int UHNum;
  const char *UHflag = getenv("TH_UH_FLAG");
  const char *UHdefault = getenv("TH_UH_DEFAULT");
  unsigned int UHvmin = 1;
  th_getenvUInt(&UHvmin, "TH_UH_VMIN");
  int UHvmax = 3;
  th_getenvInt(&UHvmax, "TH_UH_VMAX");
  unsigned int UHopidx = hestOptAdd_Nv_UShort(&hopt, UHflag ? "UH" : NULL, "ushort",
                                              UHvmin, UHvmax, &UH, UHdefault,
                                              "some number of ushort", &UHNum);
  int *SI;
  unsigned int SINum;
  const char *SIflag = getenv("TH_SI_FLAG");
  const char *SIdefault = getenv("TH_SI_DEFAULT");
  unsigned int SIvmin = 1;
  th_getenvUInt(&SIvmin, "TH_SI_VMIN");
  int SIvmax = 3;
  th_getenvInt(&SIvmax, "TH_SI_VMAX");
  unsigned int SIopidx = hestOptAdd_Nv_Int(&hopt, SIflag ? "SI" : NULL, "int", SIvmin,
                                           SIvmax, &SI, SIdefault, "some number of int",
                                           &SINum);
  unsigned int *UI;
  unsigned int UINum;
  const char *UIflag = getenv("TH_UI_FLAG");
  const char *UIdefault = getenv("TH_UI_DEFAULT");
  unsigned int UIvmin = 1;
  th_getenvUInt(&UIvmin, "TH_UI_VMIN");
  int UIvmax = 3;
  th_getenvInt(&UIvmax, "TH_UI_VMAX");
  unsigned int UIopidx = hestOptAdd_Nv_UInt(&hopt, UIflag ? "UI" : NULL, "uint", UIvmin,
                                            UIvmax, &UI, UIdefault,
                                            "some number of uint", &UINum);
  long *SL;
  unsigned int SLNum;
  const char *SLflag = getenv("TH_SL_FLAG");
  const char *SLdefault = getenv("TH_SL_DEFAULT");
  unsigned int SLvmin = 1;
  th_getenvUInt(&SLvmin, "TH_SL_VMIN");
  int SLvmax = 3;
  th_getenvInt(&SLvmax, "TH_SL_VMAX");
  unsigned int SLopidx = hestOptAdd_Nv_Long(&hopt, SLflag ? "SL" : NULL, "long", SLvmin,
                                            SLvmax, &SL, SLdefault,
                                            "some number of long", &SLNum);
  unsigned long *UL;
  unsigned int ULNum;
  const char *ULflag = getenv("TH_UL_FLAG");
  const char *ULdefault = getenv("TH_UL_DEFAULT");
  unsigned int ULvmin = 1;
  th_getenvUInt(&ULvmin, "TH_UL_VMIN");
  int ULvmax = 3;
  th_getenvInt(&ULvmax, "TH_UL_VMAX");
  unsigned int ULopidx = hestOptAdd_Nv_ULong(&hopt, ULflag ? "UL" : NULL, "ulong",
                                             ULvmin, ULvmax, &UL, ULdefault,
                                             "some number of ulong", &ULNum);
  size_t *SZ;
  unsigned int SZNum;
  const char *SZflag = getenv("TH_SZ_FLAG");
  const char *SZdefault = getenv("TH_SZ_DEFAULT");
  unsigned int SZvmin = 1;
  th_getenvUInt(&SZvmin, "TH_SZ_VMIN");
  int SZvmax = 3;
  th_getenvInt(&SZvmax, "TH_SZ_VMAX");
  unsigned int SZopidx = hestOptAdd_Nv_Size_t(&hopt, SZflag ? "SZ" : NULL, "size_t",
                                              SZvmin, SZvmax, &SZ, SZdefault,
                                              "some number of size_t", &SZNum);
  float *FL;
  unsigned int FLNum;
  const char *FLflag = getenv("TH_FL_FLAG");
  const char *FLdefault = getenv("TH_FL_DEFAULT");
  unsigned int FLvmin = 1;
  th_getenvUInt(&FLvmin, "TH_FL_VMIN");
  int FLvmax = 3;
  th_getenvInt(&FLvmax, "TH_FL_VMAX");
  unsigned int FLopidx = hestOptAdd_Nv_Float(&hopt, FLflag ? "FL" : NULL, "float",
                                             FLvmin, FLvmax, &FL, FLdefault,
                                             "some number of float", &FLNum);
  double *DB;
  unsigned int DBNum;
  const char *DBflag = getenv("TH_DB_FLAG");
  const char *DBdefault = getenv("TH_DB_DEFAULT");
  unsigned int DBvmin = 1;
  th_getenvUInt(&DBvmin, "TH_DB_VMIN");
  int DBvmax = 3;
  th_getenvInt(&DBvmax, "TH_DB_VMAX");
  unsigned int DBopidx = hestOptAdd_Nv_Double(&hopt, DBflag ? "DB" : NULL, "double",
                                              DBvmin, DBvmax, &DB, DBdefault,
                                              "some number of double", &DBNum);
  char *CH;
  unsigned int CHNum;
  const char *CHflag = getenv("TH_CH_FLAG");
  const char *CHdefault = getenv("TH_CH_DEFAULT");
  unsigned int CHvmin = 1;
  th_getenvUInt(&CHvmin, "TH_CH_VMIN");
  int CHvmax = 3;
  th_getenvInt(&CHvmax, "TH_CH_VMAX");
  unsigned int CHopidx = hestOptAdd_Nv_Char(&hopt, CHflag ? "CH" : NULL, "char", CHvmin,
                                            CHvmax, &CH, CHdefault,
                                            "some number of char", &CHNum);
  char **SS;
  unsigned int SSNum;
  const char *SSflag = getenv("TH_SS_FLAG");
  const char *SSdefault = getenv("TH_SS_DEFAULT");
  unsigned int SSvmin = 1;
  th_getenvUInt(&SSvmin, "TH_SS_VMIN");
  int SSvmax = 3;
  th_getenvInt(&SSvmax, "TH_SS_VMAX");
  unsigned int SSopidx = hestOptAdd_Nv_String(&hopt, SSflag ? "SS" : NULL, "string",
                                              SSvmin, SSvmax, &SS, SSdefault,
                                              "some number of string", &SSNum);
  int *EE;
  unsigned int EENum;
  const char *EEflag = getenv("TH_EE_FLAG");
  const char *EEdefault = getenv("TH_EE_DEFAULT");
  unsigned int EEvmin = 1;
  th_getenvUInt(&EEvmin, "TH_EE_VMIN");
  int EEvmax = 3;
  th_getenvInt(&EEvmax, "TH_EE_VMAX");
  unsigned int EEopidx = hestOptAdd_Nv_Enum(&hopt, EEflag ? "EE" : NULL, "enum", EEvmin,
                                            EEvmax, &EE, EEdefault,
                                            "some number of enum", &EENum, airEndian);
  double *XY;
  unsigned int XYNum;
  const char *XYflag = getenv("TH_XY_FLAG");
  const char *XYdefault = getenv("TH_XY_DEFAULT");
  unsigned int XYvmin = 1;
  th_getenvUInt(&XYvmin, "TH_XY_VMIN");
  int XYvmax = 3;
  th_getenvInt(&XYvmax, "TH_XY_VMAX");
  unsigned int XYopidx = hestOptAdd_Nv_Other(&hopt, XYflag ? "XY" : NULL, "ostat",
                                             XYvmin, XYvmax, &XY, XYdefault,
                                             "some number of ostat", &XYNum, &posCB);
  Combo **CO;
  unsigned int CONum;
  const char *COflag = getenv("TH_CO_FLAG");
  const char *COdefault = getenv("TH_CO_DEFAULT");
  unsigned int COvmin = 1;
  th_getenvUInt(&COvmin, "TH_CO_VMIN");
  int COvmax = 3;
  th_getenvInt(&COvmax, "TH_CO_VMAX");
  unsigned int COopidx = hestOptAdd_Nv_Other(&hopt, COflag ? "CO" : NULL, "odyn", COvmin,
                                             COvmax, &CO, COdefault,
                                             "some number of odyn", &CONum, &comboCB);

  int pret = hestParse(hopt, argc - 1, argv + 1, NULL, hparm);
  // fprintf(stderr, "!%s: hestParse returned pret=%d\n", argv[0], pret);
  if (pret) {
    // have already fprintf'd to stderr
    hestParmFree(hparm);
    hestOptFree(hopt);
    exit(1);
  }
  if (hopt->helpWanted) {
    hestUsage(stdout, hopt, argv[0], hparm, AIR_TRUE /* oneline */);
    hestUsage(stdout, hopt, argv[0], hparm, AIR_FALSE /* oneline */);
    if (2 != hopt->helpWanted) hestGlossary(stdout, hopt, hparm);
    hestParmFree(hparm);
    hestOptFree(hopt);
    exit(0);
  }

  /* print what was learned from hest. Unfortunately, the means of generating this code
  was simplistic, and the following output is not any kind of good example of
  human-written C code; sorry! */
  printf("BB=");
  printf("[");
  for (unsigned int vi = 0; vi < BBNum; vi++) {
    if (vi) printf("|");
    printf("%s", airEnumStr(airBool, BB[vi]));
  }
  printf("]");
  printf("\n");
  THPRINT_SOURCE("BB", hopt, BBopidx);
  printf("SH=");
  printf("[");
  for (unsigned int vi = 0; vi < SHNum; vi++) {
    if (vi) printf("|");
    printf("%hd", SH[vi]);
  }
  printf("]");
  printf("\n");
  THPRINT_SOURCE("SH", hopt, SHopidx);
  printf("UH=");
  printf("[");
  for (unsigned int vi = 0; vi < UHNum; vi++) {
    if (vi) printf("|");
    printf("%hu", UH[vi]);
  }
  printf("]");
  printf("\n");
  THPRINT_SOURCE("UH", hopt, UHopidx);
  printf("SI=");
  printf("[");
  for (unsigned int vi = 0; vi < SINum; vi++) {
    if (vi) printf("|");
    printf("%d", SI[vi]);
  }
  printf("]");
  printf("\n");
  THPRINT_SOURCE("SI", hopt, SIopidx);
  printf("UI=");
  printf("[");
  for (unsigned int vi = 0; vi < UINum; vi++) {
    if (vi) printf("|");
    printf("%u", UI[vi]);
  }
  printf("]");
  printf("\n");
  THPRINT_SOURCE("UI", hopt, UIopidx);
  printf("SL=");
  printf("[");
  for (unsigned int vi = 0; vi < SLNum; vi++) {
    if (vi) printf("|");
    printf("%ld", SL[vi]);
  }
  printf("]");
  printf("\n");
  THPRINT_SOURCE("SL", hopt, SLopidx);
  printf("UL=");
  printf("[");
  for (unsigned int vi = 0; vi < ULNum; vi++) {
    if (vi) printf("|");
    printf("%lu", UL[vi]);
  }
  printf("]");
  printf("\n");
  THPRINT_SOURCE("UL", hopt, ULopidx);
  printf("SZ=");
  printf("[");
  for (unsigned int vi = 0; vi < SZNum; vi++) {
    if (vi) printf("|");
    printf("%zu", SZ[vi]);
  }
  printf("]");
  printf("\n");
  THPRINT_SOURCE("SZ", hopt, SZopidx);
  printf("FL=");
  printf("[");
  for (unsigned int vi = 0; vi < FLNum; vi++) {
    if (vi) printf("|");
    airSinglePrintf(stdout, NULL, 0, "%g", FL[vi]);
  }
  printf("]");
  printf("\n");
  THPRINT_SOURCE("FL", hopt, FLopidx);
  printf("DB=");
  printf("[");
  for (unsigned int vi = 0; vi < DBNum; vi++) {
    if (vi) printf("|");
    airSinglePrintf(stdout, NULL, 0, "%lg", DB[vi]);
  }
  printf("]");
  printf("\n");
  THPRINT_SOURCE("DB", hopt, DBopidx);
  printf("CH=");
  printf("[");
  for (unsigned int vi = 0; vi < CHNum; vi++) {
    if (vi) printf("|");
    printf("%c", CH[vi]);
  }
  printf("]");
  printf("\n");
  THPRINT_SOURCE("CH", hopt, CHopidx);
  printf("SS=");
  printf("[");
  for (unsigned int vi = 0; vi < SSNum; vi++) {
    if (vi) printf("|");
    printf("%s", SS[vi]);
  }
  printf("]");
  printf("\n");
  THPRINT_SOURCE("SS", hopt, SSopidx);
  printf("EE=");
  printf("[");
  for (unsigned int vi = 0; vi < EENum; vi++) {
    if (vi) printf("|");
    printf("%s", airEnumStr(airEndian, EE[vi]));
  }
  printf("]");
  printf("\n");
  THPRINT_SOURCE("EE", hopt, EEopidx);
  printf("XY=");
  printf("[");
  for (unsigned int vi = 0; vi < XYNum; vi++) {
    if (vi) printf("|");
    pos_print(stdout, XY + (2 * vi));
  }
  printf("]");
  printf("\n");
  THPRINT_SOURCE("XY", hopt, XYopidx);
  printf("CO=");
  printf("[");
  for (unsigned int vi = 0; vi < CONum; vi++) {
    if (vi) printf("|");
    combo_print(stdout, CO[vi]);
  }
  printf("]");
  printf("\n");
  THPRINT_SOURCE("CO", hopt, COopidx);

  /* free the memory allocated by parsing ... */
  hestParseFree(hopt);
  /* ... and the other stuff */
  hopt = hestOptFree(hopt);
  hparm = hestParmFree(hparm);
  exit(0);
}
