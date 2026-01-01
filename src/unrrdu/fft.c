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

#define INFO "Fast Fourier Transform of selected axes"
static const char *unu_fftInfoL_yes = (INFO ". Powered by the FFTW3 library.\n "
                                            "* Uses nrrdFFT");

static const char *unu_fftInfoL_no
  = (INFO ". This Teem has NOT been compiled with FFTW3 <http://www.fftw.org/>. "
          "If it had been, "
          "this would be a command-line interface to that functionality. "
          "There is currently no non-FFTW implementation of the FFT available.\n "
          "* Uses nrrdFFT");

/* We create an airEnum to parse the "forward" and "backwards" values
   needed to specify which kind of transform to run */

static const char *dirStr[] = {"(unknown direction)", "forward", "backward"};

static const char *dirDesc[] = {"unknown direction", "forward transform",
                                "backward (inverse) transform"};

/*  from fftw3.h
#define FFTW_FORWARD (-1)
#define FFTW_BACKWARD (+1)
*/

#define FORW (-1)
#define BACK (+1)

static const int dirVal[] = {0, FORW, BACK};

static const char *dirStrEqv[] = {"f",        "forw", "forward", "b",       "back",
                                  "backward", "i",    "inv",     "inverse", ""};

static const int dirValEqv[] = {FORW, FORW, FORW, BACK, BACK, BACK, BACK, BACK, BACK};

static const airEnum dir_ae = {"direction", 2,         dirStr,    dirVal,
                               dirDesc,     dirStrEqv, dirValEqv, AIR_FALSE};

static const airEnum *const direction_ae = &dir_ae;

static int
unu_fftMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
#if 0
  { // checking on the private airEnums
    char eerr[AIR_STRLEN_LARGE + 1];
    if (airEnumCheck(eerr, direction_ae)) {
      fprintf(stderr, "%s: problem with enum \"%s\":\n%s", __func__, direction_ae->name,
              eerr);
      exit(1); /* inside `#if 0` for airEnum checking */
    }
    fprintf(stderr, "%s: %s enum ok\n", __func__, direction_ae->name);
  }
#endif
  hestOpt *hopt = NULL;
  int sign;
  hestOptAdd_1_Enum(&hopt, NULL, "dir", &sign, NULL,
                    "forward (\"forw\", \"f\") or backward/inverse "
                    "(\"back\", \"b\") transform ",
                    direction_ae);
  uint *axes, axesLen;
  hestOptAdd_Nv_UInt(&hopt, "a,axes", "ax0", 1, -1, &axes, NULL,
                     "the one or more axes that should be transformed", &axesLen);
  int rigor;
  hestOptAdd_1_Enum(&hopt, "pr,planrigor", "pr", &rigor, "est",
                    "rigor with which fftw plan is constructed. Options include:\n "
                    "\b\bo \"e\", \"est\", \"estimate\": only an estimate\n "
                    "\b\bo \"m\", \"meas\", \"measure\": standard amount of "
                    "measurements of system properties\n "
                    "\b\bo \"p\", \"pat\", \"patient\": slower, more measurements\n "
                    "\b\bo \"x\", \"ex\", \"exhaustive\": slowest, most measurements",
                    nrrdFFTWPlanRigor);
  int rescale;
  hestOptAdd_1_Bool(&hopt, "r,rescale", "bool", &rescale, "true",
                    "scale fftw output (by sqrt(1/N)) so that forward and backward "
                    "transforms will get back to original values");
  char *wispath;
  hestOptAdd_1_String(&hopt, "w,wisdom", "filename", &wispath, "",
                      "A filename here is used to read in fftw wisdom (if the file "
                      "exists already), and is used to save out updated wisdom "
                      "after the transform.  By default (not using this option), "
                      "no wisdom is read or saved. Note: no wisdom is gained "
                      "(that is, learned by FFTW) with planning rigor \"estimate\".");
  Nrrd *_nin;
  MY_OPT_ADD_NIN(_nin, "input nrrd");
  int realInput;
  hestOptAdd_Flag(&hopt, "ri,realinput", &realInput,
                  "input is real-valued, so insert new length-2 axis 0 "
                  "and set complex component to 0.0.  Axes to transform "
                  "(indicated by \"-a\") will be incremented accordingly.");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me,
                  (nrrdFFTWEnabled ? unu_fftInfoL_yes : unu_fftInfoL_no));
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  Nrrd *nin = NULL;
  if (realInput) {
    Nrrd *ntmp = nrrdNew();
    airMopAdd(mop, ntmp, nrrdNuke_mop, airMopAlways);
    if (nrrdAxesInsert(ntmp, _nin, 0)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error creating complex axis:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
    nin = nrrdNew();
    airMopAdd(mop, nin, nrrdNuke_mop, airMopAlways);
    ptrdiff_t minPad[NRRD_DIM_MAX], maxPad[NRRD_DIM_MAX];
    minPad[0] = 0;
    maxPad[0] = 1;
    for (uint axi = 1; axi < ntmp->dim; axi++) {
      minPad[axi] = 0;
      maxPad[axi] = AIR_CAST(ptrdiff_t, ntmp->axis[axi].size - 1);
    }
    if (nrrdPad_nva(nin, ntmp, minPad, maxPad, nrrdBoundaryPad, 0.0)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error padding out complex axis:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
    /* increment specified axes to transform */
    for (uint axi = 0; axi < axesLen; axi++) {
      axes[axi]++;
    }
    /* ntmp is really done with, we can free up the space now; this
       is one of the rare times we want airMopSub */
    airMopSub(mop, ntmp, nrrdNuke_mop);
    nrrdNuke(ntmp);
  } else {
    /* input is apparently already complex */
    nin = _nin;
  }

  FILE *fwise;
  if (airStrlen(wispath) && nrrdFFTWEnabled) {
    fwise = fopen(wispath, "r");
    if (fwise) {
      if (nrrdFFTWWisdomRead(fwise)) {
        airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
        fprintf(stderr, "%s: error with fft wisdom:\n%s", me, err);
        airMopError(mop);
        return 1;
      }
      fclose(fwise);
    } else {
      fprintf(stderr,
              "%s: (\"%s\" couldn't be opened, will try to save "
              "wisdom afterwards)",
              me, wispath);
    }
  }

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (nrrdFFT(nout, nin, axes, axesLen, sign, rescale, rigor)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error with fft:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  if (airStrlen(wispath) && nrrdFFTWEnabled) {
    if (!(fwise = fopen(wispath, "w"))) {
      fprintf(stderr, "%s: couldn't open %s for writing: %s\n", me, wispath,
              strerror(errno));
      airMopError(mop);
      return 1;
    }
    if (nrrdFFTWWisdomWrite(fwise)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error with fft wisdom:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
    fclose(fwise);
  }

  MY_HISTORY(nout, nin);
  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(fft, INFO);
