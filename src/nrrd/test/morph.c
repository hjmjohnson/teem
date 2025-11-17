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

#include "../nrrd.h"

const char *morphInfo = ("testing a hacky approach to gray-scale morphology");

int
morph(Nrrd *nout, Nrrd *_nin, Nrrd *_nkern, float scl) {
  static const char me[] = "morph";
  airArray *mop;
  Nrrd *nin, *nkern;
  int sx, sy, sz, kd, kr, xx, yy, zz, ii, jj, kk;
  float *in, *out, *kern;

  mop = airMopNew();
  if (nrrdTypeFloat == _nin->type) {
    nin = _nin;
  } else {
    nin = nrrdNew();
    airMopAdd(mop, nin, nrrdNuke_mop, airMopAlways);
    if (nrrdConvert(nin, _nin, nrrdTypeFloat)) {
      biffAddf(NRRD, "%s: trouble 1", me);
      airMopError(mop);
      return 1;
    }
  }
  if (nrrdTypeFloat == _nkern->type) {
    nkern = _nkern;
  } else {
    nkern = nrrdNew();
    airMopAdd(mop, nkern, nrrdNuke_mop, airMopAlways);
    if (nrrdConvert(nkern, _nkern, nrrdTypeFloat)) {
      biffAddf(NRRD, "%s: trouble 2", me);
      airMopError(mop);
      return 1;
    }
  }
  if (nrrdCopy(nout, nin)) {
    biffAddf(NRRD, "%s: trouble allocating output", me);
    airMopError(mop);
    return 1;
  }
  in = AIR_CAST(float *, nin->data);
  out = AIR_CAST(float *, nout->data);
  kern = AIR_CAST(float *, nkern->data);
  sx = AIR_INT(nin->axis[0].size); /* HEY unsigned */
  sy = AIR_INT(nin->axis[1].size);
  sz = AIR_INT(nin->axis[2].size);
  kd = AIR_INT(nkern->axis[0].size);
  kr = kd / 2; /* 5 -> 2 */

  /* min_i (f(x+i)- k(i)) */
  for (zz = kr; zz < sz - kr; zz++) {
    fprintf(stderr, "%d/%d\n", zz, sz - kr);
    for (yy = kr; yy < sy - kr; yy++) {
      for (xx = kr; xx < sx - kr; xx++) {
        float mind, ival, kval;
        /*
        int verb;

        verb = ((24 == xx && 24 == yy && 9 == zz) ||
                (24 == xx && 24 == yy && 10 == zz));
        */
        mind = AIR_POS_INF;
        for (kk = -kr; kk <= kr; kk++) {
          for (jj = -kr; jj <= kr; jj++) {
            for (ii = -kr; ii <= kr; ii++) {
              ival = in[xx + ii + sx * (yy + jj + sy * (zz + kk))];
              kval = kern[ii + kr + kd * (jj + kr + kd * (kk + kr))];
              mind = AIR_MIN(mind, ival - scl * kval);
            }
          }
        }
        out[xx + sx * (yy + sy * zz)] = mind;
      }
    }
  }

  airMopOkay(mop);
  return 0;
}

int
main(int argc, const char **argv) {
  const char *me = argv[0];
  airArray *mop = airMopNew();
  hestParm *hparm = hestParmMopNew();
  hestOpt *hopt = NULL;
  airMopAdd(mop, hparm, hestParmFree_mop, airMopAlways);
  Nrrd *nin;
  hestOptAdd_1_Other(&hopt, "i", "nin", &nin, NULL, "input image", nrrdHestNrrd);
  Nrrd *nkern;
  hestOptAdd_1_Other(&hopt, "k", "nin", &nkern, NULL, "kernel", nrrdHestNrrd);
  float scl;
  hestOptAdd_1_Float(&hopt, "s", "scl", &scl, "1.0", "scaling on kernel");
  char *outS;
  hestOptAdd_1_String(&hopt, "o", "nout", &outS, "-", "output filename");

  hestParseOrExit(hopt, argc - 1, argv + 1, hparm, me, morphInfo);
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);

  if (!(3 == nkern->dim && nkern->axis[0].size == nkern->axis[1].size
        && nkern->axis[0].size == nkern->axis[2].size && 1 == nkern->axis[0].size % 2)) {
    fprintf(stderr, "%s: need cubical kernel w/ odd # sample on edge", me);
    airMopError(mop);
    exit(1);
  }
  if (!(3 == nin->dim)) {
    fprintf(stderr, "%s: need 3D input", me);
    airMopError(mop);
    exit(1);
  }

  char *err;
  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (morph(nout, nin, nkern, scl)) {
    airMopAdd(mop, err = biffGet(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble:\n%s", me, err);
    airMopError(mop);
    exit(1);
  }
  if (nrrdSave(outS, nout, NULL)) {
    airMopAdd(mop, err = biffGet(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble saving \"%s\":\n%s", me, outS, err);
    airMopError(mop);
    exit(1);
  }

  airMopOkay(mop);
  exit(0);
}
