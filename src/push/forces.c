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

#include "push.h"
#include "privatePush.h"

#define SPRING  "spring"
#define GAUSS   "gauss"
#define COULOMB "coulomb"
#define COTAN   "cotan"
#define ZERO    "zero"

/* clang-format off */
static const char *
energyTypeStr[PUSH_ENERGY_TYPE_MAX+1] = {
  "(unknown_energy)",
  SPRING,
  GAUSS,
  COULOMB,
  COTAN,
  ZERO
};

static const char *
energyTypeDesc[PUSH_ENERGY_TYPE_MAX+1] = {
  "unknown_energy",
  "Hooke's law-based potential, with a tunable region of attraction",
  "Gaussian potential",
  "Coulomb electrostatic potential, with tunable cut-off",
  "Cotangent-based potential (from Meyer et al. SMI '05)",
  "no energy"
};

static const airEnum
energyType_impl = {
  "energy",
  PUSH_ENERGY_TYPE_MAX,
  energyTypeStr,  NULL,
  energyTypeDesc,
  NULL, NULL,
  AIR_FALSE
};
const airEnum *const
pushEnergyType = &energyType_impl;
/* clang-format on */

/* ----------------------------------------------------------------
** ------------------------------ UNKNOWN -------------------------
** ----------------------------------------------------------------
*/
static void
energyUnknownEval(double *enr, double *frc, double dist, const double *parm) {
  static const char me[] = "energyUnknownEval";

  AIR_UNUSED(dist);
  AIR_UNUSED(parm);
  *enr = AIR_NAN;
  *frc = AIR_NAN;
  fprintf(stderr, "%s: ERROR- using unknown energy.\n", me);
  return;
}

static double
energyUnknownSupport(const double *parm) {
  static const char me[] = "energyUnknownSupport";

  AIR_UNUSED(parm);
  fprintf(stderr, "%s: ERROR- using unknown energy.\n", me);
  return AIR_NAN;
}

static const pushEnergy energyUnknown_impl = {"unknown", 0, energyUnknownEval,
                                              energyUnknownSupport};
const pushEnergy *const pushEnergyUnknown = &energyUnknown_impl;

/* ----------------------------------------------------------------
** ------------------------------ SPRING --------------------------
** ----------------------------------------------------------------
** 1 parms:
** parm[0]: width of pull region (beyond 1.0)
**
** learned: "1/2" is not 0.5 !!!!!
*/
static void
energySpringEval(double *enr, double *frc, double dist, const double *parm) {
  /* static const char me[] = "energySpringEval"; */
  double xx, pull;

  pull = parm[0];
  xx = dist - 1.0;
  if (xx > pull) {
    *enr = 0;
    *frc = 0;
  } else if (xx > 0) {
    *enr = xx * xx * (xx * xx / (4 * pull * pull) - 2 * xx / (3 * pull) + 1.0 / 2.0);
    *frc = xx * (xx * xx / (pull * pull) - 2 * xx / pull + 1);
  } else {
    *enr = xx * xx / 2;
    *frc = xx;
  }
  /*
  if (!AIR_EXISTS(ret)) {
    fprintf(stderr, "!%s: dist=%g, pull=%g, blah=%d --> ret=%g\n",
            me, dist, pull, blah, ret);
  }
  */
  return;
}

static double
energySpringSupport(const double *parm) {

  return 1.0 + parm[0];
}

static const pushEnergy energySpring_impl = {SPRING, 1, energySpringEval,
                                             energySpringSupport};
const pushEnergy *const pushEnergySpring = &energySpring_impl;

/* ----------------------------------------------------------------
** ------------------------------ GAUSS --------------------------
** ----------------------------------------------------------------
** 1 parms:
** (distance to inflection point of force function is always 1.0)
** parm[0]: cut-off (as a multiple of standard dev (which is 1.0))
*/
/* HEY: copied from teem/src/nrrd/kernel.c */
#define GAUSS_EVAL(x, sig, cut)                                                         \
  (x >= sig * cut ? 0 : exp(-x * x / (2.0 * sig * sig)) / (sig * 2.50662827463100050241))

#define DGAUSS_EVAL(x, sig, cut)                                                        \
  (x >= sig * cut ? 0                                                                   \
                  : -exp(-x * x / (2.0 * sig * sig)) * x                                \
                      / (sig * sig * sig * 2.50662827463100050241))

