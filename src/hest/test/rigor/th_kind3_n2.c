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

// th_kind3_n2.c: AUTOMATICALLY GENERATED; EDITING POINTLESS; SEE gen_kind.py
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
  int BB[2];
  const char *BBflag = getenv("TH_BB_FLAG");
  const char *BBdefault = getenv("TH_BB_DEFAULT");
  unsigned int BBopidx = hestOptAdd_2_Bool(&hopt, BBflag ? "BB" : NULL, "bool0 bool1",
                                           BB, BBdefault, "exactly 2 bool");
  short SH[2];
  const char *SHflag = getenv("TH_SH_FLAG");
  const char *SHdefault = getenv("TH_SH_DEFAULT");
  unsigned int SHopidx = hestOptAdd_2_Short(&hopt, SHflag ? "SH" : NULL, "short0 short1",
                                            SH, SHdefault, "exactly 2 short");
  unsigned short UH[2];
  const char *UHflag = getenv("TH_UH_FLAG");
  const char *UHdefault = getenv("TH_UH_DEFAULT");
  unsigned int UHopidx = hestOptAdd_2_UShort(&hopt, UHflag ? "UH" : NULL,
                                             "ushort0 ushort1", UH, UHdefault,
                                             "exactly 2 ushort");
  int SI[2];
  const char *SIflag = getenv("TH_SI_FLAG");
  const char *SIdefault = getenv("TH_SI_DEFAULT");
  unsigned int SIopidx = hestOptAdd_2_Int(&hopt, SIflag ? "SI" : NULL, "int0 int1", SI,
                                          SIdefault, "exactly 2 int");
  unsigned int UI[2];
  const char *UIflag = getenv("TH_UI_FLAG");
  const char *UIdefault = getenv("TH_UI_DEFAULT");
  unsigned int UIopidx = hestOptAdd_2_UInt(&hopt, UIflag ? "UI" : NULL, "uint0 uint1",
                                           UI, UIdefault, "exactly 2 uint");
  long SL[2];
  const char *SLflag = getenv("TH_SL_FLAG");
  const char *SLdefault = getenv("TH_SL_DEFAULT");
  unsigned int SLopidx = hestOptAdd_2_Long(&hopt, SLflag ? "SL" : NULL, "long0 long1",
                                           SL, SLdefault, "exactly 2 long");
  unsigned long UL[2];
  const char *ULflag = getenv("TH_UL_FLAG");
  const char *ULdefault = getenv("TH_UL_DEFAULT");
  unsigned int ULopidx = hestOptAdd_2_ULong(&hopt, ULflag ? "UL" : NULL, "ulong0 ulong1",
                                            UL, ULdefault, "exactly 2 ulong");
  size_t SZ[2];
  const char *SZflag = getenv("TH_SZ_FLAG");
  const char *SZdefault = getenv("TH_SZ_DEFAULT");
  unsigned int SZopidx = hestOptAdd_2_Size_t(&hopt, SZflag ? "SZ" : NULL,
                                             "size_t0 size_t1", SZ, SZdefault,
                                             "exactly 2 size_t");
  float FL[2];
  const char *FLflag = getenv("TH_FL_FLAG");
  const char *FLdefault = getenv("TH_FL_DEFAULT");
  unsigned int FLopidx = hestOptAdd_2_Float(&hopt, FLflag ? "FL" : NULL, "float0 float1",
                                            FL, FLdefault, "exactly 2 float");
  double DB[2];
  const char *DBflag = getenv("TH_DB_FLAG");
  const char *DBdefault = getenv("TH_DB_DEFAULT");
  unsigned int DBopidx = hestOptAdd_2_Double(&hopt, DBflag ? "DB" : NULL,
                                             "double0 double1", DB, DBdefault,
                                             "exactly 2 double");
  char CH[2];
  const char *CHflag = getenv("TH_CH_FLAG");
  const char *CHdefault = getenv("TH_CH_DEFAULT");
  unsigned int CHopidx = hestOptAdd_2_Char(&hopt, CHflag ? "CH" : NULL, "char0 char1",
                                           CH, CHdefault, "exactly 2 char");
  char *SS[2];
  const char *SSflag = getenv("TH_SS_FLAG");
  const char *SSdefault = getenv("TH_SS_DEFAULT");
  unsigned int SSopidx = hestOptAdd_2_String(&hopt, SSflag ? "SS" : NULL,
                                             "string0 string1", SS, SSdefault,
                                             "exactly 2 string");
  int EE[2];
  const char *EEflag = getenv("TH_EE_FLAG");
  const char *EEdefault = getenv("TH_EE_DEFAULT");
  unsigned int EEopidx = hestOptAdd_2_Enum(&hopt, EEflag ? "EE" : NULL, "enum0 enum1",
                                           EE, EEdefault, "exactly 2 enum", airEndian);
  double XY[4];
  const char *XYflag = getenv("TH_XY_FLAG");
  const char *XYdefault = getenv("TH_XY_DEFAULT");
  unsigned int XYopidx = hestOptAdd_2_Other(&hopt, XYflag ? "XY" : NULL, "ostat0 ostat1",
                                            XY, XYdefault, "exactly 2 ostat", &posCB);
  Combo *CO[2];
  const char *COflag = getenv("TH_CO_FLAG");
  const char *COdefault = getenv("TH_CO_DEFAULT");
  unsigned int COopidx = hestOptAdd_2_Other(&hopt, COflag ? "CO" : NULL, "odyn0 odyn1",
                                            CO, COdefault, "exactly 2 odyn", &comboCB);

  int pret = hestParse(hopt, argc - 1, argv + 1, NULL, hparm);
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
  printf("%s", airEnumStr(airBool, BB[0]));
  printf(";");
  printf("%s", airEnumStr(airBool, BB[1]));
  printf(";");
  printf("]");
  printf("\n");
  THPRINT_SOURCE("BB", hopt, BBopidx);
  printf("SH=");
  printf("[");
  printf("%hd", SH[0]);
  printf(";");
  printf("%hd", SH[1]);
  printf(";");
  printf("]");
  printf("\n");
  THPRINT_SOURCE("SH", hopt, SHopidx);
  printf("UH=");
  printf("[");
  printf("%hu", UH[0]);
  printf(";");
  printf("%hu", UH[1]);
  printf(";");
  printf("]");
  printf("\n");
  THPRINT_SOURCE("UH", hopt, UHopidx);
  printf("SI=");
  printf("[");
  printf("%d", SI[0]);
  printf(";");
  printf("%d", SI[1]);
  printf(";");
  printf("]");
  printf("\n");
  THPRINT_SOURCE("SI", hopt, SIopidx);
  printf("UI=");
  printf("[");
  printf("%u", UI[0]);
  printf(";");
  printf("%u", UI[1]);
  printf(";");
  printf("]");
  printf("\n");
  THPRINT_SOURCE("UI", hopt, UIopidx);
  printf("SL=");
  printf("[");
  printf("%ld", SL[0]);
  printf(";");
  printf("%ld", SL[1]);
  printf(";");
  printf("]");
  printf("\n");
  THPRINT_SOURCE("SL", hopt, SLopidx);
  printf("UL=");
  printf("[");
  printf("%lu", UL[0]);
  printf(";");
  printf("%lu", UL[1]);
  printf(";");
  printf("]");
  printf("\n");
  THPRINT_SOURCE("UL", hopt, ULopidx);
  printf("SZ=");
  printf("[");
  printf("%zu", SZ[0]);
  printf(";");
  printf("%zu", SZ[1]);
  printf(";");
  printf("]");
  printf("\n");
  THPRINT_SOURCE("SZ", hopt, SZopidx);
  printf("FL=");
  printf("[");
  airSinglePrintf(stdout, NULL, 0, "%g", FL[0]);
  printf(";");
  airSinglePrintf(stdout, NULL, 0, "%g", FL[1]);
  printf(";");
  printf("]");
  printf("\n");
  THPRINT_SOURCE("FL", hopt, FLopidx);
  printf("DB=");
  printf("[");
  airSinglePrintf(stdout, NULL, 0, "%lg", DB[0]);
  printf(";");
  airSinglePrintf(stdout, NULL, 0, "%lg", DB[1]);
  printf(";");
  printf("]");
  printf("\n");
  THPRINT_SOURCE("DB", hopt, DBopidx);
  printf("CH=");
  printf("[");
  printf("%c", CH[0]);
  printf(";");
  printf("%c", CH[1]);
  printf(";");
  printf("]");
  printf("\n");
  THPRINT_SOURCE("CH", hopt, CHopidx);
  printf("SS=");
  printf("[");
  printf("%s", SS[0]);
  printf(";");
  printf("%s", SS[1]);
  printf(";");
  printf("]");
  printf("\n");
  THPRINT_SOURCE("SS", hopt, SSopidx);
  printf("EE=");
  printf("[");
  printf("%s", airEnumStr(airEndian, EE[0]));
  printf(";");
  printf("%s", airEnumStr(airEndian, EE[1]));
  printf(";");
  printf("]");
  printf("\n");
  THPRINT_SOURCE("EE", hopt, EEopidx);
  printf("XY=");
  printf("[");
  pos_print(stdout, XY + (2 * 0));
  printf(";");
  pos_print(stdout, XY + (2 * 1));
  printf(";");
  printf("]");
  printf("\n");
  THPRINT_SOURCE("XY", hopt, XYopidx);
  printf("CO=");
  printf("[");
  combo_print(stdout, CO[0]);
  printf(";");
  combo_print(stdout, CO[1]);
  printf(";");
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
