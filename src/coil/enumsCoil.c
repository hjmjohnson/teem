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

#include "coil.h"

/* clang-format off */
static const char *
methodTypeStr[COIL_METHOD_TYPE_MAX+1] = {
  "(unknown_method)",
  "testing",
  "homogeneous",
  "perona-malik",
  "modified curvature",
  "modified curvature rings",
  "curvature flow",
  "self",
  "finish"
};

static const char *
methodTypeDesc[COIL_METHOD_TYPE_MAX+1] = {
  "unknown_method",
  "nothing, actually, just here for testing",
  "homogenous isotropic diffusion (Gaussian blurring)",
  "Perona-Malik",
  "modified curvature diffusion",
  "modified curvature diffusion rings",
  "curvature flow",
  "self-diffusion of diffusion tensors",
  "finish a phd already"
};

static const char *
methodTypeStrEqv[] = {
  "test", "testing",
  "iso", "homog", "homogeneous",
  "pm", "perona-malik",
  "mcde", "modified curvature",
  "mcder", "modified curvature rings",
  "flow", "curvature flow",
  "self",
  "finish",
  ""
};

static const int
methodTypeValEqv[] = {
  coilMethodTypeTesting, coilMethodTypeTesting,
  coilMethodTypeHomogeneous, coilMethodTypeHomogeneous, coilMethodTypeHomogeneous,
  coilMethodTypePeronaMalik, coilMethodTypePeronaMalik,
  coilMethodTypeModifiedCurvature, coilMethodTypeModifiedCurvature,
  coilMethodTypeModifiedCurvatureRings, coilMethodTypeModifiedCurvatureRings,
  coilMethodTypeCurvatureFlow, coilMethodTypeCurvatureFlow,
  coilMethodTypeSelf,
  coilMethodTypeFinish,
};

static const airEnum
methodType_ae = {
  "method",
  COIL_METHOD_TYPE_MAX,
  methodTypeStr,  NULL,
  methodTypeDesc,
  methodTypeStrEqv, methodTypeValEqv,
  AIR_FALSE
};
const airEnum *const coilMethodType = &methodType_ae;

/* -------------------------------------------------- */

static const char *
kindTypeStr[COIL_KIND_TYPE_MAX+1] = {
  "(unknown_kind)",
  "scalar",
  "3color",
  "7tensor"
};

static const char *
kindTypeDesc[COIL_KIND_TYPE_MAX+1] = {
  "unknown_kind",
  "plain old scalar quantities",
  "3-component color",
  "ten-style 7-valued tensor"
};

static const char *
kindTypeStrEqv[] = {
  "scalar",
  "3color",
  "7tensor", "tensor",
  ""
};

static const int
kindTypeValEqv[] = {
  coilKindTypeScalar,
  coilKindType3Color,
  coilKindType7Tensor, coilKindType7Tensor
};

static const airEnum
kindType_ae = {
  "kind",
  COIL_KIND_TYPE_MAX,
  kindTypeStr,  NULL,
  kindTypeDesc,
  kindTypeStrEqv, kindTypeValEqv,
  AIR_FALSE
};
const airEnum *const coilKindType = &kindType_ae;
/* clang-format on */
