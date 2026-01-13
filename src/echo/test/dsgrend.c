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

#include "../echo.h"

const char *dsgdirInfo = ("renders a glyph to show one diffusion sensitizing gradient"
                          "(maybe belongs in ten/tend but is so special-purpose)");

int
main(int argc, const char *argv[]) {
  const char *me = argv[0];
  hestOpt *hopt = NULL;
  float g[3];
  hestOptAdd_3_Float(&hopt, "g", "x y z", g, NULL,
                     "the diffusion sensitizing gradient to show (will be normalized)");
  float fr[3];
  hestOptAdd_3_Float(&hopt, "fr", "x y z", fr, NULL,
                     "the look-from point (look-at is origin); also the "
                     "source of the (head) light");
  float up[3];
  hestOptAdd_3_Float(&hopt, "up", "x y z", up, NULL, "the pseudo-up vector");
  float rgb[3];
  hestOptAdd_3_Float(&hopt, "c", "r g b", rgb, "1 1 1", "object color");
  unsigned int isz;
  hestOptAdd_1_UInt(&hopt, "sz", "size", &isz, "300", "size of output image");
  unsigned int numsamp;
  hestOptAdd_1_UInt(&hopt, "ns", "#samples", &numsamp, "1",
                    "number of jittered samples (or 0 or 1 for no jittering)");
  char *outS;
  hestOptAdd_1_String(&hopt, "o", "fname", &outS, "rend.nrrd",
                      "output filename for rendering");
  hestParseOrExit(hopt, argc - 1, argv + 1, NULL, me, dsgdirInfo);
  airArray *mop = airMopNew();
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);
  char *err;

  {
    float gtmp[3], glen;
    ELL_3V_NORM(gtmp, g, glen);
    if (!glen) {
      fprintf(stderr, "%s: need non-zero dsg vector (not %g %g %g)\n", me, g[0], g[1],
              g[2]);
      airMopError(mop);
      return 1;
    }
    ELL_3V_COPY(g, gtmp);
  }

  limnCamera *cam = limnCameraNew();
  airMopAdd(mop, cam, limnCameraNix_mop, airMopAlways);
  ELL_3V_COPY(cam->from, fr);
  ELL_3V_SET(cam->at, 0, 0, 0);
  ELL_3V_COPY(cam->up, up);
  ELL_2V_SET(cam->uRange, -1, 1);
  ELL_2V_SET(cam->vRange, -1, 1);
  cam->aspect = 1;
  // (not setting fov)
  cam->neer = -1;
  cam->faar = 1;
  cam->dist = 0;
  cam->atRelative = AIR_TRUE;
  cam->orthographic = AIR_TRUE; // WTF why is this not working ...
  cam->rightHanded = AIR_TRUE;

  echoRTParm *parm = echoRTParmNew();
  airMopAdd(mop, parm, echoRTParmNix_mop, airMopAlways);
  if (numsamp > 1) {
    parm->jitterType = echoJitterJitter;
    parm->numSamples = numsamp;
  } else {
    parm->jitterType = echoJitterNone;
    parm->numSamples = 1;
  }
  parm->imgResU = isz;
  parm->imgResV = isz;
  parm->aperture = 0.0;
  parm->textureNN = AIR_FALSE;
  parm->renderLights = AIR_TRUE;
  parm->renderBoxes = AIR_FALSE;
  parm->seedRand = AIR_FALSE; // to turn off time-based rng seeding
  parm->maxRecDepth = 10;
  ELL_3V_SET(parm->maxRecCol, 0, 0, 0);
  parm->shadow = 0.0;

  echoGlobalState *state = echoGlobalStateNew();
  airMopAdd(mop, state, echoGlobalStateNix_mop, airMopAlways);

  echoScene *scene = echoSceneNew();
  airMopAdd(mop, scene, echoSceneNix_mop, airMopAlways);
  ELL_3V_SET(scene->ambi, 1, 1, 1);
  ELL_3V_SET(scene->bkgr, 0.2, 0.4, 0.8);

  /*
  echoObject *sphere = echoObjectNew(scene, echoTypeSphere);
  echoColorSet(sphere, 1, rgb[0], rgb[1], rgb[2]);
  echoMatterPhongSet(scene, sphere, 0.2, 0.8, 0, 1);
  echoObjectAdd(scene, sphere);
  */

  float adsp[4] = {0.2, 0.8, 0, 1};
  echoObject *cyl = echoObjectNew(scene, echoTypeCylinder);
  echoCylinderSet(cyl, 0);
  echoColorSet(cyl, 1, rgb[0], rgb[1], rgb[2]);
  echoMatterPhongSet(scene, cyl, adsp[0], adsp[1], adsp[2], adsp[3]);

  echoObject *inst = echoObjectNew(scene, echoTypeInstance);
  float A[16];
  ELL_4M_SCALE_SET(A, 0.8, 0.15, 0.15);
  echoInstanceSet(inst, A, cyl);
  echoObjectAdd(scene, inst);

  echoObject *rect = echoObjectNew(scene, echoTypeRectangle);
  {
    // GLK unsure about u,v,n names here
    float nn[3], uu[3], vv[3], dist;
    ELL_3V_NORM(nn, fr, dist);
    float lsz = dist * 0.7; /* scaling view distance to get light size (edge length of
                               square area light) */
    ELL_3V_CROSS(uu, up, nn);
    ELL_3V_CROSS(vv, nn, uu);
    ELL_3V_SCALE(uu, lsz, uu); // scaling from unit length
    ELL_3V_SCALE(vv, lsz, vv); // scaling from unit length
    float oo[3];
    ELL_3V_SCALE_ADD3(oo, 1, fr, -0.5, uu, -0.5, vv);
    echoRectangleSet(rect,                //
                     oo[0], oo[1], oo[2], //
                     uu[0], uu[1], uu[2], //
                     vv[0], vv[1], vv[2]);
  }
  echoColorSet(rect, 1, 1, 1, 1);
  echoMatterLightSet(scene, rect, 1 /* power */, 0 /* unit */);

  Nrrd *nraw = nrrdNew();
  airMopAdd(mop, nraw, nrrdNuke_mop, airMopAlways);

  if (echoRTRender(nraw, cam, scene, parm, state)) {
    airMopAdd(mop, err = biffGetDone(ECHO), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble rendering:\n%s\n", me, err);
    airMopError(mop);
    return 1;
  }
  printf("render time = %g seconds (%g fps)\n", state->time, 1.0 / state->time);
  if (nrrdSave(outS, nraw, NULL)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble saving :\n%s\n", me, err);
    airMopError(mop);
    return 1;
  }

  airMopOkay(mop);
  exit(0);
}
