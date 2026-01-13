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

#include "echo.h"

/* clang-format off */
/* ------------------------------- jitter --------------------------- */

static const char *
JitterStr[ECHO_JITTER_NUM+1] = {
  "(unknown_jitter)",
  "none",
  "grid",
  "jitter",
  "random"
};

static const int
JitterVal[ECHO_JITTER_NUM+1] = {
  echoJitterUnknown,
  echoJitterNone,
  echoJitterGrid,
  echoJitterJitter,
  echoJitterRandom
};

static const char *
JitterDesc[ECHO_JITTER_NUM+1] = {
  "unknown jitter",
  "nothing- samples are ALWAYS at center of region",
  "no jittering- samples are at regular grid vertices",
  "normal jittering- samples are randomly located within grid cells",
  "samples are randomly located within region"
};

static const char *
JitterStrEqv[] = {
  "none",
  "grid", "regular",
  "jitter",
  "random",
  ""
};

static const int
JitterValEqv[] = {
  echoJitterNone,
  echoJitterGrid, echoJitterGrid,
  echoJitterJitter,
  echoJitterRandom
};

static const airEnum
Jitter_ae = {
  "jitter",
  ECHO_JITTER_NUM,
  JitterStr,  JitterVal,
  JitterDesc,
  JitterStrEqv, JitterValEqv,
  AIR_FALSE
};
const airEnum *const
echoJitter = &Jitter_ae;

/* ------------------------------- object type --------------------------- */

static const char *
TypeStr[ECHO_TYPE_NUM+1] = {
  "(unknown_object)",
  "sphere",
  "cylinder",
  "cone",
  "superquad",
  "cube",
  "triangle",
  "rectangle",
  "mesh",
  "isosurface",
  "AABoundingBox",
  "split",
  "list",
  "instance"
};

static const int
TypeVal[ECHO_TYPE_NUM+1] = {
  echoTypeUnknown,
  echoTypeSphere,
  echoTypeCylinder,
  echoTypeCone,
  echoTypeSuperquad,
  echoTypeCube,
  echoTypeTriangle,
  echoTypeRectangle,
  echoTypeTriMesh,
  echoTypeIsosurface,
  echoTypeAABBox,
  echoTypeSplit,
  echoTypeList,
  echoTypeInstance
};

static const char *
TypeDesc[ECHO_TYPE_NUM+1] = {
  "unknown_object",
  "sphere",
  "axis-aligned cylinder",
  "z = 1 - 2*sqrt(x^2 + y^2) in bi-unit cube",
  "superquadric (actually, superellipsoid)",
  "unit cube, centered at the origin",
  "triangle",
  "rectangle",
  "mesh of triangles",
  "isosurface of scalar volume",
  "axis-aligned bounding box",
  "split",
  "list",
  "instance"
};

static const char *
TypeStrEqv[] = {
  "sphere",
  "cylinder", "cylind", "rod",
  "cone",
  "superquad", "squad",
  "cube", "box",
  "triangle", "tri",
  "rectangle", "rect",
  "mesh", "tri-mesh", "trimesh",
  "isosurface",
  "aabbox", "AABoundingBox",
  "split",
  "list",
  "instance",
  ""
};

static const int
TypeValEqv[] = {
  echoTypeSphere,
  echoTypeCylinder, echoTypeCylinder, echoTypeCylinder,
  echoTypeCone,
  echoTypeSuperquad, echoTypeSuperquad,
  echoTypeCube, echoTypeCube,
  echoTypeTriangle, echoTypeTriangle,
  echoTypeRectangle, echoTypeRectangle,
  echoTypeTriMesh, echoTypeTriMesh, echoTypeTriMesh,
  echoTypeIsosurface,
  echoTypeAABBox, echoTypeAABBox,
  echoTypeSplit,
  echoTypeList,
  echoTypeInstance
};

static const airEnum
Type_ae = {
  "object type",
  ECHO_TYPE_NUM,
  TypeStr,  TypeVal,
  TypeDesc,
  TypeStrEqv, TypeValEqv,
  AIR_FALSE
};
const airEnum *const
echoType = &Type_ae;

/* ------------------------------ material types --------------------------- */

static const char *
MatterStr[ECHO_MATTER_MAX+1] = {
  "(unknown_matter)",
  "phong",
  "glass",
  "metal",
  "light"
};

static const char *
MatterDesc[ECHO_MATTER_MAX+1] = {
  "unknown material",
  "phong shaded surface",
  "glass",
  "metal",
  "light emitter"
};

static const airEnum
Matter_ae = {
  "matter",
  ECHO_MATTER_MAX,
  MatterStr,  NULL,
  MatterDesc,
  NULL, NULL,
  AIR_FALSE
};
const airEnum *const
echoMatter = &Matter_ae;
/* clang-format on */
