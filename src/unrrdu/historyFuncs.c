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

#include "unrrdu.h"
#include "privateUnrrdu.h"
#include <assert.h>

static const char keybase[] = MY_HISTORY_KEY;
static const uint hlnMax[4] = {0, 9, 99, 999};

static void
historyKeyFormat(char *keyfmt, size_t size, uint hdig) {
  snprintf(keyfmt, size, "%s_%%0%uu", keybase, hdig);
  // fprintf(stderr, "!%s(%u): keyfmt=|%s|\n", __func__, hdig, keyfmt);
  return;
}

static uint
historyDigitsGet(const Nrrd *nin) {
  if (!nin) {
    // NULL nrrd has no history, and that's ok
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

/*
 ******** unrrduUnuHistoryClear
 *
 * empties key/value history of given `nout`
 */
void
unrrduUnuHistoryClear(Nrrd *nout) {
  uint hdig = historyDigitsGet(nout);
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

/* makes an airArray of history from given `nin`
   or just initializes an empty array from NULL */
static airArray * /* Biff: nope */
historyGet(unrrduUnuInNSPair insp, int commentIfNone) {
  // whether or not we have any input history; create the airArray
  airArray *ret = airArrayNew(NULL, NULL, sizeof(char *), 32);
  airArrayPointerCB(ret, NULL, airFree);
  uint hdig = historyDigitsGet(insp.nin);
  if (hdig) {
    // there is some history to extract
    char keyfmt[AIR_STRLEN_SMALL + 1];
    historyKeyFormat(keyfmt, sizeof(keyfmt), hdig);
    // hmax[hdig] is highest possible history line number
    for (uint hln = 1; hln <= hlnMax[hdig]; hln++) {
      char key[AIR_STRLEN_SMALL + 1];
      snprintf(key, sizeof(key), keyfmt, hln);
      // fprintf(stderr, "!%s: key=|%s|\n", __func__, key);
      char *val = nrrdKeyValueGet(insp.nin, key);
      if (!val) {
        // no value for this key; we've gone past the end of recorded history
        break;
      }
      // else we do have line `hln` of history
      uint idx = airArrayLenIncr(ret, 1);
      char **hist = ret->data;
      hist[idx] = val;
      // fprintf(stderr, "!%s: hist[%u]=|%s|\n", __func__, idx, val);
    }
  } else if (insp.name) {
    const char *envhstdin = NULL;
    if (!strcmp("-", insp.name) /* input is stdin */
        && (envhstdin = getenv(UNRRDU_UNU_HISTORY_STDIN_ENV))) {
      // use given back channel description of history (assuming only single line)
      char *hcmd = airOneLinify(airStrdup(envhstdin));
      assert(hcmd);
      size_t hlinesz = strlen(hcmd) + strlen(" |") + 1;
      char *hline = AIR_CALLOC(hlinesz, char);
      assert(hline);
      snprintf(hline, hlinesz, "%s |", hcmd);
      uint idx = airArrayLenIncr(ret, 1);
      char **hist = ret->data;
      hist[idx] = hline; // new ownership; no free(hline)
      free(hcmd);
    } else if (commentIfNone) {
      // input is not stdin and/or had no backchannel info
      // so, if requested, say that we have no history for insp.name
      uint idx = airArrayLenIncr(ret, 1);
      char **hist = ret->data;
      size_t hlinesz = strlen(insp.name) + strlen("# (\"\" has no history)") + 1;
      char *hline = AIR_CALLOC(hlinesz, char);
      assert(hline);
      snprintf(hline, hlinesz, "# (\"%s\" has no history)", insp.name);
      hist[idx] = hline; // new ownership; no free(hline)
    }
  }
  return ret;
}

airArray * /* Biff: nope */
unrrduUnuHistoryGet(const Nrrd *nin, const char *fname, int commentIfNone) {
  return historyGet((unrrduUnuInNSPair){.nin = nin, .name = fname}, commentIfNone);
}

static int /* Biff: 1 */
historyAdd(Nrrd *nout, airArray *ihArr) {
  uint hdig, ihnum = ihArr ? ihArr->len : 0;
  if (!ihnum) {
    biffAddf(UNRRDU, "%s: got no history to set", __func__);
    return 1;
  }
  // have to use unrrduUnuInNSPair because that's the type historyGet wants
  airArray *ohArr = historyGet((unrrduUnuInNSPair){.nin = nout, .name = NULL}, AIR_TRUE);
  // we are concatenating the input onto existing history in output
  uint ohnum = ohArr->len + ihnum;
  if (ohnum <= 9) {
    hdig = 1;
  } else if (ohnum <= 99) {
    hdig = 2;
  } else if (ohnum <= 999) {
    hdig = 3;
  } else {
    biffAddf(UNRRDU, "%s: %u lines of history exceeds 3-digit limit", __func__, ohnum);
    return 1;
  }
  char keyfmt[AIR_STRLEN_SMALL + 1];
  historyKeyFormat(keyfmt, sizeof(keyfmt), hdig);
  /* even though we're just adding to existing history in nout,
     the numbering may have to change from (e.g.) 1 to 2 digits,
     so to be safe we do first clear the output */
  unrrduUnuHistoryClear(nout);
  char **ohist = ohArr->data;
  char **ihist = ihArr->data;
  for (uint hidx = 0; hidx < ohnum; hidx++) {
    uint oln = hidx + 1;
    char key[AIR_STRLEN_SMALL + 1];
    snprintf(key, sizeof(key), keyfmt, oln);
    const char *hline = hidx < ohArr->len ? ohist[hidx] : ihist[hidx - ohArr->len];
    if (nrrdKeyValueAdd(nout, key, hline)) {
      biffAddf(UNRRDU, "%s: trouble adding line %u of history", __func__, oln);
      return 1;
    }
  }
  return 0;
}

/*
 ******** unrrduUnuHistoryAdd
 *
 * adds history to onsp.nout, from argc,argv command and/or insp.nin
 */
int /* Biff: 1 */
unrrduUnuHistoryAdd(unrrduUnuOutNSPair onsp,
                    int argc,
                    const char **argv,
                    unrrduUnuInNSPair insp) {
  if (!getenv(UNRRDU_UNU_HISTORY_TRACE_ENV)) {
    /* see above */
    unrrduUnuHistoryClear(onsp.nout);
    return 0;
  }
  if (!(onsp.nout && onsp.name)) {
    /* `argv` can be NULL to say: no command to add */
    /* `insp.nin` can be NULL to say: we're the starting point */
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
  /* else nrrdKeyValueGet will return newly strdup'ed strings,
     which our airArray will manage */

  airArray *histArr = historyGet(insp, AIR_TRUE);
  if (argv) {
    // we have an argc/argv command to add
    uint idx = airArrayLenIncr(histArr, 1);
    char **hist = histArr->data;
    char *cmd0 = hestArgcArgvSprint(argc, argv, AIR_FALSE);
    const char *lnbeg = (insp.name && !strcmp(insp.name, "-")
                           ? "    unu " /* indent because we were piped to */
                           : "unu " /* else assume we started w/ "-i filename" */);
    const char *lnend = (!strcmp(onsp.name, "-")
                           ? " |" /* if output is stdout, then assume piping */
                           : ";" /* else assume we're finished w/ "-o filename" */);
    char *cmd = airStrcat3(lnbeg, cmd0, lnend);
    free(cmd0);
    hist[idx] = cmd;
  }
  if (historyAdd(onsp.nout, histArr)) {
    biffAddf(UNRRDU, "%s: trouble adding history", __func__);
    airArrayNuke(histArr);
    return 1;
  }
  airArrayNuke(histArr);
  return 0;
}
