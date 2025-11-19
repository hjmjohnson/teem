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

#ifdef __cplusplus
extern "C" {
#endif

/* NOTE: these SN_,ASP1_ string macros copy-pasta'd to other private<Lib>.h files:
   SN_INCR: safely increments STR arg to point '\0'-termination of STR
   SN_COPY: safely copies SRC to DST, then increments DST
   SN_PRINTF: snprintf's into DST then increments DST
   ASP1_X: short version of AIR_STRLEN_X + 1 */
#define SN_INCR(STR, SIZE)                                                              \
  do {                                                                                  \
    size_t tmp_str_len_##STR = strlen(STR);                                             \
    STR += tmp_str_len_##STR;                                                           \
    SIZE -= tmp_str_len_##STR;                                                          \
  } while (0)
#define SN_COPY(DST, DST_SIZE, SRC)                                                     \
  do {                                                                                  \
    airStrcpy((DST), (DST_SIZE), (SRC));                                                \
    SN_INCR(DST, DST_SIZE);                                                             \
  } while (0)
#define SN_PRINTF(DST, DST_SIZE, FMT, ...)                                              \
  do {                                                                                  \
    snprintf((DST), (DST_SIZE), (FMT), __VA_ARGS__);                                    \
    SN_INCR(DST, DST_SIZE);                                                             \
  } while (0)
#define ASP1_S (AIR_STRLEN_SMALL + 1)
#define ASP1_M (AIR_STRLEN_MED + 1)
#define ASP1_L (AIR_STRLEN_LARGE + 1)
#define ASP1_H (AIR_STRLEN_HUGE + 1)

typedef unsigned int uint; /* uint is just more concise */

/* hvol.c */
extern int _baneAxisCheck(baneAxis *axis);

#define BANE_GKMS_CMD(name, info)                                                       \
  const unrrduCmd baneGkms_##name##Cmd = {#name, info, baneGkms_##name##Main, AIR_FALSE}

/* USAGE, PARSE
   all copied from unrrdu/privateUnrrdu.h */
#define USAGE(INFO)                                                                     \
  if (!argc && !hparm->noArgsIsNoProblem) {                                             \
    hestInfo(stderr, me, (INFO), hparm);                                                \
    hestUsage(stderr, opt, me, hparm, AIR_FALSE);                                       \
    hestGlossary(stderr, opt, hparm);                                                   \
    airMopError(mop);                                                                   \
    return 2;                                                                           \
  }

#define PARSE(INFO)                                                                     \
  if ((pret = hestParse(opt, argc, argv, &perr, hparm))) {                              \
    if (1 == pret) {                                                                    \
      fprintf(stderr, "%s: %s\n", me, perr);                                            \
      free(perr);                                                                       \
      hestUsage(stderr, opt, me, hparm, AIR_FALSE);                                     \
      if (hparm && hparm->noArgsIsNoProblem) {                                          \
        fprintf(stderr, "\nFor more info: \"%s --help\"\n", me);                        \
      } else {                                                                          \
        fprintf(stderr, "\nFor more info: \"%s\" or \"%s --help\"\n", me, me);          \
      }                                                                                 \
      airMopError(mop);                                                                 \
      return 2;                                                                         \
    } else {                                                                            \
      exit(1);                                                                          \
    }                                                                                   \
  } else if (opt->helpWanted) {                                                         \
    hestInfo(stdout, me, (INFO), hparm);                                                \
    hestUsage(stdout, opt, me, hparm, AIR_FALSE);                                       \
    hestGlossary(stdout, opt, hparm);                                                   \
    return 0;                                                                           \
  }

#define USAGE_PARSE(INFO) USAGE(INFO) PARSE(INFO)

#ifdef __cplusplus
}
#endif
