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

/* parse, Parser, PARSEST: please let this be the final implementation of hestParse */

/* variable name conventions:
harg  = hestArg
havec = hestArgVec
hin   = hestInput
hist  = hestInputStack
hopt  = hestOpt
*/

#define HIST_DEPTH_MAX 5u // max sensible depth of hestInputStack

// histPopt pops one hestInput from the `hist` stack
static int /* Biff: 1 */
histPop(hestInputStack *hist, const hestParm *hparm) {
  hestInput *topHin = hist->hin + hist->len - 1;
  if (topHin->dashBraceComment) {
    biffAddf(HEST,
             "%s%s%u start comment marker%s \"-{\" not balanced by equal later \"}-\"",
             MY_ME, topHin->dashBraceComment, topHin->dashBraceComment > 1 ? "s" : "");
    return 1;
  }
  if (hparm->verbose) {
    printf("%s: changing stack height: %u --> %u; popping %s source\n", __func__,
           hist->len, hist->len - 1,
           airEnumStr(hestSource, hist->hin[hist->len - 1].source));
  }
  airArrayLenIncr(hist->hinArr, -1);
  return 0;
}

/* argst* enum values
Properly parsing a character sequence into "arguments", like the shell does to convert
the typed command-line into the argv[] array, is more than just calling strtok(), but the
old hest code only did that, which is why it had to be re-written in 2025.  Instead,
handling " vs ' quotes, character escaping, and the implicit concatenation of adjacent
args, needs something like a deterministic finite automaton (DFA) to be done correctly.
Here is the POSIX info about tokenizing:
https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_03
(although hest does not do rule 5 about parameter expansion, command substitution,
or arithmetic expansion), and here are the details about quoting:
https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_02
ChatGPT helped with prototyping argstGo (and in this file, only that function)
Here is instructive example code https://github.com/nyuichi/dash.git
  in src/parser.c see the readtoken1() function and the DFA there.
*/
/* the DFA states of our arg tokenizer */
enum {
  argstUnknown,  // 0: don't know
  argstStart,    // 1: skipping whitespace to start arg
  argstInside,   // 2: currently inside a token/arg
  argstSingleQ,  // 3: inside single quoting
  argstDoubleQ,  // 4: inside double quoting
  argstEscapeIn, // 5: got \ escape from within (unquoted) word
  argstEscapeDQ, // 6: Escape Dairy Queen!
  argstComment,  // 7: #-initiated comment
};
// and airEnum for this
static const airEnum argst_aenum = {.name = "tokenizer-state",
                                    .M = 6,
                                    .str = (const char *[]){"(unknown_state)", // 0
                                                            "start",           // 1
                                                            "inside",          // 2
                                                            "singleQ",         // 3
                                                            "doubleQ",         // 4
                                                            "escapeIn",        // 5
                                                            "escapeDQ",        // 6
                                                            "#comment"},       // 7
                                    .val = NULL,
                                    .desc = NULL,
                                    .strEqv = NULL,
                                    .valEqv = NULL,
                                    .sense = AIR_FALSE};

/* argstGo
Implements the DFA (deterministic finite automaton) that tokenizes a character sequence
into arguments: the thing that the shell does for you to convert the command-line you
type into an array argv[] of args. This gets one character `icc` at a time, and a pointer
to the current DFA state `*stateP`, and builds up the output token in `tharg`. */
static int /* Biff: 1 */
argstGo(int *tokenDone, hestArg *tharg, int *stateP, int icc, int vrbo) {
  char cc = (char)icc;
  if (vrbo) {
    printf("%s: hello: getting %d=|%c| in state=%s\n", __func__, icc, cc,
           airEnumStr(&argst_aenum, *stateP));
  }
  /* hitting end of input is special enough to handle separately.  Note that
     the caller also makes the end of a string input (not just file input) into EOF */
  if (EOF == icc) {
    int ret;
    switch (*stateP) {
    case argstInside:
      // EOF ends the arg we've been building up, great: it is now done
      *tokenDone = AIR_TRUE;
      ret = 0;
      break;
    case argstStart:
      // source ended cleanly with no token in progress, no problem (caller will pop)
      ret = 0;
      break;
    case argstComment:
      // hit EOF will inside a #-comment, which is totally fine (caller will pop)
      ret = 0;
      break;
    case argstSingleQ:
      biffAddf(HEST, "%s%shit input end inside single-quoted string", MY_MEV(vrbo));
      ret = 1;
      break;
    case argstDoubleQ:
      biffAddf(HEST, "%s%shit input end inside double-quoted string", MY_MEV(vrbo));
      ret = 1;
      break;
    case argstEscapeIn:
      biffAddf(HEST, "%s%shit input end after \\ escape from arg", MY_MEV(vrbo));
      ret = 1;
      break;
    case argstEscapeDQ:
      biffAddf(HEST, "%s%shit input end after \\ escape from double-quoted string",
               MY_MEV(vrbo));
      ret = 1;
      break;
    }
    return ret;
  }
  // else not at input end
  switch (*stateP) {
  case argstStart:
    // if we got a space, we're still waiting for arg to start (stay in argstStart)
    if (!hest__isspace(icc)) {
      // we got non-space
      if ('\'' == cc) {
        *stateP = argstSingleQ;
      } else if ('"' == cc) {
        *stateP = argstDoubleQ;
      } else if ('\\' == cc) {
        *stateP = argstEscapeIn;
      } else if ('#' == cc) {
        *stateP = argstComment;
      } else { // start building up new arg
        hestArgAddChar(tharg, cc);
        *stateP = argstInside;
      }
    } // (else got space)
    break;
  case argstInside:
    if (hest__isspace(icc)) {
      // whitespace is the natural end to an arg
      *tokenDone = AIR_TRUE;
    } else if ('\'' == cc) {
      *stateP = argstSingleQ;
    } else if ('"' == cc) {
      *stateP = argstDoubleQ;
    } else if ('\\' == cc) {
      *stateP = argstEscapeIn;
    } else { // add to existing arg EVEN IF IT is a '#'
      hestArgAddChar(tharg, cc);
      // state stays at argstInside
    }
    break;
  case argstSingleQ:
    if ('\'' == cc) {
      // ending quoted string, back to arg interior
      *stateP = argstInside;
    } else {
      // add characters (including #) to arg without any other interpretation
      hestArgAddChar(tharg, cc);
    }
    break;
  case argstDoubleQ:
    if ('"' == cc) {
      // ending quoted string, back to arg interior
      *stateP = argstInside;
    } else if ('\\' == cc) {
      *stateP = argstEscapeDQ;
    } else {
      // add non-escaped characters (including #) to arg
      hestArgAddChar(tharg, cc);
    }
    break;
  case argstEscapeIn:
    if ('\n' == cc) {
      // line continuation; ignore \ and \n
    } else {
      // add escaped character (including #) to arg
      hestArgAddChar(tharg, cc);
    }
    // back to unescaped input
    *stateP = argstInside;
    break;
  case argstEscapeDQ:
    if ('\n' == cc) {
      // like above: line continuation; ignore \ and \n
    } else if ('$' == cc || '`' == cc || '\"' == cc || '\\' == cc) {
      // add escaped character to arg
      hestArgAddChar(tharg, cc);
    } else {
      // other character (needlessly) escaped, put in both \ and char
      hestArgAddChar(tharg, '\\');
      hestArgAddChar(tharg, cc);
    }
    // back to unescaped input
    *stateP = argstDoubleQ;
    break;
  case argstComment:
    if ('\n' == cc) {
      // the newline has ended the comment, prepare for next arg
      *stateP = argstStart;
    }
    // else we skip the commented-out character
    break;
  } // switch (*stateP)
  return 0;
}

/* histNextArg (tries to) produces another token from the input stack.
   It communicates back to caller with *popit ("pop top input from stack; its done")
   and *tokenDone ("I have a token for you").
   It assumes that *popit and *tokenDone are coming in both false,
   and only changes them to turn them on */
static int /* Biff : 1 */
histNextArg(int *popit, int *tokenDone, hestArg *tharg, hestInputStack *hist,
            const hestParm *hparm) {
  hestArgReset(tharg);
  hestInput *hin = hist->hin + hist->len - 1;
  if (hestSourceCommandLine == hin->source) {
    if (hin->argIdx < hin->argc) {
      hestArgSetString(tharg, hin->argv[hin->argIdx]);
      *tokenDone = AIR_TRUE;
      hin->argIdx++;
    } else {
      *popit = AIR_TRUE;
    }
  } else {
    assert(hestSourceResponseFile == hin->source || hestSourceDefault == hin->source);
    int state = argstStart;
    do { // character loop: one iteration per character read
      int icc;
      if (hestSourceDefault == hin->source) {
        icc = (hin->carIdx < hin->dfltLen) ? hin->dfltStr[hin->carIdx] : EOF;
      } else {
        icc = fgetc(hin->rfile); // will get EOF at end of file
      }
      /* We will eventually pop this input if we got an EOF, but we first need to show
         the EOF to argstGo in order to get error messages about hitting EOF unexpectedly
         (but EOF often not an error, e.g. inside a comment or after whitespace). */
      if (argstGo(tokenDone, tharg, &state, icc, hparm->verbose > 4)) {
        biffAddf(HEST, "%s%strouble at character %u of %s \"%s\"", MY_ME, hin->carIdx,
                 airEnumStr(hestSource, hin->source),
                 hestSourceResponseFile == hin->source ? hin->rfname : hin->dfltStr);
        return 1;
      }
      // printf("!%s: post argstGo(%d=%c): *tokenDone = %d  tharg->str = |%s|\n",
      // __func__,
      //        icc, icc, *tokenDone, tharg->str);
      if (EOF == icc) {
        *popit = AIR_TRUE;
      } else {
        hin->carIdx++;
      }
      // printf("!%s: just before while(!tokenDone && !popit) == while(%d && %d)\n",
      //       __func__, !(*tokenDone), !(*popit));
    } while (!(*tokenDone) && !(*popit));
  }
  if (*tokenDone) {
    // printf("!%s: we have a token!  |%s|\n", __func__, tharg->str);
    tharg->source = hin->source;
  }
  // printf("!%s: leaving with popit=%d tokenDone=%d and stack height %u\n", __func__,
  //        *popit, *tokenDone, hist->len);
  return 0;
}

