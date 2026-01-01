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

#include "bane.h"
#include "privateBane.h"

static double
measrStockAnswer(baneMeasr *measr, double *san, double *parm) {

  AIR_UNUSED(parm);
  return san[measr->offset0];
}

baneMeasr * /* Biff: NULL */
baneMeasrNew(int type, double *parm) {
  static const char me[] = "baneMeasrNew";
  baneMeasr *measr;
  int item;

  AIR_UNUSED(parm);
  if (!(AIR_IN_OP(baneMeasrUnknown, type, baneMeasrLast))) {
    biffAddf(BANE, "%s: baneMeasr %d invalid", me, type);
    return NULL;
  }
  /* for now, parm is ignored */
  measr = (baneMeasr *)calloc(1, sizeof(baneMeasr));
  if (!measr) {
    biffAddf(BANE, "%s: couldn't allocate baneMeasr!", me);
    return NULL;
  }
  measr->type = type;
  measr->range = NULL;
  GAGE_QUERY_RESET(measr->query);
  switch (type) {
    /* --------------------------------------------------------------- */
  case baneMeasrValuePositive:
    item = gageSclValue;
    snprintf(measr->name, sizeof(measr->name), "%s, positive",
             airEnumStr(gageScl, item));
    GAGE_QUERY_ITEM_ON(measr->query, item);
    measr->range = baneRangeNew(baneRangePositive);
    measr->offset0 = gageKindAnswerOffset(gageKindScl, item);
    measr->answer = measrStockAnswer;
    break;
    /* --------------------------------------------------------------- */
  case baneMeasrValueZeroCentered:
    item = gageSclValue;
    snprintf(measr->name, sizeof(measr->name), "%s, zero-centered",
             airEnumStr(gageScl, item));
    GAGE_QUERY_ITEM_ON(measr->query, item);
    measr->range = baneRangeNew(baneRangeZeroCentered);
    measr->offset0 = gageKindAnswerOffset(gageKindScl, item);
    measr->answer = measrStockAnswer;
    break;
    /* --------------------------------------------------------------- */
  case baneMeasrValueAnywhere:
    item = gageSclValue;
    snprintf(measr->name, sizeof(measr->name), "%s, anywhere",
             airEnumStr(gageScl, item));
    GAGE_QUERY_ITEM_ON(measr->query, item);
    measr->range = baneRangeNew(baneRangeAnywhere);
    measr->offset0 = gageKindAnswerOffset(gageKindScl, item);
    measr->answer = measrStockAnswer;
    break;
    /* --------------------------------------------------------------- */
  case baneMeasrGradMag:
    item = gageSclGradMag;
    snprintf(measr->name, sizeof(measr->name), "%s", airEnumStr(gageScl, item));
    GAGE_QUERY_ITEM_ON(measr->query, item);
    measr->range = baneRangeNew(baneRangePositive);
    measr->offset0 = gageKindAnswerOffset(gageKindScl, item);
    measr->answer = measrStockAnswer;
    break;
    /* --------------------------------------------------------------- */
  case baneMeasrLaplacian:
    item = gageSclLaplacian;
    snprintf(measr->name, sizeof(measr->name), "%s", airEnumStr(gageScl, item));
    GAGE_QUERY_ITEM_ON(measr->query, item);
    measr->range = baneRangeNew(baneRangeZeroCentered);
    measr->offset0 = gageKindAnswerOffset(gageKindScl, item);
    measr->answer = measrStockAnswer;
    break;
    /* --------------------------------------------------------------- */
  case baneMeasr2ndDD:
    item = gageScl2ndDD;
    snprintf(measr->name, sizeof(measr->name), "%s", airEnumStr(gageScl, item));
    GAGE_QUERY_ITEM_ON(measr->query, item);
    measr->range = baneRangeNew(baneRangeZeroCentered);
    measr->offset0 = gageKindAnswerOffset(gageKindScl, item);
    measr->answer = measrStockAnswer;
    break;
    /* --------------------------------------------------------------- */
  case baneMeasrTotalCurv:
    item = gageSclTotalCurv;
    snprintf(measr->name, sizeof(measr->name), "%s", airEnumStr(gageScl, item));
    GAGE_QUERY_ITEM_ON(measr->query, item);
    measr->range = baneRangeNew(baneRangePositive);
    measr->offset0 = gageKindAnswerOffset(gageKindScl, item);
    measr->answer = measrStockAnswer;
    break;
    /* --------------------------------------------------------------- */
  case baneMeasrFlowlineCurv:
    item = gageSclFlowlineCurv;
    snprintf(measr->name, sizeof(measr->name), "%s", airEnumStr(gageScl, item));
    GAGE_QUERY_ITEM_ON(measr->query, item);
    measr->range = baneRangeNew(baneRangePositive);
    measr->offset0 = gageKindAnswerOffset(gageKindScl, item);
    measr->answer = measrStockAnswer;
    break;
    /* --------------------------------------------------------------- */
  default:
    biffAddf(BANE, "%s: Sorry, baneMeasr %d not implemented", me, type);
    baneMeasrNix(measr);
    return NULL;
  }
  return measr;
}

double /* Biff: nope */
baneMeasrAnswer(baneMeasr *measr, gageContext *gctx) {
  static const char me[] = "baneMeasrAnswer";
  double ret;

  if (measr && gctx && 1 == gctx->pvlNum) {
    ret = measr->answer(measr, gctx->pvl[0]->answer, measr->parm);
  } else {
    fprintf(stderr, "%s: something is terribly wrong\n", me);
    ret = AIR_NAN;
  }
  return ret;
}

baneMeasr * /* Biff: NULL */
baneMeasrCopy(baneMeasr *measr) {
  static const char me[] = "baneMeasrCopy";
  baneMeasr *ret = NULL;

  ret = baneMeasrNew(measr->type, measr->parm);
  if (!ret) {
    biffAddf(BANE, "%s: couldn't make new measr", me);
    return NULL;
  }
  return ret;
}

baneMeasr * /* Biff: nope */
baneMeasrNix(baneMeasr *measr) {

  if (measr) {
    baneRangeNix(measr->range);
    airFree(measr);
  }
  return NULL;
}
