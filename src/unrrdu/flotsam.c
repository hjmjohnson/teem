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

#include "unrrdu.h"
#include "privateUnrrdu.h"

#include <ctype.h>

const int unrrduPresent = 42;

const char *const unrrduBiffKey = "unrrdu";
/* how rescale was set in {lut,rmap,mlut,mrmap}.c when
   lut,rmap,mlut,mrmap domain is implicit */
int unrrduUnuDefaultImplicitLutRmapRescale = AIR_TRUE;
const char *const unrrduUnuEnvVarDefaultImplicitLutRmapRescale = MY_UNRRDU_DILRR;

// (new for TeemV2) how unu (and others, if they want) can test for quiet quitting
// see more info in unrrdu.h's description of UNRRDU_UNU_QUIET_QUIT_ENV
int
unrrduUnuQuietQuitTest(const char *parseErrStr, const hestOpt *errOpt) {
  int ret = 0;
  AIR_UNUSED(parseErrStr);
  /*
  fprintf(stderr, "!%s: hello from a qq tester\n", __func__);
  fprintf(stderr, "!%s: flag=|%s|\n", __func__, errOpt->flag);
  fprintf(stderr, "!%s: type=|%d|\n", __func__, errOpt->type);
  fprintf(stderr, "!%s: parseErr=|%d|\n", __func__, errOpt->parseErr);
  */
  if ((getenv(UNRRDU_UNU_QUIET_QUIT_ENV_OLD) /* quiet-quit testing enabled */
       || getenv(UNRRDU_UNU_QUIET_QUIT_ENV))
      && errOpt->type == airTypeOther        /* tried to parse an "other" */
      && (errOpt->CB == nrrdHestNrrd         /* */
          || errOpt->CB == nrrdHestNrrdNoTTY /* */
          || errOpt->CB == nrrdHestIter)     /* tried to parse a Nrrd */
      && 2 == errOpt->parseErr) { /* value set by nrrd/hestNrrd.c parser functions
                                     for those hestCBs above */
    ret = 1;
  }
  return ret;
}

// setter-from-env-far for unrrduUnuDefaultImplicitLutRmapRescale
void
unrrduUnuGetenvDefaultImplicitLutRmapRescale(void) {
  const char *envStr = NULL;
  if (AIR_FALSE
      == nrrdGetenvBool(&unrrduUnuDefaultImplicitLutRmapRescale, &envStr,
                        unrrduUnuEnvVarDefaultImplicitLutRmapRescale)) {
    fprintf(stderr,
            "WARNING: envvar \"%s\" set to \"%s\" which could not be parsed as "
            "a bool, so unrrduUnuDefaultImplicitLutRmapRescale=%d unchanged\n",
            unrrduUnuEnvVarDefaultImplicitLutRmapRescale, envStr ? envStr : "(null)",
            unrrduUnuDefaultImplicitLutRmapRescale);
  }
  return;
}

/*
 ******** unrrduUnuCmdList[]
 *
 * NULL-terminated array of unrrduCmd pointers, as ordered by MY_UNRRDU_MAP macro
 */
const unrrduCmd *const unrrduUnuCmdList[] = {MY_UNRRDU_MAP(MY_UNRRDU_LIST) NULL};

/*
 ******** unrrduMopNew
 * helper function probably useful mainly to unu command implementation, but not at all
 * specific to unu.
 *
 * To be call JUST AFTER hestParseOrExit, where we now have to take on the management of
 * anything dynamically allocated.
 *     if hparm is NULL: the unrrduCmd->mainFunc did NOT need to modify the incoming
 *                       `const hestParm *hparm` before passing to hestParseOrExit,
 *                       so there is nothing more to manage in the mop
 * if hparm is non-NULL: the unrrduCmd->mainFunc DID need to make local hestParm
 *                       modifications, so copied incoming `const hestParm *_hparm` via:
 *                       hestParm *hparm = hestParmCopy(_hparm)
 *                       and now has to manage the local hparm
 */
airArray * /* Biff: nope */
unrrduMopNew(hestOpt *hopt, hestParm *hparm) {
  airArray *mop = airMopNew();
  if (hparm) {
    hestParmUnmop(hparm); // in case we got a self-mopping _hparm
    airMopAdd(mop, hparm, hestParmFree_mop, airMopAlways);
  }
  airMopAdd(mop, hopt, hestOptFree_mop, airMopAlways);
  airMopAdd(mop, hopt, hestParseFree_mop, airMopAlways);
  return mop;
}

