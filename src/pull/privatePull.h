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

#define MY_PHN PULL_PHIST_NUM /* short-cut */

/*
** These heuristics/parameters have been adjusted, and don't seem to need
** any further tweaking in order to work for all the datasets used so far
*/

/* size/allocation increment for per-bin airArray */
#define MY_PULL_BIN_INCR 32u

/* size/allocation increment for pullTrace airArray in pullTraceMulti */
#define MY_PULL_TRACE_MULTI_INCR 1024u

/* this has to be big enough to do experiments where binning is turned off */
#define MY_PULL_NEIGH_MAXNUM 4096u

/* used by pullBinsPointMaybeAdd; don't add a point if its (normalized)
   distance to an existing point is less than this */
#define MY_PULL_BINNING_MAYBE_ADD_THRESH 0.1

/* only try adding a point if the normalized neighbor offset sum is
   greater than this (making this too small only wastes time, by descending
   and testing a point that can't help reduce energy */
#define MY_PULL_NEIGH_OFFSET_SUM_THRESH 0.2

/* how far to place new points from isolated points (as a fraction of
   radiusSpace), when not using an energy with an explicit well */
#define MY_PULL_NEWPNT_DIST 0.6

/* scaling factor between point->neighDistMean and distance cap; higher
   values allow for more adventurous explorations . . . */
#define MY_PULL_DIST_CAP_RSNORM 2.0

/* travel distance limit in terms of voxelSizeSpace and voxelSizeScale */
#define MY_PULL_DIST_CAP_VOXEL 1.5

/* where along s axis to probe energySpecS to see if its attractive or
   repulsive along scale */
#define MY_PULL_TARGET_DIM_S_PROBE 0.05

/* tentative new points aren't allowed to move further than this (in
   rs-normalized space) from the original old point */
#define MY_PULL_NEWPNT_STRAY_DIST 1.3

/* fraction of bboxMax[3]-bboxMin[3] to use as step along scale
   for discrete differencing needed to find the gradient (for
   moving along scale) and 2nd derivative (for learning gamma)
   of strength
*/
#define MY_PULL_STRENGTH_ENERGY_DELTA_SCALE 0.0002

/* maximum distance, in multiples of voxelSizeSpace, that a point is
   allowed to move between start and end of constraint satisfaction */
#define MY_PULL_CONSTRAINT_TRAVEL_MAX 2.0

/* max reasonable value of iterParm->constraintIterMax */
#define MY_PULL_CONSTRAINT_ITER_MAX 50u

/* the main _iterate() function does progress indication if the number of
   points is larger than this */
#define MY_PULL_PROGRESS_POINT_NUM_MIN 100u

/* limit on # times we allow random or halton (non-ppv) seeding to fail */
#define MY_PULL_RANDOM_SEED_TRY_MAX 1000000u

/* limit on stepEnergy */
#define MY_PULL_STEP_ENERGY_MAX FLT_MAX

/* resolution of histogram of (r,s) coords of interactions ("hinter") */
#define MY_PULL_HINTER_SIZE 601u

/* initPull.c */
extern void pull__InitParmInit(pullInitParm *initParm);
extern int pull__InitParmCheck(pullInitParm *iparm);

/* parmPull.c */
extern void pull__IterParmInit(pullIterParm *iterParm);
extern int pull__IterParmCheck(pullIterParm *iterParm);
extern void pull__SysParmInit(pullSysParm *sysParm);
extern int pull__SysParmCheck(pullSysParm *sysParm);
extern void pull__FlagInit(pullFlag *flag);

/* volumePull.c */
extern pullVolume *pull__VolumeCopy(const pullContext *pctx, const pullVolume *pvol);
extern int pull__VolumeSetup(pullContext *pctx);
extern int pull__InsideBBox(pullContext *pctx, double pos[4]);
extern unsigned int pull__VolumeIndex(const pullContext *pctx, const char *volName);

/* infoPull.c */
extern unsigned int const pull__InfoLen[PULL_INFO_MAX + 1];
extern void (*const pull__InfoCopy[10])(double *, const double *);
extern int pull__InfoSetup(pullContext *pctx);

/* contextPull.c */
extern int pull__ContextCheck(pullContext *pctx);

/* taskPull.c */
extern int pull__TaskSetup(pullContext *pctx);
extern void pull__TaskFinish(pullContext *pctx);

/* actionPull.c */
extern double pull__EnergyFromPoints(pullTask *task, pullBin *bin, pullPoint *point,
                                     /* output */
                                     double egradSum[4]);
extern double pull__PointEnergyTotal(pullTask *task, pullBin *bin, pullPoint *point,
                                     int ignoreImage, double force[4]);
extern int pull__PointProcessDescent(pullTask *task, pullBin *bin, pullPoint *point,
                                     int ignoreImage);
extern double pull__EnergyInterParticle(pullContext *pctx, pullPoint *me,
                                        const pullPoint *she, double spaceDist,
                                        double scaleDist, double egrad[4]);

/* constraints.c */
extern double pull__Sigma(const pullContext *pctx, const double pos[4]);
extern int pull__ConstraintSatisfy(pullTask *task, pullPoint *point, double travelMax,
                                   int *constrFailP);
extern void pull__ConstraintTangent(pullTask *task, pullPoint *point, double proj[9]);
extern int pull__ConstraintDim(const pullContext *pctx);

/* pointPull.c */
#if PULL_PHIST
extern void pull__PointHistInit(pullPoint *point);
extern void pull__PointHistAdd(pullPoint *point, int cond, double val);
#else
#  define pull__PointHistInit(p)      /* no-op */
#  define pull__PointHistAdd(p, c, v) /* no-op */
#endif
extern double pull__StepInterAverage(const pullContext *pctx);
extern double pull__StepConstrAverage(const pullContext *pctx);
extern double pull__EnergyTotal(const pullContext *pctx);
extern void pull__PointStepEnergyScale(pullContext *pctx, double scale);
extern int pull__PointSetup(pullContext *pctx);
extern void pull__PointFinish(pullContext *pctx);

/* popcntl.c */
extern int pull__PointProcessNeighLearn(pullTask *task, pullBin *bin, pullPoint *point);
extern int pull__PointProcessAdding(pullTask *task, pullBin *bin, pullPoint *point);
extern int pull__PointProcessNixing(pullTask *task, pullBin *bin, pullPoint *point);
extern int pull__IterFinishNeighLearn(pullContext *pctx);
extern int pull__IterFinishAdding(pullContext *pctx);
extern int pull__IterFinishNixing(pullContext *pctx);
extern void pull__NixTheNixed(pullContext *pctx);

/* binningPull.c */
extern int pull__BinSetup(pullContext *pctx);
extern int pull__IterFinishDescent(pullContext *pctx);
extern void pull__BinFinish(pullContext *pctx);

/* corePull.c */
extern int pull__Verbose;
extern int pull__Iterate(pullContext *pctx, int mode);

#ifdef __cplusplus
}
#endif
