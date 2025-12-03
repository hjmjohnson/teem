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

#include "../air.h"
#include "../privateAir.h"

/*
 *  0     1    2    (3)
 * terf <max> <N>
 *
 * test/terf 3 100000000 once said:
 *
 * test/terf: Teem: 0.26568484306335449 sec to find avg [0,3] erfc val 0.1880620811708611
 * with 100000000 samples
 *
 * test/terf: libc: 0.64336109161376953 sec to find avg [0,3] erfc val
 * 0.18806208117086681 with 100000000 samples
 *
 * which suggests that airErfc() is definitely faster than libm erfc()
 * thus worth keeping around
 */
int
main(int argc, char *argv[]) {
  char *me = argv[0];
  double max;
  uint NN;
  if (3 != argc || 1 != sscanf(argv[1], "%lg", &max)
      || 1 != sscanf(argv[2], "%u", &NN)) {
    fprintf(stderr, "usage: %s <max> <N> \n", me);
    exit(1);
  }
  double avg = 0, time0 = airTime();
  for (uint ii = 0; ii < NN; ii++) {
    avg += airErfc((max * ii) / NN);
  }
  double teemTime = airTime() - time0;
  avg /= NN;
  printf("%s: Teem: %.17g sec to find avg [0,%g] erfc val %.17g with %u samples\n", me,
         teemTime, max, avg, NN);

  avg = 0;
  time0 = airTime();
  for (uint ii = 0; ii < NN; ii++) {
    avg += erfc((max * ii) / NN);
  }
  double libcTime = airTime() - time0;
  avg /= NN;
  printf("%s: libm: %.17g sec to find avg [0,%g] erfc val %.17g with %u samples\n", me,
         libcTime, max, avg, NN);

  exit(0);
}
