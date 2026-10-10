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

#ifdef _WIN32
#  include <io.h>
#  include <fcntl.h>
#endif

/* ---- BEGIN non-NrrdIO */
/* NOTE: this #ifdef..#endif block copied to other src/<lib>/private<Lib>.h files */
#ifdef _WIN32
#  ifndef STDIN_FILENO
#    define STDIN_FILENO 0
#  endif
#  ifndef STDOUT_FILENO
#    define STDOUT_FILENO 1
#  endif
#  ifndef STDERR_FILENO
#    define STDERR_FILENO 2
#  endif
#  define isatty _isatty
#else
#  include <unistd.h> /* for isatty() and STDIN_FILENO */
#endif
/* ---- END non-NrrdIO */

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
/* ---- BEGIN non-NrrdIO */
#define SN_PRINTF(DST, DST_SIZE, FMT, ...)                                              \
  do {                                                                                  \
    snprintf((DST), (DST_SIZE), (FMT), __VA_ARGS__);                                    \
    SN_INCR(DST, DST_SIZE);                                                             \
  } while (0)
/* ---- END non-NrrdIO */
#define ASP1_S (AIR_STRLEN_SMALL + 1)
#define ASP1_M (AIR_STRLEN_MED + 1)
#define ASP1_L (AIR_STRLEN_LARGE + 1)
#define ASP1_H (AIR_STRLEN_HUGE + 1)

typedef unsigned int uint; /* uint is just more concise */

#define MY_NRRD_TEXT_INCR      1024u
#define MY_NRRD_LLONG_MAX_HELP AIR_LLONG(2305843009213693951)
#define MY_NRRD_LLONG_MIN_HELP AIR_LLONG(-2305843009213693952)

#define MY_NRRD_WHITESPACE_NOTAB " \n\r\v\f" /* K+R pg. 157 */

/* ---- BEGIN non-NrrdIO */

#if NRRD_RESAMPLE_FLOAT
#  define nrrdResamplenrrd__Type nrrdTypeFloat
#  define EVALN                  evalN_f /* NrrdKernel method */
#  define RSF_to_S               airFtoS
#else
#  define nrrdResample_nrrdType nrrdTypeDouble
#  define EVALN                 evalN_d /* NrrdKernel method */
#  define RSF_to_S              airDtoS
#endif

/* to access whatever nrrd there may be in in a NrrdIter */
#define MY_NRRD_ITER_NRRD(iter) ((iter)->nrrd ? (iter)->nrrd : (iter)->ownNrrd)

/* ---- END non-NrrdIO */

typedef union {
  char **CP;
  int *I;
  unsigned int *UI;
  size_t *ST;
  double *D;
  const void *P;
  double (*V)[NRRD_SPACE_DIM_MAX];
} nrrd__AxisInfoSetPtrs;

typedef union {
  char **CP;
  int *I;
  unsigned int *UI;
  size_t *ST;
  double *D;
  void *P;
  double (*V)[NRRD_SPACE_DIM_MAX];
} nrrd__AxisInfoGetPtrs;

/* defaultsNrrd.c */
extern airLLong nrrd__LLongMaxHelp(airLLong val);
extern airLLong nrrd__LLongMinHelp(airLLong val);
extern airULLong nrrd__ULLongMaxHelp(airULLong val);

/* keyvalue.c */
extern void nrrd__WriteEscaped(FILE *file, char *dst, size_t dstSize, const char *str,
                               const char *toescape, const char *tospace);
extern int nrrd__KeyValueWrite(FILE *file, char **stringP, const char *prefix,
                               const char *key, const char *value);

/* formatXXX.c */
extern const char *const nrrd__FormatURLLine0;
extern const char *const nrrd__FormatURLLine1;
extern const NrrdFormat nrrd__FormatNRRD;
extern const NrrdFormat nrrd__FormatPNM;
extern const NrrdFormat nrrd__FormatPNG;
extern const NrrdFormat nrrd__FormatVTK;
extern const NrrdFormat nrrd__FormatText;
extern const NrrdFormat nrrd__FormatEPS;
extern int nrrd__HeaderCheck(Nrrd *nrrd, NrrdIoState *nio, int checkSeen);
extern int nrrd__FormatNRRD_whichVersion(const Nrrd *nrrd, NrrdIoState *nio);

/* encodingXXX.c */
extern const NrrdEncoding nrrd__EncodingRaw;
extern const NrrdEncoding nrrd__EncodingAscii;
extern const NrrdEncoding nrrd__EncodingHex;
extern const NrrdEncoding nrrd__EncodingGzip;
extern const NrrdEncoding nrrd__EncodingBzip2;
extern const NrrdEncoding nrrd__EncodingZRL;

