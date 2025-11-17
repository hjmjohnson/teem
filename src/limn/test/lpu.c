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

#include "../limn.h"

/* This WAS a massive gangly copy and paste from teem/src/bin/tend.c ...
   but now it is a single simple call to unrrduMainOrExit */

int
main(int argc, const char **argv) {
  /* this doesn't really need a hestParm to customize;
     but could use one here a test of the unrrduMainOrExit's handling of it */
  // hestParm *hparm = hestParmMopNew();
  // hparm->respectDashDashHelp = AIR_TRUE;
  return unrrduMainOrExit(argc, argv, "lpu", "limnPolyData Hacking", limnPuCmdList,
                          NULL);
}
