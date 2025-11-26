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

#include "ten.h"
#include "privateTen.h"

/*
** learned: don't take sqrt(FLT_EPSILON) and expect it to still be
** negligible
*/

/*
******** !!!! NOTE NOTE NOTE NOTE NOTE !!!!
********
******** THIS CODE IS NOT REALLY MEANT TO BE EDITED BY HUMANS
******** (only GLK :)
********
******** It is the worst possible example of the dangers of cut-and-paste
********
******** !!!! NOTE NOTE NOTE NOTE NOTE !!!!
*/
static float
anisoEval_Conf_f(const float eval[3]) {
  AIR_UNUSED(eval);
  return 1.0;
}
static double
anisoEval_Conf_d(const double eval[3]) {
  AIR_UNUSED(eval);
  return 1.0;
  return 1.0;
}
static float
anisoTen_Conf_f(const float ten[7]) {
  return ten[0];
}
static double
anisoTen_Conf_d(const double ten[7]) {
  return ten[0];
}

static float
anisoEval_Cl1_f(const float eval[3]) {
  float sum = eval[0] + eval[1] + eval[2];
  sum = AIR_MAX(0, sum);
  return sum ? (eval[0] - eval[1]) / sum : 0.0f;
}
static double
anisoEval_Cl1_d(const double eval[3]) {
  double sum = eval[0] + eval[1] + eval[2];
  sum = AIR_MAX(0, sum);
  return sum ? (eval[0] - eval[1]) / sum : 0.0;
}
static float
anisoTen_Cl1_f(const float ten[7]) {
  float eval[3];
  tenEigensolve_f(eval, NULL, ten);
  return anisoEval_Cl1_f(eval);
}
static double
anisoTen_Cl1_d(const double ten[7]) {
  double eval[3];
  tenEigensolve_d(eval, NULL, ten);
  return anisoEval_Cl1_d(eval);
}

static float
anisoEval_Cp1_f(const float eval[3]) {
  float sum = eval[0] + eval[1] + eval[2];
  sum = AIR_MAX(0, sum);
  return sum ? 2 * (eval[1] - eval[2]) / sum : 0.0f;
}
static double
anisoEval_Cp1_d(const double eval[3]) {
  double sum = eval[0] + eval[1] + eval[2];
  sum = AIR_MAX(0, sum);
  return sum ? 2 * (eval[1] - eval[2]) / sum : 0.0;
}
static float
anisoTen_Cp1_f(const float ten[7]) {
  float eval[3];
  tenEigensolve_f(eval, NULL, ten);
  return anisoEval_Cp1_f(eval);
}
static double
anisoTen_Cp1_d(const double ten[7]) {
  double eval[3];
  tenEigensolve_d(eval, NULL, ten);
  return anisoEval_Cp1_d(eval);
}

static float
anisoEval_Ca1_f(const float eval[3]) {
  float sum = eval[0] + eval[1] + eval[2];
  sum = AIR_MAX(0, sum);
  return sum ? (eval[0] + eval[1] - 2 * eval[2]) / sum : 0.0f;
}
static double
anisoEval_Ca1_d(const double eval[3]) {
  double sum = eval[0] + eval[1] + eval[2];
  sum = AIR_MAX(0, sum);
  return sum ? (eval[0] + eval[1] - 2 * eval[2]) / sum : 0.0;
}
static float
anisoTen_Ca1_f(const float ten[7]) {
  float eval[3];
  tenEigensolve_f(eval, NULL, ten);
  return anisoEval_Ca1_f(eval);
}
static double
anisoTen_Ca1_d(const double ten[7]) {
  double eval[3];
  tenEigensolve_d(eval, NULL, ten);
  return anisoEval_Ca1_d(eval);
}

static float
anisoEval_Clpmin1_f(const float eval[3]) {
  float cl, cp, sum = eval[0] + eval[1] + eval[2];
  sum = AIR_MAX(0, sum);
  cl = sum ? (eval[0] - eval[1]) / sum : 0.0f;
  cp = sum ? 2 * (eval[1] - eval[2]) / sum : 0.0f;
  return AIR_MIN(cl, cp);
}
static double
anisoEval_Clpmin1_d(const double eval[3]) {
  double cl, cp, sum = eval[0] + eval[1] + eval[2];
  sum = AIR_MAX(0, sum);
  cl = sum ? (eval[0] - eval[1]) / sum : 0.0;
  cp = sum ? 2 * (eval[1] - eval[2]) / sum : 0.0;
  return AIR_MIN(cl, cp);
}
static float
anisoTen_Clpmin1_f(const float ten[7]) {
  float eval[3];
  tenEigensolve_f(eval, NULL, ten);
  return anisoEval_Clpmin1_f(eval);
}
static double
anisoTen_Clpmin1_d(const double ten[7]) {
  double eval[3];
  tenEigensolve_d(eval, NULL, ten);
  return anisoEval_Clpmin1_d(eval);
}

static float
anisoEval_Cs1_f(const float eval[3]) {
  float sum = eval[0] + eval[1] + eval[2];
  sum = AIR_MAX(0, sum);
  return sum ? 3 * eval[2] / sum : 0.0f;
}
static double
anisoEval_Cs1_d(const double eval[3]) {
  double sum = eval[0] + eval[1] + eval[2];
  sum = AIR_MAX(0, sum);
  return sum ? 3 * eval[2] / sum : 0.0;
}
static float
anisoTen_Cs1_f(const float ten[7]) {
  float eval[3];
  tenEigensolve_f(eval, NULL, ten);
  return anisoEval_Cs1_f(eval);
}
static double
anisoTen_Cs1_d(const double ten[7]) {
  double eval[3];
  tenEigensolve_d(eval, NULL, ten);
  return anisoEval_Cs1_d(eval);
}

static float
anisoEval_Ct1_f(const float _eval[3]) {
  float dem, mn, eval[3];
  mn = (_eval[0] + _eval[1] + _eval[2]) / 3;
  ELL_3V_SET(eval, _eval[0] - mn, _eval[1] - mn, _eval[2] - mn);
  dem = eval[0] + eval[1] - 2 * eval[2];
  return dem ? 2 * (eval[1] - eval[2]) / dem : 0.0f;
}
static double
anisoEval_Ct1_d(const double _eval[3]) {
  double dem, mn, eval[3];
  mn = (_eval[0] + _eval[1] + _eval[2]) / 3;
  ELL_3V_SET(eval, _eval[0] - mn, _eval[1] - mn, _eval[2] - mn);
  dem = eval[0] + eval[1] - 2 * eval[2];
  return dem ? 2 * (eval[1] - eval[2]) / dem : 0.0;
}
static float
anisoTen_Ct1_f(const float ten[7]) {
  float eval[3];
  tenEigensolve_f(eval, NULL, ten);
  return anisoEval_Ct1_f(eval);
}
static double
anisoTen_Ct1_d(const double ten[7]) {
  double eval[3];
  tenEigensolve_d(eval, NULL, ten);
  return anisoEval_Ct1_d(eval);
}