static int /* Biff: 1 */
histPushCommandLine(hestInputStack *hist, int argc, const char **argv,
                    const hestParm *hparm) {
  /** this isn't actually reachable is it?
   * if (HIST_DEPTH_MAX == hist->len) {
   *   biffAddf(HEST, "%s%sinput stack depth already at max %u", MY_ME, HIST_DEPTH_MAX);
   *   return 1;
   * } */
  if (hparm->verbose) {
    printf("%s: changing stack height: %u --> %u with argc=%d,argv=%p; "
           "setting argIdx to 0\n",
           __func__, hist->hinArr->len, hist->hinArr->len + 1, argc, AIR_VOIDP(argv));
  }
  uint idx = airArrayLenIncr(hist->hinArr, 1);
  if (hparm->verbose > 1) {
    printf("%s: new hinTop = %p\n", __func__, AIR_VOIDP(hist->hin + idx));
  }
  hist->hin[idx].source = hestSourceCommandLine;
  hist->hin[idx].argc = AIR_UINT(argc);
  hist->hin[idx].argv = argv;
  hist->hin[idx].argIdx = 0;
  return 0;
}

static int /* Biff: 1 */
histPushResponseFile(hestInputStack *hist, const char *rfname, const hestParm *hparm) {
  if (HIST_DEPTH_MAX == hist->len) {
    biffAddf(HEST, "%s%sinput stack depth already at max %u", MY_ME, HIST_DEPTH_MAX);
    return 1;
  }
  if (!strlen(rfname)) {
    biffAddf(HEST,
             "%s%ssaw arg start with response file flag \"%c\" "
             "but no filename followed",
             MY_ME, MY_RESPONSE_FILE_FLAG);
    return 1;
  }
  // are we trying to read stdin twice?
  if (!strcmp("-", rfname) && hist->stdinRead) {
    biffAddf(HEST, "%s%sresponse filename \"%s\" but previously read stdin", MY_ME,
             rfname);
    return 1;
  }
  // try to open response file
  FILE *rfile = airFopen(rfname, stdin, "r");
  if (!(rfile)) {
    biffAddf(HEST, "%s%scouldn't fopen(\"%s\",\"r\"): %s", MY_ME, rfname,
             strerror(errno));
    return 1;
  }
  if (!strcmp("-", rfname)) {
    hist->stdinRead = AIR_TRUE;
  }
  // okay, we actually opened the response file; put it on the stack
  uint idx = airArrayLenIncr(hist->hinArr, 1);
  if (hparm->verbose > 1) {
    printf("%s: (hist depth %u) new hinTop = %p\n", __func__, hist->len,
           AIR_VOIDP(hist->hin + idx));
  }
  hist->hin[idx].source = hestSourceResponseFile;
  /* We need our own copy of this filename for debugging and error messages;
     the rfname we were passed was probably the `str` of some tmp hestArg */
  hist->hin[idx].rfname = airStrdup(rfname);
  hist->hin[idx].rfile = rfile;
  hist->hin[idx].carIdx = 0;
  return 0;
}

static int /* Biff: 1 */
histPushDefault(hestInputStack *hist, const char *dflt, const hestParm *hparm) {
  /** this isn't actually reachable is it?  With the current organization,
   * defaults are parsed in Phase 5, and are pushed onto an empty stack.
   * if (HIST_DEPTH_MAX == hist->len) {
   *   biffAddf(HEST, "%s%sinput stack depth already at max %u", MY_ME, HIST_DEPTH_MAX);
   *   return 1;
   * } */
  if (hparm->verbose) {
    printf("%s: changing stack height: %u --> %u with dflt=|%s|; "
           "dfltLen %u, dfltIdx 0\n",
           __func__, hist->hinArr->len, hist->hinArr->len + 1, dflt,
           AIR_UINT(strlen(dflt)));
  }
  uint idx = airArrayLenIncr(hist->hinArr, 1);
  hist->hin[idx].source = hestSourceDefault;
  hist->hin[idx].dfltStr = dflt;
  hist->hin[idx].dfltLen = AIR_UINT(strlen(dflt));
  hist->hin[idx].carIdx = 0;
  return 0;
}

/* histProcess
Consumes args (tokens) from the stack `hist` as they are produced by histNextArg,
usually mostly just copying them into `havec`. However, there are some tokens that
are interpret here:
  (what)                   (allowed sources)              (controlling hestParm field)
- commenting with -{ , }-  all (even a default string)    respectDashBraceComments
- response files           command-line or response file  responseFileEnable
- ask for --help, --usage  command-line only              respectDashDashHelp
- stopping at --           command-line only              respectDashDashMeansStop
These are interpreted here because they are not specific to any hestOpt. The -.
flag to end variadic parms is recognized by havecExtractFlagged and havecExtractUnlagged,
and we do not understand or know anything about any particular hestOpt. Upon seeing a
request for a response file, we push it to the input stack and the next call to
histNextArg will read from it.

Other command-line parsers that implement response files may recognize -- as the "stop
processing here" flag even when it appears within a response file, but it seems more
likely that extra non-hest args will show up on the command-line than in a file (and with
this distinction, response files offer a tunnel for -- to appear as parms for other
hest options).

This function studiously avoids taking ownership of anything.  The tmp arg holder `tharg`
is passed in here and cleaned up by caller, and airArray *remainArr accumulates args
appearing after "--".
*/
static int /* Biff: 1 */
histProcess(hestArgVec *havec, int *helpWantedP, airArray *remainArr, hestArg *tharg,
            hestInputStack *hist, const hestParm *hparm) {
  if (helpWantedP) *helpWantedP = AIR_FALSE;
  uint iters = 0;
  // printf("!%s: hello hist->len %u\n", __func__, hist->len);
  // initialize destination havec
  airArrayLenSet(havec->hargArr, 0);
  /* We `return` directly from this loop ONLY when we MUST stop processing the stack,
     because of an error, or because of user asking for help.
     Otherwise, we loop again. */
  int remaining = AIR_FALSE; /* we saw -- and hparm->respectDashDashMeansStop so now all
                                tokens are being shunted to remainArr */
  airPtrPtrUnion appu;
  while (hist->hin) {
    iters += 1;
    // various ways to describe current input source
    hestInput *topHin = hist->hin + hist->len - 1;
    int srcval = topHin->source;
    const char *srcstr = airEnumStr(hestSource, srcval);
    // read next arg into tharg
    int popit = AIR_FALSE, tokenDone = AIR_FALSE;
    if (histNextArg(&popit, &tokenDone, tharg, hist, hparm)) {
      biffAddf(HEST, "%s%s(iter %u, on %s) unable to get next arg", MY_ME, iters,
               srcstr);
      return 1;
    }
    if (!tokenDone) {
      // we didn't get a token, then we skip over code to contemplate token
      goto maybepop;
    }
    if (remaining) {
      /* just collecting the remaining args. for simplicity only the airArray was passed
         to us, so we have to relearn the char ** pointer each time */
      char *rstr = airStrdup(tharg->str);
      assert(rstr);
      unsigned int ridx = airArrayLenIncr(remainArr, 1);
      assert(remainArr->data);
      appu.v = remainArr->dataP;
      (*appu.cp)[ridx] = rstr;
      // go back for more
      goto maybepop;
    }
    /* only got non-NULL remainArr for command-line with hparm->respectDashDashMeansStop
       but we might now be on a response file */
    if (remainArr && !strcmp("--", tharg->str)
        && hestSourceCommandLine == tharg->source) {
      // turning on remaining and go back for more
      remaining = AIR_TRUE;
      goto maybepop;
    }
    // else we have a token that we either interpret or accumulate
    // is it turning off commenting?
    if (hparm->respectDashBraceComments && !strcmp("}-", tharg->str)) {
      if (topHin->dashBraceComment) {
        topHin->dashBraceComment -= 1;
        if (hparm->verbose) {
          printf("%s: topHin->dashBraceComment now %u\n", __func__,
                 topHin->dashBraceComment);
        }
        goto maybepop; // since }- does not belong in havec
      } else {
        biffAddf(HEST,
                 "%s%s(iter %u, on %s) end comment marker \"}-\" not "
                 "balanced by prior \"-{\"",
                 MY_ME, iters, srcstr);
        return 1;
      }
    }
    // not ending comment, are we starting (or deepening) one?
    if (hparm->respectDashBraceComments && !strcmp("-{", tharg->str)) {
      topHin->dashBraceComment += 1;
      if (hparm->verbose) {
        printf("%s: topHin->dashBraceComment now %u\n", __func__,
               topHin->dashBraceComment);
      }
      goto maybepop; // since -{ does not belong in havec
    }
    // if in comment, move along
    if (topHin->dashBraceComment) {
      if (hparm->verbose > 1) {
        printf("%s: (iter %u, on %s) skipping commented-out |%s|\n", __func__, iters,
               srcstr, tharg->str);
      }
      goto maybepop; // skipping over commented-out token
    }
    // else this arg token is not in a comment and is not related to commenting
    int helpWant = 0;
    // int usageWant = 0;
    if (hparm->respectDashDashHelp                                 /* */
        && ((helpWant = !strcmp("--help", tharg->str))             /* */
            || (/* usageWant = */ !strcmp("--usage", tharg->str))) /* */
    ) {
      if (hestSourceCommandLine == tharg->source) {
        /* user asking for help halts further parsing work: user is not looking
           for parsing results nor error messages about that process */
        // HERE is where we set the magic numbers 1 for --help, 2 for --usage
        *helpWantedP = helpWant ? 1 : 2;
        return 0;
      } else {
        biffAddf(HEST, "%s%s(iter %u, on %s) \"%s\" not supported in a %s source", MY_ME,
                 iters, srcstr, helpWant ? "--help" : "--usage",
                 airEnumStr(hestSource, tharg->source));
        return 1;
      }
    }
    /* else not respecting --help/--usage, or it wasn't asked for help/usage */
    if (hparm->verbose > 2) {
      printf("%s: (iter %u, on %s) looking at latest tharg |%s|\n", __func__, iters,
             srcstr, tharg->str);
    }
    if (hparm->responseFileEnable && tharg->str[0] == MY_RESPONSE_FILE_FLAG) {
      if (hestSourceDefault == tharg->source) {
        biffAddf(HEST,
                 "%s%s(iter %u, on %s) \"%s\" response file not handled in %s source",
                 MY_ME, iters, srcstr, tharg->str,
                 airEnumStr(hestSource, tharg->source));
        return 1;
      } else {
        // tharg->str is asking to open response file named `tharg->str + 1`
        // make sure we're not already reading it
        const char *rfname = tharg->str + 1;
        for (uint hidx = 0; hidx < hist->len; hidx++) {
          if (hestSourceResponseFile == hist->hin[hidx].source
              && !strcmp(hist->hin[hidx].rfname, rfname)) {
            uint deep = hist->len - hidx;
            biffAddf(HEST,
                     "%s%s(iter %u, on %s) already reading response file \"%s\" "
                     "(%u level%s deeper on input stack): "
                     "cannot recursively re-read it",
                     MY_ME, iters, srcstr, rfname, deep, deep == 1 ? "" : "s");
            return 1;
          }
        }
        if (histPushResponseFile(hist, rfname, hparm)) {
          biffAddf(HEST, "%s%s(iter %u, on %s) unable to process response file %s",
                   MY_ME, iters, srcstr, tharg->str);
          return 1;
        }
        // have just added response file to stack, next iter will start reading from it
        // NOTE that we did this push even with popit: it will get popped later
        continue;
      }
    }
    // this arg is not specially handled by us; add it to the arg vec
    hestArgVecAppendString(havec, tharg->str);
    if (hparm->verbose > 1) {
      printf("%s: (iter %u, on %s) added |%s| to havec, now len %u\n", __func__, iters,
             srcstr, tharg->str, havec->len);
    }
    // set source in the hestArg we just appended
    havec->harg[havec->len - 1]->source = srcval;
  maybepop:
    if (popit) {
      if (histPop(hist, hparm)) {
        biffAddf(HEST, "%s%strouble popping %s", MY_ME,
                 airEnumStr(hestSource, topHin->source));
        return 1;
      }
    }
  } // while (hist->hin)
  return 0;
}

