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

/*
 define ell_3v_PRINT ell_3v_print_d
 define ell_3m_PRINT ell_3m_print_d
 define ell_3v_PERP ell_3v_perp_d
 define nrrdLOOKUP nrrdDLookup
 define EVALN evalN_d       / * NrrdKernel method * /
*/

/* the what to put in gctx->errStr when !generateErrStr */
#define MY_GAGE_NON_ERR_STR "(error)"

/* shape.c */
extern int gage__ShapeSet(const gageContext *ctx, gageShape *shape, const Nrrd *nin,
                          unsigned int baseDim);

/* ctx.c */
extern int gage__Probe(gageContext *ctx, double xi, double yi, double zi,
                       double stackIdx);
extern int gage__ProbeSpace(gageContext *ctx, double xx, double yy, double zz, double ss,
                            int indexSpace, int clamp);

/* pvl.c */
extern gagePerVolume *gage__PerVolumeCopy(gagePerVolume *pvl, unsigned int fd);
extern double *gage__AnswerPointer(const gageContext *ctx, gagePerVolume *pvl, int item);

/* print.c */
// extern void gage__Print_off(FILE *, gageContext *ctx); (apparently unused)
extern void gage__Print_fslw(FILE *, gageContext *ctx);

/* filter.c */
extern int gage__LocationSet(gageContext *ctx, double x, double y, double z, double s);

/* stack.c */
extern int gage__StackBaseIv3Fill(gageContext *ctx);

/* sclprint.c */
extern void gage__SclIv3Print(FILE *, gageContext *ctx, gagePerVolume *pvl);

/* sclfilter.c */
extern void gage__SclFilter(gageContext *ctx, gagePerVolume *pvl);

/* sclanswer.c */
extern void gage__SclAnswer(gageContext *ctx, gagePerVolume *pvl);

/* vecprint.c */
extern void gage__VecIv3Print(FILE *, gageContext *ctx, gagePerVolume *pvl);

#ifdef __cplusplus
}
#endif
