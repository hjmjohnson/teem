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

#define INFO "Cheap histogram-based median/mode filtering"
static const char *unu_cmedianInfoL
  = (INFO ". Only works on 1, 2, or 3 dimensions.  The window "
          "over which filtering is done is always square, and "
          "only a simplistic weighting scheme is available. "
          "The method is cheap because it does the median "
          "or mode based on a histogram, which enforces a quantization to the number "
          "of bins in the histogram, which probably means a loss of precision for "
          "anything except 8-bit data.  Also, integral values can be recovered "
          "exactly only when the number of bins is exactly min-max+1 (as reported "
          "by \"unu minmax\").\n "
          "* Uses nrrdCheapMedian, plus nrrdSlice and nrrdJoin in "
          "case of \"-c\"");

static int
unu_cmedianMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  hestOpt *hopt = NULL;
  uint radius;
  hestOptAdd_1_UInt(&hopt, "r,radius", "radius", &radius, "1",
                    "how big a window to filter over. \"-r 1\" leads to a "
                    "3x3 window in an image, and a 3x3x3 window in a volume");
  int mode;
  hestOptAdd_Flag(&hopt, "mode", &mode,
                  "By default, median filtering is done.  Using this option "
                  "enables mode filtering, in which the most common value is "
                  "used as output");
  uint bins;
  hestOptAdd_1_UInt(&hopt, "b,bins", "num", &bins, "256",
                    "# of bins in histogram.  It is in your interest to minimize "
                    "this number, since big histograms mean slower execution "
                    "times.  8-bit data needs at most 256 bins.");
  float wght;
  hestOptAdd_1_Float(&hopt, "w,weight", "weight", &wght, "1.0",
                     "How much higher to preferentially weight samples that are "
                     "closer to the center of the window.  \"1.0\" weight means that "
                     "all samples are uniformly weighted over the window, which "
                     "facilitates a simple speed-up. ");
  /* FYI: these are weights which are just high enough to preserve
     an island of N contiguous high pixels in a row:
     1: 7.695
     2: 6.160
     3: 4.829 (actually only the middle pixel remains */
  int pad;
  hestOptAdd_Flag(&hopt, "p,pad", &pad,
                  "Pad the input (with boundary method \"bleed\"), "
                  "and crop the output, so as to "
                  "overcome our cheapness and correctly "
                  "handle the border.  Obviously, this takes more memory.");
  int chan;
  hestOptAdd_Flag(&hopt, "c,channel", &chan,
                  "Slice the input along axis 0, run filtering on all slices, "
                  "and join the results back together.  This is the way you'd "
                  "want to process color (multi-channel) images or volumes.");
  int verberr;
  hestOptAdd_Flag(&hopt, "ve,verberr", &verberr,
                  "Filtering 3D and 4D data is slow, so progress information can be "
                  "useful. The standard place to send verbose diagnostics is stdout, "
                  "but because unu commands are typically piped, we defy conventions "
                  "and (if this \"-ve\" option is used) we print it to stderr");
  Nrrd *nin;
  hestOptAdd_1_Other(&hopt, "i,input", "nin", &nin, "-", "input nrrd",
                     nrrdHestNrrdNoTTY);
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for YES some hparm modification)
  hestParm *hparm = hestParmCopy(_hparm);
  hparm->noArgsIsNoProblem = AIR_TRUE; // thanks to nrrdHestNrrdNoTTY
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_cmedianInfoL);
  airArray *mop = unrrduMopNew(hopt, hparm);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (chan) {
    uint nsize = AIR_UINT(nin->axis[0].size);
    Nrrd **mnout = AIR_CALLOC(nsize, Nrrd *);
    airMopAdd(mop, mnout, airFree, airMopAlways);
    Nrrd *ntmp = nrrdNew();
    airMopAdd(mop, ntmp, nrrdNuke_mop, airMopAlways);
    for (uint ni = 0; ni < nsize; ni++) {
      if (nrrdSlice(ntmp, nin, 0, ni)) {
        airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
        fprintf(stderr, "%s: error slicing input at pos = %u:\n%s", me, ni, err);
        airMopError(mop);
        return 1;
      }
      airMopAdd(mop, mnout[ni] = nrrdNew(), nrrdNuke_mop, airMopAlways);
      if (nrrdCheapMedian(mnout[ni], ntmp, pad, mode, radius, wght, bins, verberr)) {
        airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
        fprintf(stderr, "%s: error doing cheap median:\n%s", me, err);
        airMopError(mop);
        return 1;
      }
    }
    if (nrrdJoin(nout, (const Nrrd *const *)mnout, nsize, 0, AIR_TRUE)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error doing final join:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
    nrrdAxisInfoCopy(nout, nin, NULL, NRRD_AXIS_INFO_NONE);
    if (nrrdBasicInfoCopy(nout, nin,
                          NRRD_BASIC_INFO_DATA_BIT /* */
                            | NRRD_BASIC_INFO_TYPE_BIT | NRRD_BASIC_INFO_BLOCKSIZE_BIT
                            | NRRD_BASIC_INFO_DIMENSION_BIT | NRRD_BASIC_INFO_CONTENT_BIT
                            | NRRD_BASIC_INFO_COMMENTS_BIT
                            | (nrrdStateKeyValuePairsPropagate
                                 ? 0
                                 : NRRD_BASIC_INFO_KEYVALUEPAIRS_BIT))) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error copying basic info:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
  } else {
    if (nrrdCheapMedian(nout, nin, pad, mode, radius, wght, bins, verberr)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error doing cheap median:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
  }

  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(cmedian, INFO);
