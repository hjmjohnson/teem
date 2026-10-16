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

#include <teem/nrrd.h>
#include "testutil.h"

#include <locale.h>
#include <string.h>
#if !defined(_WIN32)
#  include <sys/mman.h>
#  include <unistd.h>
#endif

/*
** Tests C-locale-emulation: numbers must read and write as in the C locale whatever
** LC_NUMERIC the program has set.  Usage: tcle <case>, one case per CTest test.
** Exit 0 pass, 1 fail, 77 skip (needed locale not installed).
*/

#define SKIP 77

static const char *const commaLocales[] = {"de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8",
                                           "fr_FR.utf8",  "de_DE",      NULL};
/* macOS fa_IR and ar_SA, and glibc ps_AF, use the 2-byte U+066B separator */
static const char *const twoByteLocales[] = {"fa_IR.UTF-8", "ps_AF.UTF-8", "ps_AF.utf8",
                                             "ar_SA.UTF-8", NULL};
/* macOS tolower maps 'a' to 'z' under these LC_CTYPEs */
static const char *const oddCtypeLocales[] = {"kk_KZ.PT154", "zh_HK.Big5HKSCS", NULL};

static const char *
numericLocaleWithSepLen(const char *const *cands, size_t sepLen) {
  unsigned int ii;
  for (ii = 0; cands[ii]; ii++) {
    if (setlocale(LC_NUMERIC, cands[ii])
        && sepLen == strlen(localeconv()->decimal_point)) {
      return cands[ii];
    }
  }
  setlocale(LC_NUMERIC, "C");
  return NULL;
}

#if !defined(_WIN32)
/* writable buffer of len bytes ending at an inaccessible page, so any read past
   buf[len-1] faults */
static char *
guardedTail(size_t len) {
  long pgsz = sysconf(_SC_PAGESIZE);
  char *base = mmap(NULL, 2 * (size_t)pgsz, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANON, -1, 0);
  if (MAP_FAILED == base || mprotect(base + pgsz, (size_t)pgsz, PROT_NONE)) {
    return NULL;
  }
  return base + pgsz - len;
}
#endif

static Nrrd *
spacedNrrd(airArray *mop, unsigned int dim, double spc) {
  Nrrd *nrrd = nrrdNew();
  size_t size[NRRD_DIM_MAX];
  double spcs[NRRD_DIM_MAX];
  unsigned int ai;
  airMopAdd(mop, nrrd, nrrdNuke_mop, airMopAlways);
  for (ai = 0; ai < dim; ai++) {
    size[ai] = 2;
    spcs[ai] = spc;
  }
  if (nrrdMaybeAlloc_nva(nrrd, nrrdTypeFloat, dim, size)) {
    airMopAdd(mop, biffGetDone(NRRD), airFree, airMopAlways);
    return NULL;
  }
  nrrdAxisInfoSet_nva(nrrd, nrrdAxisInfoSpacing, spcs);
  return nrrd;
}

