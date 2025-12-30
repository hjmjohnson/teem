/*
  Teem: Tools to process and visualize scientific data and images
  Copyright (C) 2009--2025  University of Chicago
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

#include "unrrdu.h"
#include "privateUnrrdu.h"

static const char keybase[] = "unu_history";
static const uint hlnMax[4] = {0, 9, 99, 999};

static void
historyKeyFormat(char *keyfmt, size_t size, uint hdig) {
  snprintf(keyfmt, size, "%s_%%%u", keybase, hdig);
  fprintf(stderr, "!%s(%u): keyfmt=|%s|\n", __func__, hdig, keyfmt);
  return;
}

static uint
historyDigitsGet(const Nrrd *nin) {
  if (!nin) {
    // NULL nrrd has no history
    return 0;
  }
  char key[AIR_STRLEN_SMALL + 1], *val;
  // try single digit
  snprintf(key, sizeof(key), "%s_1", keybase);
  val = nrrdKeyValueGet(nin, key);
  if (val) {
    free(val);
    return 1;
  }
  // try two digit
  snprintf(key, sizeof(key), "%s_01", keybase);
  val = nrrdKeyValueGet(nin, key);
  if (val) {
    free(val);
    return 2;
  }
  // to be save, try three digit
  snprintf(key, sizeof(key), "%s_001", keybase);
  val = nrrdKeyValueGet(nin, key);
  if (val) {
    free(val);
    return 3;
  }
  // (not trying four digits)
  return 0;
}

static void
historyClear(Nrrd *nout) {
  uint hdig = historyDigitsGet(nin);
  if (!hdig) {
    // no history needs clearing
    return;
  }
  char keyfmt[AIR_STRLEN_SMALL + 1];
  historyKeyFormat(keyfmt, sizeof(keyfmt), hdig);
  for (uint hln = 1; hln <= hlnMax[hdig]; hln++) {
    char key[AIR_STRLEN_SMALL + 1];
    snprintf(key, sizeof(key), keyfmt, hln);
    // don't even try to get the value from `key`, just erase it
    nrrdKeyValueErase(nout, key);
  }
  return;
}

static airArray *
historyGet(const Nrrd *nin) {
  uint hdig = historyDigitsGet(nin);
  // else have 0, 1, 2, or 3 digits of history: create airArray
  airPtrPtrUnion appu;
  char **hist;
  appu.cp = &hist;
  airArray *ret = airArrayNew(appu.v, NULL, sizeof(char *), 32);
  airArrayPointerCB(ret, NULL, airFree);
  if (hdig) {
    // there is some history to extract
    char keyfmt[AIR_STRLEN_SMALL + 1];
    historyKeyFormat(keyfmt, sizeof(keyfmt), hdig);
    // hmax[hdig] is highest possible history line number
    for (uint hln = 1; hln <= hlnMax[hdig]; hln++) {
      char key[AIR_STRLEN_SMALL + 1];
      snprintf(key, sizeof(key), keyfmt, hln);
      fprintf(stderr, "!%s: key=|%s|\n", __func__, key);
      char *val = nrrdKeyValueGet(nin, key);
      if (!val) {
        // no value for this key; we've gone past the end of recorded history
        break;
      }
      // else we do have line `hln` of history
      uint idx = airArrayLenIncr(ret, 1);
      hist[idx] = val;
      fprintf(stderr, "!%s: hist[%u]=|%s|\n", __func__, idx, val);
    }
  }
  return ret;
}

static int /* Biff: 1 */
historySet(Nrrd *nout, airArray *histArr) {
  uint hdig, hnum = histArr ? histArr->len : 0;
  if (!hnum) {
    biffAddf(UNRRDU, "%s: got no history to set", __func__);
    return 1;
  }
  if (hnum <= 9) {
    hdig = 1;
  } else if (hnum <= 99) {
    hdig = 2;
  } else if (hnum <= 999) {
    hdig = 3;
  } else {
    biffAddf(UNRRDU, "%s: %u lines of history exceeds 3-digit limit") return 1;
  }
  char keyfmt[AIR_STRLEN_SMALL + 1];
  historyKeyFormat(keyfmt, sizeof(keyfmt), hdig);
  historyClear(nout);
  char **hist = histArr->data;
  for (uint hln = 1; hln <= hlnMax[hdig]; hln++) {
    char key[AIR_STRLEN_SMALL + 1];
    snprintf(key, sizeof(key), keyfmt, hln);
    nrrdKeyValueSet(nin, key);
    if (nrrdKeyValueAdd(nout, key, hist[hln - 1])) {
      biffAddf(UNRRDU, "%s: trouble adding line %u of history", __func__, hln);
      return 1;
    }
  }
  return 0;
}

// assumes that there is no history already in nout
int /* Biff: 1 */
unrrduUnuKeyValueHistoryAdd(Nrrd *nout, int argc, const char **argv, const Nrrd *nin,
                            const char *fname) {
  if (!(nout && argv && (nin || fname))) {
    biffAddf(UNRRDU, "%s: got NULL pointer", __func__);
    return 1;
  }
  if (nrrdStateKeyValueReturnInternalPointers) {
    biffAddf(UNRRDU,
             "%s: sorry, handling true "
             "nrrdStateKeyValueReturnInternalPointers not implemented",
             __func__);
    return 1;
  }
  // else nrrdKeyValueGet will return newly strdup'ed strings

  airArray *histArr = historyGet(nin);
  uint idx = airArrayLenIncr(histArr, 1);
  char **hist = histArr->data;
  hist[idx] = hestArgcArgvSprint(argc, argv, AIR_FALSE);
  if (historySet(nout, histArr)) {
    biffAddf(UNRRDU, "%s: trouble adding history", __func__);
    airArrayNuke(histArr);
    return 1;
  }
  airArrayNuke(histArr);
  return 0;
}
