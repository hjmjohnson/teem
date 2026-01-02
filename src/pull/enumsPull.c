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

#include "pull.h"
#include "privatePull.h"

/* clang-format off */
/* --------------------------------------------------------- */

static const char *
sourceStr[] = {
  "(unknown pullSource)",
  "gage",
  "prop"
};

static const airEnum
source_ae = {
  "pullSource",
  PULL_SOURCE_MAX,
  sourceStr, NULL,
  NULL,
  NULL, NULL,
  AIR_FALSE
};
const airEnum *const pullSource = &source_ae;

/* --------------------------------------------------------- */

static const char *
infoStr[] = {
  "(unknown pullInfo)",
  "ten",
  "teni",
  "hess",
  "in",
  "ingradvec",
  "hght",
  "hghtgradvec",
  "hghthessian",
  "hghtlapl",
  "seedprethresh",
  "seedthresh",
  "livethresh",
  "livethresh2",
  "livethresh3",
  "tan1",
  "tan2",
  "negtan1",
  "negtan2",
  "isoval",
  "isogradvec",
  "isohessian",
  "strength",
  "quality"
};

static const int
infoVal[] = {
  pullInfoUnknown,
  pullInfoTensor,             /* [7] tensor here */
  pullInfoTensorInverse,      /* [7] inverse of tensor here */
  pullInfoHessian,            /* [9] hessian used for force distortion */
  pullInfoInside,             /* [1] containment scalar */
  pullInfoInsideGradient,     /* [3] containment vector */
  pullInfoHeight,             /* [1] for gravity */
  pullInfoHeightGradient,     /* [3] for gravity */
  pullInfoHeightHessian,      /* [9] for gravity */
  pullInfoHeightLaplacian,    /* [1]  */
  pullInfoSeedPreThresh,      /* [1] */
  pullInfoSeedThresh,         /* [1] scalar for thresholding seeding */
  pullInfoLiveThresh,         /* [1] */
  pullInfoLiveThresh2,        /* [1] */
  pullInfoLiveThresh3,        /* [1] */
  pullInfoTangent1,           /* [3] first tangent to constraint surf */
  pullInfoTangent2,           /* [3] second tangent to constraint surf */
  pullInfoNegativeTangent1,   /* [3] */
  pullInfoNegativeTangent2,   /* [3] */
  pullInfoIsovalue,           /* [1] */
  pullInfoIsovalueGradient,   /* [3] */
  pullInfoIsovalueHessian,    /* [9] */
  pullInfoStrength,           /* [1] */
  pullInfoQuality             /* [1] */
};

static const char *
infoStrEqv[] = {
  "ten",
  "teni",
  "hess",
  "in",
  "ingradvec",
  "hght", "h",
  "hghtgradvec", "hgvec",
  "hghthessian", "hhess",
  "hghtlapl", "hlapl",
  "seedprethresh", "spthr",
  "seedthresh", "sthr",
  "livethresh", "lthr",
  "livethresh2", "lthr2",
  "livethresh3", "lthr3",
  "tan1",
  "tan2",
  "ntan1", "negtan1",
  "ntan2", "negtan2",
  "isoval", "iso",
  "isogradvec", "isogvec",
  "isohessian", "isohess",
  "strength", "strn",
  "quality", "qual",
  ""
};

static const int
infoValEqv[] = {
  pullInfoTensor,
  pullInfoTensorInverse,
  pullInfoHessian,
  pullInfoInside,
  pullInfoInsideGradient,
  pullInfoHeight, pullInfoHeight,
  pullInfoHeightGradient, pullInfoHeightGradient,
  pullInfoHeightHessian, pullInfoHeightHessian,
  pullInfoHeightLaplacian, pullInfoHeightLaplacian,
  pullInfoSeedPreThresh, pullInfoSeedPreThresh,
  pullInfoSeedThresh, pullInfoSeedThresh,
  pullInfoLiveThresh, pullInfoLiveThresh,
  pullInfoLiveThresh2, pullInfoLiveThresh2,
  pullInfoLiveThresh3, pullInfoLiveThresh3,
  pullInfoTangent1,
  pullInfoTangent2,
  pullInfoNegativeTangent1, pullInfoNegativeTangent1,
  pullInfoNegativeTangent2, pullInfoNegativeTangent2,
  pullInfoIsovalue, pullInfoIsovalue,
  pullInfoIsovalueGradient, pullInfoIsovalueGradient,
  pullInfoIsovalueHessian, pullInfoIsovalueHessian,
  pullInfoStrength, pullInfoStrength,
  pullInfoQuality, pullInfoQuality
};