static float
anisoEval_Cl2_f(const float eval[3]) {
  float eval0 = AIR_MAX(0, eval[0]);
  return eval0 ? (eval[0] - eval[1]) / eval0 : 0.0f;
}
static double
anisoEval_Cl2_d(const double eval[3]) {
  double eval0 = AIR_MAX(0, eval[0]);
  return eval0 ? (eval[0] - eval[1]) / eval0 : 0.0;
}
static float
anisoTen_Cl2_f(const float ten[7]) {
  float eval[3];
  tenEigensolve_f(eval, NULL, ten);
  return anisoEval_Cl2_f(eval);
}
static double
anisoTen_Cl2_d(const double ten[7]) {
  double eval[3];
  tenEigensolve_d(eval, NULL, ten);
  return anisoEval_Cl2_d(eval);
}

static float
anisoEval_Cp2_f(const float eval[3]) {
  float eval0 = AIR_MAX(0, eval[0]);
  return eval0 ? (eval[1] - eval[2]) / eval0 : 0.0f;
}
static double
anisoEval_Cp2_d(const double eval[3]) {
  double eval0 = AIR_MAX(0, eval[0]);
  return eval0 ? (eval[1] - eval[2]) / eval0 : 0.0;
}
static float
anisoTen_Cp2_f(const float ten[7]) {
  float eval[3];
  tenEigensolve_f(eval, NULL, ten);
  return anisoEval_Cp2_f(eval);
}
static double
anisoTen_Cp2_d(const double ten[7]) {
  double eval[3];
  tenEigensolve_d(eval, NULL, ten);
  return anisoEval_Cp2_d(eval);
}

static float
anisoEval_Ca2_f(const float eval[3]) {
  float eval0 = AIR_MAX(0, eval[0]);
  return eval0 ? (eval[0] - eval[2]) / eval0 : 0.0f;
}
static double
anisoEval_Ca2_d(const double eval[3]) {
  double eval0 = AIR_MAX(0, eval[0]);
  return eval0 ? (eval[0] - eval[2]) / eval0 : 0.0;
}
static float
anisoTen_Ca2_f(const float ten[7]) {
  float eval[3];
  tenEigensolve_f(eval, NULL, ten);
  return anisoEval_Ca2_f(eval);
}
static double
anisoTen_Ca2_d(const double ten[7]) {
  double eval[3];
  tenEigensolve_d(eval, NULL, ten);
  return anisoEval_Ca2_d(eval);
}

static float
anisoEval_Clpmin2_f(const float eval[3]) {
  float cl, cp, eval0 = AIR_MAX(0, eval[0]);
  cl = eval0 ? (eval[0] - eval[1]) / eval0 : 0.0f;
  cp = eval0 ? (eval[1] - eval[2]) / eval0 : 0.0f;
  return AIR_MIN(cl, cp);
}
static double
anisoEval_Clpmin2_d(const double eval[3]) {
  double cl, cp, eval0 = AIR_MAX(0, eval[0]);
  cl = eval0 ? (eval[0] - eval[1]) / eval0 : 0.0;
  cp = eval0 ? (eval[1] - eval[2]) / eval0 : 0.0;
  return AIR_MIN(cl, cp);
}
static float
anisoTen_Clpmin2_f(const float ten[7]) {
  float eval[3];
  tenEigensolve_f(eval, NULL, ten);
  return anisoEval_Clpmin2_f(eval);
}
static double
anisoTen_Clpmin2_d(const double ten[7]) {
  double eval[3];
  tenEigensolve_d(eval, NULL, ten);
  return anisoEval_Clpmin2_d(eval);
}

static float
anisoEval_Cs2_f(const float eval[3]) {
  float eval0 = AIR_MAX(0, eval[0]);
  return eval0 ? eval[2] / eval0 : 0.0f;
}
static double
anisoEval_Cs2_d(const double eval[3]) {
  double eval0 = AIR_MAX(0, eval[0]);
  return eval0 ? eval[2] / eval0 : 0.0;
}
static float
anisoTen_Cs2_f(const float ten[7]) {
  float eval[3];
  tenEigensolve_f(eval, NULL, ten);
  return anisoEval_Cs2_f(eval);
}
static double
anisoTen_Cs2_d(const double ten[7]) {
  double eval[3];
  tenEigensolve_d(eval, NULL, ten);
  return anisoEval_Cs2_d(eval);
}

static float
anisoEval_Ct2_f(const float eval[3]) {
  float denom;
  denom = eval[0] - eval[2];
  return denom ? (eval[1] - eval[2]) / denom : 0.0f;
}
static double
anisoEval_Ct2_d(const double eval[3]) {
  double denom;
  denom = eval[0] - eval[2];
  return denom ? (eval[1] - eval[2]) / denom : 0.0;
}
static float
anisoTen_Ct2_f(const float ten[7]) {
  float eval[3];
  tenEigensolve_f(eval, NULL, ten);
  return anisoEval_Ct2_f(eval);
}
static double
anisoTen_Ct2_d(const double ten[7]) {
  double eval[3];
  tenEigensolve_d(eval, NULL, ten);
  return anisoEval_Ct2_d(eval);
}

