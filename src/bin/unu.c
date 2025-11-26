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

#include <teem/unrrdu.h>

#define UNU "unu"

/* copied from a privateLib.h */
/* NOTE: these SN_,ASP1_ string macros copy-pasta'd to other private<Lib>.h files:
   SN_INCR: safely increments STR arg to point '\0'-termination of STR
   SN_COPY: safely copies SRC to DST, then increments DST
   SN_PRINTF: snprintf's into DST then increments DST
   ASP1_X: short version of AIR_STRLEN_X + 1 */
#define SN_INCR(STR, SIZE)                                                              \
  do {                                                                                  \
    size_t tmp_str_len_##STR = strlen(STR);                                             \
    STR += tmp_str_len_##STR;                                                           \
    SIZE -= tmp_str_len_##STR;                                                          \
  } while (0)
#define SN_COPY(DST, DST_SIZE, SRC)                                                     \
  do {                                                                                  \
    airStrcpy((DST), (DST_SIZE), (SRC));                                                \
    SN_INCR(DST, DST_SIZE);                                                             \
  } while (0)
#define SN_PRINTF(DST, DST_SIZE, FMT, ...)                                              \
  do {                                                                                  \
    snprintf((DST), (DST_SIZE), (FMT), __VA_ARGS__);                                    \
    SN_INCR(DST, DST_SIZE);                                                             \
  } while (0)
#define ASP1_S (AIR_STRLEN_SMALL + 1)
#define ASP1_M (AIR_STRLEN_MEDIUM + 1)
#define ASP1_L (AIR_STRLEN_LARGE + 1)
#define ASP1_H (AIR_STRLEN_HUGE + 1)

typedef unsigned int uint; /* uint is just more concise */

/*
 * unu-specific usage printer: prints out a little banner, and a listing of all
 * available unu commands with their one-line descriptions Would use unrrduUsage but
 * have to handle hidden commands and the possible non-availability of "fft"
 */
static void
usage(const char *me, const hestParm *hparm, int alsoHidden) {
  size_t maxlen = 0;
  for (uint cmdi = 0; unrrduUnuCmdList[cmdi]; cmdi++) {
    size_t cnsize = strlen(unrrduUnuCmdList[cmdi]->name);
    maxlen = AIR_MAX(maxlen, cnsize);
  }
  char _buff[ASP1_L], _fmt[ASP1_L];
  size_t _buffSize = ASP1_L, _fmtSize = ASP1_L;
  airStrcpy(_buff, _buffSize, "--- unu: Utah Nrrd Utilities command-line interface ---");
  size_t len = strlen(_buff);
  uint half = (hparm->columns > len ? hparm->columns - len : 0) / 2 + len - 1;
  snprintf(_fmt, _fmtSize, "%%%us\n", half);
  printf(_fmt, _buff);
  if (alsoHidden) {
    airStrcpy(_buff, _buffSize, "(hidden commands have \" : \" in listing)");
    len = strlen(_buff);
    half = (hparm->columns > len ? hparm->columns - len : 0) / 2 + len - 1;
    snprintf(_fmt, _fmtSize, "%%%us\n", half);
    printf(_fmt, _buff);
  }
  for (uint cmdi = 0; unrrduUnuCmdList[cmdi]; cmdi++) {
    int hidden;
    if (unrrduUnuCmdList[cmdi]->hidden) {
      if (!alsoHidden) {
        /* nothing to see here! */
        continue;
      }
      hidden = AIR_TRUE;
    } else {
      hidden = AIR_FALSE;
    }
    int nofft = !strcmp(unrrduUnuCmdList[cmdi]->name, "fft") && !nrrdFFTWEnabled;
    len = strlen(unrrduUnuCmdList[cmdi]->name);
    len += !!nofft;
    char *buff = _buff;
    size_t buffSize = _buffSize;
    buff[0] = '\0';
    for (size_t chi = len; chi < maxlen + hparm->indent; chi++)
      SN_COPY(buff, buffSize, " ");
    if (nofft) {
      SN_COPY(buff, buffSize, "(");
    }
    SN_PRINTF(buff, buffSize, "%s %s", me, unrrduUnuCmdList[cmdi]->name);
    if (!hidden) {
      SN_COPY(buff, buffSize, " ... ");
    } else {
      SN_COPY(buff, buffSize, "  :  ");
    }
    len = strlen(_buff);
    fprintf(stdout, "%s", _buff);
    if (nofft) {
      /* luckily, still fits within 80 columns */
      fprintf(stdout, "Not Enabled: ");
      size_t infoSize = strlen(unrrduUnuCmdList[cmdi]->info) + 2;
      char *infop = AIR_CALLOC(infoSize, char);
      snprintf(infop, infoSize, "%s)", unrrduUnuCmdList[cmdi]->info);
      hestPrintStr(stdout, len, len, hparm->columns, infop, AIR_FALSE);
      free(infop);
    } else {
      hestPrintStr(stdout, len, len, hparm->columns, unrrduUnuCmdList[cmdi]->info,
                   AIR_FALSE);
    }
  }
  return;
}

