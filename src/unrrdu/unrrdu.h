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

#ifndef UNRRDU_HAS_BEEN_INCLUDED
#define UNRRDU_HAS_BEEN_INCLUDED

#include <teem/air.h>
#include <teem/biff.h>
#include <teem/hest.h>
#include <teem/nrrd.h>
/* moss.h is included where it is needed, in ilk.c */

#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(TEEM_STATIC)
#  if defined(TEEM_BUILD) || defined(unrrdu_EXPORTS) || defined(teem_EXPORTS)
#    define UNRRDU_EXPORT extern __declspec(dllexport)
#  else
#    define UNRRDU_EXPORT extern __declspec(dllimport)
#  endif
#else /* TEEM_STATIC || UNIX */
#  if !defined(TEEM_STATIC)
/*   If built w/ -fvisibility=hidden, only EXPORTed symbols are visible in shared lib */
#    define UNRRDU_EXPORT extern __attribute__((visibility("default")))
#  else
#    define UNRRDU_EXPORT extern
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define UNRRDU unrrduBiffKey

/*
NOTE: unrrdu started as the place to put the implementation of the "unu" command:
all the glue between the command-line and the nrrd library calls.
With time, that functionality, especially having (sub)comamnds described by a
unrrduCmd structs in an array, was leveraged by other programs (like "tend"),
and then others (like limn/test/lpu, and the many programs for GLK's SciVis class
at UChicago). Still, the "unrrdu" name remains even though it has outgrown
its specificity to "unu" (which is short for Utah Nrrd Utilities).
Efforts are made to mark the unu-specific elements below as such.
*/

/*
UNRRDU_QUIET_QUIT_ENV: the name of environment variable to set to enable unu to "quiet
quit": if the command fails to run because upon trying to read its input it immediately
hit EOF, then maybe don't complain about that to the terminal and generate lots of
unwanted usage info.  In settings where many unu commands are piped together, this is
important for leaving the initial important error message legible in the terminal, and
quietly eliding the uninformative stuff.

For more context:
- hest/hest.h:hestParm struct includes a quietQuit function pointer
- hest/parseHest.c:hestParseOrExit() calls the quietQuit function upon parse failure
- unrrdu/env.c (implements "unu env") includes info about how this envvar is set
- unrrdu/flotsam.c:unrrduQuietQuitTest() is used in . . .
- bin/unu.c as the callback to set in hparm->quietQuit for unu,
  which getenv()s this envvar, and then (among other considerations)
  tests for whether hopt->parseErr is 2, a special value set by . . .
- nrrd/hestNrrd.c:parserBoth(), the parse-Nrrd-from-filename-string function for both
  nrrdHestNrrd and nrrdHestNrrdNoTTY, returns 2 for the EOF error and 1 otherwise.
  nrrd/hestNrrd.c's nrrdHestIter uses the same test to similarly return 2.
  But this is (currently) a hack that merely looks for string _nrrdReadHitEOF
  (private to nrrd) in the biff error message set by . . .
- nrrd/read.c:_nrrdRead()
Note that some unu commands do not use nrrdHestIter, nrrdHestNrrd, or nrrdHestNrrdNoTTY
to get a Nrrd out of a string from the command-line (e.g. unu minmax, unu describe,
unu cksum, maybe others); these handle themselves the nrrdLaod()ing from a string,
and do not (currently) support quiet quitting.

This functionality was created (in a completely undocumented and even more fragile way)
when the unrrdu library was entirely unu-specific, but now "UNRRDU_QUIET_QUIT" appears in
so many shell scripts that the name remains (rather then being something more logically
unu-specific like like UNU_QUIET_QUIT_ENV or UNRRDU_UNU_QUIET_QUIT_ENV)
*/
#define UNRRDU_QUIET_QUIT_ENV "UNRRDU_QUIET_QUIT"