/* whichOptFlag(): for which option (by index) is this the flag?

Given an arg string `flarg` (which may be an flag arg (like "-size") or not (like "512"),
this finds which one, of the options in the given hestOpt array `hopt`, is identified by
`flarg`. If there is a match, returns the index of that option, else returns UINT_MAX.
*/
static uint /* Biff: nope */
whichOptFlag(const hestOpt *hopt, const char *flarg, const hestParm *hparm) {
  uint optNum = hopt->arrLen;
  if (hparm->verbose > 3)
    printf("%s: looking for maybe-is-flag |%s| in optNum=%u options\n", __func__, flarg,
           optNum);
  for (uint optIdx = 0; optIdx < optNum; optIdx++) {
    // if (hparm->verbose > 3)
    //   printf("%s:      optIdx %u |%s| ?\n", __func__, optIdx,
    //          hopt[optIdx].flag ? hopt[optIdx].flag : "(nullflag)");
    const char *optFlag = hopt[optIdx].flag;
    if (!optFlag) continue; // it can't be for this unflagged option
    if (strchr(optFlag, MY_MULTI_FLAG_SEP)) {
      // look for both long and short versions
      size_t buffSize = strlen("--") + strlen(flarg) + strlen(optFlag) + 1;
      char *buff = AIR_CALLOC(buffSize, char);
      char *ofboth = airStrdup(optFlag);
      char *sep = strchr(ofboth, MY_MULTI_FLAG_SEP);
      *sep = '\0'; // break short and long into separate strings
      /* first try the short version */
      snprintf(buff, buffSize, "-%s", ofboth);
      if (!strcmp(flarg, buff)) return (free(buff), free(ofboth), optIdx);
      /* else try the long version */
      snprintf(buff, buffSize, "--%s", sep + 1);
      if (!strcmp(flarg, buff)) return (free(buff), free(ofboth), optIdx);
      free(buff);
      free(ofboth);
    } else {
      /* flag only comes in short version */
      size_t buffSize = strlen("-") + strlen(optFlag) + 1;
      char *buff = AIR_CALLOC(buffSize, char);
      snprintf(buff, buffSize, "-%s", optFlag);
      if (!strcmp(flarg, buff)) return (free(buff), optIdx);
      free(buff);
    }
  }
  if (hparm->verbose > 3) printf("%s: no match, returning UINT_MAX\n", __func__);
  return UINT_MAX;
}

/* hestOptIdentStr sprints into `ident` (and returns same `ident`)
   a way to identify `hopt` in error and usage messages */
char * /* Biff: nope */
hestOptIdentStr(char *ident, size_t identSize, const hestOpt *hopt) {
  if (hopt->flag) {
    if (strchr(hopt->flag, MY_MULTI_FLAG_SEP)) {
      char *fcopy = airStrdup(hopt->flag);
      char *sep = strchr(fcopy, MY_MULTI_FLAG_SEP);
      *sep = '\0';
      snprintf(ident, identSize, "\"-%s%c--%s\" option", fcopy, MY_MULTI_FLAG_SEP,
               sep + 1);
      free(fcopy);
    } else {
      snprintf(ident, identSize, "\"-%s\" option", hopt->flag);
    }
  } else {
    snprintf(ident, identSize, "unflagged \"<%s>\" option", hopt->name);
  }
  return ident;
}

// return havec->harg[popIdx] and shift higher indices down
// formerly hestArgVecRemove but only used in this file; moving it
// here removes cautious error checking that tests can't exercise
static hestArg *
havecRemove(hestArgVec *havec, uint popIdx) {
  hestArg *ret = havec->harg[popIdx];
  uint ai;
  for (ai = popIdx; ai < havec->len - 1; ai++) {
    havec->harg[ai] = havec->harg[ai + 1];
  }
  // NULL out final out, using final value of ai
  havec->harg[ai] = NULL;
  // decrement the nominal length of havec->harg
  airArrayLenIncr(havec->hargArr, -1);
  return ret;
}

/* havecTransfer
(if `num`) moves `num` args from `hvsrc` (starting at `srcIdx`) to `hopt->havec`. This
takes `hestOpt *hopt` instead of `hopt->havec` so that we can also set `hopt->source`
according to the incoming `hvsrc->harg[]->source`. To minimize cleverness, we set
`hopt->source` with every transferred argument, which means that the per-option source
remembered is the source of the *last* argument of the option.

This is only called from two functions havecExtractFlagged and havecExtractUnflagged,
both of which have already carefully ensured that there are enough args to transfer */
void
havecTransfer(hestOpt *hopt, hestArgVec *hvsrc, uint srcIdx, uint num) {
  if (num) {
    hestArgVecReset(hopt->havec);
    for (uint ai = 0; ai < num; ai++) {
      hestArg *harg = havecRemove(hvsrc, srcIdx);
      hestArgVecAppendArg(hopt->havec, harg);
      hopt->source = harg->source;
    }
  }
  return /* 0 */;
}

static void
optPrint(const hestOpt *hopt, uint opi) {
  printf("--- opt %u:", opi);
  printf("\t%s%s", hopt->flag ? "flag-" : "", hopt->flag ? hopt->flag : "UNflag");
  printf("\tname|%s|\t k%d (%u)--(%d) \t%s ", hopt->name ? hopt->name : "(null)",
         hopt->kind, hopt->min, hopt->max, hest__TypeStr[hopt->type]);
  printf("\t%sdflt%s%s%s\n", hopt->dflt ? "" : "NO-", hopt->dflt ? "|" : "",
         hopt->dflt ? hopt->dflt : "", hopt->dflt ? "|" : "");
  printf("    source %s\n", airEnumStr(hestSource, hopt->source));
  hestArgVecPrint("", "    havec:", hopt->havec);
  return;
}

