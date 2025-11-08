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

/* NOTE: these SN_ string,size macros copy-pasta'd to other private<Lib>.h files:
   SN_INCR: safely increments STR arg to point '\0'-termination of STR
   SN_COPY: safely copies SRC to DST, then increments DST
   SN_PRINTF: snprintf's into DST then increments DST */
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
/* oops can't use this upstream of NrrdIO; variadic macros are C99
#define SN_PRINTF(DST, DST_SIZE, FMT, ...)                                              \
  do {                                                                                  \
    snprintf((DST), (DST_SIZE), (FMT), __VA_ARGS__);                                    \
    SN_INCR(DST, DST_SIZE);                                                             \
  } while (0)
*/

typedef unsigned int uint; /* uint is just more concise */

/*
 * This private header was created because the _biffMsgAddVL and _biffMsgMoveVL functions
 * are only used within in the biff sources. They take a va_list, which is unusual, and
 * (currently) used for no other public functions in Teem.
 *
 * Furthermore, pre-1.13 release it became apparent that nothing else in Teem (outside of
 * biff) was using any biffMsg anything, so these were also all moved into here.
 * For TeemV2, the _ prefix was belatedly added to the biffMsg symbols (as is expected of
 * "private" text symbols in the library), and some were removed because they were only
 * used once in biffbiff.c (so their body could be effectively inlined at their usage)
 */

/*
** biffMsg struct
**
** externally usable thing for holding error messages
*/
typedef struct {
  char *key;           /* string for identifying the general source
                          of the error message; set once, at time
                          of biffMsg creation */
  char **err;          /* array of error strings; the err array itself
                          is NOT null-terminated */
  unsigned int errNum; /* length of "err" == # strings stored */
  airArray *errArr;    /* air array for err and num */
} biffMsg;

/* biffmsg.c */
extern const biffMsg *const _biffMsgNoop;
extern void _biffMsgAddVL(biffMsg *msg, const char *errfmt, va_list args);
extern void _biffMsgMoveVL(biffMsg *dest, biffMsg *src, const char *errfmt,
                           va_list args);

extern biffMsg *_biffMsgNew(const char *key);
extern biffMsg *_biffMsgNix(biffMsg *msg);
extern void _biffMsgAdd(biffMsg *msg, const char *err);
extern void _biffMsgMove(biffMsg *dest, biffMsg *src, const char *err);

#ifdef __cplusplus
}
#endif
