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

#include "../hest.h"

typedef unsigned int uint;

int
main(int argc, const char **argv) {

  // airEnumPrint(stdout, hestSource);
  char enerr[AIR_STRLEN_LARGE + 1];
  if (airEnumCheck(enerr, hestSource)) {
    fprintf(stderr, "%s: problem:\n%s\n", argv[0], enerr);
    exit(1);
  }

  int ret = 0;
  hestOpt *opt = NULL;
  hestParm *hparm = hestParmNew();
  hparm->respectDashDashHelp = AIR_TRUE;
  hparm->responseFileEnable = AIR_TRUE;
  hparm->verbose = 10;

  int flag;
  hestOptAdd_Flag(&opt, "b,bingo", &flag, "a flag");
  int verb;
  hestOptAdd_1_Int(&opt, "v", "verb", &verb, "0", "verbosity");
  int funny;
  hestOptAdd_1v_Int(&opt, "funny", "wut", &funny, "0", "humor level");
  /*
  int *size;
  uint slen;
  hestOptAdd_Nv_Int(&opt, "s,sizes", "sx sy", 2, -1, &size, NULL, "image resolutions",
                    &slen);
                    */
  char **kvp;
  uint kvpLen;
  hestOptAdd_Nv_String(&opt, "kvp", "k0", 0, -1, &kvp, "",
                       "what \"kind\" is each axis, from the nrrdKind airEnum "
                       "(e.g. space, time, 3-vector, 3D-masked-symmetric-matrix, "
                       "or \"none\" to signify no kind)",
                       &kvpLen);
  double *off;
  uint offLen;
  hestOptAdd_Nv_Double(&opt, "off,offset", "off0", 0, -1, &off, "",
                       "For each axis, an offset or shift to the position (in index "
                       "space) of the lower end of the sampling domain. "
                       "Either -off can be used, or -min and -max "
                       "together, or none of these (so that, by default, the full "
                       "domain of the axis is resampled).",
                       &offLen);
  int unpA[2];
  uint opiA = hestOptAdd_2_Int(&opt, NULL, "A0 A1", unpA, "0 1", "unflagged A");
  int *unpB;
  uint sawB;
  uint opiB = hestOptAdd_Nv_Int(&opt, NULL, "B0", 2, -1, &unpB, "23 34", "unflagged B",
                                &sawB);
  int unpC[2];
  uint opiC = hestOptAdd_2_Int(&opt, NULL, "C C", unpC, /* "dfltC0 dfltC1" */ NULL,
                               "unflagged C");
#if 0
  // to test disallowance of >1 one unflagged variadic option
  int *unpC;
  uint sawC;
  hestOptAdd_Nv_Int(&opt, NULL, "C0 C1", 1, 2, &unpC, NULL, "unflagged C", &sawC);
#endif

  hestParseOrDie(opt, argc - 1, argv + 1, hparm, argv[0], "info about tparse");
  printf("funny = %d\n", funny);
  printf("kvpLen = %u\n", kvpLen);
  for (uint ki = 0; ki < kvpLen; ki++) {
    printf("kvp[%u] = |%s|\n", ki, kvp[ki]);
  }
  printf("offLen = %u\n", offLen);
  for (uint ii = 0; ii < offLen; ii++) {
    printf("off[%u] = %g\n", ii, off[ii]);
  }
  printf("unpA = %d %d (source %s)\n", unpA[0], unpA[1],
         airEnumStr(hestSource, opt[opiA].source));
  printf("unpB (source %s) = ...\n", airEnumStr(hestSource, opt[opiB].source));
  for (uint ii = 0; ii < sawB; ii++) {
    printf("   [%u] = %u\n", ii, unpB[ii]);
  }
  printf("unpC = %d %d (source %s)\n", unpC[0], unpC[1],
         airEnumStr(hestSource, opt[opiC].source));
  hestParseFree(opt);
  hestOptFree(opt);
  hestParmFree(hparm);

  exit(ret);
}