/* arrays.c */
extern const int nrrd__FieldValidInImage[NRRD_FIELD_MAX + 1];
extern const int nrrd__FieldValidInText[NRRD_FIELD_MAX + 1];
extern const int nrrd__FieldOnePerAxis[NRRD_FIELD_MAX + 1];
extern const int nrrd__FieldRequired[NRRD_FIELD_MAX + 1];

/* simple.c */
extern char *nrrd__ContentGet(const Nrrd *nin);
extern int nrrd__ContentSet_va(Nrrd *nout, const char *func, char *content,
                               const char *format, ...);
extern int (*const nrrd__FieldCheck[NRRD_FIELD_MAX + 1])(const Nrrd *nrrd, int useBiff);
extern void nrrd__SplitSizes(size_t *pieceSize, size_t *pieceNum, Nrrd *nrrd,
                             unsigned int listDim);

/* axis.c */
extern int nrrd__KindAltered(int kindIn, int resampling);
extern void nrrd__AxisInfoCopy(NrrdAxisInfo *dest, const NrrdAxisInfo *src, int bitflag);
extern void nrrd__AxisInfoInit(NrrdAxisInfo *axis);
extern void nrrd__AxisInfoNewInit(NrrdAxisInfo *axis);
extern int nrrd__Center(int center);
extern int nrrd__Center2(int center, int def);
/* ---- BEGIN non-NrrdIO */
extern int nrrd__Dblcmp(double aa, double bb);

/* convertNrrd.c */
extern void (*const nrrd__Conv[][NRRD_TYPE_MAX + 1])(void *, const void *, size_t);
extern void (*const nrrd__ClampConv[][NRRD_TYPE_MAX + 1])(void *, const void *, size_t);
extern void (*const nrrd__CastClampRound[][NRRD_TYPE_MAX + 1])(void *, const void *,
                                                               size_t, int doClamp,
                                                               int roundd);
/* ---- END non-NrrdIO */

/* read.c */
extern const char *const nrrd__ReadHitEOF;
extern const char *const nrrd__FieldSep;
extern const char *const nrrd__NoSpaceVector;
extern int nrrd__CharIsFieldSep(char cc);
extern int nrrd__ByteSkipSkip(FILE *dataFile, Nrrd *nrrd, NrrdIoState *nio,
                              long int byteSkip);
extern int nrrd__Calloc(Nrrd *nrrd, NrrdIoState *nio);
extern void nrrd__SplitName(char **dirP, char **baseP, const char *name);

/* write.c */
extern int nrrd__FieldInteresting(const Nrrd *nrrd, NrrdIoState *nio, int field);
extern void nrrd__SprintFieldInfo(char **strP, const char *prefix, const Nrrd *nrrd,
                                  NrrdIoState *nio, int field, int dropAxis0);
extern void nrrd__FprintFieldInfo(FILE *file, const char *prefix, const Nrrd *nrrd,
                                  NrrdIoState *nio, int field, int dropAxis0);

/* parseNrrd.c */
extern int nrrd__ReadNrrdParseField(NrrdIoState *nio, int useBiff);

/* methodsNrrd.c */
extern int nrrd__Copy(Nrrd *nout, const Nrrd *nin, int bitflag);
extern int nrrd__SizeCheck(const size_t *size, unsigned int dim, int useBiff);
extern int nrrd__MaybeAllocMaybeZero_nva(Nrrd *nrrd, int type, unsigned int dim,
                                         const size_t *size, int zeroWhenNoAlloc);

#ifdef TEEM_ZLIB
#  ifdef TEEM_VTK_MANGLE
#    include "vtk_zlib_mangle.h"
#  endif
#  include <zlib.h> /* NrrdIO-hack-004 */

/* gzio.c */
extern gzFile nrrd__GzOpen(FILE *fd, const char *mode);
extern int nrrd__GzClose(gzFile file);
extern int nrrd__GzRead(gzFile file, void *buf, unsigned int len, unsigned int *read);
extern int nrrd__GzWrite(gzFile file, const void *buf, unsigned int len,
                         unsigned int *written);
#else
extern int nrrd__GzDummySymbol(void);
#endif

/* ---- BEGIN non-NrrdIO */
/* apply1D.c */
extern double nrrd__ApplyDomainMin(const Nrrd *nmap, int ramps, unsigned int mapAxis);
extern double nrrd__ApplyDomainMax(const Nrrd *nmap, int ramps, unsigned int mapAxis);

/* ---- END non-NrrdIO */

#ifdef __cplusplus
}
#endif
