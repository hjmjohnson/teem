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

FILE *
myopen(const char *name) {

  if (!strcmp(name, "-")) {
    return stdin;
  } else {
    return fopen(name, "r");
  }
}

void
myclose(FILE *file) {

  if (file != stdin) {
    fclose(file);
  }
  return;
}

int
main(int argc, const char *argv[]) {
  const char *me = argv[0];
  if (3 != argc) {
    /*                       0    1     2  (3) */
    fprintf(stderr, "usage: %s <size> <file>\n", me);
    exit(1);
  }
  const char *sizeStr = argv[1];
  unsigned int sizeui;
  if (1 != sscanf(sizeStr, "%u", &sizeui)) {
    fprintf(stderr, "%s: couldn't parse \"%s\" as uint\n", me, sizeStr);
    exit(1);
  }
  size_t size = sizeui;
  const char *fileS = argv[2];
  FILE *file;
  if (!(file = myopen(fileS))) {
    fprintf(stderr, "%s: couldn't open \"%s\" for reading\n", me, fileS);
    exit(1);
  }

  char *line = size ? AIR_CALLOC(size, char) : NULL;
  /* (copied from airOneLine info)
   * Return values:
   * 0 and line[0]=='\0': immediately hit EOF, or arguments are invalid
   * 0 and line[0]!='\0': read something into `line` but hit EOF before seeing newline;
   *                      caller will need strlen() to find out its length
   *                   1: line was a single newline (or other line termination)
   *      N with N<=size: line was N-1 characters followed by newline
   *              size+1: didn't see a newline within size-1 characters
   */
  size_t ret;
  do {
    ret = airOneLine(file, line, size);
    if (ret || (line && line[0])) {
      printf("%3zu %c|%s|=%zu\n", ret,
             (ret == size + 1 ? 'M'                    /* maxed out */
                              : (!ret && line[0] ? 'E' /* hit EOF before end-of-line */
                                                 : ' ')),
             line, strlen(line));
    }
  } while (ret > 0);
  size++;
  free(line);
  myclose(file);
  exit(0);
}