static void
optAllPrint(const char *func, const char *ctx, const hestOpt *hoptall) {
  printf("%s: %s:\n", func, ctx);
  printf("%s: v.v.v.v.v.v.v.v.v hestOpt %p has %u options (allocated for %u):\n", func,
         AIR_CVOIDP(hoptall), hoptall->arrLen, hoptall->arrAlloc);
  for (uint opi = 0; opi < hoptall->arrLen; opi++) {
    optPrint(hoptall + opi, opi);
  }
  printf("%s: ^'^'^'^'^'^'^'^'^\n", func);
  return;
}

/* havecExtractFlagged
Extracts the parameter args associated with all flagged options from the given
`hestArgVec *havec` (as generated by histProc()) and stores them the corresponding
hopt->havec. Also sets hopt->source according to where that flag arg appeared in the case
of stand-alone flags, or (via havecTransfer) wherever the last parm arg came from.

In the case of variadic parameter options, this does the work of figuring out which args
belong with the option. In any case, this only extracts and preserves (in opt->havec) the
parameter args, not the flag arg that identified which option was being set.

As a result of this work, the passed `havec` is shortened: all args associated with
flagged opts are removed, so that later work can extract args for unflagged opts.

For variadic parameter options, the sawP information is not set here, since it is better
set at the final value parsing time, which happens after defaults are enstated.

If hparm->respectDashPeriodMeansEnd, this is where "-." (see MY_VAR_PARM_END_FLAG in
privateHest.h) is recognized as a marker for the end of a flagged variadic parameter
option (in which case the "-." marker is removed from `havec`. */
static int /* Biff: 1 */
havecExtractFlagged(hestOpt *hopt, hestArgVec *havec, const hestParm *hparm) {
  if (!hest__flagOptNum(hopt)) {
    if (hparm->verbose > 1) {
      printf("%s: no flagged options; finishing up\n", __func__);
    }
    goto haveNoFlaggedOptions;
  }
  char ident1[AIR_STRLEN_SMALL + 1], ident2[AIR_STRLEN_SMALL + 1];
  uint len0 = havec->len;     // original number of args
  uint optNum = hopt->arrLen; // number of options (flagged or unflagged)
  airArray *mop = airMopNew();
  char *gotArgsStr = hestArgVecSprint(havec, MY_SHAI);
  airMopAdd(mop, gotArgsStr, airFree, airMopAlways);

  uint argIdx = 0;
  hestOpt *theOpt = NULL;
  const char VPE[3] = {'-', MY_VAR_PARM_END_FLAG, '\0'}; // should be "-."
  int lookVPE = hparm->respectDashPeriodMeansEnd;
  /* txfNum is # of time args have been transferred to a flagged option but not
     # of options seen (which could be less; an option might have been repeated) */
  uint txfNum = 0;
  while (argIdx < havec->len) { // NOTE: havec->len may decrease within an interation!
    if (hparm->verbose > 1) {
      printf("%s: ------------- argIdx = %u (of %u) -> argv[argIdx] = |%s|\n", __func__,
             argIdx, havec->len, havec->harg[argIdx]->str);
    }
    /* If current arg is "-." that's ok: it could be ending of variadic parms for
       an unflagged option. It cannot be recognized as any flagged options flag
       because of checks in hest__OPCheck */
    uint optIdx = whichOptFlag(hopt, havec->harg[argIdx]->str, hparm);
    if (UINT_MAX == optIdx) {
      // havec->harg[argIdx]->str is not a flag for any option, move on to next arg
      if (hparm->verbose > 2) {
        printf("%s: |%s| not a flag arg, continue\n", __func__,
               havec->harg[argIdx]->str);
      }
      argIdx++;
      continue;
    }
    // else option with index optIdx aka theOpt has flag havec->harg[argIdx]->str
    theOpt = hopt + optIdx;
    if (hparm->verbose) {
      printf("%s: argv[%u]=|%s| is flag of opt %u \"%s\"\n", __func__, argIdx,
             havec->harg[argIdx]->str, optIdx, theOpt->flag);
      printf("%s: any associated parms?\n", __func__);
    }
    /* see if we can associate some parameters with the flag */
    /* k4pp/k4parse/k4v are only to implement modest cleverness in case of kind4 (this
       opt is a single variadic): don't consume an arg that obviously isn't valid for
       kind=4, since it probably belongs to an unflagged option. This goes against the
       general principle that extracting/consuming args is a game of moving strings
       around prior to any parsing.  For this kind=4 opt, we will still move a parm arg
       string around, but only if our sneaky parsing succeeded. */
    int kind4 = (4 == theOpt->kind);
    hest__PPair k4pp;
    k4pp.hopt = kind4 ? hopt : NULL;
    int (*k4parse)(void *, const char *,
                   hest__PPair *) = (kind4 ? hest__ParseSingle[theOpt->type] : NULL);
    double k4v[2]; // bigger than needed to store largest possible kind=4 value
    int varParm = (kind4 || 5 == theOpt->kind);
    int sawVPE = AIR_FALSE;     // caring about -. and saw it
    uint nextOptIdx = UINT_MAX, // what is index of option who's flag we saw next
      parmNum = 0;          // how many parm args have we see that plausibly belong to us
    int sawEnd = AIR_FALSE; // saw end of havec
    // compute parmNum by looking at but not moving args
    // (while we have less than the max number)
    while (AIR_INT(parmNum) < hest__Max(theOpt->max)) {
      // npai = next possible parm's arg index
      uint npai = argIdx + 1 + parmNum;
      // stop if we hit end of havec
      if ((sawEnd = !(npai < havec->len))) break;
      char *pastr = havec->harg[npai]->str; // candidate parm arg string
      if (varParm) {
        // stop if next arg is a "-." we need to respect
        if ((sawVPE = (lookVPE && !strcmp(VPE, pastr)))) break;
        // stop if is a kind=4 option and next arg doesn't parse as its type
        if (kind4 && k4parse(k4v, pastr, &k4pp)) break;
      }
      nextOptIdx = whichOptFlag(hopt, pastr, hparm);
      // stop if we're looking at start of another flagged option
      if (UINT_MAX != nextOptIdx) break;
      // finally: this passes as another parm arg for this option
      if (hparm->verbose)
        printf("%s: optIdx %u |%s|; argIdx %u < %u |%s| --> parmNum --> %u\n", __func__,
               optIdx, theOpt->flag, argIdx, npai, pastr, parmNum + 1);
      parmNum++;
    }
    // we've counted parmNum, stopping for any of the reasons described above
    if (hparm->verbose)
      printf("%s: optIdx %u |%s|: stopped w/ "
             "parmNum=%u sawEnd=%d lookVPE=%d sawVPE=%d nextOptIdx=%u\n",
             __func__, optIdx, theOpt->flag, parmNum, sawEnd, lookVPE, sawVPE,
             nextOptIdx);
    if (parmNum < theOpt->min) { // didn't see required min # parms
      biffAddf(HEST, "%s%sbefore getting %u parameter%s for %s (got only %u)", MY_ME,
               theOpt->min, theOpt->min > 1 ? "s" : "",
               hestOptIdentStr(ident1, sizeof(ident1), theOpt), parmNum);
      if (sawEnd) {
        biffAddf(HEST, "%s%shit end of args", MY_ME);
      } else if (sawVPE) {
        biffAddf(HEST, "%s%shit \"%s\" (variadic-parameter-end flag)", MY_ME, VPE);
      } else if (UINT_MAX != nextOptIdx) {
        biffAddf(HEST, "%s%ssaw %s flag", MY_ME,
                 hestOptIdentStr(ident2, sizeof(ident2), hopt + nextOptIdx));
      }
      if (txfNum) {
        char *haveArgStr = hestArgVecSprint(havec, MY_SHAI);
        airMopAdd(mop, haveArgStr, airFree, airMopAlways);
        biffAddf(HEST, "%s%sin remaining %sargv: %s", MY_ME,
                 MY_SHAI ? "(index: labeled) " : "", haveArgStr);
      }
      goto sidewaysF;
    }
    if (hparm->verbose) {
      printf("%s: ________ argv[%u]=|%s|: optIdx %u |%s| followed by %u parms\n",
             __func__, argIdx, havec->harg[argIdx]->str, optIdx, theOpt->flag, parmNum);
    }
    if (hparm->verbose > 1) {
      hestArgVecPrint(__func__, "main havec as it came", havec);
    }
    /* remember from whence this flagged option came, which is necessary when there are
    zero parms. For nonzero parmNum, havecTransfer will also (overriding this) set
    theOpt->source to every source of each arg extracted */
    theOpt->source = havec->harg[argIdx]->source;
    // lose the flag argument, then transfer the parm args
    hestArgNix(havecRemove(havec, argIdx));
    havecTransfer(theOpt, havec, argIdx, parmNum);
    txfNum += 1;
    if (sawVPE) {
      // lose the variadic-parameter-stop flag
      hestArgNix(havecRemove(havec, argIdx));
    }
    if (hparm->verbose) {
      char info[AIR_STRLEN_HUGE + 1];
      snprintf(info, sizeof(info),
               "main havec after extracting optIdx %u |%s| and %u parms", optIdx,
               theOpt->flag, parmNum);
      hestArgVecPrint(__func__, info, havec);
      snprintf(info, sizeof(info), "optIdx %u |%s|'s own havec", optIdx, theOpt->flag);
      hestArgVecPrint(__func__, info, theOpt->havec);
    }
    // do NOT increment argIdx
  }

  /* make sure that flagged options without default were given */
  for (uint opi = 0; opi < optNum; opi++) {
    theOpt = hopt + opi;
    if (!theOpt->flag) {
      // its unflagged, move along
      continue;
    } // else this is a flagged option we should have handled above
    int needing = (1 != theOpt->kind    // i.e. this kind of option can take a parm
                   && !(theOpt->dflt)); // and this option has no default
    if (hparm->verbose > 1) {
      printf("%s: flagged opt %u |%s| source = %s%s\n", __func__, opi, theOpt->flag,
             airEnumStr(hestSource, theOpt->source),
             needing ? " <-- w/ parm but w/out default" : "");
    }
    // if needs to be set but hasn't been
    if (needing && hestSourceUnknown == theOpt->source) {
      biffAddf(HEST, "%s%sdidn't get required (default-less) %s[%u]", MY_ME,
               hestOptIdentStr(ident1, sizeof(ident1), theOpt), opi);
      goto sidewaysF;
    }
  }

  airMopOkay(mop);
haveNoFlaggedOptions:
  if (hparm->verbose) {
    optAllPrint(__func__, "end of havecExtractFlagged", hopt);
    hestArgVecPrint(__func__, "end of havecExtractFlagged", havec);
  }
  return 0;

sidewaysF:
  /* There was previously so much repetition of biff calls, to give context to a
     just-detected error and prior to the return 1, that it was moved here.
     Weird but now the code can be much more concise. Same dubious trick used
     in other functions in this file. */
  if (txfNum) {
    biffAddf(HEST, "%s%safter getting %u args for %u other flagged option%s", MY_ME,
             len0 - havec->len, txfNum, 1 == txfNum ? "" : "s");
  }
  biffAddf(HEST, "%s%sstarting with %sargv: %s", MY_ME,
           MY_SHAI ? "(index: labeled) " : "", gotArgsStr);
  airMopError(mop);
  return 1;
}

