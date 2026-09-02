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

#define INFO "Connect slices and/or slabs into a bigger nrrd"
static const char *unu_joinInfoL
  = (INFO ". Can stich images into volumes, or tile images side "
          "by side, or attach images onto volumes.  If there are many many "
          "files to name in the \"-i\" option, and using wildcards won't work, "
          "consider putting the list of "
          "filenames into a separate text file (e.g. \"slices.txt\"), and then "
          "name this file as a response file (e.g. \"-i @slices.txt\"). "
          "This command now allows you to set the same pieces of information that "
          "previously had to be set with \"unu axinfo\": label, spacing, and min/max. "
          "These can be use whether the join axis is new (because of \"-incr\") or "
          "not.\n "
          "* Uses nrrdJoin");

static int
unu_joinMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  hestOpt *hopt = NULL;
  Nrrd **nin;
  uint ninLen;
  hestOptAdd_Mv_Other(&hopt, "i,input", "nin0", 1, -1, &nin, NULL,
                      "everything to be joined together", &ninLen, nrrdHestNrrdNoTTY);
  uint axis;
  MY_OPT_ADD_AXIS(axis, "axis to join along");
  int incrDim;
  hestOptAdd_Flag(&hopt, "incr", &incrDim,
                  "in situations where the join axis is *not* among the existing "
                  "axes of the input nrrds, then this flag signifies that the join "
                  "axis should be *inserted*, and the output dimension should "
                  "be one greater than input dimension.  Without this flag, the "
                  "nrrds are joined side-by-side, along an existing axis.");
  char *label;
  hestOptAdd_1_String(&hopt, "l,label", "label", &label, "",
                      "label to associate with join axis");
  char *kindStr;
  hestOptAdd_1_String(&hopt, "k,kind", "kind", &kindStr, "",
                      "kind to set on join axis. "
                      "Not using this option leaves the kind as is");
  double mm[2];
  hestOptAdd_2_Double(&hopt, "mm,minmax", "min max", mm, "nan nan",
                      "min and max values along join axis");
  double spc;
  hestOptAdd_1_Double(&hopt, "sp,spacing", "spc", &spc, "nan",
                      "spacing between samples along join axis");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");
  AIR_UNUSED(outHidx); // TODO handle history

  // (boilerplate for YES some hparm modification)
  hestParm *hparm = hestParmCopy(_hparm);
  hparm->responseFileEnable = AIR_TRUE;
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_joinInfoL);
  airArray *mop = unrrduMopNew(hopt, hparm);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (nrrdJoin(nout, AIR_CAST(const Nrrd *const *, nin), ninLen, axis, incrDim)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: error joining nrrds:\n%s", me, err);
    airMopError(mop);
    return 1;
  }
  if (strlen(label)) {
    nout->axis[axis].label = (char *)airFree(nout->axis[axis].label);
    nout->axis[axis].label = airStrdup(label);
  }
  int kind;
  if (airStrlen(kindStr)) {
    if (!strcmp("none", kindStr) || !strcmp("???", kindStr)) {
      kind = nrrdKindUnknown;
    } else {
      if (!(kind = airEnumVal(nrrdKind, kindStr))) {
        fprintf(stderr, "%s: couldn't parse \"%s\" as %s\n", me, kindStr,
                nrrdKind->name);
        airMopError(mop);
        return 1;
      }
    }
    nout->axis[axis].kind = kind;
  }
  if (AIR_EXISTS(mm[0])) {
    nout->axis[axis].min = mm[0];
  }
  if (AIR_EXISTS(mm[1])) {
    nout->axis[axis].max = mm[1];
  }
  if (AIR_EXISTS(spc)) {
    nout->axis[axis].spacing = spc;
  }

  MY_SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(join, INFO);
