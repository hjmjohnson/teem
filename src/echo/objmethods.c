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

#include "echo.h"
#include "privateEcho.h"

#define NEW_TMPL(TYPE, BODY)                                                            \
  static echo##TYPE *TYPE##_new(void) {                                                 \
    echo##TYPE *obj;                                                                    \
                                                                                        \
    obj = (echo##TYPE *)calloc(1, sizeof(echo##TYPE));                                  \
    obj->type = echoType##TYPE;                                                         \
    do {                                                                                \
      BODY                                                                              \
    } while (0);                                                                        \
    return obj;                                                                         \
  }

#define NIX_TMPL(TYPE, BODY)                                                            \
  static echo##TYPE *TYPE##_nix(echo##TYPE *obj) {                                      \
    int dummy = 0;                                                                      \
                                                                                        \
    if (obj) {                                                                          \
      do {                                                                              \
        BODY dummy = dummy;                                                             \
      } while (0);                                                                      \
      airFree(obj);                                                                     \
    }                                                                                   \
    return NULL;                                                                        \
  }

static void
MatterInit(echoObject *obj) {

  obj->matter = echoMatterUnknown;
  ELL_4V_SET(obj->rgba, 0, 0, 0, 0);
  memset(obj->mat, 0, ECHO_MATTER_PARM_NUM * sizeof(echoCol_t));
  obj->ntext = NULL;
}

NEW_TMPL(Sphere, MatterInit(OBJECT(obj)); ELL_3V_SET(obj->pos, 0, 0, 0); obj->rad = 1.0;)

NEW_TMPL(Cylinder, MatterInit(OBJECT(obj)); obj->axis = 2;)

NEW_TMPL(Cone, MatterInit(OBJECT(obj));)

NEW_TMPL(Superquad, MatterInit(OBJECT(obj)); obj->axis = 2; obj->A = obj->B = 1;)

NEW_TMPL(Cube, MatterInit(OBJECT(obj));)

NEW_TMPL(Triangle, MatterInit(OBJECT(obj)); ELL_3V_SET(obj->vert[0], 0, 0, 0);
         ELL_3V_SET(obj->vert[1], 0, 0, 0); ELL_3V_SET(obj->vert[2], 0, 0, 0);)

NEW_TMPL(Rectangle, MatterInit(OBJECT(obj)); ELL_3V_SET(obj->origin, 0, 0, 0);
         ELL_3V_SET(obj->edge0, 0, 0, 0); ELL_3V_SET(obj->edge1, 0, 0, 0);)

NEW_TMPL(TriMesh, MatterInit(OBJECT(obj)); ELL_3V_SET(obj->meanvert, 0, 0, 0);
         ELL_3V_SET(obj->min, ECHO_POS_MAX, ECHO_POS_MAX, ECHO_POS_MAX);
         ELL_3V_SET(obj->max, ECHO_POS_MIN, ECHO_POS_MIN, ECHO_POS_MIN);
         obj->numV = obj->numF = 0; obj->pos = NULL; obj->vert = NULL;)
NIX_TMPL(TriMesh, obj->pos = (echoPos_t *)airFree(obj->pos);
         obj->vert = (int *)airFree(obj->vert);)

NEW_TMPL(Isosurface, MatterInit(OBJECT(obj)); obj->volume = NULL; obj->value = 0.0;
         /* ??? */
)

NEW_TMPL(AABBox, obj->obj = NULL;
         ELL_3V_SET(obj->min, ECHO_POS_MAX, ECHO_POS_MAX, ECHO_POS_MAX);
         ELL_3V_SET(obj->max, ECHO_POS_MIN, ECHO_POS_MIN, ECHO_POS_MIN);)