/*
 * (tweaked and tried-to-make-uniform for TeemV2)
 * variable names and terminology for some of the following:
 * `prog` or "program" = name of the command-line program that we're here to implement
 *                       e.g. "unu" or "lpu"
 * `cmd` or  "command" = a sub-command of `prog`, like the "slice" of "unu slice"
 *      `argc`, `argv` = within this file: the way the actual top-level `main()` function
 *                       received the command-line arguments.
 *                       BUT, unrrduCmd->mainFunc is called with argc-2,argv+2
 *                       so downstream of there, argc and argv describe all the
 *                       arguments AFTER the program command
 *             `argv0` = like it says, just argv[0] = what user typed (modulo aliases)
 *                       to get here, which probably ends with `prog`
 *                `me` = "`prog` `cmd`", the way a downstream "main" function
 *                       (a unrrduCmd->mainFunc) may self-identify e.g. "unu slice",
 *                       but confusingly *not* the current function name
 *                       i.e. what C99 calls __func__
 *           `cmdList` = an array of unrrduCmd structs, one per command
 * NOTE: This terminology may not have propogated to other unu-specific code;
 */

/* cmdListCheckLen:
 * helper function for unrrduUsage an unrrduMainOrExit
 * that checks cmdList and does other things involving cmdList
 * ensuring that you can safely sprintf "<prog> <cmd>" into str[AIR_STRLEN_SMALL+1]
 */
static int /* Biff: nope */
cmdListCheckLen(const char *prog, const unrrduCmd *const *cmdList, size_t *maxlenP,
                uint *aboutIdxP) {
  if (!(prog && cmdList)) {
    fprintf(stderr, "%s: got NULL prog (%p) or cmdList (%p)\n", __func__,
            AIR_CVOIDP(prog), AIR_CVOIDP(cmdList));
    return 1;
  }
  if (maxlenP) {
    // learn max length of a command-line, AND validate list of names
    *maxlenP = UINT_MAX;
    size_t maxlen = 0, minlen = UINT_MAX; // max,min length of command names
    size_t proglen = strlen(prog);
    for (uint ci = 0; cmdList[ci]; ci++) {
      size_t len = airStrlen(cmdList[ci]->name);
      minlen = AIR_MIN(minlen, len);
      maxlen = AIR_MAX(maxlen, len);
    }
    /* maxlen + proglen + 2 == max strlen("<prog> <cmd>") + 1 */
    if (!(minlen >= 1 && maxlen + proglen + 2 < AIR_STRLEN_SMALL)) {
      fprintf(stderr,
              "%s: min (%zu), max (%zu) strlen of %s command names seem not right\n",
              __func__, minlen, maxlen, prog);
      return 1;
    }
    for (uint ci = 0; cmdList[ci]; ci++) {
      for (uint cj = 0; cj < ci; cj++) {
        if (!strcmp(cmdList[ci]->name, cmdList[cj]->name)) {
          fprintf(stderr, "%s: %s commands [%u] and [%u] have same name: \"%s\"\n",
                  __func__, prog, ci, cj, cmdList[cj]->name);
          return 1;
        }
      }
    }
    *maxlenP = maxlen;
  }
  if (aboutIdxP) {
    *aboutIdxP = UINT_MAX;
    for (uint ci = 0; cmdList[ci]; ci++) {
      if (!strcmp("about", cmdList[ci]->name)) {
        *aboutIdxP = ci;
        break;
      }
    }
  }
  return 0;
}

/*
 ******** unrrduUsage
 *
 * For other programs (not unu) that are leveraging the unrrduCmd infrastructure: a
 * simple implementation of the top-level usage command that lists (on stdout) all the
 * other commands (so this is *not* for giving usage info specific to one command), or,
 * in case of bad args will fprintf(stderr) what went wrong and return 1.
 *
 * Why not use biff: because whatever gave rise to the error is something that very
 * likely set up at compile-time (as opposed to based on any user inputs), so there's no
 * run-time way to fix the error.
 */