#define SQRT6 2.44948974278317809819
static float
anisoEval_RA_f(const float eval[3]) {
  float mean, stdv;
  mean = (eval[0] + eval[1] + eval[2]) / 3;
  stdv = AIR_FLOAT(sqrt((mean - eval[0]) * (mean - eval[0]) /* not exactly stdv */
                        + (mean - eval[1]) * (mean - eval[1])
                        + (mean - eval[2]) * (mean - eval[2])));
  return mean ? AIR_FLOAT(stdv / (mean * SQRT6)) : 0.0f;
}
static double
anisoEval_RA_d(const double eval[3]) {
  double mean, stdv;
  mean = (eval[0] + eval[1] + eval[2]) / 3;
  stdv = sqrt((mean - eval[0]) * (mean - eval[0]) /* not exactly standard dev */
              + (mean - eval[1]) * (mean - eval[1])
              + (mean - eval[2]) * (mean - eval[2]));
  return mean ? stdv / (mean * SQRT6) : 0.0;
}
static float
anisoTen_RA_f(const float tt[7]) {
  float mn, stdv, dev[7];
  mn = TEN_T_TRACE(tt) / 3;
  TEN_T_SET(dev, tt[0], tt[1] - mn, tt[2], tt[3], tt[4] - mn, tt[5], tt[6] - mn);
  stdv = AIR_FLOAT(sqrt(TEN_T_DOT(dev, dev)));
  return mn ? AIR_FLOAT(stdv / (mn * SQRT6)) : 0.0f;
}
static double
anisoTen_RA_d(const double tt[7]) {
  double mn, stdv, dev[7];
  mn = TEN_T_TRACE(tt) / 3;
  TEN_T_SET(dev, tt[0], tt[1] - mn, tt[2], tt[3], tt[4] - mn, tt[5], tt[6] - mn);
  stdv = sqrt(TEN_T_DOT(dev, dev));
  return mn ? stdv / (mn * SQRT6) : 0.0;
}

static float
anisoEval_FA_f(const float eval[3]) {
  float denom, mean, stdv;
  denom = 2.0f * (eval[0] * eval[0] + eval[1] * eval[1] + eval[2] * eval[2]);
  mean = (eval[0] + eval[1] + eval[2]) / 3;
  stdv = AIR_FLOAT((mean - eval[0]) * (mean - eval[0]) /* not exactly stdv */
                   + (mean - eval[1]) * (mean - eval[1])
                   + (mean - eval[2]) * (mean - eval[2]));
  return denom ? AIR_FLOAT(sqrt(3.0 * stdv / denom)) : 0.0f;
}
static double
anisoEval_FA_d(const double eval[3]) {
  double denom, mean, stdv;
  denom = 2.0 * (eval[0] * eval[0] + eval[1] * eval[1] + eval[2] * eval[2]);
  mean = (eval[0] + eval[1] + eval[2]) / 3;
  stdv = ((mean - eval[0]) * (mean - eval[0]) /* not exactly standard dev */
          + (mean - eval[1]) * (mean - eval[1]) + (mean - eval[2]) * (mean - eval[2]));
  return denom ? sqrt(3.0 * stdv / denom) : 0.0;
}
static float
anisoTen_FA_f(const float tt[7]) {
  float denom, mn, stdv, dev[7];
  denom = AIR_FLOAT(2.0 * TEN_T_DOT(tt, tt));
  mn = TEN_T_TRACE(tt) / 3;
  TEN_T_SET(dev, tt[0], tt[1] - mn, tt[2], tt[3], tt[4] - mn, tt[5], tt[6] - mn);
  stdv = TEN_T_DOT(dev, dev);
  return denom ? AIR_FLOAT(sqrt(3.0 * stdv / denom)) : 0.0f;
}
static double
anisoTen_FA_d(const double tt[7]) {
  double denom, mn, stdv, dev[7];
  denom = 2.0 * TEN_T_DOT(tt, tt);
  mn = TEN_T_TRACE(tt) / 3;
  TEN_T_SET(dev, tt[0], tt[1] - mn, tt[2], tt[3], tt[4] - mn, tt[5], tt[6] - mn);
  stdv = TEN_T_DOT(dev, dev);
  return denom ? AIR_FLOAT(sqrt(3.0 * stdv / denom)) : 0.0;
}

static float
anisoEval_VF_f(const float eval[3]) {
  float mean;
  mean = (eval[0] + eval[1] + eval[2]) / 3.0f;
  mean = mean * mean * mean;
  return 1.0f - (mean ? eval[0] * eval[1] * eval[2] / mean : 0.0f);
}
static double
anisoEval_VF_d(const double eval[3]) {
  double mean;
  mean = (eval[0] + eval[1] + eval[2]) / 3;
  mean = mean * mean * mean;
  return 1.0 - (mean ? eval[0] * eval[1] * eval[2] / mean : 0.0);
}
static float
anisoTen_VF_f(const float ten[7]) {
  float mean;
  mean = TEN_T_TRACE(ten) / 3.0f;
  mean = mean * mean * mean;
  return 1.0f - (mean ? TEN_T_DET(ten) / mean : 0.0f);
}
static double
anisoTen_VF_d(const double ten[7]) {
  double mean;
  mean = TEN_T_TRACE(ten) / 3.0;
  mean = mean * mean * mean;
  return 1.0 - (mean ? TEN_T_DET(ten) / mean : 0.0);
}

static float
anisoEval_B_f(const float eval[3]) {
  return eval[0] * eval[1] + eval[0] * eval[2] + eval[1] * eval[2];
}
static double
anisoEval_B_d(const double eval[3]) {
  return eval[0] * eval[1] + eval[0] * eval[2] + eval[1] * eval[2];
}
static float
anisoTen_B_f(const float ten[7]) {
  return (ten[1] * ten[4] + ten[1] * ten[6] + ten[4] * ten[6] - ten[2] * ten[2]
          - ten[3] * ten[3] - ten[5] * ten[5]);
}
static double
anisoTen_B_d(const double ten[7]) {
  return (ten[1] * ten[4] + ten[1] * ten[6] + ten[4] * ten[6] - ten[2] * ten[2]
          - ten[3] * ten[3] - ten[5] * ten[5]);
}

static float
anisoEval_Q_f(const float eval[3]) {
  float A, B;
  A = -(eval[0] + eval[1] + eval[2]);
  B = anisoEval_B_f(eval);
  A = (A * A - 3.0f * B) / 9.0f;
  return AIR_MAX(0, A);
}
static double
anisoEval_Q_d(const double eval[3]) {
  double A, B;
  A = -(eval[0] + eval[1] + eval[2]);
  B = anisoEval_B_d(eval);
  A = (A * A - 3.0 * B) / 9.0;
  return AIR_MAX(0, A);
}
static float
anisoTen_Q_f(const float ten[7]) {
  float A, B;
  A = -TEN_T_TRACE(ten);
  B = anisoTen_B_f(ten);
  A = (A * A - 3.0f * B) / 9.0f;
  return AIR_MAX(0, A);
}
static double
anisoTen_Q_d(const double ten[7]) {
  double A, B;
  A = -TEN_T_TRACE(ten);
  B = anisoTen_B_d(ten);
  A = (A * A - 3.0 * B) / 9.0;
  return AIR_MAX(0, A);
}