static void
energyGaussEval(double *enr, double *frc, double dist, const double *parm) {
  double cut;

  cut = parm[0];
  *enr = GAUSS_EVAL(dist, 1.0, cut);
  *frc = DGAUSS_EVAL(dist, 1.0, cut);
  return;
}

static double
energyGaussSupport(const double *parm) {

  return parm[0];
}

static const pushEnergy energyGauss_impl = {GAUSS, 1, energyGaussEval,
                                            energyGaussSupport};
const pushEnergy *const pushEnergyGauss = &energyGauss_impl;

/* ----------------------------------------------------------------
** ------------------------------ CHARGE --------------------------
** ----------------------------------------------------------------
** 1 parms:
** (scale: distance to "1.0" in graph of x^(-2))
** parm[0]: cut-off (as multiple of "1.0")
*/
static void
energyCoulombEval(double *enr, double *frc, double dist, const double *parm) {

  *enr = (dist > parm[0] ? 0 : 1.0 / dist);
  *frc = (dist > parm[0] ? 0 : -1.0 / (dist * dist));
  return;
}

static double
energyCoulombSupport(const double *parm) {

  return parm[0];
}

static const pushEnergy energyCoulomb_impl = {COULOMB, 1, energyCoulombEval,
                                              energyCoulombSupport};
const pushEnergy *const pushEnergyCoulomb = &energyCoulomb_impl;

/* ----------------------------------------------------------------
** ------------------------------ COTAN ---------------------------
** ----------------------------------------------------------------
** 0 parms!
*/
static void
energyCotanEval(double *enr, double *frc, double dist, const double *parm) {
  double pot, cc;

  AIR_UNUSED(parm);
  pot = AIR_PI / 2.0;
  cc = 1.0 / (FLT_MIN + tan(dist * pot));
  *enr = dist > 1 ? 0 : cc + dist * pot - pot;
  *frc = dist > 1 ? 0 : -cc * cc * pot;
  return;
}

static double
energyCotanSupport(const double *parm) {

  AIR_UNUSED(parm);
  return 1;
}

static const pushEnergy energyCotan_impl = {COTAN, 0, energyCotanEval,
                                            energyCotanSupport};
const pushEnergy *const pushEnergyCotan = &energyCotan_impl;

/* ----------------------------------------------------------------
** ------------------------------- ZERO ---------------------------
** ----------------------------------------------------------------
** 0 parms:
*/
static void
energyZeroEval(double *enr, double *frc, double dist, const double *parm) {

  AIR_UNUSED(dist);
  AIR_UNUSED(parm);
  *enr = 0;
  *frc = 0;
  return;
}

static double
energyZeroSupport(const double *parm) {

  AIR_UNUSED(parm);
  return 1.0;
}

static const pushEnergy energyZero_impl = {ZERO, 0, energyZeroEval, energyZeroSupport};
const pushEnergy *const pushEnergyZero = &energyZero_impl;

/* ----------------------------------------------------------------
** ----------------------------------------------------------------
** ----------------------------------------------------------------
*/

const pushEnergy *const pushEnergyAll[PUSH_ENERGY_TYPE_MAX + 1] = {
  &energyUnknown_impl, /* 0 */
  &energySpring_impl,  /* 1 */
  &energyGauss_impl,   /* 2 */
  &energyCoulomb_impl, /* 3 */
  &energyCotan_impl,   /* 4 */
  &energyZero_impl     /* 5 */
};

pushEnergySpec * /* Biff: nope */
pushEnergySpecNew(void) {
  pushEnergySpec *ensp;
  int pi;

  ensp = (pushEnergySpec *)calloc(1, sizeof(pushEnergySpec));
  if (ensp) {
    ensp->energy = pushEnergyUnknown;
    for (pi = 0; pi < PUSH_ENERGY_PARM_NUM; pi++) {
      ensp->parm[pi] = AIR_NAN;
    }
  }
  return ensp;
}

void
pushEnergySpecSet(pushEnergySpec *ensp, const pushEnergy *energy,
                  const double parm[PUSH_ENERGY_PARM_NUM]) {
  unsigned int pi;

  if (ensp && energy && parm) {
    ensp->energy = energy;
    for (pi = 0; pi < PUSH_ENERGY_PARM_NUM; pi++) {
      ensp->parm[pi] = parm[pi];
    }
  }
  return;
}

pushEnergySpec * /* Biff: nope */
pushEnergySpecNix(pushEnergySpec *ensp) {

  airFree(ensp);
  return NULL;
}
AIR_MOPPER(pushEnergySpecNix)