int /* Biff: nope */
unrrduUsage(const char *prog, const char *title, const unrrduCmd *const *cmdList,
            const hestParm *hparm) {
  if (!(prog && title)) {
    fprintf(stderr, "%s: need non-NULL prog and title", __func__);
    return 1;
  }
  size_t maxlen;
  if (cmdListCheckLen(prog, cmdList, &maxlen, NULL)) {
    /* already fprintf(stderr) what went wrong */
    return 1;
  }
  char _buff[ASP1_L], fmt[ASP1_L];
  snprintf(_buff, sizeof(_buff), "--- %s: %s ---", prog, title);
  size_t len = strlen(_buff);
  uint columns = hparm ? hparm->columns : hestDefaultColumns;
  uint indent = hparm ? hparm->indent : hestDefaultIndent;
  size_t spleft = (columns > len ? columns - len : 0) / 2 + len - 1;
  snprintf(fmt, sizeof(fmt), "%%%zus\n", spleft);
  fprintf(stdout, fmt, _buff);
  for (uint ci = 0; cmdList[ci]; ci++) {
    char *buff = _buff;
    size_t buffSize = sizeof(_buff);
    len = strlen(cmdList[ci]->name);
    buff[0] = '\0';
    for (uint chi = len; chi < maxlen + indent; chi++) {
      SN_COPY(buff, buffSize, " ");
    }
    SN_PRINTF(buff, buffSize, "%s %s ... ", prog, cmdList[ci]->name);
    printf("%s", _buff);
    len = buff - _buff;
    hestPrintStr(stdout, len + indent, len, columns, cmdList[ci]->info,
                 AIR_FALSE /* bslash */);
  }
  printf("For more on using \"%s <cmd>\" listed above, run: %s <cmd> --help\n", prog,
         prog);
  return 0;
}