static float
anisoEval_R_f(const float eval[3]) {
  float A, B, C;
  A = -(eval[0] + eval[1] + eval[2]);
  B = anisoEval_B_f(eval);
  C = -eval[0] * eval[1] * eval[2];
  return (-2 * A * A * A + 9 * A * B - 27 * C) / 54;
}
static double
anisoEval_R_d(const double eval[3]) {
  double A, B, C;
  A = -(eval[0] + eval[1] + eval[2]);
  B = anisoEval_B_d(eval);
  C = -eval[0] * eval[1] * eval[2];
  return (-2 * A * A * A + 9 * A * B - 27 * C) / 54;
}
static float
anisoTen_R_f(const float ten[7]) {
  float A, B, C;
  A = -TEN_T_TRACE(ten);
  B = anisoTen_B_f(ten);
  C = -TEN_T_DET(ten);
  return (-2 * A * A * A + 9 * A * B - 27 * C) / 54;
}
static double
anisoTen_R_d(const double ten[7]) {
  double A, B, C;
  A = -TEN_T_TRACE(ten);
  B = anisoTen_B_d(ten);
  C = -TEN_T_DET(ten);
  return (-2 * A * A * A + 9 * A * B - 27 * C) / 54;
}

static float
anisoEval_S_f(const float eval[3]) {
  return eval[0] * eval[0] + eval[1] * eval[1] + eval[2] * eval[2];
}
static double
anisoEval_S_d(const double eval[3]) {
  return eval[0] * eval[0] + eval[1] * eval[1] + eval[2] * eval[2];
}
static float
anisoTen_S_f(const float ten[7]) {
  return TEN_T_DOT(ten, ten);
}
static double
anisoTen_S_d(const double ten[7]) {
  return TEN_T_DOT(ten, ten);
}

#define OOSQRT2 0.70710678118654752440
static float
anisoEval_Skew_f(const float _eval[3]) {
  float Q, num, dnm, ret, mn, eval[3];
  mn = (_eval[0] + _eval[1] + _eval[2]) / 3;
  ELL_3V_SET(eval, _eval[0] - mn, _eval[1] - mn, _eval[2] - mn);
  Q = anisoEval_Q_f(eval);
  num = anisoEval_R_f(eval);
  dnm = AIR_FLOAT(Q * sqrt(2 * Q));
  ret = dnm ? AIR_FLOAT(num / dnm) : 0.0f;
  return AIR_FLOAT(AIR_CLAMP(-OOSQRT2, ret, OOSQRT2));
}
static double
anisoEval_Skew_d(const double _eval[3]) {
  double Q, num, dnm, ret, mn, eval[3];
  mn = (_eval[0] + _eval[1] + _eval[2]) / 3;
  ELL_3V_SET(eval, _eval[0] - mn, _eval[1] - mn, _eval[2] - mn);
  Q = anisoEval_Q_d(eval);
  num = anisoEval_R_d(eval);
  dnm = Q * sqrt(2 * Q);
  ret = dnm ? num / dnm : 0.0;
  return AIR_CLAMP(-OOSQRT2, ret, OOSQRT2);
}
static float
anisoTen_Skew_f(const float _t[7]) {
  float Q, num, dnm, ret, mn, ten[7];
  mn = TEN_T_TRACE(_t) / 3;
  TEN_T_SET(ten, _t[0], _t[1] - mn, _t[2], _t[3], _t[4] - mn, _t[5], _t[6] - mn);
  Q = anisoTen_Q_f(ten);
  num = anisoTen_R_f(ten);
  dnm = AIR_FLOAT(Q * sqrt(2 * Q));
  ret = dnm ? AIR_FLOAT(num / dnm) : 0.0f;
  return AIR_FLOAT(AIR_CLAMP(-OOSQRT2, ret, OOSQRT2));
}
static double
anisoTen_Skew_d(const double _t[7]) {
  double Q, num, dnm, ret, mn, ten[7];
  mn = TEN_T_TRACE(_t) / 3;
  TEN_T_SET(ten, _t[0], _t[1] - mn, _t[2], _t[3], _t[4] - mn, _t[5], _t[6] - mn);
  Q = anisoTen_Q_d(ten);
  num = anisoTen_R_d(ten);
  dnm = Q * sqrt(2 * Q);
  ret = dnm ? num / dnm : 0.0;
  return AIR_CLAMP(-OOSQRT2, ret, OOSQRT2);
}

static float
anisoEval_Mode_f(const float _eval[3]) {
  float n, d, mn, e[3], ret;
  mn = (_eval[0] + _eval[1] + _eval[2]) / 3;
  ELL_3V_SET(e, _eval[0] - mn, _eval[1] - mn, _eval[2] - mn);
  n = (e[0] + e[1] - 2 * e[2]) * (2 * e[0] - e[1] - e[2]) * (e[0] - 2 * e[1] + e[2]);
  d = (e[0] * e[0] + e[1] * e[1] + e[2] * e[2] - e[0] * e[1] - e[1] * e[2]
       - e[0] * e[2]);
  d = AIR_FLOAT(sqrt(AIR_MAX(0, d)));
  d = 2 * d * d * d;
  ret = d ? AIR_FLOAT(n / d) : 0.0f;
  return AIR_CLAMP(-1, ret, 1);
}
static double
anisoEval_Mode_d(const double _eval[3]) {
  double n, d, mn, e[3], ret;
  mn = (_eval[0] + _eval[1] + _eval[2]) / 3;
  ELL_3V_SET(e, _eval[0] - mn, _eval[1] - mn, _eval[2] - mn);
  n = (e[0] + e[1] - 2 * e[2]) * (2 * e[0] - e[1] - e[2]) * (e[0] - 2 * e[1] + e[2]);
  d = (e[0] * e[0] + e[1] * e[1] + e[2] * e[2] - e[0] * e[1] - e[1] * e[2]
       - e[0] * e[2]);
  d = sqrt(AIR_MAX(0, d));
  d = 2 * d * d * d;
  ret = d ? n / d : 0.0;
  return AIR_CLAMP(-1, ret, 1);
}
static float
anisoTen_Mode_f(const float tt[7]) {
  float mn, dev[7], tmp, ret;
  mn = TEN_T_TRACE(tt) / 3.0f;
  TEN_T_SET(dev, tt[0], tt[1] - mn, tt[2], tt[3], tt[4] - mn, tt[5], tt[6] - mn);
  tmp = AIR_FLOAT(TEN_T_NORM(dev));
  tmp = tmp ? 1.0f / tmp : 0.0f;
  TEN_T_SCALE(dev, tmp, dev);
  ret = AIR_FLOAT(3 * SQRT6 * TEN_T_DET(dev));
  return AIR_CLAMP(-1, ret, 1);
}
static double
anisoTen_Mode_d(const double tt[7]) {
  double mn, dev[7], tmp, ret;
  mn = TEN_T_TRACE(tt) / 3.0;
  TEN_T_SET(dev, tt[0], tt[1] - mn, tt[2], tt[3], tt[4] - mn, tt[5], tt[6] - mn);
  tmp = TEN_T_NORM(dev);
  tmp = tmp ? 1.0 / tmp : 0.0;
  TEN_T_SCALE(dev, tmp, dev);
  ret = 3 * SQRT6 * TEN_T_DET(dev);
  return AIR_CLAMP(-1, ret, 1);
}

