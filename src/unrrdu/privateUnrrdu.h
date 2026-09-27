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

#ifdef _WIN32
#  include <io.h>
#  include <fcntl.h>
#endif

/* NOTE: this #ifdef..#endif block copied to other src/<lib>/private<Lib>.h files */
#ifdef _WIN32
#  ifndef STDIN_FILENO
#    define STDIN_FILENO 0
#  endif
#  ifndef STDOUT_FILENO
#    define STDOUT_FILENO 1
#  endif
#  ifndef STDERR_FILENO
#    define STDERR_FILENO 2
#  endif
#  define isatty _isatty
#else
#  include <unistd.h> /* for isatty() and STDIN_FILENO */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* NOTE: these SN_,ASP1_ string macros copy-pasta'd to other private<Lib>.h files:
   SN_INCR: safely increments STR arg to point '\0'-termination of STR
   SN_COPY: safely copies SRC to DST, then increments DST
   SN_PRINTF: snprintf's into DST then increments DST
   ASP1_X: short version of AIR_STRLEN_X + 1 */
#define SN_INCR(STR, SIZE)                                                              \
  do {                                                                                  \
    size_t tmp_str_len_##STR = strlen(STR);                                             \
    STR += tmp_str_len_##STR;                                                           \
    SIZE -= tmp_str_len_##STR;                                                          \
  } while (0)
#define SN_COPY(DST, DST_SIZE, SRC)                                                     \
  do {                                                                                  \
    airStrcpy((DST), (DST_SIZE), (SRC));                                                \
    SN_INCR(DST, DST_SIZE);                                                             \
  } while (0)
#define SN_PRINTF(DST, DST_SIZE, FMT, ...)                                              \
  do {                                                                                  \
    snprintf((DST), (DST_SIZE), (FMT), __VA_ARGS__);                                    \
    SN_INCR(DST, DST_SIZE);                                                             \
  } while (0)
#define ASP1_S (AIR_STRLEN_SMALL + 1)
#define ASP1_M (AIR_STRLEN_MED + 1)
#define ASP1_L (AIR_STRLEN_LARGE + 1)
#define ASP1_H (AIR_STRLEN_HUGE + 1)

typedef unsigned int uint; /* uint is just more concise */

#define MY_UNRRDU_DILRR "UNRRDU_DEFAULT_IMPLICIT_LUT_RMAP_RESCALE"

#define MY_UNRRDU_DILRR_INFO(WUT)                                                       \
  "You can set the " MY_UNRRDU_DILRR " environment variable to "                        \
  "control whether, with the \"-r\" option is not used, to rescale "                    \
  "data values to the domain of a " WUT " with an implicit domain "                     \
  "(by default rescaling does happen in this case).\n "

