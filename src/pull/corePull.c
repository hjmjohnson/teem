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

#include "pull.h"
#include "privatePull.h"

int pull__Verbose = 0;

#define DECREASE(ell, enn)                                                              \
  (2 * ((ell) - (enn))                                                                  \
   / ((AIR_ABS(ell) + AIR_ABS(enn)) ? (AIR_ABS(ell) + AIR_ABS(enn)) : 1))
/*
** this is the core of the worker threads: as long as there are bins
** left to process, get the next one, and process it
*/
static int /* Biff: 1 */
process(pullTask *task) {
  static const char me[] = "process";
  unsigned int binIdx;

  while (task->pctx->binNextIdx < task->pctx->binNum) {
    /* get the index of the next bin to process */
    if (task->pctx->threadNum > 1) {
      airThreadMutexLock(task->pctx->binMutex);
    }
    /* note that we entirely skip bins with no points */
    do {
      binIdx = task->pctx->binNextIdx;
      if (task->pctx->binNextIdx < task->pctx->binNum) {
        task->pctx->binNextIdx++;
      }
    } while (binIdx < task->pctx->binNum && 0 == task->pctx->bin[binIdx].pointNum);
    if (task->pctx->threadNum > 1) {
      airThreadMutexUnlock(task->pctx->binMutex);
    }
    if (binIdx == task->pctx->binNum) {
      /* no more bins to process! */
      break;
    }
    if (task->pctx->verbose > 1) {
      fprintf(stderr, "%s(%u): calling pullBinProcess(%u)\n", me, task->threadIdx,
              binIdx);
    }
    if (pullBinProcess(task, binIdx)) {
      biffAddf(PULL, "%s(%u): had trouble on bin %u", me, task->threadIdx, binIdx);
      return 1;
    }
  }
  return 0;
}

/* the main loop for each worker thread */
static void * /* Biff: NULL */
worker(void *_task) {
  static const char me[] = "worker";
  void *ret;
  pullTask *task;

  ret = _task;
  task = (pullTask *)_task;

  while (1) {
    if (task->pctx->verbose > 1) {
      fprintf(stderr, "%s(%u): waiting on barrier A\n", me, task->threadIdx);
    }
    /* pushFinish sets finished prior to the barriers */
    airThreadBarrierWait(task->pctx->iterBarrierA);
    if (task->pctx->finished) {
      if (task->pctx->verbose > 1) {
        fprintf(stderr, "%s(%u): done!\n", me, task->threadIdx);
      }
      break;
    }
    /* else there's work to do . . . */
    if (task->pctx->verbose > 1) {
      fprintf(stderr, "%s(%u): starting to process\n", me, task->threadIdx);
    }
    if (process(task)) {
      /* HEY clearly not threadsafe to have errors . . . */
      biffAddf(PULL, "%s: thread %u trouble", me, task->threadIdx);
      task->pctx->finished = AIR_TRUE;
      /* return NULL; (for biff auto-scan) */
      ret = NULL;
    }
    if (task->pctx->verbose > 1) {
      fprintf(stderr, "%s(%u): waiting on barrier B\n", me, task->threadIdx);
    }
    airThreadBarrierWait(task->pctx->iterBarrierB);
  }

  return ret;
}