/* havecExtractUnflagged()
Extracts the parameter args associated with all unflagged options (of `hestOpt *hopt`)
from the given `hestArgVec *havec` and (like havecExtractFlagged) extracts those args
and saves them in the corresponding hopt[].havec.  This also looks for -. as the end of
variadic parms.

This is the function that has to handle the tricky logic of allowing there to be
multiple unflagged options, only one of which may have a variadic number of parms
(and that one has to be extracted last).

If hparm->respectDashPeriodMeansEnd, this is where "-." (see MY_VAR_PARM_END_FLAG in
privateHest.h) is recognized as a marker for the end of a unflagged variadic parameter
option (in which case the "-." marker is removed from `havec`. */
static int /* Biff: 1 */
havecExtractUnflagged(hestOpt *hopt, hestArgVec *havec, const hestParm *hparm) {
  char ident[AIR_STRLEN_SMALL + 1];
  uint optNum = hopt->arrLen; // number of options (flagged or unflagged)
  uint Foptnum = hest__flagOptNum(hopt);
  uint UFoptnum = hest__unflagOptNum(hopt); // number of unflagged options
  airArray *mop = airMopNew();
  char *gotArgsStr = hestArgVecSprint(havec, MY_SHAI);
  airMopAdd(mop, gotArgsStr, airFree, airMopAlways);
  if (!UFoptnum) {
    // no unflagged options; we're done-ish
    goto finishUF;
  }
  // else we have unflagged options
  uint *ufOpi = AIR_CALLOC(UFoptnum, uint);
  assert(ufOpi);
  airMopAdd(mop, ufOpi, airFree, airMopAlways);
  uint upii = 0;            // index into ufOpi
  int haveVari = AIR_FALSE; // we have an unflagged variadic parm option
  uint ufVariOpi = optNum;  // index (haveVari) of the unflagged variadic parm option
  for (uint opi = 0; opi < optNum; opi++) {
    if (!hopt[opi].flag) {
      /* starting at beginning of option list,
         store option indices of unflagged options in ufOpi[] */
      if (5 == hopt[opi].kind) {
        // but stop at (single) variadic one
        haveVari = AIR_TRUE;
        ufVariOpi = opi;
        break;
      }
      ufOpi[upii] = opi;
      upii++;
    }
  }
  if (haveVari) {
    // have to finish ufOpi[] by working from end of option list
    for (uint ipo = 0; ipo < optNum; ipo++) {
      uint opi = optNum - 1 - ipo;
      if (!hopt[opi].flag && upii < UFoptnum) {
        // this option is unflagged, and we haven't stored all the unflagged options
        ufOpi[upii] = opi;
        upii++;
      }
    }
  }
  if (hparm->verbose) {
    printf("%s: consumption order of unflagged options (ufOpi):", __func__);
    for (upii = 0; upii < UFoptnum; upii++) {
      printf("  %s:%u", ufOpi[upii] < ufVariOpi ? "up" : "dn", ufOpi[upii]);
    }
    printf("\n");
  }
  /* now, if there is an unflagged variadic option (NOTE that hest__OPCheck()
     ensured that there is at most one of these), then ufVariOpi is its index in opt[].
     If there is no unflagged variadic option, ufVariOpi is optNum. */
  if (hparm->verbose) {
    if (haveVari) {
      printf("%s: DO have unflagged variadic and ufVariOpi = %u\n", __func__, ufVariOpi);
    } else {
      printf("%s: do NOT have unflagged variadic\n", __func__);
    }
  }

  /* grab parameters for all unflagged opts EXCEPT the variadic, with one
     loop that traverses the pre-variadic and post-variadic (as per ufOpi[]) */
  uint len0 = havec->len;
  int E = 0;
  for (upii = 0; upii < UFoptnum; upii++) {
    uint opi = ufOpi[upii];
    if (opi == ufVariOpi) { // this is the variadic
      break;
    }
    if (hparm->verbose) {
      printf("%s: looking at opi = %u kind %d\n", __func__, opi, hopt[opi].kind);
    }
    /* 2026 revisit simplified the previous confusingly subtle logic, in which whether
    an unflagged option consumed args depended on whether it had a default (and was
    complicated by needing vmin > 0, or else how can you say who said that there are 0
    parms given). Now: if there are enough args in havec to meet our minimum, we
    consume them. If there are args but not enough, that's an error. If there are no
    args, that's not an error but will be later (in `finishUF:` below) if options don't
    have defaults. */
    if (havec->len) {
      char *nowStr = hestArgVecSprint(havec, MY_SHAI);
      airMopAdd(mop, nowStr, airFree, airMopAlways);
      if (hopt[opi].min /* == max */ <= havec->len) {
        // we have enough or more than enough
        uint fromIdx;
        if (opi < ufVariOpi) {
          /* either ufVariOpi==optNum because we have no variadic option, in which case
           we only consume from the beginning, or we do have a variadic option and
           we're working up to it opi < ufVariOpi < optNum */
          fromIdx = 0;
        } else {
          // we consume from th the end
          fromIdx = havec->len - hopt[opi].min;
        }
        if (hparm->verbose > 1) hestArgVecPrint(__func__, "before transfer", havec);
        havecTransfer(hopt + opi, havec, fromIdx, hopt[opi].min);
        if (hparm->verbose > 1) hestArgVecPrint(__func__, "after transfer", havec);
      } else {
        biffAddf(HEST,
                 "%s%scannot get %u args for %s%s[%u] from only %u remaining arg%s: %s",
                 MY_ME, hopt[opi].min, !hopt[opi].dflt ? "default-less " : "",
                 hestOptIdentStr(ident, sizeof(ident), hopt + opi), opi, havec->len,
                 1 == havec->len ? "" : "s", nowStr);
        E = 1;
        break;
      }
    }
  }
  if (E) {
    biffAddf(HEST, "%s%safter getting %u args for %u other unflagged option%s", MY_ME,
             len0 - havec->len, upii, 1 == upii ? "" : "s");
    goto sidewaysUF;
  }
  // now handle the single unflagged variadic, if we have it
  if (haveVari) {
    const char VPE[3] = {'-', MY_VAR_PARM_END_FLAG, '\0'}; // should be "-."
    int lookVPE = hparm->respectDashPeriodMeansEnd;
    hestOpt *variOpt = hopt + ufVariOpi;
    uint minArg = variOpt->min; /* min < max for sure */
    uint maxArg = hest__Max(variOpt->max);
    if ((!minArg && !variOpt->dflt && !havec->len)) {
      /* min==0, there's no default, and nothing given:
         We meet the very particular circumstance where having no args should be
         taken to mean "I'm giving you 0 args" */
      hestArgVecReset(variOpt->havec);
      variOpt->source = hestSourceCommandLine; // actually just a guess!
      goto finishUF;
    } else if (!havec->len) {
      /* Otherwise having no args means there's really thing to do */
      goto finishUF;
    }
    // else havec->len > 0: we have args to collect for the unflagged variadic
    // start by resetting the per-option havec, and setting the source
    hestArgVecReset(variOpt->havec);
    variOpt->source = havec->harg[0]->source;
    if (hparm->verbose) {
      printf("%s: ufVariOpi=%u   min, have, max = %u %u %d (source now %s)\n", __func__,
             ufVariOpi, minArg, havec->len, maxArg,
             airEnumStr(hestSource, variOpt->source));
    }
    for (uint ai = 0; ai < minArg; ai++) {
      // (try to) consume the min args we need
      int noMore = !havec->len;
      int hitEnd = !noMore && (lookVPE && !strcmp(VPE, havec->harg[0]->str));
      if (noMore || hitEnd) {
        biffAddf(HEST,
                 "%s%s%s%s "
                 "before getting (min) %u args (from %s) for %s%s[%u]",
                 MY_ME, //
                 noMore ? "ran out of args" : "hit end-of-args marker ",
                 noMore ? "" : VPE, //
                 minArg, airEnumStr(hestSource, variOpt->source),
                 !(variOpt->dflt) ? "default-less " : "",
                 hestOptIdentStr(ident, sizeof(ident), variOpt), ufVariOpi);
        goto sidewaysUF;
      }
      hestArg *harg = havecRemove(havec, 0);
      hestArgVecAppendArg(variOpt->havec, harg);
      variOpt->source = harg->source;
    }
    // consume any remaining args, up to max
    for (uint ai = 0; ai < maxArg - minArg && havec->len; ai++) {
      hestArg *harg = havecRemove(havec, 0); // we own briefly harg before ...
      if (strcmp(VPE, harg->str) || !lookVPE) {
        // as long as this arg is not a consequential "-." save it
        hestArgVecAppendArg(variOpt->havec, harg); // ... handing it off to opt's havec
        variOpt->source = harg->source;
      } else {
        // ... else it is "-.": we consume but don't store it; we have to free it.
        hestArgNix(harg);
        // break now so that we only consume one "-."
        break;
      }
    }
  }

finishUF:
  // make sure that unflagged options without default were given
  for (upii = 0; upii < UFoptnum; upii++) {
    uint opi = ufOpi[upii];
    if (!(hopt[opi].dflt) && hestSourceUnknown == hopt[opi].source) {
      biffAddf(HEST, "%s%sdidn't get required (default-less) %s[%u]", MY_ME,
               hestOptIdentStr(ident, sizeof(ident), hopt + opi), opi);
      goto sidewaysUF;
    }
  }
  // currently it's an error to have un-accounted-for args left over
  if (havec->len) {
    biffAddf(HEST, "%s%shave %u unexpected arg%s %s\"%s\"", MY_ME, havec->len,
             havec->len > 1 ? "s," : "", havec->len > 1 ? "starting with " : "",
             havec->harg[0]->str);
    if (UFoptnum) {
      biffAddf(HEST, "%s%safter getting %u unflagged opts", MY_ME, UFoptnum);
    }
    goto sidewaysUF;
  }

  if (hparm->verbose) {
    optAllPrint(__func__, "end of havecExtractUnflagged", hopt);
    hestArgVecPrint(__func__, "end of havecExtractUnflagged", havec);
  }
  airMopOkay(mop);
  return 0;

sidewaysUF: // things have gone sideways
  if (airStrlen(gotArgsStr)) {
    biffAddf(HEST, "%s%sgot %s%sargv: %s", MY_ME, Foptnum ? "remaining " : "",
             MY_SHAI ? "(index: labeled) " : "", gotArgsStr);
  } else {
    biffAddf(HEST, "%s%sgot empty argv", MY_ME);
  }
  if (Foptnum) {
    biffAddf(HEST, "%s%safter handling %u flagged options", MY_ME, Foptnum);
  }
  airMopError(mop);
  return 1;
}

