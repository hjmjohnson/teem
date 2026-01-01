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

#ifndef BIFF_HAS_BEEN_INCLUDED
#define BIFF_HAS_BEEN_INCLUDED

/* ---- BEGIN non-NrrdIO */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <teem/air.h>

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(TEEM_STATIC)
#  if defined(TEEM_BUILD) || defined(biff_EXPORTS) || defined(Teem_EXPORTS)
#    define BIFF_EXPORT extern __declspec(dllexport)
#  else
#    define BIFF_EXPORT extern __declspec(dllimport)
#  endif
#else /* TEEM_STATIC || UNIX */
#  if !defined(TEEM_STATIC)
/*   If built w/ -fvisibility=hidden, only EXPORTed symbols are visible in shared lib */
#    define BIFF_EXPORT extern __attribute__((visibility("default")))
#  else
#    define BIFF_EXPORT extern
#  endif
#endif
/* ---- END non-NrrdIO */

#ifdef __cplusplus
extern "C" {
#endif

/* biffbiff.c */
/* ---- BEGIN non-NrrdIO */
BIFF_EXPORT const int biffPresent;
/* ---- END non-NrrdIO */
BIFF_EXPORT void biffAdd(const char *key, const char *err);
BIFF_EXPORT void biffAddf(const char *key, const char *errfmt, ...)
#ifdef __GNUC__
  __attribute__((format(printf, 2, 3)))
#endif
  ;
BIFF_EXPORT void biffMaybeAdd(const char *key, const char *err, int useBiff);
BIFF_EXPORT void biffMaybeAddf(int useBiff, const char *key, const char *errfmt, ...)
#ifdef __GNUC__
  __attribute__((format(printf, 3, 4)))
#endif
  ;
BIFF_EXPORT char *biffGet(const char *key);
/* ---- BEGIN non-NrrdIO */
BIFF_EXPORT unsigned int biffCheck(const char *key);
BIFF_EXPORT void biffMove(const char *destKey, const char *err, const char *srcKey);
BIFF_EXPORT void biffMovef(const char *destKey, const char *srcKey, const char *errfmt,
                           ...)
#ifdef __GNUC__
  __attribute__((format(printf, 3, 4)))
#endif
  ;
/* ---- END non-NrrdIO */
BIFF_EXPORT void biffDone(const char *key);
BIFF_EXPORT char *biffGetDone(const char *key);

#ifdef __cplusplus
}
#endif

#endif /* BIFF_HAS_BEEN_INCLUDED */
