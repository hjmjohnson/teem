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

#include "../hest.h"

int
main(int argc, const char **argv) {
  char *err = NULL,
       info[] = "This program does nothing in particular, though it does attempt "
                "to pose as some sort of command-line image processing program. "
                "As usual, any implied functionality is purely coincidental, "
                "especially since this is the output of a unicyclist.";

  /* to debug how we're doing C-locale-emulation */
  airSetLocaleFromEnvOrExit(argv[0], airStderr());

  airArray *mop = airMopNew();
  hestParm *hparm = hestParmMopNew();
  airMopAdd(mop, hparm, hestParmFree_mop, airMopAlways);
  hparm->verbose = 10;

  hestOpt *hopt = NULL;
  int flag;
  hestOptAdd(&hopt, "f,flag", NULL, airTypeInt, 0, 0, &flag, NULL,
             "a flag created via hestOptAdd");
  int glag;
  hestOptAdd_Flag(&hopt, "g,glag", &glag, "a flag created via hestOptAdd_Flag");
  char *out;
  hestOptAdd_1_String(&hopt, "out", "file", &out, "output.ppm", "PPM image output");
  int v;
  hestOptAdd_1v_Int(&hopt, "v,verbose", "level", &v, "0", "verbosity level");
  double dbl;
  hestOptAdd_1_Double(&hopt, "d,double", "val", &dbl, "1.5", "an FP value");
  char *blah[3];
  hestOptAdd_3_String(&hopt, "blah", "input", blah, "a b c", "input image file(s)");
  int norenorm;
  hestOptAdd_Flag(&hopt, "nrn", &norenorm,
                  "do NOT do per-pass kernel weight renormalization. "
                  "Doing the renormalization is not a performance hit (hence is "
                  "enabled by default), and the renormalization is sometimes "
                  "needed to avoid \"grating\" on non-integral "
                  "down-sampling.  Disabling the renormalization is needed for "
                  "correct results with artificially narrow kernels. ");

  /*
  // 2025 re-write: only allow simple scalar types as single variadic parm (not String)
  char *option = NULL;
  hestOptAdd(&hopt, "option", "opt", airTypeString, 0, 1, &option, "default",
             "this is just a test");
   */
  int *ints;
  unsigned int numN;
  hestOptAdd_Mv_Int(&hopt, "ints", "N", 1, -1, &ints, "10 20 30", "a list of integers",
                    &numN);
  int res[2];
  hestOptAdd_2_Int(&hopt, "res", "sx sy", res, NULL, "image resolution");
  char **in;
  unsigned int numIn;
  hestOptAdd_Mv_String(&hopt, NULL, "input", 1, -1, &in, NULL, "input image file(s)",
                       &numIn);
  // hparm->orExitWithoutNewerFeatures = AIR_TRUE;
  hestParseOrExit(hopt, argc - 1, argv + 1, hparm, argv[0], info);
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);
  {
    unsigned int opi, numO;
    numO = hestOptNum(hopt);
    for (opi = 0; opi < numO; opi++) {
      printf("opt %u/%u:\n", opi, numO);
      printf("  flag=%s; ", hopt[opi].flag ? hopt[opi].flag : "(null)");
      printf("  name=%s\n", hopt[opi].name ? hopt[opi].name : "(null)");
      printf("  source=%s; ", hestSourceDefault == hopt[opi].source
                                ? "default"
                                : (hestSourceUser(hopt[opi].source) ? "user" : "???"));
      printf("  parmStr=|%s|\n", hopt[opi].parmStr ? hopt[opi].parmStr : "(null)");
    }
  }
  printf("(err = %s)\n", err ? err : "(null)");
  printf("  v = %d\n", v);
  printf("  flag glag = %d %d\n", flag, glag);
  airSinglePrintf(airStdout(), NULL, 0, "dbl = %lg\n", dbl);
  printf("out = \"%s\"\n", out ? out : "(null)");
  printf("blah = \"%s\" \"%s\" \"%s\"\n", blah[0], blah[1], blah[2]);
  // printf("option = \"%s\"\n", option ? option : "(null)");
  printf("res = %d %d\n", res[0], res[1]);
  printf("\nin = %u files:", numIn);
  for (unsigned int n = 0; n <= numIn - 1; n++) {
    printf(" \"%s\"", in[n] ? in[n] : "(null)");
  }
  printf("\n");
  printf("ints = %u ints:", numN);
  for (unsigned int n = 0; n <= numN - 1; n++) {
    printf(" %d", ints[n]);
  }
  printf("\n");

  /* free the memory allocated by parsing ... */
  /* ... and the other stuff */
  airMopOkay(mop);
  exit(0);
}
