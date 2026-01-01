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

#include "moss.h"
#include "privateMoss.h"

#define MAT_COPY(m2, m1)                                                                \
  (m2)[0] = (m1)[0];                                                                    \
  (m2)[1] = (m1)[1];                                                                    \
  (m2)[2] = (m1)[2];                                                                    \
  (m2)[3] = (m1)[3];                                                                    \
  (m2)[4] = (m1)[4];                                                                    \
  (m2)[5] = (m1)[5]

static int
mHestTransformParse(void *ptr, const char *_str, char err[AIR_STRLEN_HUGE + 1]) {
  static const char me[] = "mHestTransformParse";
  char *str;
  double **matP, tx, ty, sx, sy, angle, mat[6], shf, sha;
  airArray *mop;

  if (!(ptr && _str)) {
    snprintf(err, ASP1_H, "%s: got NULL pointer", me);
    return 1;
  }
  matP = (double **)ptr;
  mop = airMopNew();
  *matP = (double *)calloc(6, sizeof(double));
  airMopMem(mop, matP, airMopOnError);
  str = airToLower(airStrdup(_str));
  airMopMem(mop, &str, airMopAlways);

  if (!strcmp("identity", str)) {
    mossMatIdentitySet(*matP);

  } else if (1 == sscanf(str, "flip:%lf", &angle)) {
    mossMatFlipSet(*matP, angle);

  } else if (2 == sscanf(str, "translate:%lf,%lf", &tx, &ty)) {
    mossMatTranslateSet(*matP, tx, ty);
  } else if (2 == sscanf(str, "t:%lf,%lf", &tx, &ty)) {
    mossMatTranslateSet(*matP, tx, ty);

  } else if (1 == sscanf(str, "rotate:%lf", &angle)) {
    mossMatRotateSet(*matP, angle);
  } else if (1 == sscanf(str, "r:%lf", &angle)) {
    mossMatRotateSet(*matP, angle);

  } else if (2 == sscanf(str, "scale:%lf,%lf", &sx, &sy)) {
    mossMatScaleSet(*matP, sx, sy);
  } else if (2 == sscanf(str, "s:%lf,%lf", &sx, &sy)) {
    mossMatScaleSet(*matP, sx, sy);

  } else if (2 == sscanf(str, "shear:%lf,%lf", &shf, &sha)) {
    mossMatShearSet(*matP, shf, sha);

  } else if (6
             == sscanf(str, "%lf,%lf,%lf,%lf,%lf,%lf", mat + 0, mat + 1, mat + 2,
                       mat + 3, mat + 4, mat + 5)) {
    MAT_COPY(*matP, mat);

  } else {
    snprintf(err, ASP1_H, "%s: couldn't parse \"%s\" as a transform", me, _str);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  return 0;
}

static const hestCB mHestTransform_hcb = {sizeof(double *), "2D transform",
                                          mHestTransformParse, airFree};

const hestCB *const mossHestTransform = &mHestTransform_hcb;

/* ----------------------------------------------------------------- */

/*
** mHestOriginParse()
**
** parse an origin specification
** p(x,y): absolute pixel position --> val[3] = (0,x,y)
** u(x,y): position in unit box [0,1]x[0,1] --> val[3] = (1,x,y)
*/
static int
mHestOriginParse(void *ptr, const char *str, char err[AIR_STRLEN_HUGE + 1]) {
  static const char me[] = "mHestOriginParse";
  double **valP;
  airArray *mop;

  valP = (double **)ptr;
  mop = airMopNew();
  *valP = (double *)calloc(3, sizeof(double));
  airMopMem(mop, valP, airMopOnError);
  if (2 == sscanf(str, "p:%lf,%lf", *valP + 1, *valP + 2)) {
    (*valP)[0] = 0;
  } else if (2 == sscanf(str, "u:%lf,%lf", *valP + 1, *valP + 2)) {
    (*valP)[0] = 1;
  } else {
    snprintf(err, ASP1_H, "%s: couldn't parse \"%s\" as origin", me, str);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  return 0;
}

static const hestCB mHestOrigin_hcb = {sizeof(double *), "origin specification",
                                       mHestOriginParse, airFree};

const hestCB *const mossHestOrigin = &mHestOrigin_hcb;
