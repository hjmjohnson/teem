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

/*
** Tests:
** nrrdOriginCalculate
**
** The "all axes have a min" test read axis[0]->min for every axis instead of
** axis[ai]->min.  With a min on axis 0 but not on later axes, the function
** wrongly reported nrrdOriginStatusOkay and computed an origin from the
** non-existent (NaN) mins.  A partial set of axis mins must report
** nrrdOriginStatusNoMin.
*/

int
main(int argc, const char **argv) {
  const char *me;
  Nrrd *nrrd;
  airArray *mop;
  unsigned int axisIdx[3] = {0, 1, 2};
  double origin[3];
  int status;

  AIR_UNUSED(argc);
  me = argv[0];
  mop = airMopNew();
  nrrd = nrrdNew();
  airMopAdd(mop, nrrd, nrrdNuke_mop, airMopAlways);

  if (nrrdMaybeAlloc_va(nrrd, nrrdTypeUChar, 3, AIR_SIZE_T(2), AIR_SIZE_T(2),
                        AIR_SIZE_T(2))) {
    airMopAdd(mop, biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble allocating\n", me);
    airMopError(mop);
    return 1;
  }
  /* min on axis 0 only; axes 1 and 2 have no min */
  nrrd->axis[0].min = 0.0;
  nrrd->axis[0].spacing = 1.0;
  nrrd->axis[1].min = AIR_NAN;
  nrrd->axis[1].spacing = 1.0;
  nrrd->axis[2].min = AIR_NAN;
  nrrd->axis[2].spacing = 1.0;

  status = nrrdOriginCalculate(nrrd, axisIdx, 3, nrrdCenterCell, origin);
  if (nrrdOriginStatusNoMin != status) {
    fprintf(stderr, "%s: partial axis mins gave status %d, expected NoMin (%d)\n", me,
            status, nrrdOriginStatusNoMin);
    airMopError(mop);
    return 1;
  }
  fprintf(stderr, "%s: partial axis mins correctly reported NoMin\n", me);
  airMopOkay(mop);
  return 0;
}