/* NOTE: yes, the AIR_CLAMPs here are needed even though
** the _Skew_ functions clamp their output
*/
#define SQRT2 1.41421356237309504880
static float
anisoEval_Th_f(const float eval[3]) {
  float tmp;
  tmp = AIR_FLOAT(SQRT2 * anisoEval_Skew_f(eval));
  return AIR_FLOAT(acos(AIR_CLAMP(-1, tmp, 1)) / 3);
}
static double
anisoEval_Th_d(const double eval[3]) {
  double tmp;
  tmp = SQRT2 * anisoEval_Skew_d(eval);
  return acos(AIR_CLAMP(-1, tmp, 1)) / 3;
}
static float
anisoTen_Th_f(const float ten[7]) {
  float tmp;
  tmp = AIR_FLOAT(SQRT2 * anisoTen_Skew_f(ten));
  return AIR_FLOAT(acos(AIR_CLAMP(-1, tmp, 1)) / 3);
}
static double
anisoTen_Th_d(const double ten[7]) {
  double tmp;
  tmp = SQRT2 * anisoTen_Skew_d(ten);
  return acos(AIR_CLAMP(-1, tmp, 1)) / 3;
}

static float
anisoEval_Omega_f(const float eval[3]) {
  return anisoEval_FA_f(eval) * (1.0f + anisoEval_Mode_f(eval)) / 2.0f;
}
static double
anisoEval_Omega_d(const double eval[3]) {
  return anisoEval_FA_d(eval) * (1.0f + anisoEval_Mode_d(eval)) / 2.0f;
}
static float
anisoTen_Omega_f(const float ten[7]) {
  return anisoTen_FA_f(ten) * (1.0f + anisoTen_Mode_f(ten)) / 2.0f;
}
static double
anisoTen_Omega_d(const double ten[7]) {
  return anisoTen_FA_d(ten) * (1.0f + anisoTen_Mode_d(ten)) / 2.0f;
}

static float
anisoEval_Det_f(const float eval[3]) {
  return eval[0] * eval[1] * eval[2];
}
static double
anisoEval_Det_d(const double eval[3]) {
  return eval[0] * eval[1] * eval[2];
}
static float
anisoTen_Det_f(const float ten[7]) {
  return TEN_T_DET(ten);
}
static double
anisoTen_Det_d(const double ten[7]) {
  return TEN_T_DET(ten);
}

static float
anisoEval_Tr_f(const float eval[3]) {
  return eval[0] + eval[1] + eval[2];
}
static double
anisoEval_Tr_d(const double eval[3]) {
  return eval[0] + eval[1] + eval[2];
}
static float
anisoTen_Tr_f(const float ten[7]) {
  return TEN_T_TRACE(ten);
}
static double
anisoTen_Tr_d(const double ten[7]) {
  return TEN_T_TRACE(ten);
}

static float
anisoEval_eval0_f(const float eval[3]) {
  return eval[0];
}
static double
anisoEval_eval0_d(const double eval[3]) {
  return eval[0];
}
static float
anisoTen_eval0_f(const float ten[7]) {
  float eval[3];
  tenEigensolve_f(eval, NULL, ten);
  return eval[0];
}
static double
anisoTen_eval0_d(const double ten[7]) {
  double eval[3];
  tenEigensolve_d(eval, NULL, ten);
  return eval[0];
}

static float
anisoEval_eval1_f(const float eval[3]) {
  return eval[1];
}
static double
anisoEval_eval1_d(const double eval[3]) {
  return eval[1];
}
static float
anisoTen_eval1_f(const float ten[7]) {
  float eval[3];
  tenEigensolve_f(eval, NULL, ten);
  return eval[1];
}
static double
anisoTen_eval1_d(const double ten[7]) {
  double eval[3];
  tenEigensolve_d(eval, NULL, ten);
  return eval[1];
}

static float
anisoEval_eval2_f(const float eval[3]) {
  return eval[2];
}
static double
anisoEval_eval2_d(const double eval[3]) {
  return eval[2];
}
static float
anisoTen_eval2_f(const float ten[7]) {
  float eval[3];
  tenEigensolve_f(eval, NULL, ten);
  return eval[2];
}
static double
anisoTen_eval2_d(const double ten[7]) {
  double eval[3];
  tenEigensolve_d(eval, NULL, ten);
  return eval[2];
}

float (*const tenAnisoEvalArray_f[TEN_ANISO_MAX + 1])(const float eval[3])
  = {NULL,
     anisoEval_Conf_f,
     anisoEval_Cl1_f,
     anisoEval_Cp1_f,
     anisoEval_Ca1_f,
     anisoEval_Clpmin1_f,
     anisoEval_Cs1_f,
     anisoEval_Ct1_f,
     anisoEval_Cl2_f,
     anisoEval_Cp2_f,
     anisoEval_Ca2_f,
     anisoEval_Clpmin2_f,
     anisoEval_Cs2_f,
     anisoEval_Ct2_f,
     anisoEval_RA_f,
     anisoEval_FA_f,
     anisoEval_VF_f,
     anisoEval_B_f,
     anisoEval_Q_f,
     anisoEval_R_f,
     anisoEval_S_f,
     anisoEval_Skew_f,
     anisoEval_Mode_f,
     anisoEval_Th_f,
     anisoEval_Omega_f,
     anisoEval_Det_f,
     anisoEval_Tr_f,
     anisoEval_eval0_f,
     anisoEval_eval1_f,
     anisoEval_eval2_f};

