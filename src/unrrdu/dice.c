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

#define INFO "Save all slices along one axis into separate files"
static const char *unu_diceInfoL
  = (INFO ". Calls \"unu slice\" for each position "
          "along the indicated axis, and saves out a different "
          "file for each sample along that axis.\n "
          "* Uses repeated calls to nrrdSlice and nrrdSave");

static int
unu_diceMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  uint axis;
  MY_OPT_ADD_AXIS(axis, "axis to slice along");
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  AIR_UNUSED(inHidx); // TODO? save history of multiple outs; how?
  uint start;
  hestOptAdd_1_UInt(&hopt, "s,start", "start", &start, "0",
                    "integer value to start numbering with");
  char *ftmpl; /* format template */
  hestOptAdd_1_String(&hopt, "ff,format", "form", &ftmpl, "",
                      "a printf-style format to use for generating all "
                      "filenames.  Use this to override the number of characters "
                      "used to represent the slice position, or the file format "
                      "of the output, e.g. \"-ff %03d.ppm\" for 000.ppm, "
                      "001.ppm, etc. By default (not using this option), slices "
                      "are saved in NRRD format (or PNM or PNG where possible) "
                      "with shortest possible filenames.");
  // the fact that we're using uint instead of size_t is its own kind of sanity check
  uint sanity;
  hestOptAdd_1_UInt(&hopt, "l,limit", "max#", &sanity, "999",
                    "a sanity check on how many slice files should be saved "
                    "out, to prevent accidentally dicing the wrong axis "
                    "or the wrong array. Can raise this value if needed.");
  char *base;
  hestOptAdd_1_String(&hopt, "o,output", "prefix", &base, NULL,
                      "output filename prefix (excluding info set via \"-ff\"), "
                      "basically to set path of output files (so be sure to end "
                      "with \"/\".");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_diceInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  if (!(axis < nin->dim)) {
    fprintf(stderr, "%s: given axis (%u) outside range [0,%u]\n", me, axis,
            nin->dim - 1);
    airMopError(mop);
    return 1;
  }
  if (nin->axis[axis].size > sanity) {
    fprintf(stderr, "%s: axis %u size %zu > sanity limit %u; increase via \"-l\"\n", me,
            axis, nin->axis[axis].size, sanity);
    airMopError(mop);
    return 1;
  }
  uint size = AIR_UINT(nin->axis[axis].size);
  /* (getting this just for the verbose value) */
  NrrdIoState *nio = nrrdIoStateNew();
  airMopAdd(mop, nio, nrrdIoStateNix_mop, airMopAlways);

  /* HEY: this should use nrrdSaveMulti(), and if there's additional
     smarts here, they should be moved into nrrdSaveMulti() */
  char fffname[AIR_STRLEN_MED + 1]; /* format for filename */
  if (airStrlen(ftmpl)) {
    if (!(nrrdContainsPercentThisAndMore(ftmpl, 'd')
          || nrrdContainsPercentThisAndMore(ftmpl, 'u'))) {
      fprintf(stderr,
              "%s: given filename format \"%s\" doesn't seem to "
              "have the conversion specification to print an integer\n",
              me, ftmpl);
      airMopError(mop);
      return 1;
    }
    snprintf(fffname, sizeof(fffname), "%%s%s", ftmpl);
  } else {
    uint dignum = 0, tmps, top = start + size - 1;
    tmps = top;
    do {
      dignum++;
      tmps /= 10;
    } while (tmps);
    /* sprintf the number of digits into the string that will be used
       to sprintf the slice number into the filename with e.g. "%03u.nrrd" */
    snprintf(fffname, sizeof(fffname), "%%s%%0%uu.nrrd", dignum);
  }
  size_t fn_len = strlen(fffname);
  if (fn_len + 1 == sizeof(fffname)) {
    fprintf(stderr, "%s: filename format \"%s\" may have exceeded %zu-size buffer\n", me,
            fffname, sizeof(fffname));
    airMopError(mop);
    return 1;
  }

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  for (uint pos = 0; pos < size; pos++) {
    if (nrrdSlice(nout, nin, axis, pos)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error slicing nrrd:%s\n", me, err);
      airMopError(mop);
      return 1;
    }
    if (0 == pos && !airStrlen(ftmpl)) {
      /* See if these slices would be better saved as PNG or PNM images. Altering the
         file name (to longer end with ".nrrd") will tell nrrdSave() to use a different
         file format.
         We wait till now to check this so that we can work from the actual slice. */
      char *fn_end = fffname + fn_len - 4;
      size_t fn_size = sizeof(fffname) - fn_len;
      if (nrrdFormatPNG->fitsInto(nout, nrrdEncodingRaw, AIR_FALSE)) {
        airStrcpy(fn_end, fn_size, "png");
      } else {
        int fit = nrrdFormatPNM->fitsInto(nout, nrrdEncodingRaw, AIR_FALSE);
        if (2 == fit) {
          airStrcpy(fn_end, fn_size, "pgm");
        } else if (3 == fit) {
          airStrcpy(fn_end, fn_size, "ppm");
        }
      }
    }
    char fnout[AIR_STRLEN_MED + 1]; /* file name out */
    snprintf(fnout, sizeof(fnout), fffname, base, pos + start);
    if (nio->verbose > 0) {
      fprintf(stderr, "%s: %s ...\n", me, fnout);
    }
    if (nrrdSave(fnout, nout, NULL)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error writing nrrd to \"%s\":%s\n", me, fnout, err);
      airMopError(mop);
      return 1;
    }
  }

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(dice, INFO);