static int                   /* Biff: nope */
unu_Prime(int argc,          /* as received by main() */
          const char **argv, /* as received by main() */
          const char *prog,  /* conventional name of command-line program */
          const char *title, /* for top of listing sub-commands */
          const unrrduCmd *const *cmdList, /* all the sub-commands */
          const hestParm *_hparm,
          const char *biffKey) {
  if (!(argc >= 1 && argv && airStrlen(argv[0]) && airStrlen(prog) && airStrlen(title)
        && cmdList)) { // bad args (_hparm, biffKey allowed to be NULL)
    fprintf(stderr,
            "%s: got weird argc (%d) NULL argv (%p) (or empty argv[0] (%p)), "
            "prog (%p), title (%p), or cmdList (%p)",
            __func__, argc, AIR_CVOIDP(argv), AIR_CVOIDP(argv[0]), AIR_CVOIDP(prog),
            AIR_CVOIDP(title), AIR_CVOIDP(cmdList));
    // not calling hestParmMopDone(_hparm) because something is amiss
    exit(1); // (in unu_Prime)
  }
  int ret;
  if (1 == argc) {
    ret = unrrduUsage(prog, title, cmdList, _hparm);
    /* if `ret`: already fprintf(stderr) what went wrong */
    hestParmMopDone(_hparm);
    exit(ret); // (in unu_Prime)
  }
  // else argc >= 2
  if (2 == argc && !strcmp("--version", argv[1])) {
    char pfx[ASP1_L];
    snprintf(pfx, ASP1_L, "%s built with", prog);
    hestParmMopDone(_hparm);
    airTeemVersionPrintAndExit(pfx);
  }
  // handle "--usage" (but not yet "--help" or "about")
  uint aboutCmdIdx;
  if (cmdListCheckLen(prog, cmdList, NULL, &aboutCmdIdx)) {
    // already fprintf(stderr) what went wrong
    // not calling hestParmMopDone(_hparm) because something is amiss
    exit(1); // (in unu_Prime)
  }
  int aboutHave = (aboutCmdIdx != UINT_MAX);
  if (2 == argc && !strcmp("--usage", argv[1])) {
    printf("Usage: %s <command> [options ...]\n", prog);
    if (aboutHave) {
      printf("Run just \"%s\" for list of %s commands, or\n", argv[0], prog);
      printf("\"%s --help\" or \"%s about\" to learn more about %s.\n", argv[0], argv[0],
             prog);
    } else {
      printf("Run \"%s\" or \"%s --help\" for list of %s commands\n", argv[0], argv[0],
             prog);
    }
    hestParmMopDone(_hparm);
    exit(0); // (in unu_Prime)
  }
  if (2 == argc && !strcmp("--help", argv[1]) && !aboutHave) {
    // without an "about" command, --help is another way to ask for command list
    int uret = unrrduUsage(prog, title, cmdList, _hparm);
    hestParmMopDone(_hparm);
    exit(uret); // (in unu_Prime)
  }
  /* now:
     either we're run with "--help" which effectively equals "about",
     or, we're here to try running some other command */
  const unrrduCmd *torun = NULL;
  if (!strcmp("--help", argv[1]) && aboutHave) {
    torun = cmdList[aboutCmdIdx];
  } else {
    for (uint ci = 0; cmdList[ci]; ci++) {
      if (!strcmp(argv[1], cmdList[ci]->name)) {
        torun = cmdList[ci];
        break;
      }
    }
  }
  if (!torun) {
    fprintf(stderr, "%s: unknown %s command \"%s\"; run \"%s\" for complete list\n",
            argv[0], prog, argv[1], argv[0]);
    hestParmMopDone(_hparm);
    exit(1); // (in unu_Prime)
  }
  // now: we actually know we're going to run `torun`
  /* parse environment variables first, in case they break nrrdDefault*
     or nrrdState* variables in a way that nrrdSanity() should see */
  nrrdDefaultGetenv();
  nrrdStateGetenv();
  /* unu would do some unu-specific environment-variable handling here */
  nrrdSanityOrExit(argv[0]);

  hestParm *hparm = _hparm ? hestParmCopy(_hparm) : hestParmMopNew();
  if (!_hparm) {
    /* if nothing given, enstate some useful things */
    hparm->elideSingleEnumType = AIR_TRUE;
    hparm->elideSingleOtherType = AIR_TRUE;
    hparm->elideSingleOtherDefault = AIR_FALSE;
    hparm->elideSingleNonExistFloatDefault = AIR_TRUE;
    hparm->elideMultipleNonExistFloatDefault = AIR_TRUE;
    hparm->elideSingleEmptyStringDefault = AIR_TRUE;
    hparm->elideMultipleEmptyStringDefault = AIR_TRUE;
    hparm->cleverPluralizeOtherY = AIR_TRUE;
    /* columns aren't set here:
       either caller can to hestParmColumnsIoctl(), or it will
       be tried later if mainFunc calls hestParseOrExit */
  }
  // NOTE: this is a STATIC allocation of the string!
  char me[AIR_STRLEN_SMALL + 1]; // size sufficiency ensured by cmdListCheckLen
  snprintf(me, sizeof(me), "%s %s", prog, torun->name);
  // run the individual command, saving its exit status
  ret = torun->mainFunc(argc - 2, argv + 2, me, hparm);
  if (ret && biffKey) {
    /* mainFunc returned but there was an error, and because we got a biffKey,
       it's our job to get and display the error message before we return.
       Our caller has no biff responsibility (hence the Biff: nope annotation),
       but we were requested to use biff here. */
    char *err = biffGetDone(biffKey);
    if (err) {
      /* we emulate the output of hestParseOrExit()
         no final "\n" because biff included that in `err` */
      fprintf(stderr, "%s: error:\n%s", me, err);
      free(err);
      /* Should not show hest usage info, right? The problem was in running the command,
         not in collecting the info to start the command, so "usage" per se wasn't the
         issue (perhaps this needs revisiting)?  Even if we wanted to show usage info
         here, we can't actually call hestUsage(), because we don't have access to the
         per-command hestOpt array; that is dynamically built inside its mainFunc(),
         and isn't available out here. But we can point towards more help, in a
         way that emulates the final output line of hestParseOrExit(). */
      fprintf(stderr, "\nFor more info: %s --help\n", me);
    }
    // fall through to code below
  }
  /* if mainFunc returned, unhook any self-mopping that hestParmCopy may have created,
     just like hestParseOrExit does after successful hestParse */
  hestParmUnmop(hparm);
  hestParmFree(hparm);
  /* whether or not ret is non-zero, at this point we don't exit(): the fact that we
     are returning means that we were able to run torun->mainFunc. It could have
     called exit() on its own but for whatever chose not too, we just return
     whatever torun->mainFunc returned. Applications of unrrduMainOrExit() or
     unrrduMainBiffExit() may be a one-line main() simply returns whatever that
     function returns, so the `ret` here may well become the program exit status.
   */
  return ret;
}