static const airEnum
info_ae = {
  "pullInfo",
  PULL_INFO_MAX,
  infoStr, infoVal,
  NULL,
  infoStrEqv, infoValEqv,
  AIR_FALSE
};
const airEnum *const pullInfo = &info_ae;

/* --------------------------------------------------------- */

static const char *
propStr[] = {
  "(unknown_prop)",
  "idtag",            /* pullPropIdtag */
  "idcc",             /* pullPropIdCC */
  "energy",           /* pullPropEnergy */
  "stepEnergy",       /* pullPropStepEnergy */
  "stepConstr",       /* pullPropStepConstr */
  "stuck",            /* pullPropStuck */
  "position",         /* pullPropPosition */
  "force",            /* pullPropForce */
  "neighDistMean",    /* pullPropNeighDistMean */
  "scale",            /* pullPropScale */
  "neighCovar",       /* pullPropNeighCovar */
  "neighCovar7Ten",   /* pullPropNeighCovar7Ten */
  "neighTanCovar",    /* pullPropNeighTanCovar */
  "internum",         /* pullPropNeighInterNum */
  "neighCovarTrace",  /* pullPropNeighCovarTrace */
  "neighCovarDet",    /* pullPropNeighCovarDet */
  "stability"         /* pullPropStability */
};

static const airEnum
prop_ae = {
  "pullProp",
  PULL_PROP_MAX,
  propStr, NULL,
  NULL,
  NULL, NULL,
  AIR_FALSE
};
const airEnum *const pullProp = &prop_ae;

/* --------------------------------------------------------- */

static const char *
processModeStr[] = {
  "(unknown_mode)",
  "descent",
  "nlearn",
  "adding",
  "nixing"
};

static const airEnum
processMode_ae = {
  "process mode",
  PULL_PROCESS_MODE_MAX,
  processModeStr,  NULL,
  NULL, NULL, NULL,
  AIR_FALSE
};
const airEnum *const pullProcessMode = &processMode_ae;

/* --------------------------------------------------------- */

static const char *
traceStopStr[] = {
  "(unknown_trace_stop)",
  "speeding",
  "constrfail",
  "bounds",
  "length",
  "stub",
  "voledge",
  "orientconstrfail"
};

static const airEnum
traceStop_ae = {
  "trace stop",
  PULL_TRACE_STOP_MAX,
  traceStopStr,  NULL,
  NULL, NULL, NULL,
  AIR_FALSE
};
const airEnum *const pullTraceStop = &traceStop_ae;

/* --------------------------------------------------------- */

static const char *
initMethodStr[] = {
  "(unknown_init_method)",
  "random",
  "halton",
  "ppv",
  "given"
};

static const airEnum
initMethod_ae = {
  "init method",
  PULL_INIT_METHOD_MAX,
  initMethodStr,  NULL,
  NULL, NULL, NULL,
  AIR_FALSE
};
const airEnum *const pullInitMethod = &initMethod_ae;

/* --------------------------------------------------------- */

static const char *
countStr[] = {
  "(unknown_countable)",
  "descent",
  "teststep",
  "enrg(img)",
  "frc(img)",
  "enrg(pts)",
  "frc(pts)",
  "probe",
  "constr",
  "adding",
  "nixing",
  "pts stuck",
  "pts",
  "CC",
  "iter"
};

static const airEnum
count_ae = {
  "countable",
  PULL_COUNT_MAX,
  countStr,  NULL,
  NULL, NULL, NULL,
  AIR_FALSE
};
const airEnum *const pullCount = &count_ae;

/* --------------------------------------------------------- */

static const char *
constraintFailStr[PULL_CONSTRAINT_FAIL_MAX+1] = {
  "(unknown or no contraint fail)",
  "needed Hessian 0 (A)",
  "needed Hessian 0 (B)",
  "projected gradient 0 (A)",
  "projected gradient 0 (B)",
  "iter max exceeded",
  "travel exceeded",
  "plen doesn't exist"
};

static const airEnum
constraintFail_ae = {
  "constraint fail",
  PULL_CONSTRAINT_FAIL_MAX,
  constraintFailStr, NULL,
  NULL, NULL, NULL,
  AIR_FALSE
};
const airEnum *const pullConstraintFail = &constraintFail_ae;

/* clang-format on */