/*
Twisted C-preprocessor tricks.  The idea is to make it as simple as possible to add new
commands to unu, so that the new commands have to be added to only one thing in this
source file, and the Makefile.

Associated with each unu command are some pieces of information: the single word command
(e.g. "slice") that is used by invoke it, the short (approx. one-line) description of its
function, and the "main" function to call with the appropriate argc, argv.  It would be
nice to use a struct to hold this information, and we can: the unrrduCmd struct is
defined above.  It would also be nice to have all the command's information be held in
one array of unrrduCmds. Unfortunately, declaring this is not possible unless all the
unrrduCmds and their fields are IN THIS FILE, because otherwise they're not constant
expressions, so they can't initialize an aggregate data type.  So, we instead make an
array of unrrduCmd POINTERS, which can be initialized with the addresses of individual
unrrduCmd structs, declared and defined in the global scope. is done in flotsam.c.  Each
of the source files for the various unu commands are responsible for setting the fields
(at compile-time) of their associated unrrduCmd.

We use three macros to automate this somewhat:
MY_UNRRDU_DECLARE: declares unrrdu_xxxCmd as an extern unrrduCmd
                   (defined in xxx.c), used later in this header file.
MY_UNRRDU_LIST:    the address of unrrdu_xxxCmd, for listing in the array of
                   unrrduCmd structs in the (compile-time) definition of
                   unrrduCmdList[].  This is used in flotsam.c.

Then, to facilitate running these macros on each of the different commands, there is a
UNRRDU_MAP macro which is used to essentially map the two macros above over the list of
unu commands.  Functional programming meets the C pre-processor.  Therefore:
***********************************************************
**  You add command foo to unu by:
**  1) adding F(foo) to definition of UNRRDU_MAP()
**  2) implement foo.c, and list foo.o in GNUmakefile and CmakeLists.txt
**  That's it.
**********************************************************
*/
#define MY_UNRRDU_DECLARE(C) UNRRDU_EXPORT const unrrduCmd unrrdu_##C##Cmd;
#define MY_UNRRDU_LIST(C)    &unrrdu_##C##Cmd,
#define MY_UNRRDU_MAP(F)                                                                \
  F(about)                                                                              \
  F(built)                                                                              \
  F(env)                                                                                \
  F(i2w)                                                                                \
  F(w2i)                                                                                \
  F(make)                                                                               \
  F(head)                                                                               \
  F(describe)                                                                           \
  F(history)                                                                            \
  F(data)                                                                               \
  F(convert)                                                                            \
  F(resample)                                                                           \
  F(fft)                                                                                \
  F(cmedian)                                                                            \
  F(dering)                                                                             \
  F(dist)                                                                               \
  F(minmax)                                                                             \
  F(cksum)                                                                              \
  F(diff)                                                                               \
  F(quantize)                                                                           \
  F(unquantize)                                                                         \
  F(project)                                                                            \
  F(slice)                                                                              \
  F(sselect)                                                                            \
  F(dice)                                                                               \
  F(splice)                                                                             \
  F(join)                                                                               \
  F(crop)                                                                               \
  F(acrop)                                                                              \
  F(inset)                                                                              \
  F(pad)                                                                                \
  F(reshape)                                                                            \
  F(permute)                                                                            \
  F(swap)                                                                               \
  F(shuffle)                                                                            \
  F(flip)                                                                               \
  F(unorient)                                                                           \
  F(basinfo)                                                                            \
  F(block)                                                                              \
  F(unblock)                                                                            \
  F(axinfo)                                                                             \
  F(axinsert)                                                                           \
  F(axsplit)                                                                            \
  F(axdelete)                                                                           \
  F(axmerge)                                                                            \
  F(tile)                                                                               \
  F(untile)                                                                             \
  F(histo)                                                                              \
  F(dhisto)                                                                             \
  F(jhisto)                                                                             \
  F(histax)                                                                             \
  F(heq)                                                                                \
  F(gamma)                                                                              \
  F(1op)                                                                                \
  F(2op)                                                                                \
  F(3op)                                                                                \
  F(affine)                                                                             \
  F(lut)                                                                                \
  F(mlut)                                                                               \
  F(subst)                                                                              \
  F(rmap)                                                                               \
  F(mrmap)                                                                              \
  F(imap)                                                                               \
  F(lut2)                                                                               \
  F(ccfind)                                                                             \
  F(ccadj)                                                                              \
  F(ccmerge)                                                                            \
  F(ccsettle)                                                                           \
  F(dnorm)                                                                              \
  F(vidicon)                                                                            \
  F(grid)                                                                               \
  F(ninspect)                                                                           \
  F(ilk)                                                                                \
  F(hack)                                                                               \
  F(aabplot)                                                                            \
  F(undos)                                                                              \
  F(uncmt)                                                                              \
  F(save)
/* xxx.c */
/* Declare the extern unrrduCmds unrrdu_xxxCmd, for all xxx.  These are
   defined in as many different source files as there are commands. */
MY_UNRRDU_MAP(MY_UNRRDU_DECLARE)