/*
 ******** unrrduMainOrExit
 *
 * (See unu_Prime above for the real function;
 *  this and unrrduMainBiffOrExit are its wrappers)
 *
 * A "main" function for unu-like programs, which is very similar to
 * teem/src/bin/unu.c:main() and teem/src/bin/tend.c:main().
 * As of TeemV2, now used for src/limn/test/lpu.c
 *
 * Like hestParseOrExit: this is an odd function that (like the name implies) does
 * sometimes call `exit`. Purists will point out that once the real `main()` function
 * is called, it never returns to any other user-controlled function, so `main` may as
 * well end by calling `exit`, thus the "Main" in our name is inaccurate. Sure. Unlike
 * real `main`, this function *does* `return` when we are able to find and run the
 * requested command without problems.  We `return` instead of `exit` because the caller
 * may have other clean-up, timing, or other post-processing to do.
 * NOTE that when we call the command's `mainFunc`, we are careful to not have done any
 * additional dynamic allocation, besides the hparm, that needs clean-up: we do not
 * get to pass a mop of other things that need cleanin up. The self-mopping hparm is it.
 *
 * There are many circumstances were we call exit:
 * - bad arguments (likely can only be fixed by code modification i.e. not at run-time)
 * - got no command-line arguments ==> implicitly asking for command listing
 * - got "--version": call airTeemVersionPrintAndExit
 * - got "--usage": prints a short message and exits
 * - got "--help" and cmdList has an "about" command: another way to get command listing
 * - got an unknown command: prints error message and exits
 * - if nrrdSanityOrExit() fails then that exits
 * else we actually have a command to run;
 * we return from here the return value of the command's `mainFunc`
 */
int                                 /* Biff: nope */
unrrduMainOrExit(int argc,          /* as received by main() */
                 const char **argv, /* as received by main() */
                 const char *prog,  /* conventional name of command-line program */
                 const char *title, /* for top of listing sub-commands */
                 const unrrduCmd *const *cmdList, /* all the sub-commands */
                 const hestParm *_hparm) {
  return unu_Prime(argc, argv, prog, title, cmdList, _hparm, NULL);
}

/*
 ******** unrrduMainBiffOrExit
 *
 * (See unu_Prime for the real function;
 *  this and unrrduMainOrExit are its wrappers)
 *
 * Like unrrduMainOrExit, but it's our job to do final collection and printing of a biff
 * error message prior to returning.
 */
int                                     /* Biff: nope */
unrrduMainBiffOrExit(int argc,          /* as received by main() */
                     const char **argv, /* as received by main() */
                     const char *prog,  /* conventional name of command-line program */
                     const char *title, /* for top of listing sub-commands */
                     const unrrduCmd *const *cmdList, /* all the sub-commands */
                     const hestParm *_hparm,
                     const char *biffKey) {
  if (!biffKey) {
    fprintf(stderr, "%s: got NULL biffKey\n", __func__);
    exit(1); // (in unrrduMainBiffOrExit)
  }
  return unu_Prime(argc, argv, prog, title, cmdList, _hparm, biffKey);
}

/* --------------------------------------------------------- */
/* --------------------------------------------------------- */
/* --------------------------------------------------------- */

/*
 ******** unrrduUnuHestPosCB
 *
 * For parsing position along an axis. Can be a simple (long) integer, or "M" to signify
 * last position along axis (#samples-1), or "M+<int>"" or "M-<int>"" to signify some
 * position relative to the end.
 *
 * It can also be "m+<int>"" to signify some position relative to some "minimum",
 * assuming that a minimum position is being specified at the same time as this one.
 * Obviously, there has to be some error handling to make sure that no one is trying to
 * define a minimum position with respect to itself.  And, the ability to specify a
 * position as "m+<int>" shouldn't be advertised in situations (unu slice) where you only
 * have one position, rather than an interval between two positions (unu crop and unu
 * pad).
 *
 * This information is represented with two integers, pos[0] and pos[1]:
 * pos[0] ==  0: pos[1] gives the absolute position
 * pos[0] ==  1: pos[1] gives the position relative to the last index
 * pos[0] == -1: pos[1] gives the position relative to a "minimum" position
 */
static int
parsePos(void *ptr, const char *str, char err[ASP1_H]) {
  long int *pos;

  if (!(ptr && str)) {
    snprintf(err, ASP1_H, "%s: got NULL pointer", __func__);
    return 1;
  }
  pos = (long int *)ptr;
  if (!strcmp("M", str)) {
    pos[0] = 1;
    pos[1] = 0;
    return 0;
  }
  if ('M' == str[0]) {
    if (!('-' == str[1] || '+' == str[1])) {
      snprintf(err, ASP1_H, "%s: \'M\' can be followed only by \'+\' or \'-\'",
               __func__);
      return 1;
    }
    pos[0] = 1;
    if (1 != sscanf(str + 1, "%ld", &(pos[1]))) {
      snprintf(err, ASP1_H, "%s: can't parse \"%s\" as M+<int> or M-<int>", __func__,
               str);
      return 1;
    }
    return 0;
  }
  if ('m' == str[0]) {
    if ('+' != str[1]) {
      snprintf(err, ASP1_H, "%s: \'m\' can only be followed by \'+\'", __func__);
      return 1;
    }
    pos[0] = -1;
    if (1 != sscanf(str + 1, "%ld", &(pos[1]))) {
      snprintf(err, ASP1_H, "%s: can't parse \"%s\" as m+<int>", __func__, str);
      return 1;
    }
    if (pos[1] < 0) {
      snprintf(err, ASP1_H, "%s: int in m+<int> must be non-negative (not %ld)",
               __func__, pos[1]);
      return 1;
    }
    return 0;
  }
  /* else its just a plain unadorned integer */
  pos[0] = 0;
  if (1 != sscanf(str, "%ld", &(pos[1]))) {
    snprintf(err, ASP1_H, "%s: can't parse \"%s\" as int", __func__, str);
    return 1;
  }
  return 0;
}