/* optProcessDefaults
All the command-line arguments (and any response files invoked therein) should now be
processed (by transferring the arguments to per-option hopt->havec arrays), but we need
to ensure that every option has information from which to set values. The per-option
hopt->dflt string is what we look to now, to finish setting per-option hopt->havec
arrays for all the options for which hopt->havec have not already been set.  We use
`!hopt->source` (aka hestSourceUnknown) as the indicator of not already being set.
*/
static int /* Biff: 1 */
optProcessDefaults(hestOpt *hopt, hestArg *tharg, hestInputStack *hist,
                   const hestParm *hparm) {
  char ident[AIR_STRLEN_SMALL + 1];
  uint optNum = hopt->arrLen;
  for (uint opi = 0; opi < optNum; opi++) {
    if (hparm->verbose) {
      printf(" -> %s incoming", __func__);
      optPrint(hopt + opi, opi);
    }
    if (hopt[opi].source) {
      /* the source is already set (to something other than hestSourceUnknown),
         so there's no need for using the default */
      continue;
    }
    hopt[opi].source = hestSourceDefault;
    if (1 == hopt[opi].kind) {
      /* There is no meaningful "default" for stand-alone flags (and in fact
         hopt[opi].dflt is enforced to be NULL) so there is no default string to
         tokenize, but we above set source to default for sake of completeness, and to
         signal that the flag was not given by user */
      goto nextopt;
    }
    hestOptIdentStr(ident, sizeof(ident), hopt + opi);
    /* havecExtractFlagged/havecExtractUnflagged have already given an error
    if there was a flagged/unflagged option without a default that was not given,
    so we don't need to check again if we have a default (we know we do) */
    if (hparm->verbose) {
      printf("%s: looking at %s[%u] default string |%s|\n", __func__, ident, opi,
             hopt[opi].dflt);
    }
    /* printf("!%s: hopt[%u].havec %p   havec->len = %u\n", __func__, opi,
       hopt[opi].havec, hopt[opi].havec->len); */
    /* Getting args from default strings is a little nuanced because of some
       exceptions, handled here, which side-step calling histProcess */
    if (5 == hopt[opi].kind && 0 == hopt[opi].min && !strlen(hopt[opi].dflt)) {
      /* A multiple variadic option allowing min==0 is getting its value from an empty
      (non-NULL) default string. An established hest idiom (certainly used by unu, e.g.
      unu resample -off) for variadic options with min=0 is to give a default as the ""
      empty string to mean: 0 values. In the old (pre-2025) code based on strtok:
      strtok would look at "" and see zero tokens and say: 0 values. Even if the intent
      (for, say, string-type values) was to say "the default value is 1 empty string",
      that can be expressed as "''" instead of "", which is clearer anyway. So we
      directly assert that we have 0 args. Also, this is why we set hopt[opi].source
      above to hestSourceDefault, so that we'd know the source even if it isn't
      apparent in any of the (non-existent) args. */
      if (hparm->verbose > 1) {
        printf("%s: empty default string for min=0 a multiple "
               "variadic option ==> 0 args\n",
               __func__);
      }
      airArrayLenSet(hopt[opi].havec->hargArr, 0);
    } else if (2 == hopt[opi].kind
               && (airTypeString == hopt[opi].type || airTypeOther == hopt[opi].type)) {
      /* We want exactly 1 string or "other". While it is tempting to say for consistency
      "then get on the hist stack and tokenize (with histProcess) your single string just
      like everyone else does", it is arguably more straightforward, for this single
      string (because there is no tokenization needed!) to just take the default string
      as it is be the single arg. This seems to mostly replicate pre-2025 hest behavior,
      but now we respect leading white-space (the old code only respected one out of
      multiple leading spaces). Side-stepping histProcess has the effect of allowing an
      empty (but non-NULL) string to be a single default value, which matters for how
      some unu commands use an empty string to mean "no value given" (i.e. to make a
      hestOpt actually optional) */
      airArrayLenSet(hopt[opi].havec->hargArr, 0);
      hestArgVecAppendString(hopt[opi].havec, hopt[opi].dflt);
    } else {
      // the option's default string should be processed like normal
      if (histPushDefault(hist, hopt[opi].dflt, hparm)
          || histProcess(hopt[opi].havec, NULL, NULL, tharg, hist, hparm)) {
        biffAddf(HEST, "%s%sproblem tokenizing %s[%u] default string", MY_ME, ident,
                 opi);
        return 1;
      }
    }
    /* havecExtractFlagged and havecExtractUnflagged have done the work of ensuring
    that the minimum number of parm args have been extracted for each option. We should
    do something analogous for args tokenized from the default strings. */
    if (hopt[opi].havec->len < hopt[opi].min) {
      biffAddf(HEST,
               "%s%s%s[%u] default string \"%s\" supplied %u arg%s but need at least %u",
               MY_ME, ident, opi, hopt[opi].dflt, hopt[opi].havec->len,
               hopt[opi].havec->len > 1 ? "s" : "", hopt[opi].min);
      return 1;
    }
  nextopt:
    if (hparm->verbose) {
      printf("<-  %s: outgoing", __func__);
      optPrint(hopt + opi, opi);
    }
  }
  /* actually this seems to be entirely unreachable, but keeping for posterity
  for (uint opi = 0; opi < optNum; opi++) {
    hestOptIdentStr(ident, sizeof(ident), hopt + opi);
    / * (Yes, half of this test is redundant with check above on whether the default
    string supplied at least hopt[opi].min args, but erring with doing more checks).
    Now that we've made a hopt[opi].havec array, and will soon parse strings to set
    values, we can check that the number of args matches what the option needs * /
    int maxArg = hest__Max(hopt[opi].max);
    int haveArg = AIR_INT(hopt[opi].havec->len);
    if (!(AIR_INT(hopt[opi].min) <= haveArg && haveArg <= maxArg)) {
      biffAddf(HEST,
               "%s%s%s[%u] got (from user or from default) %u args, but that is "
               "outside [min,max]=[%u,%d] range",
               MY_ME, ident, opi, hopt[opi].havec->len, hopt[opi].min, maxArg);
      return 1;
    }
  } */
  return 0;
}

