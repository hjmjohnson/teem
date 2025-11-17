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

#define INFO "Remove one or more singleton axes from a nrrd"
static const char *_unrrdu_axdeleteInfoL
  = (INFO ". Singleton axes have only a single sample along them. "
          "The underlying linear ordering of the samples is "
          "unchanged, and the information about the other axes is "
          "shifted downwards as needed.\n "
          "* Uses nrrdAxesDelete");

static int
unrrdu_axdeleteMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  /* really needs to be signed int, because of hack */
  int _axis;
  hestOptAdd_1_Int(&hopt, "a,axis", "axis", &_axis, NULL,
                   "dimension (axis index) of the axis to remove. As a total hack, if "
                   "you give -1 as the axis, this will do a matlab-style \"squeeze\", "
                   "in which any and all singleton axes are removed.");
  Nrrd *nin;
  OPT_ADD_NIN(nin, "input nrrd");
  char *out;
  OPT_ADD_NOUT(out, "output nrrd");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, _unrrdu_axdeleteInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, (airMopper)nrrdNuke, airMopAlways);

  if (_axis < -1) {
    fprintf(stderr, "%s: given -a axis %d negative but not -1 for squeeze\n", me, _axis);
    airMopError(mop);
    return 1;
  } else if (-1 == _axis) {
    Nrrd *ntmp = nrrdNew();
    airMopAdd(mop, ntmp, (airMopper)nrrdNuke, airMopAlways);
    if (nrrdCopy(nout, nin)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error copying axis:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
    uint axis;
    for (axis = 0; axis < nout->dim && nout->axis[axis].size > 1; axis++)
      ;
    while (axis < nout->dim) {
      if (nrrdAxesDelete(ntmp, nout, axis) || nrrdCopy(nout, ntmp)) {
        airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
        fprintf(stderr, "%s: error deleting axis:\n%s", me, err);
        airMopError(mop);
        return 1;
      }
      for (axis = 0; axis < nout->dim && nout->axis[axis].size > 1; axis++)
        ;
    }
  } else {
    if (nrrdAxesDelete(nout, nin, AIR_UINT(_axis))) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: error deleting axis:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
  }

  SAVE(out, nout, NULL);

  airMopOkay(mop);
  return 0;
}

UNRRDU_CMD(axdelete, INFO);