/*
 ******** unrrduCmd
 *
 * How we associate the one word for a command `name`,
 * the one-line `info` string, and
 * the single `main` function to call that which implements the command.
 */
typedef struct {
  const char *name, // command name; e.g. "slice" for "unu slice"
    *info;          // short (one-line) command description for command listing
  /* NOTE: argc,argv here are NOT the same as what real top-level `main()` got:
     these describe the command-line arguments AFTER the initial argument
     (i.e. original argv[0]) or arguments (e.g. "unu slice") that worked
     to select this particular unurrduCmd.  The `me` arg is one string
     to represent those args, such as "vprobe" or "unu slice". */
  int (*mainFunc)(int argc, const char **argv, const char *me, const hestParm *hparm);
  int hidden; // whether to hide this from default command listing
} unrrduCmd;

/*
** (unu-specific) used with unrrduHestScaleCB to identify for "unu resample -s"
** whether to resample along an axis, and if so, how the number of
** samples on an axis should be scaled (though now its more general
** than just scaling) */
enum {
  unrrduScaleUnknown,       /* 0: */
  unrrduScaleNothing,       /* 1: "=" */
  unrrduScaleMultiply,      /* 2: e.g. "x2" */
  unrrduScaleDivide,        /* 3: e.g. "/2" */
  unrrduScaleAdd,           /* 4: e.g. "+2" */
  unrrduScaleSubtract,      /* 5: e.g. "-2" */
  unrrduScaleAspectRatio,   /* 6: "a" */
  unrrduScaleExact,         /* 7: e.g. "128" */
  unrrduScaleSpacingTarget, /* 8: e.g. "s0.89" */
  unrrduScaleLast
};

// flotsam.c
UNRRDU_EXPORT const int unrrduPresent;
UNRRDU_EXPORT const char *const unrrduBiffKey;
UNRRDU_EXPORT int unrrdQuietQuitTest(const char *parseErrStr, const hestOpt *errOpt);
// these globals are unu-specific
UNRRDU_EXPORT int unrrduDefaultImplicitLutRmapRescale;
UNRRDU_EXPORT const char *const unrrduEnvVarDefaultImplicitLutRmapRescale;
UNRRDU_EXPORT void unrrduGetenvDefaultImplicitLutRmapRescale(void);
UNRRDU_EXPORT const hestCB unrrduHestPosCB;
UNRRDU_EXPORT const hestCB unrrduHestMaybeTypeCB;
UNRRDU_EXPORT const hestCB unrrduHestScaleCB;
UNRRDU_EXPORT const hestCB unrrduHestBitsCB;
UNRRDU_EXPORT const hestCB unrrduHestFileCB;
UNRRDU_EXPORT const hestCB unrrduHestEncodingCB;
UNRRDU_EXPORT const hestCB unrrduHestFormatCB;
/* unrrduUnuCmdList[] gives addresses of unrrdu_fooCmd for all commands "unu foo".
   The list of all commands is maintained in privateUnrrdu.h */
UNRRDU_EXPORT const unrrduCmd *const unrrduUnuCmdList[];
/* these next three functions can be used by command-line tools like unu */
UNRRDU_EXPORT airArray *unrrduMopNew(hestOpt *hopt, hestParm *hparm);
UNRRDU_EXPORT int unrrduUsage(const char *argv0, const char *prog, const char *title,
                              const unrrduCmd *const *cmdList, const hestParm *hparm);
UNRRDU_EXPORT int unrrduMainOrExit(int argc, const char **argv, const char *cmd,
                                   const char *title, const unrrduCmd *const *cmdList,
                                   const hestParm *hparm);
UNRRDU_EXPORT int unrrduMainBiffOrExit(int argc, const char **argv, const char *prog,
                                       const char *title,
                                       const unrrduCmd *const *cmdList,
                                       const hestParm *_hparm, const char *biffKey);

#ifdef __cplusplus
}
#endif

#endif /* UNRRDU_HAS_BEEN_INCLUDED */
