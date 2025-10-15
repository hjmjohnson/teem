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
  parm->verbosity = 3;

  opt = NULL;
  int flag;
  hestOptAdd(&opt, "f,flag", NULL, airTypeInt, 0, 0, &flag, NULL,
             "a flag created via hestOptAdd");
  int glag;
  hestOptAdd_Flag(&opt, "g,glag", &glag, "a flag created via hestOptAdd_Flag");
  int v;
  hestOptAdd_1v_Int(&opt, "v,verbose", "level", &v, "0", "verbosity level");
  char *out;
  hestOptAdd_1_String(&opt, "out", "file", &out, "output.ppm", "PPM image output");
  char *blah[3];
  hestOptAdd_3_String(&opt, "blah", "input", blah, "a b c", "input image file(s)");
  /*
  // 2025 re-write: only allow simple scalar types as single variadic parm (not String)
  char *option = NULL;
  hestOptAdd(&opt, "option", "opt", airTypeString, 0, 1, &option, "default",
             "this is just a test");
   */
  int *ints;
  unsigned int numN;
  hestOptAdd_Nv_Int(&opt, "ints", "N", 1, -1, &ints, "10 20 30", "a list of integers",
                    &numN);
  int res[2];
  hestOptAdd_2_Int(&opt, "res", "sx sy", res, NULL, "image resolution");
  char **in;
  unsigned int numIn;
  hestOptAdd_Nv_String(&opt, NULL, "input", 1, -1, &in, NULL, "input image file(s)",
                       &numIn);
  parm->dieWithoutNewerFeatures = 0;
  hestParseOrDie(opt, argc - 1, argv + 1, parm, argv[0], info);

  {
    unsigned int opi, numO;
    numO = hestOptNum(opt);
    for (opi = 0; opi < numO; opi++) {
      printf("opt %u/%u:\n", opi, numO);
      printf("  flag=%s; ", opt[opi].flag ? opt[opi].flag : "(null)");
      printf("  name=%s\n", opt[opi].name ? opt[opi].name : "(null)");
      printf("  source=%s; ", hestSourceDefault == opt[opi].source
                                ? "default"
                                : (hestSourceUser(opt[opi].source) ? "user" : "???"));
      printf("  parmStr=|%s|\n", opt[opi].parmStr ? opt[opi].parmStr : "(null)");
    }
  }
  printf("(err = %s)\n", err ? err : "(null)");
  printf("  v = %d\n", v);
  printf("  flag glag = %d %d\n", flag, glag);
  printf("out = \"%s\"\n", out ? out : "(null)");
  printf("blah = \"%s\" \"%s\" \"%s\"\n", blah[0], blah[1], blah[2]);
  // printf("option = \"%s\"\n", option ? option : "(null)");
  printf("res = %d %d\n", res[0], res[1]);
  printf("\nin = %d files:", numIn);
  for (unsigned int n = 0; n <= numIn - 1; n++) {
    printf(" \"%s\"", in[n] ? in[n] : "(null)");
  }
  printf("\n");
  printf("ints = %d ints:", numN);
  for (unsigned int n = 0; n <= numN - 1; n++) {
    printf(" %d", ints[n]);
  }
  printf("\n");

  /* free the memory allocated by parsing ... */
  hestParseFree(opt);
  /* ... and the other stuff */
  opt = hestOptFree(opt);
  parm = hestParmFree(parm);
  exit(0);
}