int /* Biff: 1 */
pullStart(pullContext *pctx) {
  static const char me[] = "pullStart";
  unsigned int tidx;

  if (pctx->verbose) {
    fprintf(stderr, "%s: hello %p\n", me, AIR_VOIDP(pctx));
  }
  pctx->iter = 0; /* have to initialize this here because of seedOnly hack */

  /* the ordering of steps below is important! e.g. gage context has
     to be set up (pull__VolumeSetup) by before its copied (pull__TaskSetup) */
  if (pull__ContextCheck(pctx) || pull__VolumeSetup(pctx) || pull__InfoSetup(pctx)
      || pull__TaskSetup(pctx) || pull__BinSetup(pctx)) {
    biffAddf(PULL, "%s: trouble starting to set up context", me);
    return 1;
  }
  if (!(pctx->flag.startSkipsPoints)) {
    if (pull__PointSetup(pctx)) {
      biffAddf(PULL, "%s: trouble setting up points", me);
      return 1;
    }
  }

  if (pctx->threadNum > 1) {
    pctx->binMutex = airThreadMutexNew();
    pctx->iterBarrierA = airThreadBarrierNew(pctx->threadNum);
    pctx->iterBarrierB = airThreadBarrierNew(pctx->threadNum);
    /* start threads 1 and up running; they'll all hit iterBarrierA  */
    for (tidx = 1; tidx < pctx->threadNum; tidx++) {
      if (pctx->verbose > 1) {
        fprintf(stderr, "%s: spawning thread %u\n", me, tidx);
      }
      airThreadStart(pctx->task[tidx]->thread, worker, (void *)(pctx->task[tidx]));
    }
  } else {
    pctx->binMutex = NULL;
    pctx->iterBarrierA = NULL;
    pctx->iterBarrierB = NULL;
  }
  if (pctx->verbose) {
    fprintf(stderr, "%s: setup for %u threads done\n", me, pctx->threadNum);
  }

  pctx->timeIteration = 0;
  pctx->timeRun = 0;

  return 0;
}

/*
** this is called *after* pullOutputGet
**
** should nix everything created by the many pull__*Setup() functions
*/
int /* Biff: 1 */
pullFinish(pullContext *pctx) {
  static const char me[] = "pullFinish";
  unsigned int tidx;

  if (!pctx) {
    biffAddf(PULL, "%s: got NULL pointer", me);
    return 1;
  }

  pctx->finished = AIR_TRUE;
  if (pctx->threadNum > 1) {
    if (pctx->verbose > 1) {
      fprintf(stderr, "%s: finishing workers\n", me);
    }
    airThreadBarrierWait(pctx->iterBarrierA);
    /* worker threads now pass barrierA and see that finished is AIR_TRUE,
       and then bail, so now we collect them */
    for (tidx = pctx->threadNum; tidx > 0; tidx--) {
      if (tidx - 1) {
        airThreadJoin(pctx->task[tidx - 1]->thread, &(pctx->task[tidx - 1]->returnPtr));
      }
    }
    pctx->binMutex = airThreadMutexNix(pctx->binMutex);
    pctx->iterBarrierA = airThreadBarrierNix(pctx->iterBarrierA);
    pctx->iterBarrierB = airThreadBarrierNix(pctx->iterBarrierB);
  }

  /* no need for pull__VolumeFinish(pctx), at least not now */
  /* no need for pull__InfoFinish(pctx), at least not now */
  pull__TaskFinish(pctx);
  pull__BinFinish(pctx);
  pull__PointFinish(pctx); /* yes, nixed bins deleted pnts inside, but
                             other buffers still have to be freed */

  return 0;
}
/*
** pull__Iterate
**
** (documentation)
**
** NB: this implements the body of thread 0, the master thread
*/
int /* Biff: (private) 1 */
pull__Iterate(pullContext *pctx, int mode) {
  static const char me[] = "pull__Iterate";
  double time0;
  int myError, E;
  unsigned int ti;

  if (!pctx) {
    biffAddf(PULL, "%s: got NULL pointer", me);
    return 1;
  }
  if (airEnumValCheck(pullProcessMode, mode)) {
    biffAddf(PULL, "%s: process mode %d unrecognized", me, mode);
    return 1;
  }
  if (!pctx->task) {
    biffAddf(PULL, "%s: NULL task array, didn't call pullStart()?", me);
    return 1;
  }

  /* if this is descent, pull down eip a bit */
  if (pullProcessModeDescent == mode) {
    pctx->sysParm.energyIncreasePermit *= pctx->eipScale;
  }

#if PULL_HINTER
  /* zero-out/alloc hinter if need be */
  if (pullProcessModeDescent == mode && pctx->nhinter) {
    if (nrrdMaybeAlloc_va(pctx->nhinter, nrrdTypeFloat, 2,
                          AIR_SIZE_T(MY_PULL_HINTER_SIZE),
                          AIR_SIZE_T(MY_PULL_HINTER_SIZE))) {
      biffMovef(PULL, NRRD, "%s: setting up nhinter", me);
      return 1;
    }
  }
#endif

  /* tell all tasks what mode they're in */
  for (ti = 0; ti < pctx->threadNum; ti++) {
    pctx->task[ti]->processMode = mode;
  }
  if (pctx->verbose) {
    fprintf(stderr, "%s(%s): iter %u goes w/ eip %g, %u pnts, enr %g%s\n", me,
            airEnumStr(pullProcessMode, mode), pctx->iter,
            pctx->sysParm.energyIncreasePermit, pullPointNumber(pctx),
            pull__EnergyTotal(pctx), (pctx->flag.permuteOnRebin ? " (por)" : ""));
  }

  time0 = airTime();
  pctx->pointNum = pullPointNumber(pctx);

  /* the _pullWorker checks finished after iterBarrierA */
  pctx->finished = AIR_FALSE;

  /* initialize index of next bin to be doled out to threads */
  pctx->binNextIdx = 0;

  if (pctx->threadNum > 1) {
    airThreadBarrierWait(pctx->iterBarrierA);
  }
  myError = AIR_FALSE;
  if (process(pctx->task[0])) {
    biffAddf(PULL, "%s: master thread trouble w/ iter %u", me, pctx->iter);
    pctx->finished = AIR_TRUE;
    myError = AIR_TRUE;
  }
  if (pctx->threadNum > 1) {
    airThreadBarrierWait(pctx->iterBarrierB);
  }
  if (pctx->finished) {
    if (!myError) {
      /* we didn't set finished- one of the workers must have */
      biffAddf(PULL, "%s: worker error on iter %u", me, pctx->iter);
    }
    return 1;
  }
  if (pctx->verbose) {
    if (pctx->pointNum > MY_PULL_PROGRESS_POINT_NUM_MIN) {
      fprintf(stderr, ".\n"); /* finishing line of progress indicators */
    }
  }

  /* depending on mode, run one of the iteration finishers */
  E = 0;
  switch (mode) {
  case pullProcessModeDescent:
    E = pull__IterFinishDescent(pctx); /* includes rebinning */
    break;
  case pullProcessModeNeighLearn:
    E = pull__IterFinishNeighLearn(pctx);
    break;
  case pullProcessModeAdding:
    E = pull__IterFinishAdding(pctx);
    break;
  case pullProcessModeNixing:
    E = pull__IterFinishNixing(pctx);
    break;
  default:
    biffAddf(PULL, "%s: process mode %d unrecognized", me, mode);
    return 1;
    break;
  }
  if (E) {
    pctx->finished = AIR_TRUE;
    biffAddf(PULL, "%s: trouble finishing iter %u", me, pctx->iter);
    return 1;
  }

  pctx->timeIteration = airTime() - time0;

#if PULL_HINTER
  if (pullProcessModeDescent == mode && pctx->nhinter) {
    char fname[AIR_STRLEN_SMALL + 1];
    snprintf(fname, sizeof(fname), "hinter-%05u.nrrd", pctx->iter);
    if (nrrdSave(fname, pctx->nhinter, NULL)) {
      biffMovef(PULL, NRRD, "%s: saving hinter to %s", me, fname);
      return 1;
    }
  }
#endif

  return 0;
}

