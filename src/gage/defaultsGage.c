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

#include "gage.h"
#include "privateGage.h"

const char *const gageBiffKey = "gage";

int gageDefaultVerbose = 0;

double gageDefaultGradMagCurvMin = 0.0;

int gageDefaultRenormalize = AIR_FALSE;

int gageDefaultCheckIntegrals = AIR_TRUE;

int gageDefaultK3Pack = AIR_TRUE;

int gageDefaultCurvNormalSide = 1;

double gageDefaultKernelIntegralNearZero = 0.0001;

int gageDefaultStackNormalizeRecon = AIR_FALSE;

int gageDefaultStackNormalizeDeriv = AIR_FALSE;

double gageDefaultStackNormalizeDerivBias = 0.0;

int gageDefaultOrientationFromSpacing = AIR_FALSE;
/* Before teem 1.10.0, gage behaved inconsistently: Derivatives were
   taken as if orientationFromSpacing were TRUE, the index space to
   world space mapping acted as if it were FALSE. Now, you have the
   choice and get consistent results in either case. */

int gageDefaultGenerateErrStr = AIR_TRUE;
/* Before Teem 1.11, error strings were always sprintf, which can easily
   become a bottleneck in some situations, but this should still stay
   the default behavior */
