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

#define INFO "Write data segment of a nrrd file"
static const char *unu_dataInfoL
  = (INFO ".  The value of this is to pass the data segment in isolation to a "
          "stand-alone decoder, in case this Teem build lacks an optional "
          "data encoding required for a given nrrd file.  Caveats: "
          "Will start copying characters from the datafile "
          "to stdout until EOF is hit, so this won't work "
          "correctly if the datafile has extraneous content at the end.  Will "
          "skip lines (as per \"line skip:\" header field) if needed, but can only "
          "skip bytes (as per \"byte skip:\") if the encoding is NOT a compression. "
          "\n \n "
          "To make vol.raw contain the uncompressed data from vol.nrrd "
          "which uses \"gz\" encoding: \"unu data vol.nrrd | gunzip > vol.raw\"\n "
          "\n "
          "* Uses nrrdLoad with nio->skipData and nio->keepNrrdDataFileOpen both "
          "true in the NrrdIoState nio.");

static int
unu_dataMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  char *inS = NULL;
  hestOptAdd_1_String(&hopt, NULL, "nin", &inS, NULL, "input nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_dataInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  NrrdIoState *nio = nrrdIoStateNew();
  airMopAdd(mop, nio, nrrdIoStateNix_mop, airMopAlways);
  nio->skipData = AIR_TRUE;
  nio->keepNrrdDataFileOpen = AIR_TRUE;
  Nrrd *nin = nrrdNew();
  airMopAdd(mop, nin, nrrdNuke_mop, airMopAlways);

  if (nrrdLoad(nin, inS, nio)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error reading header:\n%s", me, err);
    airMopError(mop);
    return 1;
  }
  if (nrrdIoDataFNNumber(nio) > 1) {
    fprintf(stderr,
            "%s: sorry, currently can't operate with multiple "
            "detached datafiles\n",
            me);
    airMopError(mop);
    return 1;
  }
  if (!(nrrdFormatNRRD == nio->format)) {
    fprintf(stderr, "%s: can only print data of NRRD format files\n", me);
    airMopError(mop);
    return 1;
  }
  int car = fgetc(nio->dataFile);
#ifdef _MSC_VER
  /* needed because otherwise printing a carraige return will
     automatically also produce a newline */
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  while (EOF != car) {
    fputc(car, stdout);
    car = fgetc(nio->dataFile);
  }
  airFclose(nio->dataFile);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(data, INFO);