const hestCB unrrduUnuHestPosCB = {2 * sizeof(long int), "position", parsePos, NULL};

/* --------------------------------------------------------- */
/* --------------------------------------------------------- */
/* --------------------------------------------------------- */

/*
 ******** unrrduHestMaybeTypeCB
 *
 * although nrrdType is an airEnum that hest already knows how to parse, we want the
 * ability to have "unknown" be a valid parsable value, contrary to how airEnums usually
 * work with hest. For instance, we might want to use "unknown" to represent "same type
 * as the input, whatever that is".
 *
 * 18 July 03: with new nrrdTypeDefault, this function becomes less of a hack, and more
 * necessary, because the notion of an unknown but valid type (as a default type is)
 * falls squarely outside the nrrdType airEnum framework.  Added a separate test for
 * "default", even though currently nrrdTypeUnknown is the same value as nrrdTypeDefault.
 */
static int
parseMaybeType(void *ptr, const char *str, char err[ASP1_H]) {
  int *typeP;

  /* fprintf(stderr, "!%s: str = \"%s\"\n", __func__, str); */
  if (!(ptr && str)) {
    snprintf(err, ASP1_H, "%s: got NULL pointer", __func__);
    return 1;
  }
  typeP = (int *)ptr;
  if (!strcmp("unknown", str)) {
    *typeP = nrrdTypeUnknown;
  } else if (!strcmp("default", str)) {
    *typeP = nrrdTypeDefault;
  } else {
    *typeP = airEnumVal(nrrdType, str);
    if (nrrdTypeUnknown == *typeP) {
      snprintf(err, ASP1_H, "%s: can't parse \"%s\" as type", __func__, str);
      return 1;
    }
  }
  /* fprintf(stderr, "!%s: *typeP = %d\n", __func__, *typeP); */
  return 0;
}

const hestCB unrrduHestMaybeTypeCB = {sizeof(int), "type", parseMaybeType, NULL};

/* --------------------------------------------------------- */
/* --------------------------------------------------------- */
/* --------------------------------------------------------- */

/*
 ******** unrrduUnuHestBitsCB
 *
 * for parsing an int that can be 8, 16, or 32
 */
static int
parseBits(void *ptr, const char *str, char err[ASP1_H]) {
  unsigned int *bitsP;

  if (!(ptr && str)) {
    snprintf(err, ASP1_H, "%s: got NULL pointer", __func__);
    return 1;
  }
  bitsP = (unsigned int *)ptr;
  if (1 != sscanf(str, "%u", bitsP)) {
    snprintf(err, ASP1_H, "%s: can't parse \"%s\" as int", __func__, str);
    return 1;
  }
  if (!(8 == *bitsP || 16 == *bitsP || 32 == *bitsP)) {
    snprintf(err, ASP1_H, "%s: bits (%u) not 8, 16, or 32", __func__, *bitsP);
    return 1;
  }
  return 0;
}

const hestCB unrrduUnuHestBitsCB = {sizeof(int), "quantization bits", parseBits, NULL};

/* --------------------------------------------------------- */
/* --------------------------------------------------------- */
/* --------------------------------------------------------- */

/*
 ******** unrrduUnuParseScale
 *
 * parse the strings used with "unu resample -s" to indicate
 * the new number of samples.
 * =         : unrrduUnuScaleNothing
 * a         : unrrduUnuScaleAspectRatio
 * x<float>  : unrrduUnuScaleMultiply
 * x=<float> : unrrduUnuScaleMultiply
 * /<float>  : unrrduUnuScaleDivide
 * /=<float> : unrrduUnuScaleDivide
 * +=<uint>  : unrrduUnuScaleAdd
 * -=<uint>  : unrrduUnuScaleSubstract
 * <uint>    : unrrduUnuScaleExact
 * s<float>  : unrrduUnuScaleSpacingTarget
 */
