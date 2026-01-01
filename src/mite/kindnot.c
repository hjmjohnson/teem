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

#include "mite.h"
#include "privateMite.h"

/* clang-format off */
static const char *
valStr[] = {
  "(unknown miteVal)",
  "Xw",
  "Xi",
  "Yw",
  "Yi",
  "Zw",
  "Zi",
  "Rw",
  "Ri",
  "Tw",
  "Ti",
  "V",
  "N",
  "NdotV",
  "NdotL",
  "VrefN",
  "GTdotV",
  "VdefT",
  "VdefTdotV",
  "WdotD"
};

static const int
valVal[] = {
  miteValUnknown,
  miteValXw,
  miteValXi,
  miteValYw,
  miteValYi,
  miteValZw,
  miteValZi,
  miteValRw,
  miteValRi,
  miteValTw,
  miteValTi,
  miteValView,
  miteValNormal,
  miteValNdotV,
  miteValNdotL,
  miteValVrefN,
  miteValGTdotV,
  miteValVdefT,
  miteValVdefTdotV,
  miteValWdotD,
};

static const char *
valStrEqv[] = {
  "xw",
  "xi",
  "yw",
  "yi",
  "zw",
  "zi",
  "rw",
  "ri",
  "tw",
  "ti",
  "view", "v",
  "normal", "n",
  "ndotv", "vdotn",
  "ndotl", "ldotn",
  "vrefn",
  "gtdotv",
  "vdeft",
  "vdeftdotv",
  "wdotd",
  ""
};

static const int
valValEqv[] = {
  miteValXw,
  miteValXi,
  miteValYw,
  miteValYi,
  miteValZw,
  miteValZi,
  miteValRw,
  miteValRi,
  miteValTw,
  miteValTi,
  miteValView, miteValView,
  miteValNormal, miteValNormal,
  miteValNdotV, miteValNdotV,
  miteValNdotL, miteValNdotL,
  miteValVrefN,
  miteValGTdotV,
  miteValVdefT,
  miteValVdefTdotV,
  miteValWdotD
};

static const airEnum
val_ae = {
  "miteVal",
  MITE_VAL_ITEM_MAX,
  valStr, valVal,
  NULL,
  valStrEqv, valValEqv,
  AIR_FALSE
};
const airEnum *const miteVal = &val_ae;

/*
** again, this is not a true gageKind- mainly because these items may
** depend on items in different gageKinds (scalar and vector).  So,
** the prerequisites here are all blank.  Go look in miteQueryAdd()
** to see these items' true prereqs
*/
static gageItemEntry
valTable[MITE_VAL_ITEM_MAX+1] = {
  /* enum value        len,deriv, prereqs,  parent item, index, needData*/
  {miteValUnknown,       0,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValXw,            1,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValXi,            1,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValYw,            1,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValYi,            1,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValZw,            1,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValZi,            1,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValRw,            1,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValRi,            1,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValTw,            1,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValTi,            1,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValView,          3,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValNormal,        3,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValNdotV,         1,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValNdotL,         1,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValVrefN,         3,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValGTdotV,        1,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValVdefT,         3,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValVdefTdotV,     1,  0,   {0},              0,   0,     AIR_FALSE},
  {miteValWdotD,         1,  0,   {0},              0,   0,     AIR_FALSE}
};


static const gageKind
valGageKind_gk = {
  AIR_FALSE,
  "mite",
  &val_ae,
  0,
  0,
  MITE_VAL_ITEM_MAX,
  valTable,
  NULL,
  NULL,
  NULL,
  NULL, NULL, NULL, NULL,
  NULL
};

const gageKind *const miteValGageKind = &valGageKind_gk;
/* clang-format on */
