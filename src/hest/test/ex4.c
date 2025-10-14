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
parse(void *_ptr, const char *str, char *err) {
  double *ptr;
  int ret;

  ptr = _ptr;
  ret = sscanf(str, "%lf,%lf", ptr + 0, ptr + 1);
  if (2 != ret) {
    sprintf(err, "parsed %d (not 2) doubles", ret);
    return 1;
  }
  return 0;
}

hestCB cbinfo = {2 * sizeof(double), "location", parse, NULL};

int
main(int argc, const char **argv) {
  hestOpt *opt = NULL;

  double single[2];
  hestOptAdd_1_Other(&opt, "A", "x,y", single, "30,50", "testing A", &cbinfo);
  double triple[6];
  hestOptAdd_3_Other(&opt, "B", "x1,y1 x2,y2 x3,y3", triple, "1,2 3,4 5,6", "testing B",
                     &cbinfo);
  /*
  2025 re-write: only allow simple scalar types as single variadic parm (not Other)
  double maybe[2];
  hestOptAdd(&opt, "C", "mx,my", airTypeOther, 0, 1, maybe, "-0.1,-0.2",
             "testing C. The utility of this can be better "
             "demonstrated in the following manner:\n "
             "- wash the dishes\n "
             "- put the dishes in the cupboard\n "
             "- watch football on TV\n "
             "- remember to walk the dog",
             NULL, NULL, &cbinfo);
  */
  double *many;
  unsigned howMany;
  hestOptAdd_Nv_Other(&opt, "D", "nx,ny", 1, -1, &many, "8,8 7,7", "testing D", &howMany,
                      &cbinfo);
  int N;
  hestOptAdd_1_Int(&opt, "int", "N", &N, NULL, "an integer");

  hestParseOrDie(opt, argc - 1, argv + 1, NULL, argv[0], "hest/text/ex4",
                 AIR_TRUE /* doInfo */, AIR_TRUE /* doUsage */,
                 AIR_TRUE /* doGlossary */);

  printf("single: (%g,%g)\n", single[0], single[1]);
  printf("triple: (%g,%g) (%g,%g) (%g,%g)\n", triple[0], triple[1], triple[2], triple[3],
         triple[4], triple[5]);
  /*
  printf("maybe: (%g,%g)\n", maybe[0], maybe[1]);
  */
  printf("many(%d):", howMany);
  for (unsigned int i = 0; i <= howMany - 1; i++) {
    printf(" (%g,%g)", many[0 + 2 * i], many[1 + 2 * i]);
  }
  printf("\n");

  hestParseFree(opt);
  hestOptFree(opt);
  exit(0);
}