/*
******** MY_UNRRDU_CMD
**
** This is used at the very end of the various command sources
** ("xxx.c") to simplify defining a unrrduCmd.  "name" should just be
** the command, UNQUOTED, such as flip or slice.
*/
#define MY_UNRRDU_CMD(name, info)                                                       \
  const unrrduCmd unrrdu_##name##Cmd = {#name, info, unu_##name##Main, AIR_FALSE}
#define MY_UNRRDU_CMD_HIDE(name, info)                                                  \
  const unrrduCmd unrrdu_##name##Cmd = {#name, info, unu_##name##Main, AIR_TRUE}

/*
** OPT_ADD_XXX
**
** These macros are used for setting up command-line options for the various
** unu commands.  They define options which are common across many different
** commands, so that the unu interface is as consistent as possible.  They
** all assume a hestOpt *opt variable, but they take the option variable
** and option description as arguments.  The expected type of the variable
** is given before each macro.
*/
/* Nrrd *var */
#define MY_OPT_ADD_NIN(var, desc)                                                       \
  uint inHidx = hestOptAdd_1_Other(&hopt, "i,input", "nin", &(var), "-", desc,          \
                                   nrrdHestNrrdNoTTY);                                  \
  /* TeemV2 hestOpt.priority separates option parsing order from definition order;      \
     this allows the -i input to be early in definition order but parsed last */        \
  hopt[inHidx].priority -= 10

/* char *var
   NOTE this also creates `outHidx`: the index into `hopt` of the output option */
#define MY_OPT_ADD_NOUT(var, desc)                                                      \
  uint outHidx = hestOptAdd_1_String(&hopt, "o,output", "nout", &(var), "-", desc)

/* unsigned int var */
#define MY_OPT_ADD_AXIS(var, desc)                                                      \
  hestOptAdd_1_UInt(&hopt, "a,axis", "axis", &(var), NULL, desc)

/* int *var; int saw */
#define MY_OPT_ADD_BOUND(name, needmin, var, deflt, desc, saw)                          \
  hestOptAdd_Mv_Other(&hopt, name, "pos0", needmin, -1, &(var), deflt, desc, &(saw),    \
                      &unrrduUnuHestPosCB)

/* int var */
#define MY_OPT_ADD_TYPE(var, desc, dflt)                                                \
  hestOptAdd_1_Enum(&hopt, "t,type", "type", &(var), dflt, desc, nrrdType)

// for historyFuncs.c
#define MY_NULL_INSP   ((unrrduUnuInNSPair){.nin = NULL, .name = NULL})
// for unu history and historyFuncs.c
#define MY_HISTORY_KEY "teem_kvp_history"

/* adds line of key/value pair history, using both
   `inHidx` created by MY_OPT_ADD_NIN and
   `outHidx` created by MY_OPT_ADD_NOUT.
   This ONLY handles (common) case of single input and single output */
#define MY_HISTORY(NOUT, NIN)                                                           \
  unrrduUnuHistoryClear((NOUT)); /* needed since likely at some point earlier did */    \
                                 /* nrrdCopy(nout, nin) */                              \
  if (unrrduUnuHistoryAdd((unrrduUnuOutNSPair){.nout = (NOUT),                          \
                                               .name = hopt[outHidx].parmStr},          \
                          argc + 1, argv - 1,                                           \
                          (unrrduUnuInNSPair){.nin = (NIN),                             \
                                              .name = hopt[inHidx].parmStr})) {         \
    airMopAdd(mop, err = biffGetDone(UNRRDU), airFree, airMopAlways);                   \
    fprintf(stderr, "%s: error adding history %s\n", me, err);                          \
    airMopError(mop);                                                                   \
    return 1;                                                                           \
  }

#define MY_SAVE(OUTS, NOUT, IO)                                                         \
  if (nrrdSave((OUTS), (NOUT), (IO))) {                                                 \
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);                     \
    fprintf(stderr, "%s: error saving nrrd to \"%s\":\n%s\n", me, (OUTS), err);         \
    airMopError(mop);                                                                   \
    return 1;                                                                           \
  }

#ifdef __cplusplus
}
#endif
