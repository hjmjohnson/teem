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

#include <math.h>
#include "../nrrd.h"

const char *quadInfo = ("generates quadratic test volumes, with isosurfaces "
                        "which should resemble z = A*x^2 + B*y^2");

float
quadFunc(float x, float y, float z, float A, float B, float off) {

  return A * x * x + B * y * y - z + off;
}

typedef unsigned int uint;

int
main(int argc, const char *argv[]) {
  const char *me = argv[0];
  hestOpt *hopt = NULL;
  uint size[3];
  hestOptAdd_3_UInt(&hopt, "s", "sx sy sz", size, "128 128 128",
                    "dimensions of output volume");
  float min[3];
  hestOptAdd_3_Float(&hopt, "min", "x y z", min, "-1 -1 -1",
                     "lower bounding corner of volume");
  float max[3];
  hestOptAdd_3_Float(&hopt, "max", "x y z", max, "1 1 1",
                     "upper bounding corner of volume");
  float AB[2];
  hestOptAdd_2_Float(&hopt, "c", "A B", AB, NULL, "A and B quadratic coefficients");
  float off;
  hestOptAdd_1_Float(&hopt, "off", "z offset", &off, "0.0", "vertical offset");
  char *out;
  hestOptAdd_1_String(&hopt, "o", "filename", &out, "-", "file to write output nrrd to");
  hestParm *hparm = hestParmMopNew(); /* Mop for hestParseOrExit */
  hestParseOrExit(hopt, argc - 1, argv + 1, hparm, me, quadInfo);
  airArray *mop = airMopNew();
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);
  airMopAdd(mop, hparm, hestParmFree_mop, airMopAlways);

  char *err;
  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  if (nrrdAlloc_va(nout, nrrdTypeFloat, 3, AIR_SIZE_T(size[0]), AIR_SIZE_T(size[1]),
                   AIR_SIZE_T(size[2]))) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: problem allocating volume:\n%s\n", me, err);
    airMopError(mop);
    return 1;
  }

  float *data = (float *)nout->data;
  for (uint zi = 0; zi < size[2]; zi++) {
    float z = AIR_FLOAT(AIR_AFFINE(0, zi, size[2] - 1, min[2], max[2]));
    for (uint yi = 0; yi < size[1]; yi++) {
      float y = AIR_FLOAT(AIR_AFFINE(0, yi, size[1] - 1, min[1], max[1]));
      for (uint xi = 0; xi < size[0]; xi++) {
        float x = AIR_FLOAT(AIR_AFFINE(0, xi, size[0] - 1, min[0], max[0]));
        *data = quadFunc(x, y, z, AB[0], AB[1], off);
        data += 1;
      }
    }
  }

  nrrdAxisInfoSet_va(nout, nrrdAxisInfoMin, min[0], min[1], min[2]);
  nrrdAxisInfoSet_va(nout, nrrdAxisInfoMax, max[0], max[1], max[2]);
  nrrdAxisInfoSpacingSet(nout, 0);
  nrrdAxisInfoSpacingSet(nout, 1);
  nrrdAxisInfoSpacingSet(nout, 2);
  if (nrrdSave(out, nout, NULL)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: problem saving output:\n%s\n", me, err);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  exit(0);
}
