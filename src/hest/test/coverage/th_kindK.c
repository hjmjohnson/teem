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

// (K) th_kindK.c: prototype for hest test file that uses all types, but
// (K) specialized for one kind (except for kind=1 = stand-alone flag),
// (K) and for kind=3 = multiple fixed, the number of values.
// (K) gen_kind.py processes this to create th_kind{2,3,4,5}*.c files.
// (K) Unlike other th*.c in this directory, this is all
// (K) human(GLK)-written (based on ../ex6.c).
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
  _DA_BB_;
  _DA_SH_;
  _DA_UH_;
  _DA_SI_;
  _DA_UI_;
  _DA_SL_;
  _DA_UL_;
  _DA_SZ_;
  _DA_FL_;
  _DA_DB_;
  _DA_CH_;
  _DA_SS_;
  _DA_EE_;
  _DA_XY_;
  _DA_CO_;

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
  _PR_BB_;
  _PR_SH_;
  _PR_UH_;
  _PR_SI_;
  _PR_UI_;
  _PR_SL_;
  _PR_UL_;
  _PR_SZ_;
  _PR_FL_;
  _PR_DB_;
  _PR_CH_;
  _PR_SS_;
  _PR_EE_;
  _PR_XY_;
  _PR_CO_;

  /* free the memory allocated by parsing ... */
  hestParseFree(hopt);
  /* ... and the other stuff */
  hopt = hestOptFree(hopt);
  hparm = hestParmFree(hparm);
  exit(0);
}