/* optSetValues
Finally: the parsing of hopt[opi].havec to set values in whatever hopt[opi].valueP
points to. No caller or parent mop is passed anymore, since the new per-hestOpt
parseMop can remember what needs to be freed. The return value from this is (except for
an instance of hestOpt kind confusion) in turn the return value from whichever
hest__ParseSingle[] function first returned non-zero to indicate a parsing error. That
return could be anything non-zero, but the Biff: annotations currently do not have a
way of expressing that. */
static int /* Biff: 1 # BUT Actually maybe not; see above */
optSetValues(hestOpt *hopt, const hestParm *hparm) {
  char ident[AIR_STRLEN_SMALL + 1];
  void *valueP;
  uint optNum = hopt->arrLen;

  airArray *mop = airMopNew();
  uint ooi = 0, *optOrder = AIR_CALLOC(optNum, uint);
  airMopAdd(mop, optOrder, airFree, airMopAlways);
  int priMin, priMax;
  hest__optPriorityMinMax(&priMin, &priMax, hopt);
  // hest__OPCheck has ensured that [priMin,priMax] range is reasonable
  for (int pri = priMax; pri >= priMin; pri--) {
    for (uint opi = 0; opi < optNum; opi++) {
      if (pri == hopt[opi].priority) {
        optOrder[ooi++] = opi;
      }
    }
  }
  if (hparm->verbose > 1 && priMin != priMax) {
    printf("%s: option parsing order:\n", __func__);
    for (uint ooi = 0; ooi < optNum; ooi++) {
      printf("   %u : %u\n", ooi, optOrder[ooi]);
    }
  }

  for (uint ooi = 0; ooi < optNum; ooi++) {
    uint opi = optOrder[ooi];
    int pret;
    hestOptIdentStr(ident, sizeof(ident), hopt + opi);
    // hopt[opi].source has already been set
    /* 2023 GLK notes that r6388 2020-05-14 GLK was asking:
         How is it that, once the command-line has been parsed, there isn't an
         easy way to see (or print, for an error message) the parameter (or
         concatenation of parameters) that was passed for a given option?
    and it turns out that adding this was as simple as adding this one following
    line. The inscrutability of the hest code (or really the self-reinforcing
    learned fear of working with the hest code) seems to have been the barrier.
    (2025 GLK notes that the fear was justified, given how long the re-write took ...)
    (2026 GLK quietly nods his head ...) */
    if (4 != hopt[opi].kind) {
      // single variadic options are weird, handled differently below
      hopt[opi].parmStr = hestArgVecSprint(hopt[opi].havec, AIR_FALSE);
      /* since 2025 havec adoption, not: airStrdup(optParms[opi]); */
    }
    int type = hopt[opi].type;
    size_t size = (airTypeEnum == type /* */
                     ? sizeof(int)
                     : (airTypeOther == type /* */
                          ? hopt[opi].CB->size
                          : hest__TypeSize[type]));
    valueP = hopt[opi].valueP;
    if (hparm->verbose) {
      printf("%s: hopt[%u/%u]: havec_%c|%s| \t|%s| \t--> kind=%d, type=%d, size=%u\n",
             __func__, opi, optNum, airEnumStr(hestSource, hopt[opi].source)[0],
             hopt[opi].parmStr, ident, hopt[opi].kind, type, (unsigned int)size);
    }
    /* we may over-write these */
    if (hopt[opi].sawP) {
      *(hopt[opi].sawP) = 0;
    }
    hest__PPair _hpp;
    hest__PPair *hpp;
    if (1 == hopt[opi].kind) {
      hpp = NULL;
    } else {
      hpp = &_hpp;
      hpp->hopt = hopt + opi;
      // hpp->err set by parseSingleT
    }
    switch (hopt[opi].kind) {
    case 1: { // -------- parameter-less boolean flags --------
      /* valueP is always assumed to be an int* */
      int *ivalueP = (int *)valueP;
      *ivalueP = hestSourceDefault != hopt[opi].source;
      if (hparm->verbose) {
        printf("   --> set value %d\n", *ivalueP);
      }
      hopt[opi].parseErr = 0; /* no parser actually called but plausible fib */
    } break;
    case 4: { // -------- optional single variadics --------
      const char *strsrc = NULL;
      int invert = AIR_FALSE;
      /* 2025 hest__OPCheck now restricts possible types; these are no longer allowed:
         airTypeChar, airTypeString, airTypeEnum, airTypeOther. As for the semantics,
         the old web page seems to be clear (though we now disallow unflagged kind 4):
           - option flag does not appear
             --> value is set from the default
           - option flag appears, but with no parm
             --> default is parsed, say, as value V, then the value is set to !V
                (*this* is the mysterious "inversion" mentioned at various places in
                 the old code)
           - option flag appears, with single parm
             --> value is set from parsing that parm
         In any case, some string has to be parsed; we call it `strsrc` */
      if (hestSourceDefault == hopt[opi].source) {
        // option flag does not appear
        strsrc = hopt[opi].dflt;
        invert = AIR_FALSE;
      } else if (hestSourceDefault != hopt[opi].source && 0 == hopt[opi].havec->len) {
        // option flag appears, but with no parm
        strsrc = hopt[opi].dflt;
        invert = AIR_TRUE;
      } else if (hestSourceDefault != hopt[opi].source && 1 == hopt[opi].havec->len) {
        // option flag appears, with single parm
        strsrc = hopt[opi].havec->harg[0]->str;
        invert = AIR_FALSE;
      } /* actually unreachable given all the other prior tests
         else {
           biffAddf(HEST, "%s%sconfused by (kind=4) %s[%u] source %s and havec->len
         %u", MY_ME, ident, opi, airEnumStr(hestSource, hopt[opi].source),
                    hopt[opi].havec->len);
           return 1;
         } */
      if ((pret = hest__ParseSingle[type](valueP, strsrc, hpp))) {
        biffAddf(HEST, "%s%sfor (kind=4) %s[%u] could not parse |%s| as single %s",
                 MY_ME, ident, opi, strsrc, hest__TypeStr[type]);
        hopt[opi].parseErr = pret;
        hopt->parseErrOptIdx = AIR_INT(opi);
        airMopError(mop);
        return pret;
      }
      if (invert) {
        hest__InvertScalar[type](valueP);
      }
      // special handling of parmStr for kind 4
      hopt[opi].parmStr = airStrdup(strsrc);
    } // end case 4 {
    break;
    case 2: // -------- one required parameter --------
      if ((pret = hest__ParseSingle[type](valueP, hopt[opi].havec->harg[0]->str, hpp))) {
        // hpp->err as just generated can have newlines
        // (from %s) . . .  airEnumStr(hestSource, hopt[opi].source)
        biffAddf(HEST, "%s%s%s", MY_ME, hpp->err);
        biffAddf(HEST, "%s%sproblem parsing for %s[%u]", MY_ME, ident, opi);
        hopt[opi].parseErr = pret;
        hopt->parseErrOptIdx = AIR_INT(opi);
        airMopError(mop);
        return pret;
      }
      break;
    case 3: { // -------- multiple required parameters --------
      // user already allocated array at valueP
      char *cvalueP = (char *)valueP;
      for (uint argi = 0; argi < hopt[opi].havec->len; argi++) {
        if ((pret = hest__ParseSingle[type](cvalueP + size * argi,
                                            hopt[opi].havec->harg[argi]->str, hpp))) {
          biffAddf(HEST, "%s%s%s", MY_ME, hpp->err);
          biffAddf(HEST, "%s%sproblem parsing arg[%u] (of %u) for %s[%u]", MY_ME, argi,
                   hopt[opi].havec->len, ident, opi);
          hopt[opi].parseErr = pret;
          hopt->parseErrOptIdx = AIR_INT(opi);
          airMopError(mop);
          return pret;
        }
      }
    } break;
    // (case 4 handled above)
    case 5: { // -------- multiple variadic parameters --------
      // we allocate the array to hold multiple values
      // initialize pointer to NULL in case we have zero
      *((void **)valueP) = NULL;
      if (airTypeString == type) {
        /* this is sneakiness: we allocate one more element so that
           the resulting char** is, like argv, NULL-terminated */
        *((void **)valueP) = calloc(hopt[opi].havec->len + 1, size);
      } else if (hopt[opi].havec->len) {
        // only allocate if there's a need to
        *((void **)valueP) = calloc(hopt[opi].havec->len, size);
      }
      if (*((void **)valueP)) {
        hopt[opi].parseMop = airMopNew();
        airMopMem(hopt[opi].parseMop, valueP, airMopAlways);
      }
      if (hopt[opi].sawP) {
        /* quiet clang-tidy; non-NULLity enforced by methodsHest.c/hest__OPCheck() */
        *(hopt[opi].sawP) = hopt[opi].havec->len;
      }
      char *cvalueP = *((void **)valueP);
      // (RIP hammerhead.ucsd.edu, Intel Itanium, and last days of grad school)
      for (uint argi = 0; argi < hopt[opi].havec->len; argi++) {
        if ((pret = hest__ParseSingle[type](cvalueP + size * argi,
                                            hopt[opi].havec->harg[argi]->str, hpp))) {
          biffAddf(HEST, "%s%s%s", MY_ME, hpp->err);
          biffAddf(HEST, "%s%sproblem parsing arg[%u] (of %u) for (kind=5) %s[%u]",
                   MY_ME, argi, hopt[opi].havec->len, ident, opi);
          hopt[opi].parseErr = pret;
          hopt->parseErrOptIdx = AIR_INT(opi);
          airMopError(mop);
          return pret;
        }
      }
      if (airTypeString == type) {
        // finish NULL-termination sneakiness
        char **argv = (char **)cvalueP;
        if (argv && *argv) {
          // quiet clang-tidy: non-NULLity enforced elsewhere
          argv[hopt[opi].havec->len] = NULL;
        }
      }
    } break;
    } // end switch
  } // for opi ...
  airMopOkay(mop);
  return 0;
}