double (*const tenAnisoEvalArray_d[TEN_ANISO_MAX + 1])(const double eval[3])
  = {NULL,
     anisoEval_Conf_d,
     anisoEval_Cl1_d,
     anisoEval_Cp1_d,
     anisoEval_Ca1_d,
     anisoEval_Clpmin1_d,
     anisoEval_Cs1_d,
     anisoEval_Ct1_d,
     anisoEval_Cl2_d,
     anisoEval_Cp2_d,
     anisoEval_Ca2_d,
     anisoEval_Clpmin2_d,
     anisoEval_Cs2_d,
     anisoEval_Ct2_d,
     anisoEval_RA_d,
     anisoEval_FA_d,
     anisoEval_VF_d,
     anisoEval_B_d,
     anisoEval_Q_d,
     anisoEval_R_d,
     anisoEval_S_d,
     anisoEval_Skew_d,
     anisoEval_Mode_d,
     anisoEval_Th_d,
     anisoEval_Omega_d,
     anisoEval_Det_d,
     anisoEval_Tr_d,
     anisoEval_eval0_d,
     anisoEval_eval1_d,
     anisoEval_eval2_d};

float (*const tenAnisoTenArray_f[TEN_ANISO_MAX + 1])(const float ten[7])
  = {NULL,
     anisoTen_Conf_f,
     anisoTen_Cl1_f,
     anisoTen_Cp1_f,
     anisoTen_Ca1_f,
     anisoTen_Clpmin1_f,
     anisoTen_Cs1_f,
     anisoTen_Ct1_f,
     anisoTen_Cl2_f,
     anisoTen_Cp2_f,
     anisoTen_Ca2_f,
     anisoTen_Clpmin2_f,
     anisoTen_Cs2_f,
     anisoTen_Ct2_f,
     anisoTen_RA_f,
     anisoTen_FA_f,
     anisoTen_VF_f,
     anisoTen_B_f,
     anisoTen_Q_f,
     anisoTen_R_f,
     anisoTen_S_f,
     anisoTen_Skew_f,
     anisoTen_Mode_f,
     anisoTen_Th_f,
     anisoTen_Omega_f,
     anisoTen_Det_f,
     anisoTen_Tr_f,
     anisoTen_eval0_f,
     anisoTen_eval1_f,
     anisoTen_eval2_f};

double (*const tenAnisoTenArray_d[TEN_ANISO_MAX + 1])(const double ten[7])
  = {NULL,
     anisoTen_Conf_d,
     anisoTen_Cl1_d,
     anisoTen_Cp1_d,
     anisoTen_Ca1_d,
     anisoTen_Clpmin1_d,
     anisoTen_Cs1_d,
     anisoTen_Ct1_d,
     anisoTen_Cl2_d,
     anisoTen_Cp2_d,
     anisoTen_Ca2_d,
     anisoTen_Clpmin2_d,
     anisoTen_Cs2_d,
     anisoTen_Ct2_d,
     anisoTen_RA_d,
     anisoTen_FA_d,
     anisoTen_VF_d,
     anisoTen_B_d,
     anisoTen_Q_d,
     anisoTen_R_d,
     anisoTen_S_d,
     anisoTen_Skew_d,
     anisoTen_Mode_d,
     anisoTen_Th_d,
     anisoTen_Omega_d,
     anisoTen_Det_d,
     anisoTen_Tr_d,
     anisoTen_eval0_d,
     anisoTen_eval1_d,
     anisoTen_eval2_d};

float /* Biff: nope */
tenAnisoEval_f(const float eval[3], int aniso) {

  return (AIR_IN_OP(tenAnisoUnknown, aniso, tenAnisoLast)
            ? tenAnisoEvalArray_f[aniso](eval)
            : 0);
}

double /* Biff: nope */
tenAnisoEval_d(const double eval[3], int aniso) {

  return (AIR_IN_OP(tenAnisoUnknown, aniso, tenAnisoLast)
            ? tenAnisoEvalArray_d[aniso](eval)
            : 0);
}

float /* Biff: nope */
tenAnisoTen_f(const float ten[7], int aniso) {

  return (AIR_IN_OP(tenAnisoUnknown, aniso, tenAnisoLast)
            ? tenAnisoTenArray_f[aniso](ten)
            : 0);
}

double /* Biff: nope */
tenAnisoTen_d(const double ten[7], int aniso) {

  return (AIR_IN_OP(tenAnisoUnknown, aniso, tenAnisoLast)
            ? tenAnisoTenArray_d[aniso](ten)
            : 0);
}

/*
******** tenAnisoCalc_f
**
** !!! THIS FUNCTION HAS BEEN MADE OBSOLETE BY THE NEW
** !!! tenAnisoEval_{f,d} AND tenAnisoTen_{f,d} FUNCTIONS
** !!! THIS WILL LIKELY BE REMOVED FROM FUTURE RELEASES
**
** prior to TeemV2, this was commented out;
** with TeemV2, it was removed
*/

