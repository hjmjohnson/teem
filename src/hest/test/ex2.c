/*
  Teem: Tools to process and visualize scientific data and images
  Copyright (C) 2009--2023  University of Chicago
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
  hestOpt *opt = NULL;
  hestParm *parm;
  char *err = NULL,
       info[] = "This program does nothing in particular, though it does attempt "
                "to pose as some sort of command-line image processing program. "
                "As usual, any implied functionality is purely coincidental, "
                "especially since this is the output of a unicyclist.";

  parm = hestParmNew();
  parm->responseFileEnable = AIR_TRUE;
  parm->respectDashDashHelp = AIR_TRUE;

  opt = NULL;
  int res[2];
  hestOptAdd_2_Int(&opt, "res", "sx sy", res, NULL, "image resolution");
  int v;
  hestOptAdd_1v_Int(&opt, "v", "level", &v, "0", "verbosity level");
  char *out;
  hestOptAdd_1_String(&opt, "out", "file", &out, "output.ppm", "PPM image output");
  char **in;
  unsigned int numIn;
  hestOptAdd_Nv_String(&opt, NULL, "input", 1, -1, &in, NULL, "input image file(s)",
                       &numIn);

  if (1 == argc) {
    /* didn't get anything at all on command line */
    /* print program information ... */
    hestInfo(stderr, argv[0], info, parm);
    /* ... and usage information ... */
    hestUsage(stderr, opt, argv[0], parm);
    hestGlossary(stderr, opt, parm);
    /* ... and avoid memory leaks */
    opt = hestOptFree(opt);
    parm = hestParmFree(parm);
    exit(1);
  }

  /* else we got something, see if we can parse it */
  int pret = hestParse(opt, argc - 1, argv + 1, &err, parm);
  if (pret || opt->helpWanted) {
    if (pret) {
      fprintf(stderr, "ERROR: %s\n", err);
      free(err);
    }
    /* print usage information ... */
    FILE *stdo = pret ? stderr : stdout;
    hestUsage(stdo, opt, argv[0], parm);
    hestGlossary(stdo, opt, parm);
    /* ... and then avoid memory leaks */
    opt = hestOptFree(opt);
    parm = hestParmFree(parm);
    exit(pret);
  }

  printf("(err = %s)\n", err);
  printf("res = %d %d\n", res[0], res[1]);
  printf("  v = %d\n", v);
  printf("out = \"%s\"\n", out);
  printf(" in = %d files:", numIn);
  for (unsigned int n = 0; n <= numIn - 1; n++) {
    printf(" \"%s\"", in[n]);
  }
  printf("\n");

  /* free the memory allocated by parsing ... */
  hestParseFree(opt);
  /* ... and the other stuff */
  opt = hestOptFree(opt);
  parm = hestParmFree(parm);
  exit(0);
}
