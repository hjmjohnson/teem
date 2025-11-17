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

#define INFO "Normalizes array orientation and meta-data"
static const char *_unrrdu_dnormInfoL
  = (INFO ". Forces information about kind and orientation into "
          "a consistent form, and nixes various other fields. This was "
          "originally created as a utility for the Diderot project "
          "(http://diderot-language.cs.uchicago.edu), hence the name, "
          "but it has proven useful in other contexts (uses of gage) in which "
          "it is nice to have standardized orientation information.\n "
          "* Uses nrrdMetaDataNormalize");

static int
unrrdu_dnormMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  hestOpt *hopt = NULL;
  int headerOnly;
  hestOptAdd_Flag(&hopt, "h,header", &headerOnly,
                  "output header of nrrd file only, not the data itself");
  int version;
  hestOptAdd_1_Enum(&hopt, "v,version", "version", &version, "alpha",
                    "what version of canonical meta-data to convert to; "
                    "\"alpha\" is what has been used for Diderot until at least "
                    "2016",
                    nrrdMetaDataCanonicalVersion);
  int trivialOrient, recenter;
  hestOptAdd_Flag(&hopt, "to", &trivialOrient,
                  "(*t*rivial *o*rientation) "
                  "even if the input nrrd comes with full orientation or "
                  "per-axis min-max info, ignore it and instead assert the "
                  "identity mapping between index and world space");
  hestOptAdd_Flag(&hopt, "rc,recenter", &recenter,
                  "re-locate output spaceOrigin so that field is centered "
                  "around origin of space coordinates");
  double newSpacing;
  hestOptAdd_1_Double(&hopt, "sp,spacing", "scl", &newSpacing, "1.0",
                      "when having to contrive orientation information and there's "
                      "no per-axis min/max or spacing, this is the sample spacing "
                      "to assert");
  Nrrd *nin;
  hestOptAdd_1_Other(&hopt, "i,input", "nin", &nin, "-",
                     "input image. By default reads from stdin", nrrdHestNrrdNoTTY);
  char *outS;
  OPT_ADD_NOUT(outS, "output filename");

  // (boilerplate for YES some hparm modification)
  hestParm *hparm = hestParmCopy(_hparm);
  hparm->noArgsIsNoProblem = AIR_TRUE; // thanks to nrrdHestNrrdNoTTY
  hestParseOrExit(hopt, argc, argv, hparm, me, _unrrdu_dnormInfoL);
  airArray *mop = unrrduMopNew(hopt, hparm);
  char *err;

  Nrrd *nout = NULL;
  if (headerOnly) {
    /* no reason to duplicate data */
    nout = nin;
  } else {
    nout = nrrdNew();
    airMopAdd(mop, nout, (airMopper)nrrdNuke, airMopAlways);
  }

  int lostmf;
  if (nrrdMetaDataNormalize(nout, nin, version, trivialOrient,
                            AIR_FALSE /* permuteComponentAxisFastest */, recenter,
                            newSpacing, &lostmf)) {
    airMopAdd(mop, err = biffGet(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  if (lostmf) {
    fprintf(stderr,
            "%s: WARNING: input array measurement frame "
            "will be erased on output.\n",
            me);
  }
  NrrdIoState *nio = nrrdIoStateNew();
  airMopAdd(mop, nio, (airMopper)nrrdIoStateNix, airMopAlways);
  /* disable printing comments about NRRD format URL */
  nio->skipFormatURL = AIR_TRUE;
  if (headerOnly) {
    nio->skipData = AIR_TRUE;
  }
  if (nrrdSave(outS, nout, nio)) {
    airMopAdd(mop, err = biffGet(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble saving \"%s\":\n%s", me, outS, err);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  return 0;
}

UNRRDU_CMD(dnorm, INFO);
