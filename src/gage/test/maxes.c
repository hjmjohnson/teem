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

#include "../gage.h"

typedef unsigned int uint; /* uint is just more concise */

void
maxes1(Nrrd *nout, const Nrrd *nin) {
  double (*lup)(const void *, size_t), (*ins)(void *, size_t, double);

  lup = nrrdDLookup[nin->type];
  ins = nrrdDInsert[nout->type];
  int sx = AIR_UINT(nin->axis[0].size);
  for (int xi = 0; xi < sx; xi++) {
    int ismax = AIR_TRUE;
    double val = lup(nin->data, xi);
    for (int xo = -1; xo <= 1; xo += 2) {
      uint xd = !xi && xo < 0 ? sx - 1 : AIR_MOD(xi + xo, AIR_INT(sx));
      ismax &= lup(nin->data, xd) < val;
      if (!ismax) break;
    }
    ins(nout->data, xi, ismax);
  }
  return;
}

void
maxes2(Nrrd *nout, const Nrrd *nin) {
  double (*lup)(const void *, size_t), (*ins)(void *, size_t, double);

  lup = nrrdDLookup[nin->type];
  ins = nrrdDInsert[nout->type];
  int sx = AIR_UINT(nin->axis[0].size);
  int sy = AIR_UINT(nin->axis[1].size);
  for (int yi = 0; yi < sy; yi++) {
    for (int xi = 0; xi < sx; xi++) {
      int ismax = AIR_TRUE;
      double val = lup(nin->data, xi + sx * yi);
      for (int yo = -1; yo <= 1; yo++) {
        uint yd = !yi && yo < 0 ? sy - 1 : AIR_MOD(yi + yo, sy);
        for (int xo = -1; xo <= 1; xo++) {
          if (!xo && !yo) continue;
          uint xd = !xi && xo < 0 ? sx - 1 : AIR_MOD(xi + xo, sx);
          ismax &= lup(nin->data, xd + sx * yd) < val;
        }
      }
      ins(nout->data, xi + sx * yi, ismax);
    }
  }
  return;
}

void
maxes3(Nrrd *nout, const Nrrd *nin) {
  double (*lup)(const void *, size_t), (*ins)(void *, size_t, double);

  lup = nrrdDLookup[nin->type];
  ins = nrrdDInsert[nout->type];
  int sx = AIR_UINT(nin->axis[0].size);
  int sy = AIR_UINT(nin->axis[1].size);
  int sz = AIR_UINT(nin->axis[2].size);
  for (int zi = 0; zi < sz; zi++) {
    for (int yi = 0; yi < sy; yi++) {
      for (int xi = 0; xi < sx; xi++) {
        int ismax = AIR_TRUE;
        double val = lup(nin->data, xi + sx * (yi + sy * zi));
        for (int zo = -1; zo <= 1; zo++) {
          uint zd = !zi && zo < 0 ? sz - 1 : AIR_MOD(zi + zo, sz);
          for (int yo = -1; yo <= 1; yo++) {
            uint yd = !yi && yo < 0 ? sy - 1 : AIR_MOD(yi + yo, sy);
            for (int xo = -1; xo <= 1; xo++) {
              if (!xo && !yo && !zo) continue;
              uint xd = !xi && xo < 0 ? sx - 1 : AIR_MOD(xi + xo, sx);
              ismax &= lup(nin->data, xd + sx * (yd + sy * zd)) < val;
            }
          }
        }
        ins(nout->data, xi + sx * (yi + sy * zi), ismax);
      }
    }
  }
  return;
}

char *maxesInfo = ("sets point-wise local maxima to 1, else 0");

int
main(int argc, const char *argv[]) {
  const char *me;
  char *outS;
  hestOpt *hopt;
  hestParm *hparm;
  airArray *mop;

  char *err;
  Nrrd *nin, *nout;

  me = argv[0];
  mop = airMopNew();
  hparm = hestParmMopNew();
  airMopAdd(mop, hparm, (airMopper)hestParmFree, airMopAlways);
  hopt = NULL;
  hestOptAdd(&hopt, "i", "nin", airTypeOther, 1, 1, &nin, NULL, "input image", NULL,
             NULL, nrrdHestNrrd);
  hestOptAdd(&hopt, "o", "filename", airTypeString, 1, 1, &outS, NULL, "output volume");
  hestParseOrExit(hopt, argc - 1, argv + 1, hparm, me, maxesInfo);
  airMopAdd(mop, hopt, (airMopper)hestOptFree, airMopAlways);
  airMopAdd(mop, hopt, (airMopper)hestParseFree, airMopAlways);

  if (!AIR_IN_CL(1, nin->dim, 3)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: nin->dim %u not 1, 2, or 3", me, nin->dim);
    airMopError(mop);
    return 1;
  }

  nout = nrrdNew();
  airMopAdd(mop, nout, (airMopper)nrrdNuke, airMopAlways);
  if (nrrdCopy(nout, nin)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: couldn't allocate output:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  if (1 == nin->dim) {
    maxes1(nout, nin);
  } else if (2 == nin->dim) {
    maxes2(nout, nin);
  } else {
    maxes3(nout, nin);
  }

  if (nrrdSave(outS, nout, NULL)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: couldn't save output:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  exit(0);
}