int /* Biff: 1 */
tenAnisoPlot(Nrrd *nout, int aniso, unsigned int res, int hflip, int whole, int nanout) {
  static const char me[] = "tenAnisoPlot";
  float *out, tmp;
  unsigned int x, y;
  float m0[3], m1[3], m2[3], c0, c1, c2, e[3];
  float S = 1 / 3.0f, L = 1.0f, P = 1 / 2.0f; /* these make Westin's original
                                                 (cl,cp,cs) align with the
                                                 barycentric coordinates */

  if (airEnumValCheck(tenAniso, aniso)) {
    biffAddf(TEN, "%s: invalid aniso (%d)", me, aniso);
    return 1;
  }
  if (!(res > 2)) {
    biffAddf(TEN, "%s: resolution (%u) invalid", me, res);
    return 1;
  }
  if (nrrdMaybeAlloc_va(nout, nrrdTypeFloat, 2, AIR_SIZE_T(res), AIR_SIZE_T(res))) {
    biffMovef(TEN, NRRD, "%s: ", me);
    return 1;
  }
  out = (float *)nout->data;
  if (whole) {
    ELL_3V_SET(m0, 1, 0, 0);
    ELL_3V_SET(m1, 0, 1, 0);
    ELL_3V_SET(m2, 0, 0, 1);
  } else {
    ELL_3V_SET(m0, S, S, S);
    if (hflip) {
      ELL_3V_SET(m1, P, P, 0);
      ELL_3V_SET(m2, L, 0, 0);
    } else {
      ELL_3V_SET(m1, L, 0, 0);
      ELL_3V_SET(m2, P, P, 0);
    }
  }
  for (y = 0; y < res; y++) {
    for (x = 0; x <= y; x++) {
      /* (c0,c1,c2) are the barycentric coordinates */
      c0 = AIR_FLOAT(1.0 - AIR_AFFINE(-0.5, y, res - 0.5, 0.0, 1.0));
      c2 = AIR_FLOAT(AIR_AFFINE(-0.5, x, res - 0.5, 0.0, 1.0));
      c1 = 1 - c0 - c2;
      e[0] = c0 * m0[0] + c1 * m1[0] + c2 * m2[0];
      e[1] = c0 * m0[1] + c1 * m1[1] + c2 * m2[1];
      e[2] = c0 * m0[2] + c1 * m1[2] + c2 * m2[2];
      ELL_SORT3(e[0], e[1], e[2], tmp); /* got some warnings w/out this */
      out[x + res * y] = tenAnisoEval_f(e, aniso);
    }
    if (nanout) {
      for (x = y + 1; x < res; x++) {
        out[x + res * y] = AIR_NAN;
      }
    }
  }

  return 0;
}

int /* Biff: 1 */
tenAnisoVolume(Nrrd *nout, const Nrrd *nin, int aniso, double confThresh) {
  static const char me[] = "tenAnisoVolume";
  size_t N, I;
  float *out, *in, *tensor;
  int map[NRRD_DIM_MAX];
  size_t sx, sy, sz, size[3];

  if (tenTensorCheck(nin, nrrdTypeFloat, AIR_TRUE, AIR_TRUE)) {
    biffAddf(TEN, "%s: didn't get a tensor nrrd", me);
    return 1;
  }
  if (airEnumValCheck(tenAniso, aniso)) {
    biffAddf(TEN, "%s: invalid aniso (%d)", me, aniso);
    return 1;
  }
  confThresh = AIR_CLAMP(0.0, confThresh, 1.0);

  size[0] = sx = nin->axis[1].size;
  size[1] = sy = nin->axis[2].size;
  size[2] = sz = nin->axis[3].size;
  N = sx * sy * sz;
  if (nrrdMaybeAlloc_va(nout, nrrdTypeFloat, 3, sx, sy, sz)) {
    biffMovef(TEN, NRRD, "%s: trouble", me);
    return 1;
  }
  out = (float *)nout->data;
  in = (float *)nin->data;
  for (I = 0; I <= N - 1; I++) {
    /* tenVerbose = (I == 911327); */
    tensor = in + I * 7;
    if (tenAniso_Conf != aniso && tensor[0] < confThresh) {
      out[I] = 0.0;
      continue;
    }
    /* no longer used
    tenEigensolve_f(eval, NULL, tensor);
    if (!(AIR_EXISTS(eval[0]) && AIR_EXISTS(eval[1]) && AIR_EXISTS(eval[2]))) {
      NRRD_COORD_GEN(coord, size, 3, I);
      biffAddf(TEN, "%s: not all eigenvalues exist (%g,%g,%g) at sample "
               "%d = (%d,%d,%d)",
               me, eval[0], eval[1], eval[2], (int)I,
               (int)coord[0], (int)coord[1], (int)coord[2]);
      return 1;
    }
    */
    out[I] = tenAnisoTen_f(tensor, aniso);
    if (!AIR_EXISTS(out[I])) {
      size_t coord[3];
      NRRD_COORD_GEN(coord, size, 3, I);
      biffAddf(TEN,
               "%s: generated non-existent aniso %g from tensor "
               "(%g) %g %g %g   %g %g   %g at sample %u = (%u,%u,%u)",
               me, out[I], tensor[0], tensor[1], tensor[2], tensor[3], tensor[4],
               tensor[5], tensor[6], AIR_UINT(I), AIR_UINT(coord[0]), AIR_UINT(coord[1]),
               AIR_UINT(coord[2]));
      return 1;
    }
  }
  ELL_3V_SET(map, 1, 2, 3);
  if (nrrdAxisInfoCopy(nout, nin, map, NRRD_AXIS_INFO_SIZE_BIT)) {
    biffMovef(TEN, NRRD, "%s: trouble", me);
    return 1;
  }
  if (nrrdBasicInfoCopy(nout, nin, NRRD_BASIC_INFO_ALL ^ NRRD_BASIC_INFO_SPACE)) {
    biffMovef(TEN, NRRD, "%s:", me);
    return 1;
  }

  return 0;
}

