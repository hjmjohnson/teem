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

/*
hestParseOrDie()

This is a convenience wrapper around other hest functions, striving to be the most
convenient one-stop top-level function for trying parse a command-line with hest.  It
implements the most common usage of hest:
- make sure hestOpt is valid with hestOptCheck(). Then:
- if argc is 0 (and !hparm->noArgsIsNoProblem): show usage info and exit(0). Else:
- if given only "--version" or "--help", act accordingly and exit(0). Else:
- try hestParse(). If it fails, show error message, and usage, and exit(1).
- Else if parsing succeeded, then just return()

NOTE: THIS IS A WEIRD FUNCTION because it calls exit() !
Hence the "OrDie" in the name.  Also, in that case it cleans up the given hestOpt and
hestParm, which is why they aren't `const`.

For TeemV2 this was simplified to clear up accumulated needlessly complicated logic:
- This used to have 3 final args doInfo, doUsage, doGlossary. EVERY Teem use of this
  function passed these as AIR_TRUE, so there was apparently no great loss in
  doing away with those 3 args
- The hestParm->dieLessVerbose option was removed. That was a response to info,
  usage, and glossary being printed every time there was a parse error (perhaps
  because doInfo == doUsage == doGlossary == AIR_TRUE), but we can also just do
  the expected thing: in case of parse error (or empty argv implicit ask for help)
  show the usage (but not info or glossary), and also print a message about
  using --help for more info.

For TeemV2, GLK also also decided to turn on newer features that aren't on by default
(except if hparm->dieWithoutNewerFeatures):
- hparm->respectDashDashHelp
- use hestParmColumnsIoctl to set hparm->columns
To be consistent, we do this both when the given hestParm *_hparm is NULL (when enabling
these newer features is less unsurprising) AND when the given _hparm is non-NULL (when
enabling them is more surprising). But because it is surprising, we do respect the
setting of the new dieWithoutNewerFeatures. */
void
hestParseOrDie(hestOpt *hopt, int argc, const char **argv, hestParm *_hparm,
               const char *me, const char *info) {
  int argcBereft = AIR_FALSE, parseErr = 0, wantHelp = AIR_FALSE;
  char *errS = NULL;

  if (!(hopt && argv)) {
    /* nothing to do given NULL pointers (_hparm, me, info are all allowed to be NULL).
    When first written, this function originally did nothing except silently return here,
    but with 2025 re-write decided to do die with an error message here. */
    fprintf(stderr, "%s: got NULL pointer (hopt %p, argv %p); can't proceed\n", __func__,
            AIR_VOIDP(hopt), AIR_VOIDP(argv));
    exit(1);
  }
  if (hestOptCheck(hopt, &errS)) {
    fprintf(stderr, "ERROR in hestOpt setup:\n%s\n", errS);
    free(errS);
    /* exit, not return, since there's practically no recovery possible: hestOpts are
    effectively set up at compile time, even with the ubiquity of hestOptAdd and related
    functions. The caller is, in all likelihood, not going to be in a position to
    overcome (at runtime) the errors just detected. */
    hestParmFree(_hparm);
    hestOptFree(hopt);
    exit(1);
  }

  /* Pre-June 2023 checks for solo --version or first-arg --help were done only after
     hestParse error; why not check first? */
  if (1 == argc && !strcmp(argv[0], "--version")) {
    // print version info and bail
    char vbuff[AIR_STRLEN_LARGE + 1];
    airTeemVersionSprint(vbuff);
    printf("%s\n", vbuff);
    hestParmFree(_hparm); // NULL _hparm is ok
    hestOptFree(hopt);
    exit(0);
  }

  /* (maybe) enable the new features here; for simplicity we always create and use a
     local hparm struct from here on out (so have to hestParmFree it ourselves). */
  hestParm *hparm = hestParmNew();
  if (_hparm) {
    // start by copying everything; easy with nothing internal dynamically allocated
    memcpy(hparm, _hparm, sizeof(hestParm));
  }
  // turn on new features unless told not to
  if (!(hparm->dieWithoutNewerFeatures)) {
    hestParmColumnsIoctl(hparm, hestDefaultColumns);
    hparm->respectDashDashHelp = AIR_TRUE;
  }
  if (argc && !strcmp(argv[0], "--help")) {
    /* actually, not an error, --help was the first argument. Respecting this actually
       pre-dates parm->respectDashDashHelp and does not depend on it. */
    // (keep argcBereft and parseError as initialized above)
    wantHelp = AIR_TRUE;
  } else {
    if (argc || hparm->noArgsIsNoProblem) {
      // we call hestParse if there are args, or (else) having no args is ok
      argcBereft = AIR_FALSE;
      parseErr = hestParse(hopt, argc, argv, &errS, hparm);
      wantHelp = hopt->helpWanted;
      // wantHelp and parseErr should not happen at the same time
      assert(!(wantHelp && parseErr));
      // at most one of wantHelp and parseErr is true (or neither could be true)
    } else {
      // the empty command-line is an implicit call for help
      argcBereft = AIR_TRUE;
      parseErr = 0;
      /* subtle difference between argcBereft and wantHelp is for maintaining
         functionality of pre-June 2023 code */
      wantHelp = AIR_FALSE;
    }
  }
  if (!argcBereft && !wantHelp && !parseErr) {
    /* no help needed or wanted, and (if done) parsing was successful, great;
       clean up and return to caller */
    hestParmFree(hparm);
    return;
  }

  /* whether by argcBereft or wantHelp or parseErr,
     we are NOT returning to caller */
#define STDWUT (parseErr ? stderr : stdout)
#define ME     (me ? me : "")
  if (parseErr) {
    // the err string created by hestParse already ends with \n
    fprintf(stderr, "ERROR: %s", errS ? errS : "(sorry no error message known)");
    airFree(errS);
    // but no return or exit; there's more to do
  } else if (wantHelp) {
    if (info) hestInfo(stdout, ME, info, hparm);
  }
  hestUsage(STDWUT, hopt, ME, hparm);
  if (wantHelp) {
    hestGlossary(STDWUT, hopt, hparm);
  } else {
    fprintf(STDWUT, "For more info: %s --help\n", me);
  }
#undef STDWUT
#undef ME
  hestParmFree(hparm);
  hestParmFree(_hparm); // NULL _hparm ok
  hestOptFree(hopt);
  exit(!!parseErr);
}
