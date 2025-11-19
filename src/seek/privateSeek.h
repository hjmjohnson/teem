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

enum {
  flagUnknown,
  flagData,
  flagType,
  flagSamples,
  flagLowerInside,
  flagNormalsFind,
  flagStrength,
  flagStrengthUse,
  flagItemValue,
  flagItemStrength,
  flagItemNormal,
  flagItemGradient,
  flagItemEigensystem,
  flagItemHess,
  flagIsovalue,
  flagEvalDiffThresh,

  flagNinEtAl,
  flagAnswerPointers,
  flagSxSySz,
  flagReverse,
  flagTxfNormal,
  flagSlabCacheAlloc,
  flagSclDerived,
  flagSpanSpaceHist,

  flagResult,
  flagLast
};

typedef struct {
  int evti[12]; /* edge vertex index */
  double (*scllup)(const void *, size_t);
  unsigned int esIdx, /* eigensystem index */
    zi;               /* slice index we're currently on */
  int modeSign;
  const void *scldata;
  airArray *xyzwArr, *normArr, *indxArr;
} baggage;

/* extract.c: This one is also needed in textract.c: */
extern void _seekIdxProbe(seekContext *sctx, baggage *bag, double xi, double yi,
                          double zi);

/* textract.c: Some routines that are also used in descend.c */
extern void _seekHess2T(double *T, const double *evals, const double *evecs,
                        const double evalDiffThresh, const char ridge);
extern void _seekHessder2Tder(double *Tder, const double *hessder, const double *evals,
                              const double *evecs, const double evalDiffThresh,
                              const char ridge);

extern int _seekShuffleProbeT(seekContext *sctx, baggage *bag);

extern int _seekTriangulateT(seekContext *sctx, baggage *bag, limnPolyData *lpld);

#ifdef __cplusplus
}
#endif