int /* Biff: 1 */
pushEnergySpecParse(pushEnergySpec *ensp, const char *_str) {
  static const char me[] = "pushEnergySpecParse";
  char *str, *col, *_pstr, *pstr;
  int etype;
  unsigned int pi, haveParm;
  airArray *mop;
  double pval;

  if (!(ensp && _str)) {
    biffAddf(PUSH, "%s: got NULL pointer", me);
    return 1;
  }

  /* see if its the name of something that needs no parameters */
  etype = airEnumVal(pushEnergyType, _str);
  if (pushEnergyTypeUnknown != etype) {
    /* the string is the name of some energy */
    ensp->energy = pushEnergyAll[etype];
    if (0 != ensp->energy->parmNum) {
      biffAddf(PUSH, "%s: need %u parms for %s energy, but got none", me,
               ensp->energy->parmNum, ensp->energy->name);
      return 1;
    }
    /* the energy needs 0 parameters */
    for (pi = 0; pi < PUSH_ENERGY_PARM_NUM; pi++) {
      ensp->parm[pi] = AIR_NAN;
    }
    return 0;
  }

  /* start parsing parms after ':' */
  mop = airMopNew();
  str = airStrdup(_str);
  airMopAdd(mop, str, airFree, airMopAlways);
  col = strchr(str, ':');
  if (!col) {
    biffAddf(PUSH,
             "%s: \"%s\" isn't a parameter-free energy, but it has no "
             "\":\" separator to indicate parameters",
             me, str);
    airMopError(mop);
    return 1;
  }
  *col = '\0';
  etype = airEnumVal(pushEnergyType, str);
  if (pushEnergyTypeUnknown == etype) {
    biffAddf(PUSH, "%s: didn't recognize \"%s\" as a %s", me, str, pushEnergyType->name);
    airMopError(mop);
    return 1;
  }

  ensp->energy = pushEnergyAll[etype];
  if (0 == ensp->energy->parmNum) {
    biffAddf(PUSH, "%s: \"%s\" energy has no parms, but got something", me,
             ensp->energy->name);
    return 1;
  }

  _pstr = pstr = col + 1;
  /* code lifted from teem/src/nrrd/kernel.c, should probably refactor... */
  for (haveParm = 0; haveParm < ensp->energy->parmNum; haveParm++) {
    if (!pstr) {
      break;
    }
    if (1 != sscanf(pstr, "%lg", &pval)) {
      biffAddf(PUSH, "%s: trouble parsing \"%s\" as double (in \"%s\")", me, _pstr,
               _str);
      airMopError(mop);
      return 1;
    }
    ensp->parm[haveParm] = pval;
    if ((pstr = strchr(pstr, ','))) {
      pstr++;
      if (!*pstr) {
        biffAddf(PUSH, "%s: nothing after last comma in \"%s\" (in \"%s\")", me, _pstr,
                 _str);
        airMopError(mop);
        return 1;
      }
    }
  }
  /* haveParm is now the number of parameters that were parsed. */
  if (haveParm < ensp->energy->parmNum) {
    biffAddf(PUSH,
             "%s: parsed only %u of %u required parms (for %s energy)"
             "from \"%s\" (in \"%s\")",
             me, haveParm, ensp->energy->parmNum, ensp->energy->name, _pstr, _str);
    airMopError(mop);
    return 1;
  } else {
    if (pstr) {
      biffAddf(PUSH, "%s: \"%s\" (in \"%s\") has more than %u doubles", me, _pstr, _str,
               ensp->energy->parmNum);
      airMopError(mop);
      return 1;
    }
  }

  airMopOkay(mop);
  return 0;
}

static int
phEnergyParse(void *ptr, const char *str, char err[AIR_STRLEN_HUGE + 1]) {
  pushEnergySpec **enspP;
  static const char me[] = "_pushHestForceParse";
  char *perr;

  if (!(ptr && str)) {
    snprintf(err, ASP1_H, "%s: got NULL pointer", me);
    return 1;
  }
  enspP = (pushEnergySpec **)ptr;
  *enspP = pushEnergySpecNew();
  if (pushEnergySpecParse(*enspP, str)) {
    perr = biffGetDone(PUSH);
    airStrcpy(err, AIR_STRLEN_HUGE + 1, perr);
    free(perr);
    return 1;
  }
  return 0;
}

static const hestCB phEnergySpec_hcb = {sizeof(pushEnergySpec *), "energy specification",
                                        phEnergyParse, pushEnergySpecNix_mop};

const hestCB *const pushHestEnergySpec = &phEnergySpec_hcb;