NEW_TMPL(Split, obj->axis = -1;
         ELL_3V_SET(obj->min0, ECHO_POS_MAX, ECHO_POS_MAX, ECHO_POS_MAX);
         ELL_3V_SET(obj->max0, ECHO_POS_MIN, ECHO_POS_MIN, ECHO_POS_MIN);
         ELL_3V_SET(obj->min1, ECHO_POS_MAX, ECHO_POS_MAX, ECHO_POS_MAX);
         ELL_3V_SET(obj->max1, ECHO_POS_MIN, ECHO_POS_MIN, ECHO_POS_MIN);
         obj->obj0 = obj->obj1 = NULL;)

NEW_TMPL(List, echoPtrPtrUnion eppu; obj->obj = NULL;
         obj->objArr = airArrayNew((eppu.obj = &(obj->obj), eppu.v), NULL,
                                   sizeof(echoObject *), ECHO_LIST_OBJECT_INCR);)
NIX_TMPL(List, airArrayNuke(obj->objArr);)

NEW_TMPL(Instance, ELL_4M_IDENTITY_SET(obj->M); ELL_4M_IDENTITY_SET(obj->Mi);
         obj->obj = NULL;)

static echoObject *(*ObjectNew[ECHO_TYPE_NUM])(void)
  = {(echoObject * (*)(void)) Sphere_new,     (echoObject * (*)(void)) Cylinder_new,
     (echoObject * (*)(void)) Cone_new,       (echoObject * (*)(void)) Superquad_new,
     (echoObject * (*)(void)) Cube_new,       (echoObject * (*)(void)) Triangle_new,
     (echoObject * (*)(void)) Rectangle_new,  (echoObject * (*)(void)) TriMesh_new,
     (echoObject * (*)(void)) Isosurface_new, (echoObject * (*)(void)) AABBox_new,
     (echoObject * (*)(void)) Split_new,      (echoObject * (*)(void)) List_new,
     (echoObject * (*)(void)) Instance_new};

echoObject * /* Biff: nope */
echoObjectNew(echoScene *scene, signed char type) {
  echoObject *ret = NULL;
  int idx;

  if (scene && AIR_IN_OP(echoTypeUnknown, type, echoTypeLast)) {
    ret = ObjectNew[type]();
    idx = airArrayLenIncr(scene->catArr, 1);
    scene->cat[idx] = ret;
  }
  return ret;
}

int /* Biff: nope */
echoObjectAdd(echoScene *scene, echoObject *obj) {
  int idx;

  if (scene && obj) {
    idx = airArrayLenIncr(scene->rendArr, 1);
    scene->rend[idx] = obj;
  }
  return 0;
}

static echoObject *(*ObjectNix[ECHO_TYPE_NUM])(echoObject *) = {
  (echoObject * (*)(echoObject *)) airFree,   /* echoTypeSphere */
  (echoObject *(*)(echoObject *))airFree,     /* echoTypeCylinder */
  (echoObject *(*)(echoObject *))airFree,     /* echoTypeCone */
  (echoObject *(*)(echoObject *))airFree,     /* echoTypeSuperquad */
  (echoObject *(*)(echoObject *))airFree,     /* echoTypeCube */
  (echoObject *(*)(echoObject *))airFree,     /* echoTypeTriangle */
  (echoObject *(*)(echoObject *))airFree,     /* echoTypeRectangle */
  (echoObject *(*)(echoObject *))TriMesh_nix, /* echoTypeTriMesh */
  (echoObject *(*)(echoObject *))airFree,     /* echoTypeIsosurface */
  (echoObject *(*)(echoObject *))airFree,     /* echoTypeAABBox */
  (echoObject *(*)(echoObject *))airFree,     /* echoTypeSplit */
  (echoObject *(*)(echoObject *))List_nix,    /* echoTypeList */
  (echoObject *(*)(echoObject *))airFree      /* echoTypeInstance */
};

echoObject * /* Biff: nope */
echoObjectNix(echoObject *obj) {

  if (obj) {
    ObjectNix[obj->type](obj);
  }
  return NULL;
}