int
main(int argc, const char **argv) {
  int i, ret, listAll;

  /* parse environment variables first, in case they break nrrdDefault*
     or nrrdState* variables in a way that nrrdSanity() should see */
  nrrdDefaultGetenv();
  nrrdStateGetenv();

  /* if user hasn't tried to set nrrdStateKindNoop by an environment
     variable, we set it to false, since its probably what people expect */
  if (!getenv(nrrdEnvVarStateKindNoop)) {
    nrrdStateKindNoop = AIR_FALSE;
  }

  /* if user hasn't tried to set nrrdStateKeyValuePairsPropagate by an envvar,
     we set it to true, since that's probably what unu users expect */
  if (!getenv(nrrdEnvVarStateKeyValuePairsPropagate)) {
    nrrdStateKeyValuePairsPropagate = AIR_TRUE;
  }

  /* no harm done in making sure we're sane */
  nrrdSanityOrExit(argv[0]);

  hestParm *hparm = hestParmMopNew();
  hparm->elideSingleEnumType = AIR_TRUE;
  hparm->elideSingleOtherType = AIR_TRUE;
  /*
   * This prevents clarifying that the default input is "-" i.e. stdin, and it was
   * explicitly turned off in multiple commands (such as resample). In the interests of
   * clarity, no longer want to elide this info.
   * hparm->elideSingleOtherDefault = AIR_TRUE;
   */
  hparm->elideSingleNonExistFloatDefault = AIR_TRUE;
  hparm->elideMultipleNonExistFloatDefault = AIR_TRUE;
  hparm->elideSingleEmptyStringDefault = AIR_TRUE;
  hparm->elideMultipleEmptyStringDefault = AIR_TRUE;
  /* say that we look for, and know how to handle, seeing "--help" */
  hparm->respectDashDashHelp = AIR_TRUE;
  /* set hparm->columns from ioctl if possible,
     else use 78, which was value of (one in TeemV2) unrrduDefaultNumColumns */
  hestParmColumnsIoctl(hparm, 78);
  // Gone in TeemV2: hparm->greedySingleString = AIR_TRUE;
  /* New for TeemV2: a more explicit way to support quiet quitting,
     instead of hacky macros hiding in unrrdu/privateUnrrdu.h */
  hparm->quietQuit = unrrduQuietQuitTest;

  /* if there are no arguments, or "unu list" (or "unu all" shhh), then we give general
  usage information */
  listAll = (2 == argc && !strcmp("all", argv[1]));
  if (1 >= argc || listAll || (2 == argc && !strcmp("list", argv[1]))) {
    usage("unu", hparm, listAll /* alsoHidden */);
    hestParmFree(hparm);
    exit(1);
  }
  /* else, we see if its unu --version */
  if (!strcmp("--version", argv[1])) {
    char vbuff[AIR_STRLEN_SMALL + 1];
    airTeemVersionSprint(vbuff);
    printf("%s\n", vbuff);
    hestParmFree(hparm);
    exit(0);
  }
  /* else, we should see if they're asking for a command we know about */
  for (i = 0; unrrduUnuCmdList[i]; i++) {
    if (!strcmp(argv[1], unrrduUnuCmdList[i]->name)) {
      break;
    }
    if (!strcmp("about", unrrduUnuCmdList[i]->name)) {
      /* we interpret "unu help" and "unu --help" as asking for "unu about" */
      if (!strcmp("--help", argv[1]) || !strcmp("help", argv[1])) {
        break;
      }
    }
  }
  // unrrduUnuCmdList[] is NULL-terminated
  if (unrrduUnuCmdList[i]) {
    // yes, we have that command
    // initialize variables used by the various commands
    size_t meSize = strlen(UNU) + strlen(argv[1]) + 2;
    char *me = AIR_CALLOC(meSize, char);
    // hijack hparm->_selfMop to clean this dynamic allocation
    airMopAdd(hparm->_selfMop, me, airFree, airMopAlways);
    snprintf(me, meSize, "%s %s", UNU, argv[1]);
    // run the individual unu program, saving its exit status
    ret = unrrduUnuCmdList[i]->mainFunc(argc - 2, argv + 2, me, hparm);
  } else {
    fprintf(stderr,
            "%s: unrecognized command \"%s\"; run \"%s\" for "
            "complete list\n",
            UNU, argv[1], argv[0]);
    ret = 1;
  }
  // we didn't exit, it's on us to free what we created
  hestParmFree(hparm);
  exit(ret);
}
