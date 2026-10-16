/* tdatafilelast: check that "data file:" is the last header field
   Copyright (C) 2026  University of Chicago
   See ../../../LICENSE.txt for licensing terms */

#include <stdio.h>
#include <string.h>

#include <teem/nrrd.h>
#include <testutil.h>

/*
** Tests:
** formatNRRD_write emits comments and key/value pairs before the fields,
** so a "data file:" field (single or LIST form) is the last header line,
** and a LIST-form detached header with comments and key/values round-trips
*/

static int
lastHeaderLineIsDataFile(const char *me, const char *hdrName) {
  char line[AIR_STRLEN_HUGE + 1], lastField[AIR_STRLEN_HUGE + 1];
  FILE *fin;
  int sawDataFile;

  if (!(fin = fopen(hdrName, "rb"))) {
    fprintf(stderr, "%s: couldn't reopen %s\n", me, hdrName);
    return 1;
  }
  lastField[0] = '\0';
  sawDataFile = AIR_FALSE;
  while (fgets(line, AIR_STRLEN_HUGE, fin)) {
    if ('\n' == line[0] || '\r' == line[0]) {
      continue;
    }
    if (!strncmp(line, "data file:", strlen("data file:"))) {
      sawDataFile = AIR_TRUE;
      airStrcpy(lastField, AIR_STRLEN_HUGE + 1, line);
    } else if (sawDataFile && strstr(lastField, "LIST") && strncmp(line, "#", 1)
               && !strchr(line, ':')) {
      /* filenames following a LIST are part of the data file field */
      continue;
    } else {
      airStrcpy(lastField, AIR_STRLEN_HUGE + 1, line);
    }
  }
  fclose(fin);
  if (!sawDataFile) {
    fprintf(stderr, "%s: no \"data file:\" field in %s\n", me, hdrName);
    return 1;
  }
  if (strncmp(lastField, "data file:", strlen("data file:"))) {
    fprintf(stderr, "%s: last header field in %s is |%s|, not \"data file:\"\n", me,
            hdrName, lastField);
    return 1;
  }
  return 0;
}

int
main(int argc, const char **argv) {
  const char *me;
  Nrrd *nrrd, *nin;
  NrrdIoState *nio;
  airArray *mop;
  char *hdrName, *listName;
  unsigned char *data;
  unsigned int ii, jj;

  /* for debugging C-locale-emulation conformance */
  airSetLocaleFromEnvOrExit(argv[0], airStderr());

  AIR_UNUSED(argc);
  me = argv[0];
  mop = airMopNew();
  nrrd = nrrdNew();
  airMopAdd(mop, nrrd, nrrdNuke_mop, airMopAlways);
  nin = nrrdNew();
  airMopAdd(mop, nin, nrrdNuke_mop, airMopAlways);
  nio = nrrdIoStateNew();
  airMopAdd(mop, nio, nrrdIoStateNix_mop, airMopAlways);
  airMopAdd(mop, hdrName = teemTestTmpPath("tdatafilelast.nhdr"), airFree, airMopAlways);
  airMopAdd(mop, listName = teemTestTmpPath("tdatafilelist.nhdr"), airFree,
            airMopAlways);

  if (nrrdMaybeAlloc_va(nrrd, nrrdTypeUChar, 2, AIR_SIZE_T(4), AIR_SIZE_T(3))
      || nrrdCommentAdd(nrrd, "a comment that must not follow the data file field")
      || nrrdKeyValueAdd(nrrd, "key",
                         "a value that must not follow the data file field")) {
    airMopAdd(mop, biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble building nrrd\n", me);
    airMopError(mop);
    return 1;
  }
  data = AIR_CAST(unsigned char *, nrrd->data);
  for (ii = 0; ii < 12; ii++) {
    data[ii] = AIR_UCHAR(ii * 7 + 1);
  }

  /* single-file detached header */
  nio->detachedHeader = AIR_TRUE;
  if (nrrdSave(hdrName, nrrd, nio)) {
    airMopAdd(mop, biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble writing %s\n", me, hdrName);
    airMopError(mop);
    return 1;
  }
  if (lastHeaderLineIsDataFile(me, hdrName)) {
    airMopError(mop);
    return 1;
  }

  /* LIST-form detached header: one data file per slice along the slow axis */
  nrrdIoStateInit(nio);
  nio->detachedHeader = AIR_TRUE;
  nio->encoding = nrrdEncodingRaw;
  for (jj = 0; jj < 3; jj++) {
    char fn[AIR_STRLEN_SMALL + 1];
    unsigned int idx;
    snprintf(fn, sizeof(fn), "tdatafilelist-%u.raw", jj);
    idx = airArrayLenIncr(nio->dataFNArr, 1);
    nio->dataFN[idx] = airStrdup(fn);
  }
  nio->dataFileDim = 1;
  if (nrrdSave(listName, nrrd, nio)) {
    {
      char *err = biffGetDone(NRRD);
      airMopAdd(mop, err, airFree, airMopAlways);
      fprintf(stderr, "%s: trouble writing LIST-form %s:\n%s", me, listName, err);
    }
    airMopError(mop);
    return 1;
  }
  if (lastHeaderLineIsDataFile(me, listName)) {
    airMopError(mop);
    return 1;
  }
  if (nrrdLoad(nin, listName, NULL)) {
    airMopAdd(mop, biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble reading back LIST-form %s\n", me, listName);
    airMopError(mop);
    return 1;
  }
  if (nin->cmtArr->len != 1 || nrrdKeyValueSize(nin) != 1 || nrrdElementNumber(nin) != 12
      || memcmp(nin->data, nrrd->data, 12)) {
    fprintf(stderr, "%s: LIST-form round trip lost comment, key/value, or data\n", me);
    airMopError(mop);
    return 1;
  }

  fprintf(stderr, "%s: data file field is last, as required\n", me);
  airMopOkay(mop);
  return 0;
}
