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

#include <teem/biff.h>

/*
** Tests:
** biffAddf
** biffMovef
** biffGet
** biffDone
** biffGetDone
**
** Also uses:
** airMopNew, airMopAdd, airMopError, airMopDone
*/

int
main(int argc, const char *argv[]) {
  const char *me;
  char *got;
  airArray *mop;

  AIR_UNUSED(argc);
  me = argv[0];
  mop = airMopNew();

  /* HEY: the creation and comparison of these strings is not very
     flexible, especially the hard-coded "want[]" strings */
  biffAddf("axis", "the first error axis");
  biffAddf("chard", "the first error chard");
  biffAddf("chard", "the second error chard");
  biffAddf("axis", "the second error axis");
  biffAddf("chard", "the third error chard");
  biffAddf("bingo", "zero-eth bingo message");
  biffMovef("bingo", "chard", "moving chard->bingo");
  biffAddf("bingo", "the first error bingo");
  biffAddf("bingo", "the second bll boo boo boo error bingo");
  biffAddf("bingo", "the third error bingo");
  biffAddf("axis", "the third error axis");
  {
    char want[] = "[bingo] the third error bingo\n"
                  "[bingo] the second bll boo boo boo error bingo\n"
                  "[bingo] the first error bingo\n"
                  "[bingo] moving chard->bingo\n"
                  "[bingo] [chard] the third error chard\n"
                  "[bingo] [chard] the second error chard\n"
                  "[bingo] [chard] the first error chard\n"
                  "[bingo] zero-eth bingo message\n";
    got = biffGet("bingo");
    fprintf(stderr, "!%s: biffGet(bingo) = <%s>\n", __func__, got);
    airMopAdd(mop, got, airFree, airMopAlways);

    /* an ugly macro */
#define COMPARE(N)                                                                      \
  airMopAdd(mop, got, airFree, airMopAlways);                                           \
  if (strcmp(got, want)) {                                                              \
    fprintf(stderr, "%s: %d: #got=(%s)# != #want=(%s)#\n", me, N, got, want);           \
    airMopError(mop);                                                                   \
    exit(1);                                                                            \
  }

    COMPARE(1);
  }

  {
    char want[] = "[chard] No information for this key!";
    got = biffGet("chard");
    COMPARE(2);
  }

  {
    char want[] = "[axis] the third error axis\n"
                  "[axis] the second error axis\n"
                  "[axis] the first error axis\n";
    got = biffGet("axis");
    COMPARE(3);
  }

  biffAddf("harold", "the first error harold");
  biffAddf("harold", "the second error harold");
  biffAddf("harold", "the third error harold");
  {
    char want[] = "[harold] the third error harold\n"
                  "[harold] the second error harold\n"
                  "[harold] the first error harold\n";
    got = biffGetDone("harold");
    COMPARE(4);
  }

  biffDone("bingo");
  biffDone("axis");
  biffDone("chard");

  biffAddf("axis", "the first error axis");
  biffAddf("axis", "the second error axis");
  biffAddf("axis", "the third error axis");
  biffAddf("axis", "the fourth error axis");
  biffAddf("axis", "the fifth error axis");
  {
    char want[] = "[axis] the fifth error axis\n"
                  "[axis] the fourth error axis\n"
                  "[axis] the third error axis\n"
                  "[axis] the second error axis\n"
                  "[axis] the first error axis\n";
    got = biffGetDone("axis");
    COMPARE(5);
  }

  biffAddf("test", "%s: this is a test of biffAddf %d %g", "me", 1, 4.2);
  {
    char want[] = "[test] me: this is a test of biffAddf 1 4.2\n";
    got = biffGetDone("test");
    COMPARE(6);
  }

  biffAddf("test2", "%s: this is a test of biffAddf %d %g", "me", 1, 4.2);
  biffMovef("test3", "test2", "%s: testing biffMove %d.", "me", 1729);
  {
    char want[] = "[test3] me: testing biffMove 1729.\n"
                  "[test3] [test2] me: this is a test of biffAddf 1 4.2\n";
    got = biffGet("test3");
    COMPARE(7);
  }

  airMopOkay(mop);
  exit(0);
}
