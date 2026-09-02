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

#define INFO "Print header of one or more NRRD files"
static const char *unu_headInfoL
  = (INFO ".  The value of this is simply to print the contents of nrrd "
          "headers, and without any attempt to parse the header (see \"unu describe\" "
          "for something that loads and then describes the Nrrd). \"unu head\" avoids "
          "the use of \"head -N\", where N has to be determined manually, which always "
          "risks printing raw binary data (following the header) to screen, which tends "
          "to clobber terminal settings, and be generally annoying.\n "
          "* Uses nrrdOneLine");

static int /* Biff: 1 */
headDoit(const char *me, NrrdIoState *nio, const char *inS, FILE *fout) {
  if (!strcmp("-", inS) && isatty(STDIN_FILENO)) {
    biffAddf(me, "declining to try reading Nrrd from stdin as tty (terminal)");
    return 1;
  }
  airArray *mop = airMopNew();
  FILE *fin;
  if (!(fin = airFopen(inS, stdin, "rb"))) {
    biffAddf(me, "couldn't fopen(\"%s\",\"rb\"): %s\n", inS, strerror(errno));
    airMopError(mop);
    return 1;
  }
  airMopAdd(mop, fin, airFclose_mop, airMopAlways);

  size_t len;
  if (nrrdOneLine(&len, nio, fin)) {
    biffAddf(me, "error getting first line of file \"%s\"", inS);
    airMopError(mop);
    return 1;
  }
  if (!len) {
    biffAddf(me, "immediately hit EOF\n");
    airMopError(mop);
    return 1;
  }
  if (!(nrrdFormatNRRD->contentStartsLike(nio))) {
    biffAddf(me, "first line (\"%s\") isn't a nrrd magic\n", nio->line);
    airMopError(mop);
    return 1;
  }
  while (len > 1) {
    fprintf(fout, "%s\n", nio->line);
    nrrdOneLine(&len, nio, fin);
  };

  /* experience has shown that on at least windows and darwin, the writing
     process's fwrite() to stdout will fail if we exit without consuming
     everything from stdin */
  if (stdin == fin) {
    int c = getc(fin);
    while (EOF != c) {
      c = getc(fin);
    }
  }

  airMopOkay(mop);
  return 0;
}

static int
unu_headMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  hestOpt *hopt = NULL;
  char **inS;
  uint ninLen;
  hestOptAdd_Mv_String(&hopt, NULL, "nin1", 1, -1, &inS, "-",
                       "input nrrd(s). By default tries to read from stdin", &ninLen);

  // (boilerplate for YES some hparm modification)
  hestParm *hparm = hestParmCopy(_hparm);
  hparm->noArgsIsNoProblem = AIR_TRUE;
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_headInfoL);
  airArray *mop = unrrduMopNew(hopt, hparm);
  char *err;

  NrrdIoState *nio = nrrdIoStateNew();
  airMopAdd(mop, nio, nrrdIoStateNix_mop, airMopAlways);
  int okay = AIR_FALSE;
  for (uint ni = 0; ni < ninLen; ni++) {
    if (ninLen > 1) {
      fprintf(stdout, "==> %s <==\n", inS[ni]);
    }
    if (headDoit(me, nio, inS[ni], stdout)) {
      airMopAdd(mop, err = biffGetDone(me), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble reading from \"%s\":\n%s", me, inS[ni], err);
      /* continue working on the remaining files */
    } else {
      /* processed at least one file ok */
      okay = AIR_TRUE;
    }
    if (ninLen > 1 && ni < ninLen - 1) {
      fprintf(stdout, "\n");
    }
  }
  if (!okay) {
    /* none of the given files could be read; something is wrong */
    if (ninLen > 1) {
      fprintf(stderr, "\n%s: Unable to read from any file\n", me);
    }
    hestUsage(stderr, hopt, me, hparm, AIR_TRUE /* oneline */);
    fprintf(stderr, "\nFor more info: \"%s --help\"\n", me);
  }

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(head, INFO);
