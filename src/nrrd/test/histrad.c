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

#include "../nrrd.h"

const char *histradInfo = ("like unu histax, but for circles");

int
main(int argc, const char *argv[]) {
  const char *me = argv[0];
  hestOpt *hopt = NULL;
  Nrrd *nin;
  hestOptAdd(&hopt, "i", "nin", airTypeOther, 1, 1, &nin, NULL, "input image", NULL,
             NULL, nrrdHestNrrd);
  int bins[2];
  hestOptAdd(&hopt, "b", "rbins hbins", airTypeInt, 2, 2, bins, NULL,
             "# of histogram bins: radial and value");
  double vmin;
  hestOptAdd(&hopt, "min", "value", airTypeDouble, 1, 1, &vmin, "nan",
             "Value at low end of histogram. Defaults to lowest value "
             "found in input nrrd.");
  double vmax;
  hestOptAdd(&hopt, "max", "value", airTypeDouble, 1, 1, &vmax, "nan",
             "Value at high end of histogram. Defaults to highest value "
             "found in input nrrd.");
  double rmax;
  hestOptAdd(&hopt, "rmax", "max radius", airTypeDouble, 1, 1, &rmax, "nan",
             "largest radius to include in histogram");
  double cent[2];
  hestOptAdd(&hopt, "c", "center x, y", airTypeDouble, 2, 2, cent, NULL,
             "The center point around which to build radial histogram");
  char *outS;
  hestOptAdd(&hopt, "o", "filename", airTypeString, 1, 1, &outS, "-",
             "file to write histogram to");
  hestParseOrExit(hopt, argc - 1, argv + 1, NULL, me, histradInfo);
  airArray *mop = airMopNew();
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);

  if (2 != nin->dim) {
    fprintf(stderr, "%s: need 2-D input (not %u-D)\n", me, nin->dim);
    airMopError(mop);
    return 1;
  }

  char *err;
  int rbins = bins[0];
  int hbins = bins[1];
  Nrrd *nhist = nrrdNew();
  airMopAdd(mop, nhist, nrrdNuke_mop, airMopAlways);
  if (nrrdMaybeAlloc_va(nhist, nrrdTypeDouble, 2, AIR_SIZE_T(rbins),
                        AIR_SIZE_T(hbins))) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: couldn't allocate histogram:\n%s", me, err);
    airMopError(mop);
    return 1;
  }
  if (!(AIR_EXISTS(vmin) && AIR_EXISTS(vmax))) {
    NrrdRange *range = nrrdRangeNewSet(nin, nrrdStateBlind8BitRange);
    airMopAdd(mop, range, nrrdRangeNix_mop, airMopAlways);
    vmin = AIR_EXISTS(vmin) ? vmin : range->min;
    vmax = AIR_EXISTS(vmax) ? vmax : range->max;
  }

#define DIST(x0, y0, x1, y1) (sqrt((x0 - x1) * (x0 - x1) + (y0 - y1) * (y0 - y1)))

  int sx = AIR_INT(nin->axis[0].size); /* HEY unsigned? */
  int sy = AIR_INT(nin->axis[1].size);
  if (!AIR_EXISTS(rmax)) {
    rmax = 0;
    rmax = AIR_MAX(rmax, DIST(cent[0], cent[1], 0, 0));
    rmax = AIR_MAX(rmax, DIST(cent[0], cent[1], sx - 1, 0));
    rmax = AIR_MAX(rmax, DIST(cent[0], cent[1], 0, sy - 1));
    rmax = AIR_MAX(rmax, DIST(cent[0], cent[1], sx - 1, sy - 1));
  }

  double (*lup)(const void *v, size_t I) = nrrdDLookup[nin->type];
  double *hist = (double *)(nhist->data);
  for (int xi = 0; xi < sx; xi++) {
    for (int yi = 0; yi < sy; yi++) {
      double rad = DIST(cent[0], cent[1], xi, yi);
      if (!AIR_IN_OP(0, rad, rmax)) {
        continue;
      }
      double val = lup(nin->data, xi + sx * yi);
      if (!AIR_IN_OP(vmin, val, vmax)) {
        continue;
      }
      int ridx = airIndex(0, rad, rmax, rbins);
      int hidx = airIndex(vmin, val, vmax, hbins);
      hist[ridx + rbins * hidx] += 1;
    }
  }

  nhist->axis[0].min = 0;
  nhist->axis[0].max = rmax;
  nhist->axis[1].min = vmin;
  nhist->axis[1].max = vmax;
  if (nrrdSave(outS, nhist, NULL)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: couldn't save output:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  exit(0);
}
