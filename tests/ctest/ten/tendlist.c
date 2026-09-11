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

#include <teem/ten.h>

/*
** Tests:
*/

int
main(int argc, const char **argv) {
  const char *me;
  airArray *mop;

  hestParm *hparm;
  unsigned int tci;
  FILE *out;
  int ret;

  AIR_UNUSED(argc);
  out = stdout;
  me = argv[0]; // NOT how "me" is used within Teem (when not __func__)
  mop = airMopNew();
  hparm = hestParmNew();
  airMopAdd(mop, hparm, (airMopper)hestParmFree, airMopAlways);

  /* (same purpose as Testing/unrrdu/unulist.c) */

  fprintf(out, "%s: ################### BEGIN tend\n", me);
  ret = unrrduUsage("tend", tendTitle, tendCmdList, hparm);
  fprintf(out, "%s: ################### END tend (ret=%d)\n", me, ret);

  tci = 0;
  hparm->respectDashDashHelp = AIR_TRUE;
  const char *hargv[2] = {"--help", NULL};
  do {
    fprintf(out, "%s: ################### BEGIN tend %s\n", me, tendCmdList[tci]->name);
    ret = tendCmdList[tci]->mainFunc(1, hargv, tendCmdList[tci]->name, hparm);
    fprintf(out, "%s: ################### END tend %s (ret=%d)\n", me,
            tendCmdList[tci]->name, ret);
    tci++;
  } while (tendCmdList[tci]);

  airMopOkay(mop);
  return 0;
}
