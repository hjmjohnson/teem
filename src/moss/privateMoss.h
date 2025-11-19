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
#define ASP1_M (AIR_STRLEN_MEDIUM + 1)
#define ASP1_L (AIR_STRLEN_LARGE + 1)
#define ASP1_H (AIR_STRLEN_HUGE + 1)

typedef unsigned int uint; /* uint is just more concise */

#define MOSS_SX(img)                                                                    \
  AIR_INT((3 == (img)->dim ? (img)->axis[1].size : (img)->axis[0].size))
#define MOSS_SY(img)                                                                    \
  AIR_INT((3 == (img)->dim ? (img)->axis[2].size : (img)->axis[1].size))

#define MOSS_MAT_SET(mat, a, b, x, c, d, y)                                             \
  (mat)[0] = (a);                                                                       \
  (mat)[1] = (b);                                                                       \
  (mat)[2] = (x);                                                                       \
  (mat)[3] = (c);                                                                       \
  (mat)[4] = (d);                                                                       \
  (mat)[5] = (y)

#define MOSS_MAT_COPY(m2, m1)                                                           \
  (m2)[0] = (m1)[0];                                                                    \
  (m2)[1] = (m1)[1];                                                                    \
  (m2)[2] = (m1)[2];                                                                    \
  (m2)[3] = (m1)[3];                                                                    \
  (m2)[4] = (m1)[4];                                                                    \
  (m2)[5] = (m1)[5]

#define MOSS_MAT_6TO9(m2, m1)                                                           \
  ELL_3V_COPY((m2) + 0, (m1) + 0);                                                      \
  ELL_3V_COPY((m2) + 3, (m1) + 3);                                                      \
  ELL_3V_SET((m2) + 6, 0, 0, 1)

#define MOSS_MAT_9TO6(m2, m1)                                                           \
  MOSS_MAT_SET(m2, (m1)[0], (m1)[1], (m1)[2], (m1)[3], (m1)[4], (m1)[5])

/* methodsMoss.c */
extern int _mossCenter(int center);

#ifdef __cplusplus
}
#endif
