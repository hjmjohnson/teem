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

#include "hest.h"
#include "privateHest.h"

#include <string.h>

/* uint _hestErrStrlen(const hestOpt *opt, int argc, const char **argv) ...
 * This was a bad idea; removed for TeemV2. Now hest internally uses biff */

/*
 ******** hestParseFree()
 *
 * free()s whatever was allocated by hestParse()
 *
 * ignore-able quirk: returns given pointer, to facilitate use with the airMop functions
 */
hestOpt *
hestParseFree(hestOpt *opt) {
  if (opt) {
    uint optNum = opt->arrLen;
    for (uint opi = 0; opi < optNum; opi++) {
      airArrayLenSet(opt[opi].havec->hargArr,
                     0); // but not hestArgVecNix(opt[opi].havec);
      opt[opi].parmStr = airFree(opt[opi].parmStr);
      /* this gloriously replaces what used to be a lot of dense logic around
         opt[opi].alloc, opt[opi].type, and opt[opi].CB->destroy */
      if (opt[opi].parseMop) {
        opt[opi].parseMop = airMopOkay(opt[opi].parseMop);
      }
    }
  }
  return opt;
}
AIR_MOPPER(hestParseFree)

/*
hestParseOrExit()  (replaces pre-TeemV2 hestParseOrDie)

This is a wrapper around other hest functions, striving to be the most convenient
one-stop top-level function for trying parse a command-line with hest.  It implements the
most common usage of hest: either hestParse is needed and succeeds, or handle otherwise:
- validate hestOpt with hestOptCheck() and show error message if not. Else:
- if argc is 0 (and if !hparm->noArgsIsNoProblem): show usage info. Else:
- handle if argc,argv is only "--version" or "--help" or "--usage". Else:
- try hestParse(). If it fails, show error message, and usage info.
  If hestParse was not called or it failed, then exit; there's no point in
  returning to the caller because the caller wants values set by parsing.
- if hestParse succeeded, then just return 0

THIS IS A WEIRD FUNCTION because of how it can finish. As the name suggests, this can
call `exit` (either `exit(1)` after e.g. a hestParse failure, or `exit(0)` after e.g.
responding to `--version`). Also the simultaneous demands of being const-correct with
incoming parameter structs, while being clean about memory use (certainly no leaks, but
not even any memory in use at exit), has led to some new (for TeemV2) functions involving
the hestParm (e.g. functions hestParmMopNew, hestParmMopDone, hestParmUnmop).

hestParseOrExit took over pre-TeemV2 hestParseOrDie to clear up accumulated
needless complications:
- hestParseOrDie had 3 final args doInfo, doUsage, doGlossary. EVERY Teem use of it
  passed these as AIR_TRUE, so there was apparently no great loss in doing away them
- The hestParm->dieLessVerbose option was removed. That was a response to info,
  usage, and glossary being printed every time there was a parse error (probably
  because doInfo == doUsage == doGlossary == AIR_TRUE), but we can also just do
  the expected thing: in case of parse error (or empty argv implicit ask for help)
  show the usage (but not info or glossary), and also print a message about
  using --help for more info.

For TeemV2, GLK also also decided to turn on newer features that aren't on by default
(except if hparm->orExitWithoutNewerFeatures):
- hparm->respectDashDashHelp
- use hestParmColumnsIoctl to set hparm->columns.
To be consistent, we do this both when the given hestParm *_hparm is NULL (when
enabling these newer features is less unsurprising) AND when the given _hparm
is non-NULL (when enabling them may be more surprising). But because it is surprising,
we do respect the setting of the new orExitWithoutNewerFeatures. */
int                                     /* Biff: nope */
hestParseOrExit(hestOpt *hopt,          // what to parse from command-line
                int argc,               /* these argc,argv are *not* as seen by real
                                        top-level `main()`: here argc,argv describe
                                        the args *after* the whatever args got us to
                                        here: either argv[1] and onward for something
                                        like "vprobe", or argv[2] and onward for things
                                        like "unu slice" */
                const char **argv,      // (see above)
                const hestParm *_hparm, // either NULL or self-mopping
                const char *me,         // what am I, e.g. "vprobe" or "unu slice"
                const char *info        // description of purpose of program
) {
  if (!(hopt && argv && me)) { /* bad args */
    /* nothing to do given NULL pointers (_hparm, info are allowed to be NULL).
    hestParseOrDie originally did nothing except silently return here, but with
    2025 re-write decided print an error message here. */
    fprintf(stderr, "%s: got NULL pointer (hopt %p, argv %p, me %p); can't proceed\n",
            __func__, AIR_VOIDP(hopt), AIR_CVOIDP(argv), AIR_CVOIDP(me));
    exit(1); // (in hestParseOrExit)
  }
  char *optErrStr = NULL;
  if (hestOptCheck(hopt, &optErrStr)) { /* bad args */
    /* hestOpt array of options is malformed, cannot proceed */
    fprintf(stderr, "%s(%s): ERROR in given hestOpt setup:\n%s\n", __func__, me,
            optErrStr);
    free(optErrStr);
    exit(1); // (in hestParseOrExit)
  }

  // fprintf(stderr, "!%s: hello with _hparm %p\n", __func__, AIR_CVOIDP(_hparm));
  /* Pre-June 2023, checks for solo --version or first-arg --help were done only after
     hestParse error; why not check first? */
  if (1 == argc && !strcmp(argv[0], "--version")) {
    // print version info and bail
    hestParmMopDone(_hparm);
    hestOptFree(hopt);
    airTeemVersionPrintAndExit("Built with");
  }

  /* (maybe) enable the new features here; for simplicity we always create and use a
     local hparm struct from here on out (so have to hestParmFree it ourselves).
     If we got a self-mopping parm, the copy will also be self-mopping */
  hestParm *hparm = _hparm ? hestParmCopy(_hparm) : hestParmNew();
  // turn on new features unless told not to
  if (!(hparm->orExitWithoutNewerFeatures)) {
    hestParmColumnsIoctl(hparm, hestDefaultColumns);
    hparm->respectDashDashHelp = AIR_TRUE;
  }
  /* fprintf(stderr,
   *         "!%s: got _hparm %p (mop %p) have locally modified hparm %p (mop %p)\n",
   *         __func__, AIR_CVOIDP(_hparm), _hparm ? AIR_CVOIDP(_hparm->_selfMop) : NULL,
   *         AIR_VOIDP(hparm), hparm ? AIR_VOIDP(hparm->_selfMop) : NULL); */

  int argcBereft, // the smallness of argc is an implicit request for --usage
    parseErr,     // hestParse was called and parsing failed
    helpWanted,   // same as from hestParse: 1 if "--help", 2 if "--usage", else 0
    quietQuit;    // we are quiet qutting (a flag, not like hparm->quietQuit callback)
  char *parseErrStr = NULL;
  if (argc && !strcmp(argv[0], "--help")) {
    /* actually, not an error, --help was the first argument. Respecting this actually
       pre-dates parm->respectDashDashHelp and does not depend on it, but we have the
       same effect as if that was set */
    argcBereft = quietQuit = AIR_FALSE;
    parseErr = 0;
    hopt->helpWanted = helpWanted = 1;
  } else if (argc && !strcmp(argv[0], "--usage")) {
    /* similar */
    argcBereft = quietQuit = AIR_FALSE;
    parseErr = 0;
    hopt->helpWanted = helpWanted = 2;
  } else {
    quietQuit = AIR_FALSE;
    if (argc /* either we have args */
        || /* or we have no args and */ hparm->noArgsIsNoProblem) {
      argcBereft = AIR_FALSE;
      parseErr = hestParse(hopt, argc, argv, &parseErrStr, hparm);
      helpWanted = hopt->helpWanted; // should be either 1 (--help) or 2 (--usage)
      // at most one of helpWanted, parseErr should be non-zero, or neither (as in next)
      assert(!(helpWanted && parseErr));
      if (parseErr && hopt->parseErrOptIdx >= 0) {
        // hestParse's error is attributable to failure to parse a particular option
        hestOpt *errOpt = hopt + hopt->parseErrOptIdx;
        if (parseErr != errOpt->parseErr) {
          // hestParse didn't do its job: these should be consistent
          fprintf(stderr,
                  "%s(%s): ERROR in hestParse execution: "
                  "return %d != hopt[%d]->parseErr %d\n",
                  __func__, me, parseErr, hopt->parseErrOptIdx, errOpt->parseErr);
          // no cleanup
          exit(1); // (in hestParseOrExit)
        }
        // now, can run quiet quit tester
        if (hparm->quietQuit && hparm->quietQuit(parseErrStr, errOpt)) {
          // we are meeting the caller's conditions for quiet quitting
          quietQuit = AIR_TRUE;
        }
      }
    } else {
      // the empty command-line is an implicit call for help
      argcBereft = AIR_TRUE;
      parseErr = 0;
      /* subtle difference between argcBereft and helpWanted is for maintaining
         functionality of pre-June 2023 code */
      helpWanted = 0;
    }
  }
  /* fprintf(stderr, "!%s: argcBereft %d, helpWanted %d, parseErr %d\n", __func__,
   *         argcBereft, helpWanted, parseErr); */
  if (!argcBereft && !helpWanted && !parseErr) {
    /* no help needed or wanted, and (if done) parsing was successful, great;
       clean up local hparm (though hestParmFree is doing work to avoid also freeing the
       caller's parm we got as `_hparm1) and return to caller */
    // fprintf(stderr, "!%s: A about to free(%p)\n", __func__, AIR_VOIDP(hparm));
    /* if given _hparm is NULL, hestParmNew() made `hparm non-self-mopping,
          we can just hestParmFree(hparm) and that will essentially be free(hparm)
       if given _hparm is non-NULL:
          if _hparm is properly self-mopping, then so is the copy,
            so hestParmFree(hparm) would also clean up _hparm and the mop, NO.
            SO: have to ~undo our hestParmCopy so the caller can hestParmFree(_parm)
                hence call to hestParmUnmop, which is safe to use when not necessary
          if _hparm is NOT property self-mopping (=> caller is accepting in-use-at-exit),
            and then the copy `hparm` is also not self-mopping,
            and hestParmFree(hparm) will clean it up.
       In any case, then, we can pass NULL as 2nd arg to freeBeforeExit */
    hestParmUnmop(hparm);
    hestParmFree(hparm);
    return 0;
  }

  /* whether by argcBereft or helpWanted or parseErr,
     we are back in the "Else" territory; may NOT be returning to caller */
  int status;
  if (parseErr) {
    if (!quietQuit) {
      /* yes, the err string created by hestParse already ends with \n
         but we add a blank line to have better separation of final diagnostic
         error message line, and usage line */
      fprintf(stderr, "%s: error:\n%s\n", me,
              parseErrStr ? parseErrStr : "(sorry no error message known)");
      hestUsage(stderr, hopt, me, hparm, AIR_TRUE);
      fprintf(stderr, "For more info: %s --help\n", me);
    }
    airFree(parseErrStr);
    status = 1;
  } else {
    /* argcBereft or helpWanted (1 for --help, 2 for --usage)
       We treat argcBereft the same as --usage */
    int allHelp = !(argcBereft || helpWanted == 2);
    FILE *stdwut = argcBereft ? stderr : stdout;
    status = argcBereft ? 1 : 0;
    if (allHelp && info) {
      hestInfo(stdwut, me, info, hparm);
    }
    hestUsage(stdwut, hopt, me, hparm, !allHelp);
    if (allHelp) {
      hestGlossary(stdwut, hopt, hparm);
      fprintf(stdwut, "For just usage line: %s --usage\n", me);
    } else if (argcBereft) {
      fprintf(stdwut, "For more info: %s --help\n", me);
    }
  }
  /* if given _hparm is NULL, we have a non-self-mopping hparm,
        we can just hestParmFree(hparm) and that will essentially be free(hparm)
     if given _hparm is non-NULL:
        if _hparm is properly self-mopping, then so is the copy,
          and again hestParmFree(hparm) and that will clean up hparm, _hparm, and the mop
        if _hparm is NOT property self-mopping (then caller is accepting in-use-at-exit),
          and then the copy `hparm` is also not self-mopping,
          and hestParmFree(hparm) will clean it up. */
  /* fprintf(stderr, "!%s: B about to hestParmFree(%p) w/ mop %p\n", __func__,
   *         AIR_VOIDP(hparm), AIR_VOIDP(hparm->_selfMop)); */
  hestParmFree(hparm); // clean up local hparm (and maybe _hparm via self-mopping)
  hestOptFree(hopt);
  exit(status); // (in hestParseOrExit)
}