int /* Biff: 1 */
tenAnisoHistogram(Nrrd *nout, const Nrrd *nin, const Nrrd *nwght, int right, int version,
                  unsigned int res) {
  static const char me[] = "tenAnisoHistogram";
  size_t N, I;
  int csIdx, clIdx, cpIdx;
  float *tdata, *out, eval[3], cs, cl, cp, (*wlup)(const void *data, size_t idx), weight;
  unsigned int yres, xi, yi;

  if (tenTensorCheck(nin, nrrdTypeFloat, AIR_TRUE, AIR_TRUE)) {
    biffAddf(TEN, "%s: didn't get a tensor nrrd", me);
    return 1;
  }
  if (nwght) {
    if (nrrdCheck(nwght)) {
      biffMovef(TEN, NRRD, "%s: trouble with weighting nrrd", me);
      return 1;
    }
    if (nrrdElementNumber(nwght)
        != nrrdElementNumber(nin) / nrrdKindSize(nrrdKind3DMaskedSymMatrix)) {
      char stmp[2][AIR_STRLEN_SMALL + 1];
      size_t numten;
      numten = nrrdElementNumber(nin) / nrrdKindSize(nrrdKind3DMaskedSymMatrix);
      biffAddf(TEN, "%s: # elements in weight nrrd (%s) != # tensors (%s)", me,
               airSprintSize_t(stmp[0], nrrdElementNumber(nwght)),
               airSprintSize_t(stmp[1], numten));
      return 1;
    }
  }
  if (!(1 == version || 2 == version)) {
    biffAddf(TEN, "%s: version (%d) wasn't 1 or 2", me, version);
    return 1;
  }
  if (!(res > 10)) {
    biffAddf(TEN, "%s: resolution (%u) invalid", me, res);
    return 1;
  }
  if (right) {
    yres = AIR_UINT(AIR_CAST(double, res) / sqrt(3));
  } else {
    yres = res;
  }
  if (nwght) {
    wlup = nrrdFLookup[nwght->type];
  } else {
    wlup = NULL;
  }
  if (nrrdMaybeAlloc_va(nout, nrrdTypeFloat, 2, AIR_SIZE_T(res), AIR_SIZE_T(yres))) {
    biffMovef(TEN, NRRD, "%s: ", me);
    return 1;
  }
  out = (float *)nout->data;
  tdata = (float *)nin->data;
  if (right || 1 == version) {
    clIdx = tenAniso_Cl1;
    cpIdx = tenAniso_Cp1;
    csIdx = tenAniso_Cs1;
  } else {
    clIdx = tenAniso_Cl2;
    cpIdx = tenAniso_Cp2;
    csIdx = tenAniso_Cs2;
  }
  N = nrrdElementNumber(nin) / nrrdKindSize(nrrdKind3DMaskedSymMatrix);
  for (I = 0; I < N; I++) {
    tenEigensolve_f(eval, NULL, tdata);
    cl = tenAnisoEval_f(eval, clIdx);
    cp = tenAnisoEval_f(eval, cpIdx);
    cs = tenAnisoEval_f(eval, csIdx);
    if (right) {
      xi = AIR_UINT(cs * 0 + cl * (res - 1) + cp * 0);
      yi = AIR_UINT(cs * 0 + cl * (yres - 1) + cp * (yres - 1));
    } else {
      xi = AIR_UINT(cs * 0 + cl * 0 + cp * (res - 1));
      yi = AIR_UINT(cs * 0 + cl * (res - 1) + cp * (res - 1));
    }
    weight = wlup ? wlup(nwght->data, I) : 1.0f;
    if (xi < res && yi < yres - 1) {
      out[xi + res * yi] += tdata[0] * weight;
    }
    tdata += nrrdKindSize(nrrdKind3DMaskedSymMatrix);
  }

  return 0;
}

tenEvecRGBParm * /* Biff: nope */
tenEvecRGBParmNew(void) {
  tenEvecRGBParm *rgbp;

  rgbp = AIR_CAST(tenEvecRGBParm *, calloc(1, sizeof(tenEvecRGBParm)));
  if (rgbp) {
    rgbp->which = 0;
    rgbp->aniso = tenAniso_Cl2;
    rgbp->confThresh = 0.5;
    rgbp->anisoGamma = 1.0;
    rgbp->gamma = 1.0;
    rgbp->bgGray = 0.0;
    rgbp->isoGray = 0.0;
    rgbp->maxSat = 1.0;
    rgbp->typeOut = nrrdTypeFloat;
    rgbp->genAlpha = AIR_FALSE;
  }
  return rgbp;
}

tenEvecRGBParm * /* Biff: nope */
tenEvecRGBParmNix(tenEvecRGBParm *rgbp) {

  if (rgbp) {
    airFree(rgbp);
  }
  return NULL;
}
AIR_MOPPER(tenEvecRGBParmNix)

int /* Biff: 1 */
tenEvecRGBParmCheck(const tenEvecRGBParm *rgbp) {
  static const char me[] = "tenEvecRGBParmCheck";

  if (!rgbp) {
    biffAddf(TEN, "%s: got NULL pointer", me);
    return 1;
  }
  if (!(rgbp->which <= 2)) {
    biffAddf(TEN, "%s: which must be 0, 1, or 2 (not %u)", me, rgbp->which);
    return 1;
  }
  if (airEnumValCheck(tenAniso, rgbp->aniso)) {
    biffAddf(TEN, "%s: anisotropy metric %d not valid", me, rgbp->aniso);
    return 1;
  }
  if (nrrdTypeDefault != rgbp->typeOut && airEnumValCheck(nrrdType, rgbp->typeOut)) {
    biffAddf(TEN, "%s: output type (%d) not valid", me, rgbp->typeOut);
    return 1;
  }
  return 0;
}

static float
evecRGBComp_f(float conf, float aniso, float comp, const tenEvecRGBParm *rgbp) {
  double X;

  X = AIR_ABS(comp);
  X = pow(X, 1.0 / rgbp->gamma);
  X = AIR_LERP(rgbp->maxSat * aniso, rgbp->isoGray, X);
  return AIR_FLOAT(conf > rgbp->confThresh ? X : rgbp->bgGray);
}

static double
evecRGBComp_d(double conf, double aniso, double comp, const tenEvecRGBParm *rgbp) {
  double X;

  X = AIR_ABS(comp);
  X = pow(X, 1.0 / rgbp->gamma);
  X = AIR_LERP(rgbp->maxSat * aniso, rgbp->isoGray, X);
  return conf > rgbp->confThresh ? X : rgbp->bgGray;
}

void
tenEvecRGBSingle_f(float RGB[3], float conf, const float eval[3], const float evec[3],
                   const tenEvecRGBParm *rgbp) {
  float aniso;

  if (RGB && eval && rgbp) {
    aniso = tenAnisoEval_f(eval, rgbp->aniso);
    aniso = AIR_FLOAT(pow(aniso, 1.0 / rgbp->anisoGamma));
    ELL_3V_SET(RGB,
               evecRGBComp_f(conf, aniso, evec[0], rgbp),
               evecRGBComp_f(conf, aniso, evec[1], rgbp),
               evecRGBComp_f(conf, aniso, evec[2], rgbp));
  }
  return;
}

void
tenEvecRGBSingle_d(double RGB[3], double conf, const double eval[3],
                   const double evec[3], const tenEvecRGBParm *rgbp) {
  double aniso;

  if (RGB && eval && rgbp) {
    aniso = tenAnisoEval_d(eval, rgbp->aniso);
    aniso = pow(aniso, 1.0 / rgbp->anisoGamma);
    ELL_3V_SET(RGB,
               evecRGBComp_d(conf, aniso, evec[0], rgbp),
               evecRGBComp_d(conf, aniso, evec[1], rgbp),
               evecRGBComp_d(conf, aniso, evec[2], rgbp));
  }
  return;
}
