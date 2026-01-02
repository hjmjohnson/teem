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

#include "bane.h"
#include "privateBane.h"
#include <assert.h>

#define TXF_INFO "Create Levoy-style triangular 2D opacity functions"
static const char *txfInfoL
  = (TXF_INFO ". The triangles are in the 2D space of data value and gradient "
              "magnitude.  They can be tilted sideways and clipped at the bottom. "
              "This doesn't strictly speaking belong in \"gkms\" but there's no "
              "other good place in Teem.");
static int /* Biff: 1 */
txfMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  uint res[2];
  hestOptAdd_2_UInt(&hopt, "r", "Vres Gres", res, "256 256",
                    "resolution of the transfer function in value and gradient "
                    "magnitude");
  float min[2], max[2];
  hestOptAdd_2_Float(&hopt, "min", "Vmin Gmin", min, "0.0 0.0",
                     "minimum value and grad mag in txf");
  hestOptAdd_2_Float(&hopt, "max", "Vmax Gmax", max, NULL,
                     "maximum value and grad mag in txf");
  float v0, g0, gwidth;
  hestOptAdd_1_Float(&hopt, "v", "base value", &v0, NULL,
                     "data value at which to position bottom of triangle");
  hestOptAdd_1_Float(&hopt, "g", "gthresh", &g0, "0.0",
                     "lowest grad mag to receive opacity");
  hestOptAdd_1_Float(&hopt, "gw", "gwidth", &gwidth, "0.0",
                     "range of grad mag values over which to apply threshold "
                     "at low gradient magnitudes");
  float top[2];
  hestOptAdd_2_Float(&hopt, "top", "Vtop Gtop", top, NULL,
                     "data value and grad mag at center of top of triangle");
  float width, mwidth;
  hestOptAdd_1_Float(&hopt, "w", "value width", &width, NULL,
                     "range of values to be spanned at top of triangle");
  hestOptAdd_1_Float(&hopt, "mw", "value width", &mwidth, "0",
                     "range of values to be spanned at BOTTOM of triangle");
  int step;
  hestOptAdd_Flag(&hopt, "step", &step,
                  "instead of assigning opacity inside a triangular region, "
                  "make it more like a step function, in which opacity never "
                  "decreases in increasing data value");
  float maxa;
  hestOptAdd_1_Float(&hopt, "a", "max opac", &maxa, "1.0", "highest opacity to assign");
  char *out;
  hestOptAdd_1_String(&hopt, "o", "opacOut", &out, NULL,
                      "output opacity function filename");

  hestParseOrExit(hopt, argc, argv, hparm, me, txfInfoL);
  airArray *mop = airMopNew();
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  int E = 0;
  if (!E)
    E |= nrrdMaybeAlloc_va(nout, nrrdTypeFloat, 3, AIR_SIZE_T(1), AIR_SIZE_T(res[0]),
                           AIR_SIZE_T(res[1]));
  if (!E) E |= !(nout->axis[0].label = airStrdup("A"));
  if (!E) E |= !(nout->axis[1].label = airStrdup("gage(scalar:v)"));
  if (!E)
    nrrdAxisInfoSet_va(nout, nrrdAxisInfoMin, AIR_NAN, (double)min[0], (double)min[1]);
  if (!E)
    nrrdAxisInfoSet_va(nout, nrrdAxisInfoMax, AIR_NAN, (double)max[0], (double)max[1]);
  if (!E) E |= !(nout->axis[2].label = airStrdup("gage(scalar:gm)"));
  if (E) {
    biffMovef(BANE, NRRD, "%s: trouble creating opacity function nrrd", me);
    airMopError(mop);
    return 1;
  }
  assert(nout->type == nrrdTypeFloat);
  float *data = nout->data;
  float tvl = top[0] - width / 2;
  float tvr = top[0] + width / 2;
  mwidth /= 2;
  for (uint gi = 0; gi < res[1]; gi++) {
    float g = AIR_FLOAT(NRRD_CELL_POS(min[1], max[1], res[1], gi));
    for (uint vi = 0; vi < res[0]; vi++) {
      float v = AIR_FLOAT(NRRD_CELL_POS(min[0], max[0], res[0], vi));
      float vl = AIR_FLOAT(AIR_AFFINE(0, g, top[1], v0 - mwidth, tvl));
      float vr = AIR_FLOAT(AIR_AFFINE(0, g, top[1], v0 + mwidth, tvr));
      if (g > top[1]) {
        data[vi + res[0] * gi] = 0;
        continue;
      }
      float tmp = AIR_FLOAT((v - vl) / (0.00001 + vr - vl));
      tmp = 1 - AIR_ABS(2 * tmp - 1);
      if (step && v > (vr + vl) / 2) {
        tmp = 1;
      }
      tmp = AIR_MAX(0, tmp);
      data[vi + res[0] * gi] = tmp * maxa;
      tmp = AIR_FLOAT(AIR_AFFINE(g0 - gwidth / 2, g, g0 + gwidth / 2, 0.0, 1.0));
      tmp = AIR_CLAMP(0, tmp, 1);
      data[vi + res[0] * gi] *= tmp;
    }
  }

  if (nrrdSave(out, nout, NULL)) {
    biffMovef(BANE, NRRD, "%s: trouble saving opacity function", me);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  return 0;
}
const unrrduCmd baneGkms_txfCmd = {"txf", TXF_INFO, txfMain, AIR_FALSE};
