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

#define PARM_NUM 3
static const tenModelParmDesc
parmDesc[] = {
  /* 0 */ {"B0", 0.0, TEN_MODEL_B0_MAX, AIR_FALSE, AIR_FALSE, 0},
  /* 1 */ {"r", 0.0, 5.0, AIR_FALSE, AIR_FALSE, 0},
  /* 2 */ {"th", 0, 2*AIR_PI, AIR_TRUE, AIR_FALSE, 0}
};

static void
simulate(double *dwiSim, const double *parm, const tenExperSpec *espec) {
  unsigned int ii;
  double rad, theta, vec[3];

  /* not used: b0 = parm[0]; */
  rad = parm[1];
  theta = parm[2];
  ELL_3V_SET(vec, rad*cos(theta), rad*sin(theta), 0.0);
  for (ii=0; ii<espec->imgNum; ii++) {
    dwiSim[ii] = ELL_3V_DOT(vec, espec->grad + 3*ii);
  }
  return;
}

static char *
parmSprint(char str[AIR_STRLEN_MED+1], const double *parm) {
  snprintf(str, ASP1_M, "(%g) r=%g th=%g", parm[0], parm[1], parm[2]);
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
MY_TEN_SQE_FIT(tenModel1Vector2D)

MY_TEN_NLL
MY_TEN_NLL_GRAD_STUB
MY_TEN_NLL_FIT_STUB

static const tenModel
model1Vector2D = {
  TEN_MODEL_STR_1VECTOR2D,
  MY_TEN_MODEL_FIELDS
};
const tenModel *const tenModel1Vector2D = &model1Vector2D;
/* clang-format on */