/* NrrdIoState made before setlocale must still write C-locale headers */
static int
staleWrite(const char *me, airArray *mop) {
  NrrdIoState *nio = nrrdIoStateNew();
  Nrrd *nrrd;
  char *str = NULL;
  const char *loc;
  airMopAdd(mop, nio, (airMopper)nrrdIoStateNix, airMopAlways);
  nio->encoding = nrrdEncodingAscii;
  if (!(loc = numericLocaleWithSepLen(commaLocales, 1))) {
    return SKIP;
  }
  if (!(nrrd = spacedNrrd(mop, 1, 0.5))) {
    return 1;
  }
  if (nrrdStringWrite(&str, nrrd, nio)) {
    airMopAdd(mop, biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: under %s, write with pre-setlocale nio failed\n", me, loc);
    return 1;
  }
  airMopAdd(mop, str, airFree, airMopAlways);
  if (!strstr(str, "spacings: 0.5\n")) {
    fprintf(stderr, "%s: under %s, pre-setlocale nio wrote:\n%s\n", me, loc, str);
    return 1;
  }
  return 0;
}

/* whitespace around commas in a space vector is valid NRRD (main accepts it) */
static int
spaceOriginWhitespace(const char *me, airArray *mop) {
  static const char hdr[] = "NRRD0005\ntype: float\ndimension: 2\nsizes: 1 1\n"
                            "space dimension: 2\nspace directions: (1,0) (0,1)\n"
                            "space origin: ( 1.5 , 2.5 )\nencoding: ascii\n\n7\n";
  Nrrd *nrrd = nrrdNew();
  airMopAdd(mop, nrrd, nrrdNuke_mop, airMopAlways);
  if (nrrdStringRead(nrrd, hdr, NULL)) {
    airMopAdd(mop, biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: \"space origin: ( 1.5 , 2.5 )\" rejected\n", me);
    return 1;
  }
  if (!(1.5 == nrrd->spaceOrigin[0] && 2.5 == nrrd->spaceOrigin[1])) {
    fprintf(stderr, "%s: origin parsed as (%g,%g)\n", me, nrrd->spaceOrigin[0],
            nrrd->spaceOrigin[1]);
    return 1;
  }
  return 0;
}

/* empty input in a 2-byte-separator locale must not read past its terminator */
static int
emptyString(const char *me, int useRemove) {
#if defined(_WIN32)
  AIR_UNUSED(me);
  AIR_UNUSED(useRemove);
  return SKIP;
#else
  char dbuff[AIR_STRLEN_SMALL + 1];
  const char *loc, *decisep;
  char *empty;
  double val = 0;
  if (!(loc = numericLocaleWithSepLen(twoByteLocales, 2))) {
    return SKIP;
  }
  decisep = airDeciSepLearn(dbuff, NULL);
  if (!(empty = guardedTail(1))) {
    fprintf(stderr, "%s: could not set up guard page\n", me);
    return 1;
  }
  empty[0] = '\0';
  if (useRemove) {
    airDeciSepRemove(empty, decisep); /* faults if it reads empty[1] */
  } else if (airStoD(&val, empty, decisep)) {
    fprintf(stderr, "%s: under %s, airStoD accepted \"\"\n", me, loc);
    return 1;
  }
  return 0;
#endif
}

/* VTK headers must also be C-locale */
static int
vtkWrite(const char *me, airArray *mop) {
  NrrdIoState *nio;
  Nrrd *nrrd;
  const char *loc;
  char *fname, line[AIR_STRLEN_LARGE + 1];
  FILE *file;
  int found = 0;
  if (!(loc = numericLocaleWithSepLen(commaLocales, 1))) {
    return SKIP;
  }
  if (!(nrrd = spacedNrrd(mop, 3, 1.5))) {
    return 1;
  }
  nio = nrrdIoStateNew();
  airMopAdd(mop, nio, (airMopper)nrrdIoStateNix, airMopAlways);
  nio->format = nrrdFormatVTK;
  fname = teemTestTmpPath("tcle.vtk");
  airMopAdd(mop, fname, airFree, airMopAlways);
  if (nrrdSave(fname, nrrd, nio)) {
    airMopAdd(mop, biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: could not save %s\n", me, fname);
    return 1;
  }
  if (!(file = fopen(fname, "rb"))) {
    return 1;
  }
  while (fgets(line, (int)sizeof(line), file)) {
    if (!strncmp(line, "SPACING", 7)) {
      found = !strcmp(line, "SPACING 1.5 1.5 1.5\n");
      if (!found) {
        fprintf(stderr, "%s: under %s, wrote %s", me, loc, line);
      }
    }
  }
  fclose(file);
  return found ? 0 : 1;
}

static int
vtkRead(const char *me, airArray *mop) {
  static const char vtk[] = "# vtk DataFile Version 3.0\ntcle\nASCII\n"
                            "DATASET STRUCTURED_POINTS\nDIMENSIONS 2 2 2\n"
                            "ORIGIN 0.5 0.5 0.5\nSPACING 1.5 1.5 1.5\n"
                            "POINT_DATA 8\nSCALARS scalars float\nLOOKUP_TABLE default\n"
                            "0 1 2 3 4 5 6 7\n";
  Nrrd *nrrd = nrrdNew();
  const char *loc;
  char *fname;
  FILE *file;
  airMopAdd(mop, nrrd, nrrdNuke_mop, airMopAlways);
  fname = teemTestTmpPath("tcle_c.vtk");
  airMopAdd(mop, fname, airFree, airMopAlways);
  if (!(file = fopen(fname, "wb"))) {
    return 1;
  }
  fputs(vtk, file);
  fclose(file);
  if (!(loc = numericLocaleWithSepLen(commaLocales, 1))) {
    return SKIP;
  }
  if (nrrdLoad(nrrd, fname, NULL)) {
    airMopAdd(mop, biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: under %s, C-locale VTK file rejected\n", me, loc);
    return 1;
  }
  return 0;
}

/* special values must not depend on LC_CTYPE */
static int
lcAllNan(const char *me) {
  char dbuff[AIR_STRLEN_SMALL + 1];
  const char *decisep;
  unsigned int ii;
  double val = 0;
  for (ii = 0; oddCtypeLocales[ii]; ii++) {
    if (setlocale(LC_ALL, oddCtypeLocales[ii])) {
      break;
    }
  }
  if (!oddCtypeLocales[ii]) {
    return SKIP;
  }
  decisep = airDeciSepLearn(dbuff, NULL);
  if (1 != airStoD(&val, "NaN", decisep) || AIR_EXISTS(val)) {
    fprintf(stderr, "%s: under LC_ALL=%s, \"NaN\" not parsed as NaN\n", me,
            oddCtypeLocales[ii]);
    return 1;
  }
  return 0;
}

int
main(int argc, const char **argv) {
  const char *me, *cs;
  airArray *mop;
  int ret;

  airSetLocaleFromEnvOrExit(argv[0], airStderr());
  setlocale(LC_ALL, "C");
  me = argv[0];
  if (2 != argc) {
    fprintf(stderr, "usage: %s <case>\n", me);
    return 1;
  }
  cs = argv[1];
  mop = airMopNew();
  if (!strcmp(cs, "stale_write")) {
    ret = staleWrite(me, mop);
  } else if (!strcmp(cs, "space_origin_ws")) {
    ret = spaceOriginWhitespace(me, mop);
  } else if (!strcmp(cs, "empty_stod")) {
    ret = emptyString(me, AIR_FALSE);
  } else if (!strcmp(cs, "empty_decisep_remove")) {
    ret = emptyString(me, AIR_TRUE);
  } else if (!strcmp(cs, "vtk_write")) {
    ret = vtkWrite(me, mop);
  } else if (!strcmp(cs, "vtk_read")) {
    ret = vtkRead(me, mop);
  } else if (!strcmp(cs, "lc_all_nan")) {
    ret = lcAllNan(me);
  } else {
    fprintf(stderr, "%s: unknown case \"%s\"\n", me, cs);
    ret = 1;
  }
  airMopOkay(mop);
  return ret;
}