int /* Biff: 1 */
pullRun(pullContext *pctx) {
  static const char me[] = "pullRun";
  Nrrd *npos;
  double time0, time1, enrLast, enrNew = AIR_NAN, enrDecrease = AIR_NAN,
                                enrDecreaseAvg = AIR_NAN;
  int converged;
  unsigned firstIter;

  if (pctx->verbose) {
    fprintf(stderr, "%s: hello\n", me);
  }
  time0 = airTime();
  firstIter = pctx->iter;
  if (pctx->verbose) {
    fprintf(stderr, "%s: doing priming iteration (iter now %u)\n", me, pctx->iter);
  }
  if (pull__Iterate(pctx, pullProcessModeDescent)) {
    biffAddf(PULL, "%s: trouble on priming iter %u", me, pctx->iter);
    return 1;
  }
  pctx->iter += 1;
  enrLast = enrNew = pull__EnergyTotal(pctx);
  if (pctx->verbose) {
    fprintf(stderr, "%s: starting system energy = %g\n", me, enrLast);
  }
  enrDecrease = enrDecreaseAvg = 0;
  converged = AIR_FALSE;
  while ((pctx->iterParm.min && pctx->iter <= pctx->iterParm.min)
         || ((!pctx->iterParm.max || pctx->iter < pctx->iterParm.max) && !converged)) {
    /* this per iteration init had been missing for a very long time */
    pctx->addNum = pctx->nixNum = 0;
    if (pctx->iterParm.snap && !(pctx->iter % pctx->iterParm.snap)) {
      npos = nrrdNew();
      if (pullOutputGet(npos, NULL, NULL, NULL, 0.0, pctx)) {
        biffAddf(PULL, "%s: couldn't get snapshot for iter %u", me, pctx->iter);
        return 1;
      }
      char poutS[AIR_STRLEN_MED + 1];
      snprintf(poutS, sizeof(poutS), "snap.%6u.pos.nrrd", pctx->iter);
      if (nrrdSave(poutS, npos, NULL)) {
        biffMovef(PULL, NRRD, "%s: couldn't save snapshot for iter %u", me, pctx->iter);
        return 1;
      }
      npos = nrrdNuke(npos);
    }

    if (pull__Iterate(pctx, pullProcessModeDescent)) {
      biffAddf(PULL, "%s: trouble on iter %u", me, pctx->iter);
      return 1;
    }
    enrNew = pull__EnergyTotal(pctx);
    enrDecrease = DECREASE(enrLast, enrNew);
    if (firstIter + 1 == pctx->iter) {
      /* we need some way of artificially boosting enrDecreaseAvg when
         we're just starting, so that we thwart the convergence test,
         which we do because we don't have the history of iterations
         that enrDecreaseAvg is supposed to describe.  Using some scaling
         of enrDecrease is one possible hack. */
      enrDecreaseAvg = 3 * enrDecrease;
    } else {
      enrDecreaseAvg = (2 * enrDecreaseAvg + enrDecrease) / 3;
    }
    if (pctx->verbose) {
      fprintf(stderr,
              "%s: ___ done iter %u: "
              "e=%g,%g, de=%g,%g (%g), s=%g,%g\n",
              me, pctx->iter, enrLast, enrNew, enrDecrease, enrDecreaseAvg,
              pctx->sysParm.energyDecreaseMin, pull__StepInterAverage(pctx),
              pull__StepConstrAverage(pctx));
    }
    if (pctx->iterParm.popCntlPeriod) {
      if ((pctx->iterParm.popCntlPeriod - 1)
            == (pctx->iter % pctx->iterParm.popCntlPeriod)
          && enrDecreaseAvg < pctx->sysParm.energyDecreasePopCntlMin
          && (pctx->sysParm.alpha != 0 || !pctx->flag.noPopCntlWithZeroAlpha)) {
        if (pctx->verbose) {
          fprintf(stderr,
                  "%s: ***** enr decrease %g < %g: "
                  "trying pop cntl ***** \n",
                  me, enrDecreaseAvg, pctx->sysParm.energyDecreasePopCntlMin);
        }
        if (pull__Iterate(pctx, pullProcessModeNeighLearn)
            || pull__Iterate(pctx, pullProcessModeAdding)
            || pull__Iterate(pctx, pullProcessModeNixing)) {
          biffAddf(PULL, "%s: trouble with %s for pop cntl on iter %u", me,
                   airEnumStr(pullProcessMode, pctx->task[0]->processMode), pctx->iter);
          return 1;
        }
      } else {
        if (pctx->verbose > 2) {
          fprintf(stderr, "%s: ***** no pop cntl:\n", me);
          fprintf(stderr, "    iter=%u %% period=%u = %u != %u\n", pctx->iter,
                  pctx->iterParm.popCntlPeriod,
                  pctx->iter % pctx->iterParm.popCntlPeriod,
                  pctx->iterParm.popCntlPeriod - 1);
          fprintf(stderr, "    en dec avg = %g >= %g\n", enrDecreaseAvg,
                  pctx->sysParm.energyDecreasePopCntlMin);
          fprintf(stderr, "    npcwza %s && alpha = %g\n",
                  pctx->flag.noPopCntlWithZeroAlpha ? "true" : "false",
                  pctx->sysParm.alpha);
        }
      }
    }
    pctx->iter += 1;
    enrLast = enrNew;
    converged = ((pctx->flag.convergenceIgnoresPopCntl
                  || (!pctx->iterParm.popCntlPeriod || (!pctx->addNum && !pctx->nixNum)))
                 && AIR_IN_CL(0, enrDecreaseAvg, pctx->sysParm.energyDecreaseMin));
    if (pctx->verbose) {
      fprintf(stderr,
              "%s: converged %d = (%d || (%d || (%d && %d))) "
              "&& (0 <= %g <= %g)=%d\n",
              me, converged, pctx->flag.convergenceIgnoresPopCntl,
              !pctx->iterParm.popCntlPeriod, !pctx->addNum, !pctx->nixNum,
              enrDecreaseAvg, pctx->sysParm.energyDecreaseMin,
              AIR_IN_OP(0, enrDecreaseAvg, pctx->sysParm.energyDecreaseMin));
    }
    if (converged && pctx->verbose) {
      printf("%s: enrDecreaseAvg %g < %g: converged!!\n", me, enrDecreaseAvg,
             pctx->sysParm.energyDecreaseMin);
    }
    pull__PointStepEnergyScale(pctx, pctx->sysParm.opporStepScale);
    /* call the callback */
    if (!(pctx->iter % pctx->iterParm.callback) && pctx->iter_cb) {
      pctx->iter_cb(pctx->data_cb);
    }
  }
  if (pctx->verbose) {
    printf("%s: done ((%d|%d)&%d) @iter %u: enr %g, enrDec = %g,%g "
           "%u stuck\n",
           me, !pctx->iterParm.max, pctx->iter < pctx->iterParm.max, !converged,
           pctx->iter, enrNew, enrDecrease, enrDecreaseAvg, pctx->stuckNum);
  }
  time1 = airTime();

  pctx->timeRun += time1 - time0;
  pctx->energy = enrNew;

  /* we do one final neighbor-learn iteration, to set the fields
     (like stability) that are only learned then */
  if (pull__Iterate(pctx, pullProcessModeNeighLearn)) {
    biffAddf(PULL, "%s: trouble after-final iter", me);
    return 1;
  }

  if (0) {
    /* probe inter-particle energy function */
    unsigned int szimg = 300, ri, si;
    Nrrd *nout;
    pullPoint *pa, *pb;
    double rdir[3], len, r, s, *out, enr, egrad[4];
    airRandMTState *rng;
    rng = pctx->task[0]->rng;
    nout = nrrdNew();
    nrrdMaybeAlloc_va(nout, nrrdTypeDouble, 3, AIR_SIZE_T(3), AIR_SIZE_T(szimg),
                      AIR_SIZE_T(szimg));
    out = AIR_CAST(double *, nout->data);
    pa = pullPointNew(pctx);
    pb = pullPointNew(pctx);
    airNormalRand_r(pa->pos + 0, pa->pos + 1, rng);
    airNormalRand_r(pa->pos + 2, pa->pos + 3, rng);
    airNormalRand_r(rdir + 0, rdir + 1, rng);
    airNormalRand_r(rdir + 2, NULL, rng);
    ELL_3V_NORM(rdir, rdir, len);
    for (si = 0; si < szimg; si++) {
      s = AIR_AFFINE(0, si, szimg - 1, -1.5 * pctx->sysParm.radiusScale,
                     1.5 * pctx->sysParm.radiusScale);
      for (ri = 0; ri < szimg; ri++) {
        r = AIR_AFFINE(0, ri, szimg - 1, -1.5 * pctx->sysParm.radiusSpace,
                       1.5 * pctx->sysParm.radiusSpace);
        ELL_3V_SCALE_ADD2(pb->pos, 1.0, pa->pos, r, rdir);
        pb->pos[3] = pa->pos[3] + s;
        /* now points are in desired test positions */
        enr = pull__EnergyInterParticle(pctx, pa, pb, AIR_ABS(r), AIR_ABS(s), egrad);
        ELL_3V_SET(out + 3 * (ri + szimg * si), enr, ELL_3V_DOT(egrad, rdir), egrad[3]);
      }
    }
    nrrdSave("eprobe.nrrd", nout, NULL);
    pullPointNix(pa);
    pullPointNix(pb);
    nrrdNuke(nout);
  }

  return 0;
}