static int
parseScale(void *ptr, const char *str, char err[ASP1_H]) {
  double *scale;
  unsigned int num;
  DECISEP_DECL1;

  if (!(ptr && str)) {
    snprintf(err, ASP1_H, "%s: got NULL pointer", __func__);
    return 1;
  }
  scale = AIR_CAST(double *, ptr);
  if (!strcmp("=", str)) {
    scale[0] = AIR_CAST(double, unrrduUnuScaleNothing);
    scale[1] = 0.0;
  } else if (!strcmp("a", str)) {
    scale[0] = AIR_CAST(double, unrrduUnuScaleAspectRatio);
    scale[1] = 0.0;
  } else if (strlen(str) > 2 && ('x' == str[0] || '/' == str[0]) && '=' == str[1]) {
    if (1 != airSingleParse(str + 2, "%lf", scale + 1)) {
      snprintf(err, ASP1_H, "%s: can't parse \"%s\" as x=<float> or /=<float>", __func__,
               str);
      return 1;
    }
    if (!(scale[1] > 0)) {
      snprintf(err, ASP1_H, "%s: need positive float from \"%s\" (not %s)", __func__,
               str, airDtoS(Nb, scale[1], Ds));
      return 1;
    }
    scale[0] = AIR_CAST(double,
                        ('x' == str[0] ? unrrduUnuScaleMultiply : unrrduUnuScaleDivide));
  } else if (strlen(str) > 1 && ('x' == str[0] || '/' == str[0] || 's' == str[0])) {
    if (1 != airSingleParse(str + 1, "%lf", scale + 1)) {
      snprintf(err, ASP1_H,
               "%s: can't parse \"%s\" as x<float>, /<float>, "
               "or s<float>",
               __func__, str);
      return 1;
    }
    if (!(scale[1] > 0)) {
      snprintf(err, ASP1_H, "%s: need positive float from \"%s\" (not %s)", __func__,
               str, airDtoS(Nb, scale[1], Ds));
      return 1;
    }
    scale[0] = AIR_CAST(double,
                        ('x' == str[0] ? unrrduUnuScaleMultiply
                                       : ('/' == str[0] ? unrrduUnuScaleDivide
                                                        : unrrduUnuScaleSpacingTarget)));
  } else if (strlen(str) > 2 && ('+' == str[0] || '-' == str[0]) && '=' == str[1]) {
    if (1 != sscanf(str + 2, "%u", &num)) {
      snprintf(err, ASP1_H, "%s: can't parse \"%s\" as +=<uint> or -=<uint>", __func__,
               str);
      return 1;
    }
    scale[0] = AIR_CAST(double,
                        ('+' == str[0] ? unrrduUnuScaleAdd : unrrduUnuScaleSubtract));
    scale[1] = AIR_CAST(double, num);
  } else {
    if (1 != sscanf(str, "%u", &num)) {
      snprintf(err, ASP1_H, "%s: can't parse \"%s\" as uint", __func__, str);
      return 1;
    }
    scale[0] = AIR_CAST(double, unrrduUnuScaleExact);
    scale[1] = AIR_CAST(double, num);
  }
  return 0;
}

const hestCB unrrduUnuHestScaleCB = {2 * sizeof(double),
                                     "sampling/scaling specification", parseScale, NULL};

/* --------------------------------------------------------- */
/* --------------------------------------------------------- */
/* --------------------------------------------------------- */

/*
 ******** unrrduUnuHestFileCB
 *
 * for parsing a filename, which means opening it in "rb" mode and getting a FILE *.
 * "-" is interpreted as stdin, which is not fclose()ed at the end,
 * unlike all other files.
 */
static void *
maybeFclose(void *_file) {
  FILE *file;

  file = (FILE *)_file;
  if (stdin != file) {
    /* file = */ airFclose(file);
  }
  return NULL;
}

static int
parseFile(void *ptr, const char *str, char err[ASP1_H]) {
  FILE **fileP;

  if (!(ptr && str)) {
    snprintf(err, ASP1_H, "%s: got NULL pointer", __func__);
    return 1;
  }
  fileP = (FILE **)ptr;
  if (!(*fileP = airFopen(str, stdin, "rb"))) {
    snprintf(err, ASP1_H, "%s: fopen(\"%s\",\"rb\") failed: %s", __func__, str,
             strerror(errno));
    return 1;
  }
  return 0;
}

const hestCB unrrduUnuHestFileCB = {
  sizeof(FILE *),
  "filename",
  parseFile,
  maybeFclose,
};

