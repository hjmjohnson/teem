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

#include "bane.h"
#include "privateBane.h"

#define HVOL_INFO "Make histogram volume"
static const char *hvolInfoL
  = (HVOL_INFO ".  The histogram volume is a three-dimensional histogram recording "
               "the relationship between data value, gradient magnitude, and the "
               "second directional derivative along the gradient direction.  Creating "
               "it is the first step in semi-automatic transfer function generation.  ");

static int /* Biff: 1 */
hvolMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  double inc[3 * (1 + BANE_PARM_NUM)];
  hestOptAdd_3_Other(&hopt, "s", "incV incG incH", inc, "f:1.0 p:0.005 p:0.015",
                     "Strategies for determining how much of the range "
                     "of a quantity should be included and quantized in its axis "
                     "of the histogram volume.  Possibilities include:\n "
                     "\b\bo \"f:<F>\": included range is some fraction of the "
                     "total range, as scaled by F\n "
                     "\b\bo \"p:<P>\": exclude the extremal P percent of "
                     "the values\n "
                     "\b\bo \"s:<S>\": included range is S times the standard "
                     "deviation of the values\n "
                     "\b\bo \"a:<min>,<max>\": range is from <min> to <max>",
                     baneGkmsHestIncStrategy);
  uint dim[3];
  hestOptAdd_3_UInt(&hopt, "d", "dimV dimG dimH", dim, "256 256 256",
                    "Dimensions of histogram volume; number of samples along "
                    "each axis");
  NrrdKernelSpec *ksp00, *ksp11, *ksp22;
  hestOptAdd_1_Other(&hopt, "k00", "kernel", &ksp00, "tent",
                     "value reconstruction kernel", nrrdHestKernelSpec);
  hestOptAdd_1_Other(&hopt, "k11", "kernel", &ksp11, "cubicd:1,0",
                     "first derivative kernel", nrrdHestKernelSpec);
  hestOptAdd_1_Other(&hopt, "k22", "kernel", &ksp22, "cubicdd:1,0",
                     "second derivative kernel", nrrdHestKernelSpec);
  int lapl;
  hestOptAdd_Flag(&hopt, "l", &lapl,
                  "Use Laplacian instead of Hessian to approximate second "
                  "directional derivative.  No faster, less accurate.");
  int slow;
  hestOptAdd_Flag(&hopt, "slow", &slow,
                  "Instead of allocating a floating point VGH volume and measuring "
                  "V,G,H once, measure V,G,H multiple times on separate passes "
                  "(slower, but needs less memory)");
  int gz = AIR_FALSE;
  if (nrrdEncodingGzip->available()) {
    hestOptAdd_Flag(&hopt, "gz", &gz,
                    "Use gzip compression for output histo-volume; "
                    "much less disk space, slightly slower to read/write");
  }
  Nrrd *nin;
  hestOptAdd_1_Other(&hopt, "i", "volumeIn", &nin, NULL,
                     "input scalar volume for which a transfer function is needed",
                     nrrdHestNrrd);
  char *out;
  hestOptAdd_1_String(&hopt, "o", "hvolOut", &out, NULL,
                      "output histogram volume, used by \"gkms scat\" and "
                      "\"gkms info\"");

  hestParseOrExit(hopt, argc, argv, hparm, me, hvolInfoL);
  airArray *mop = airMopNew();
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  NrrdIoState *nio = nrrdIoStateNew();
  airMopAdd(mop, nio, nrrdIoStateNix_mop, airMopAlways);
  baneHVolParm *hvp = baneHVolParmNew();
  airMopAdd(mop, hvp, baneHVolParmNix_mop, airMopAlways);
  baneHVolParmGKMSInit(hvp);
  hvp->makeMeasrVol = !slow;

  // fprintf(stderr, "!%s: need to be using baneHVolParmAxisSet\n", me);
  /*
  hvp->axis[0].res = dim[perm[0]];
  hvp->axis[1].res = dim[perm[1]];
  hvp->axis[2].res = dim[perm[2]];
  hvp->axis[1].measr = lapl ? baneMeasrLapl : baneMeasrHess;
  for (i=0; i<=2; i++) {
    hvp->ax[i].inc = baneIncArray[(int)inc[(1+BANE_INC_PARM_NUM)*perm[i]]];
    for (j=0; j<BANE_INC_PARM_NUM; j++) {
      hvp->ax[i].incParm[j] = inc[1 + j + (1+BANE_INC_PARM_NUM)*perm[i]];
    }
  }
  */
  hvp->k3pack = AIR_TRUE;
  nrrdKernelParmSet(&hvp->k[gageKernel00], hvp->kparm[gageKernel00], ksp00);
  nrrdKernelParmSet(&hvp->k[gageKernel11], hvp->kparm[gageKernel11], ksp11);
  nrrdKernelParmSet(&hvp->k[gageKernel22], hvp->kparm[gageKernel22], ksp22);
  if (baneMakeHVol(nout, nin, hvp)) {
    biffAddf(BANE, "%s: trouble making histogram volume", me);
    airMopError(mop);
    return 1;
  }

  nio->encoding = gz ? nrrdEncodingGzip : nrrdEncodingRaw;
  if (nrrdSave(out, nout, nio)) {
    biffMovef(BANE, NRRD, "%s: error saving histogram volume", me);
    airMopError(mop);
    return 1;
  }
  airMopOkay(mop);
  return 0;
}
const unrrduCmd baneGkms_hvolCmd = {"hvol", HVOL_INFO, hvolMain, AIR_FALSE};
