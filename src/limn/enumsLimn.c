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

#include "limn.h"

/* clang-format off */
static const char *
spaceStr[LIMN_SPACE_MAX+1] = {
  "(unknown space)",
  "world",
  "view",
  "screen",
  "device"
};

static const airEnum
space_ae = {
  "limn space",
  LIMN_SPACE_MAX,
  spaceStr, NULL,
  NULL,
  NULL, NULL,
  AIR_FALSE
};
const airEnum *const limnSpace = &space_ae;

/* ------------------------------------------------------------ */

static const char *
polyDataInfoStr[LIMN_POLY_DATA_INFO_MAX+1] = {
  "(unknown info)",
  "rgba",
  "norm",
  "tex2",
  "tang"
};

static const airEnum
polyDataInfo_ae = {
  "limn polydata info",
  LIMN_POLY_DATA_INFO_MAX,
  polyDataInfoStr, NULL,
  NULL,
  NULL, NULL,
  AIR_FALSE
};
const airEnum *const limnPolyDataInfo = &polyDataInfo_ae;

/* ------------------------------------------------------------ */

static const char *
cameraPathTrackStr[] = {
  "(unknown limnCameraPathTrack)",
  "from",
  "at",
  "both"
};

static const char *
cameraPathTrackDesc[] = {
  "unknown limnCameraPathTrack",
  "track through eye points, quaternions for camera orientation",
  "track through look-at points, quaternions for camera orientation",
  "track eye point, look-at point, and up vector with separate splines"
};

static const char *
cameraPathTrackStrEqv[] = {
  "from", "fr",
  "at", "look-at", "lookat",
  "both",
  ""
};

static const int
cameraPathTrackValEqv[] = {
  limnCameraPathTrackFrom, limnCameraPathTrackFrom,
  limnCameraPathTrackAt, limnCameraPathTrackAt, limnCameraPathTrackAt,
  limnCameraPathTrackBoth
};

static const airEnum
cameraPathTrack_ae = {
  "limnCameraPathTrack",
  LIMN_CAMERA_PATH_TRACK_MAX,
  cameraPathTrackStr, NULL,
  cameraPathTrackDesc,
  cameraPathTrackStrEqv, cameraPathTrackValEqv,
  AIR_FALSE
};
const airEnum *const limnCameraPathTrack = &cameraPathTrack_ae;

/* ------------------------------------------------------------ */

static const char *
primitiveStr[] = {
  "(unknown limnPrimitive)",
  "noop",
  "triangles",
  "tristrip",
  "trifan",
  "quads",
  "linestrip",
  "lines"
};

static const char *
primitiveDesc[] = {
  "unknown limnPrimitive",
  "no-op",
  "triangle soup",
  "triangle strip",
  "triangle fan",
  "quad soup",
  "line strip",
  "lines"
};

static const char *
primitiveStrEqv[] = {
  "noop",
  "triangles",
  "tristrip",
  "trifan",
  "quads",
  "linestrip",
  "lines",
  ""
};

static const int
primitiveValEqv[] = {
  limnPrimitiveNoop,
  limnPrimitiveTriangles,
  limnPrimitiveTriangleStrip,
  limnPrimitiveTriangleFan,
  limnPrimitiveQuads,
  limnPrimitiveLineStrip,
  limnPrimitiveLines
};

static const airEnum
primitive_ae = {
  "limnPrimitive",
  LIMN_PRIMITIVE_MAX,
  primitiveStr, NULL,
  primitiveDesc,
  primitiveStrEqv, primitiveValEqv,
  AIR_FALSE
};
const airEnum *const limnPrimitive = &primitive_ae;
/* clang-format on */
