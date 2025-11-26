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

#include "ten.h"
#include "privateTen.h"
/* clang-format off */

#define PARM_NUM 6
static const tenModelParmDesc
parmDesc[] = {
  /* 0 */ {"B0", 0.0, TEN_MODEL_B0_MAX, AIR_FALSE, AIR_FALSE, 0},
  /* 1 */ {"length", 0.0, TEN_MODEL_DIFF_MAX, AIR_FALSE, AIR_FALSE, 0},
  /* 2 */ {"radius", 0.0, TEN_MODEL_DIFF_MAX, AIR_FALSE, AIR_FALSE, 0},
  /* 3 */ {"x", -1.0, 1.0, AIR_FALSE, AIR_TRUE, 0},
  /* 4 */ {"y", -1.0, 1.0, AIR_FALSE, AIR_TRUE, 1},
  /* 5 */ {"z", -1.0, 1.0, AIR_FALSE, AIR_TRUE, 2}
};

static void
simulate(double *dwiSim, const double *parm, const tenExperSpec *espec) {
  unsigned int ii;
  double b0, length, radius, vec[3], ten[7],
    ident[7] = {1, 1, 0, 0, 1, 0, 1};

  b0 = parm[0];
  length = parm[1];
  radius = parm[2];
  vec[0] = parm[3];
  vec[1] = parm[4];
  vec[2] = parm[5];
  TEN_T3V_OUTER(ten, vec);
  TEN_T_SCALE_ADD2(ten, length - radius, ten, radius, ident);
  for (ii=0; ii<espec->imgNum; ii++) {
    double adc, bb;
    bb = espec->bval[ii];
    adc = TEN_T3V_CONTR(ten, espec->grad + 3*ii);
    dwiSim[ii] = b0*exp(-bb*adc);
  }
  return;
}

static char *
parmSprint(char str[AIR_STRLEN_MED+1], const double *parm) {
  snprintf(str, ASP1_M, "(%g) %gX%g (%g,%g,%g)", parm[0],
          parm[1], parm[2], parm[3], parm[4], parm[5]);
  return str;
}

MY_TEN_PARM_ALLOC
MY_TEN_PARM_RAND
MY_TEN_PARM_STEP
MY_TEN_PARM_DIST
MY_TEN_PARM_COPY
MY_TEN_PARM_CONVERT_NOOP

MY_TEN_SQE
MY_TEN_SQE_GRAD_CENTDIFF
MY_TEN_SQE_FIT(tenModel1Cylinder)

MY_TEN_NLL
MY_TEN_NLL_GRAD_STUB
MY_TEN_NLL_FIT_STUB

static const tenModel
model1Cylinder = {
  TEN_MODEL_STR_1CYLINDER,
  MY_TEN_MODEL_FIELDS
};
const tenModel *const tenModel1Cylinder = &model1Cylinder;
/* clang-format on */