/* hestParse  (the 2025/2026 hestParse re-write)
Parse the `argc`,`argv` commandline according to the hestOpt array `hopt`, and as
tweaked by settings in (if non-NULL) the given `hestParm *_hparm`.  If there is an
error, an error message string describing it in detail is generated and
 - if errP: *errP is set to the newly allocated error message string
   NOTE: it is the caller's responsibility to free(*errP) later
 - if !errP: the error message is fprintf'ed to stderr

The main phases of parsing are (this is the same numbering as in 0-README.md):

1) Error checking on given `hopt` array.  If this fails (i.e. because the previous
calls to hestOptAdd were malformed), the return is -1, which is distict from
possible returns due to errors in later steps.

2) Generate internal representation of command-line that includes expanding any
response files; this all goes into the `hestArgVec *havec`.

3) From `havec`, extract/consume the args that are attributable to flagged ...
4) ... and unflagged options, moving each `hestArg` out of main `havec` and into
the per-hestOpt opt->havec

5) For options not user-supplied, process the opt's `dflt` string to set hopt->havec

6) Now, every option should have a hopt->havec set, regardless of where it came from.
So parse those per-opt args to set final values for the user to see, by calling
optSetValues; if it has a non-zero (error) return value return that from here.

What is allocated as result of work here should be freed by hestParseFree
*/
int /* Biff: nope # all biff use is internal */
hestParse(hestOpt *hopt, int argc, const char **argv, char **errP,
          const hestParm *_hparm) {
  airArray *mop = airMopNew(); // initialize the mop
  int pret;
  // make exactly one of (given) _hparm and (our) hparm non-NULL
  hestParm *hparm = NULL;
  if (!_hparm) {
    hparm = hestParmNew();
    airMopAdd(mop, hparm, hestParmFree_mop, airMopAlways);
  }
  // how to const-correctly use hparm or _hparm in an expression
#define HPARM (_hparm ? _hparm : hparm)
#if MY_ME_VERBOSE
// tweak of same "wacky hack" in privateHest.h, ~ using HPARM instead of hparm
#  define PME     ((HPARM)->verbose ? __func__ : ""), ((HPARM)->verbose ? ": " : "")
#  define PMEV(V) ((V) ? __func__ : ""), ((V) ? ": " : "")
#else
// Stub versions of the above
#  define PME     "", ""
#  define PMEV(V) "", ""
#endif

  if (HPARM->verbose > 1) {
    printf("%s: (%s) hparm->verbose %d\n", __func__, _hparm ? "given" : "default",
           HPARM->verbose);
  }

#if 0
  { // checking on the private airEnums
    char err[AIR_STRLEN_LARGE + 1];
    // static const airEnum *saenm[2] = {&argst_aenum, &nast_aenum};
    static const airEnum *saenm[1] = {&argst_aenum};
    for (uint ei = 0; ei < 1; ei++) {
      if (airEnumCheck(err, saenm[ei])) {
        fprintf(stderr, "%s: problem with enum \"%s\" (%u):\n%s", __func__,
                saenm[ei]->name, ei, err);
        exit(1); /* inside `#if 0` for airEnum checking */
      }
    }
  }
#endif

  int eret = 0;
  char *argvstr = NULL, *estr = NULL;
  // --1--1--1--1--1-- check on validity of the hestOpt array
  int phase = 1;
  if (hest__OPCheck(hopt, HPARM)) {
    biffAddf(HEST, "%s%sproblem with given hestOpt array%s", PME,
             HPARM->verbose ? " (phase 1)" : "");
    eret = -1;
    goto sidewaysParse;
  }
  if (HPARM->verbose > 1) {
    printf("%s: hest__OPCheck passed\n", __func__);
  }
  /* initialize things indicating parse error to say "none known"
     the magic -5 is used by the sidewaysParse exit code below, to avoid clobbering
     info about a phase 6 parse error with -10 times the failing phase */
  hopt[0].parseErrOptIdx = -5;
  for (uint opi = 0; opi < hopt->arrLen; opi++) {
    hopt[opi].parseErr = 0;
  }

  // allocate the state we use during parsing
  hestInputStack *hist = hestInputStackNew();
  airMopAdd(mop, hist, hestInputStackNix_mop, airMopAlways);
  hestArgVec *havec = hestArgVecNew();
  airMopAdd(mop, havec, hestArgVecNix_mop, airMopAlways);
  hestArg *tharg = hestArgNew(); // tmp hestArg
  airMopAdd(mop, tharg, hestArgNix_mop, airMopAlways);
  argvstr = hestArgcArgvSprint(argc, argv, AIR_FALSE);
  airMopAdd(mop, argvstr, airFree, airMopAlways);
  /* in case we have a parsing error, we make ourselves responsible for
     cleaning up the things allocated by parsing, prior to the error */
  airMopAdd(mop, hopt, hestParseFree_mop, airMopOnError);
  if (HPARM->verbose > 1) {
    printf("%s: parsing state allocated\n", __func__);
  }

  // --2--2--2--2--2-- initialize input stack w/ given argc,argv, process it
  phase = 2;
  // create the way to store argv remaining after "--"
  char **rmnArgv = NULL; // NOTE: NOT NULL-terminated, but hopt[0].remainArgv needs to be
  unsigned int rmnArgc;
  airPtrPtrUnion appu;
  appu.cp = &rmnArgv;
  airArray *remainArr = NULL;
  if (HPARM->respectDashDashMeansStop) {
    remainArr = airArrayNew(appu.v, &rmnArgc, sizeof(char *), 8);
    airMopAdd(mop, remainArr, airArrayNuke_mop, airMopAlways);
  }
  // else remainArr remains NULL to say: ignore "--"
  if (histPushCommandLine(hist, argc, argv, HPARM)
      || histProcess(havec, &(hopt->helpWanted), remainArr, tharg, hist, HPARM)) {
    biffAddf(HEST, "%s%sproblem with initial processing of command-line%s", PME,
             HPARM->verbose ? " (phase 2)" : "");
    eret = 1;
    goto sidewaysParse;
  }
  if (HPARM->verbose > 1) {
    // have finished input stack, what argvec did it leave us with?
    printf("%s: after histProcess, helpWanted = %d\n", __func__, hopt->helpWanted);
    hestArgVecPrint(__func__, "after histProcess", havec);
  }
  if (hopt->helpWanted) {
    // once the call for help is made, we respect it: clean up and return
    airMopOkay(mop);
    return 0;
  }

  int EE = 0;
  // --3--3--3--3--3-- extract args associated with flagged options
  if (!EE) phase = 3;
  if (!EE) EE = havecExtractFlagged(hopt, havec, HPARM);
  // --4--4--4--4--4-- extract args associated with unflagged options
  if (!EE) phase = 4;
  // this will detect extraneous args after unflagged opts are extracted
  if (!EE) EE = havecExtractUnflagged(hopt, havec, HPARM);
  if (EE) {
    if (HPARM->verbose) {
      biffAddf(HEST, "%s%sproblem extracting args for %sflagged options (phase %u)", PME,
               4 == phase ? "un" : "", phase);
    } else {
      biffAddf(HEST, "%s%sproblem extracting args for %sflagged options", PME,
               4 == phase ? "un" : "");
    }
    eret = 1;
    goto sidewaysParse;
  }

  /* --5--5--5--5--5-- process defaults strings for opts that weren't user-supplied
     Like havecExtract{,Un}Flagged, this builds up hopt->havec, but does not parse it
   */
  phase = 5;
  if (optProcessDefaults(hopt, tharg, hist, HPARM)) {
    biffAddf(HEST, "%s%sproblem with processing defaults%s", PME,
             HPARM->verbose ? " (phase 5)" : "");
    eret = 1;
    goto sidewaysParse;
  }

  // --6--6--6--6--6-- Finally, parse the args and set values
  phase = 6;
  if ((pret = optSetValues(hopt, HPARM))) {
    biffAddf(HEST, "%s%sproblem with parsing and setting values%s", PME,
             HPARM->verbose ? " (phase 6)" : "");
    eret = 1;
    goto sidewaysParse;
  }

  // if we got remaining args, record them in hopt[0]
  if (rmnArgv) {
    hopt[0].remainArgv = AIR_CALLOC(rmnArgc + 1 /* + 1 for NULL termination */, char *);
    assert(hopt[0].remainArgv);
    for (unsigned int ri = 0; ri < rmnArgc; ri++) {
      hopt[0].remainArgv[ri] = rmnArgv[ri];
    }
    hopt[0].remainArgv[rmnArgc] = NULL;
    hopt[0].remainArgc = rmnArgc;
  } else {
    hopt[0].remainArgv = NULL;
    hopt[0].remainArgc = 0;
  }
  hopt[0].parseErrOptIdx = -1; // "there was no error on any option"
  airMopOkay(mop);
  return 0;

sidewaysParse:
  if (argvstr) {
    biffAddf(HEST, "%s%sinitial given command-line: %s", PME, argvstr);
  }
  estr = biffGetDone(HEST);
  if (errP) {
    *errP = estr;
    // caller frees it, not us
  } else {
    fprintf(stderr, "%s: problem:\n%s", __func__, estr);
    airMopAdd(mop, estr, airFree, airMopAlways);
  }
  if (-5 == hopt[0].parseErrOptIdx) hopt[0].parseErrOptIdx = -10 * phase;
  airMopError(mop);
  return eret;
#undef PMEV
#undef PME
#undef HPARM
}