/* --------------------------------------------------------- */
/* --------------------------------------------------------- */
/* --------------------------------------------------------- */

/*
 ******** unrrduUnuHestEncodingCB
 *
 * for parsing output encoding, including compression flags
 * enc[0]: which encoding, from nrrdEncodingType* enum
 * enc[1]: for compressions: zlib "level" and bzip2 "blocksize"
 * enc[2]: for zlib: strategy, from nrrdZlibStrategy* enum
 */
static int
parseEncoding(void *ptr, const char *_str, char err[ASP1_H]) {
  char *str, *opt;
  int *enc;
  airArray *mop;

  if (!(ptr && _str)) {
    snprintf(err, ASP1_H, "%s: got NULL pointer", __func__);
    return 1;
  }
  enc = (int *)ptr;
  /* these are the defaults, they may not get over-written */
  enc[1] = -1;
  enc[2] = nrrdZlibStrategyDefault;

  enc[0] = airEnumVal(nrrdEncodingType, _str);
  if (nrrdEncodingTypeUnknown != enc[0]) {
    /* we're done; encoding was simple: "raw" or "gz" */
    return 0;
  }
  mop = airMopNew();
  str = airStrdup(_str);
  airMopMem(mop, &str, airMopAlways);
  opt = strchr(str, ':');
  if (!opt) {
    /* couldn't parse string as nrrdEncodingType, but there wasn't a colon */
    snprintf(err, ASP1_H, "%s: didn't recognize \"%s\" as an encoding", __func__, str);
    airMopError(mop);
    return 1;
  } else {
    *opt = '\0';
    opt++;
    enc[0] = airEnumVal(nrrdEncodingType, str);
    if (nrrdEncodingTypeUnknown == enc[0]) {
      snprintf(err, ASP1_H, "%s: didn't recognize \"%s\" as an encoding", __func__, str);
      airMopError(mop);
      return 1;
    }
    if (!nrrdEncodingArray[enc[0]]->isCompression) {
      snprintf(err, ASP1_H, "%s: only compression encodings have parameters", __func__);
      airMopError(mop);
      return 1;
    }
    while (*opt) {
      int opti = AIR_INT(*opt);
      if (isdigit(opti)) {
        enc[1] = *opt - '0';
      } else if ('d' == tolower(opti)) {
        enc[2] = nrrdZlibStrategyDefault;
      } else if ('h' == tolower(opti)) {
        enc[2] = nrrdZlibStrategyHuffman;
      } else if ('f' == tolower(opti)) {
        enc[2] = nrrdZlibStrategyFiltered;
      } else {
        snprintf(err, ASP1_H, "%s: parameter char \"%c\" not a digit or 'd','h','f'",
                 __func__, *opt);
        airMopError(mop);
        return 1;
      }
      opt++;
    }
  }
  airMopOkay(mop);
  return 0;
}

const hestCB unrrduUnuHestEncodingCB = {3 * sizeof(int), "encoding", parseEncoding,
                                        NULL};

/* --------------------------------------------------------- */
/* --------------------------------------------------------- */
/* --------------------------------------------------------- */

/*
 ******** unrrduUnuHestFormatCB
 *
 * for parsing output format
 * enc[0]: which format, from nrrdFormatType* enum
 * enc[1]: for nrrdFormatText: bool for whether to enforce plain "bare" text
 *         as indicated by one of: btext, ptext, baretext, plaintext
 */
static int
parseFormat(void *ptr, const char *str, char err[ASP1_H]) {
  int *enc;

  if (!(ptr && str)) {
    snprintf(err, ASP1_H, "%s: got NULL pointer", __func__);
    return 1;
  }
  enc = (int *)ptr;
  /* these are the defaults, they may not get over-written */
  enc[1] = 0;

  enc[0] = airEnumVal(nrrdFormatType, str);
  if (nrrdFormatTypeUnknown != enc[0]) {
    /* we're done; given format was exactly a standard format */
    return 0;
  }
  /* else given format was non-standard */
  if (!strcmp("ptext", str) || !strcmp("plaintext", str) || !strcmp("btext", str)
      || !strcmp("baretext", str)) {
    enc[0] = nrrdFormatTypeText;
    enc[1] = AIR_TRUE;
  } else {
    snprintf(err, ASP1_H, "%s: format \"%s\" not a %s or recognized alternate", __func__,
             str, nrrdFormatType->name);
    return 1;
  }
  return 0;
}

const hestCB unrrduUnuHestFormatCB = {2 * sizeof(int), "format", parseFormat, NULL};
