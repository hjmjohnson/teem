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

#include <teem/nrrd.h>

/*
** Tests:
** rnParse_sizes (and sibling per-axis field parsers) at dimension NRRD_DIM_MAX.
**
** The per-axis field parsers parse dim+1 values into a NRRD_DIM_MAX-element
** stack array purely to detect an excess of values.  At dim == NRRD_DIM_MAX a
** header supplying a (NRRD_DIM_MAX+1)-th token wrote one element past the array
** (stack buffer overflow).  This test feeds such a header and expects a clean
** parse error rather than a crash.
*/

int
main(int argc, const char **argv) {
  const char *me;
  Nrrd *nrrd;
  airArray *mop;
  /* dimension 16, but the sizes line carries 17 tokens */
  static const char header[] = "NRRD0004\n"
                               "type: unsigned char\n"
                               "dimension: 16\n"
                               "sizes: 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1\n"
                               "encoding: raw\n"
                               "\n";

  AIR_UNUSED(argc);
  me = argv[0];
  mop = airMopNew();
  nrrd = nrrdNew();
  airMopAdd(mop, nrrd, nrrdNuke_mop, airMopAlways);

  /* Must return an error (excess sizes), and must not overflow the stack. */
  if (!nrrdStringRead(nrrd, header, NULL)) {
    fprintf(stderr, "%s: nrrdStringRead accepted 17 sizes for a 16-D nrrd\n", me);
    airMopError(mop);
    return 1;
  }
  /* Expected error path: discard the biff message. */
  free(biffGetDone(NRRD));
  fprintf(stderr, "%s: excess-sizes header rejected cleanly, as expected\n", me);
  airMopOkay(mop);
  return 0;
}
