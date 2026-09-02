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

/*
 * hsetBuf sprints into `_buff` (of size `_buffSize`)
 * the flag and name part of the usage line for `hopt`,
 * e.g. -p,--position^I<pos^I...>
 * but note the tab characters ("^I") which is how to create non-breaking-space
 * for hestPrintStr
 */
static void
hsetBuf(char *_buff, size_t _buffSize, const hestOpt *hopt, /* */
        int showshort, int showlong) {
  char *buff = _buff;
  size_t buffSize = _buffSize;

  int max = hest__Max(hopt->max);
  if (hopt->flag) {
    char *sep, flagCopy[AIR_STRLEN_SMALL + 1];
    /* currently methodsHest.c/hest__OPCheck() verifies
       that strlen(hopt->flag) < AIR_STRLEN_SMALL/2 */
    airStrcpy(flagCopy, sizeof(flagCopy), hopt->flag);
    if ((sep = strchr(flagCopy, MY_MULTI_FLAG_SEP))) {
      *sep = 0;
      if (showshort) {
        SN_PRINTF(buff, buffSize, "-%s", flagCopy);
      }
      if (showlong) {
        if (showshort) {
          SN_PRINTF(buff, buffSize, "%c", MY_MULTI_FLAG_SEP);
        }
        SN_PRINTF(buff, buffSize, "--%s", sep + 1);
      }
    } else {
      SN_PRINTF(buff, buffSize, "-%s", hopt->flag);
    }
    if (hopt->min || max) {
      SN_COPY(buff, buffSize, "\t");
    }
  }
  int dobrack = (hopt->flag && !hopt->min && max) || (!hopt->flag && hopt->dflt);
  if (dobrack) {
    SN_COPY(buff, buffSize, "[");
  }
  if (hopt->min || max) {
    SN_PRINTF(buff, buffSize, "<%s", hopt->name);
    if ((int)(hopt->min) < max && max > 1) { /* HEY scrutinize casts */
      SN_COPY(buff, buffSize, "\t...");
    }
    SN_COPY(buff, buffSize, ">");
  }
  if (dobrack) {
    SN_COPY(buff, buffSize, "]");
  }
  return;
}

/* early version of hsetBuf() function preserved for nostalgia
 * #define SETBUFF(B, O)                                                              \
 *   strcat(B, O.flag ? "-" : ""), strcat(B, O.flag ? O.flag : ""),                   \
 *     strcat(B, O.flag && (O.min || _hestMax(O.max)) ? "\t" : ""),                   \
 *     strcat(B, !O.min && _hestMax(O.max) ? "[" : ""),                               \
 *     strcat(B, O.min || _hestMax(O.max) ? "<" : ""),                                \
 *     strcat(B, O.min || _hestMax(O.max) ? O.name : ""),                             \
 *     strcat(B, (O.min < _hestMax(O.max) && (_hestMax(O.max) > 1)) ? " ..." : ""),   \
 *     strcat(B, O.min || _hestMax(O.max) ? ">" : ""),                                \
 *     strcat(B, !O.min && _hestMax(O.max) ? "]" : "");
 */

/*
 ******** hestPrintStr()
 *
 * prints a string `_str` to `file` one ' '-delimited word at a time, with lines that
 * fit within `width`, with some extra tricks:
 * - multiple adjacent ' ' are collapsed to one space (by the mechanism of
 *   ' '-delimiting), but tabs '\t' are converted into non-collapsed spaces
 * - indent new paragraphs by `indent` space
 * - if the final character of a word is '\n', that marks a paragraph end
 * - assumes that the on the first line already `already` chars have been printed
 * - if the line has to wrap around, then print a '\' iff `bslash`
 */
void
hestPrintStr(FILE *file, size_t indent, size_t already, size_t width, const char *_str,
             int bslash) {
  char *str = airStrdup(_str);
  uint wrdNum = airStrntok(str, " ");
  size_t pos = already;
  char *last;

  for (uint wrdIdx = 0; wrdIdx < wrdNum; wrdIdx++) {
    /* we used airStrtok() to delimit words on spaces ... */
    char *ws = airStrtok(!wrdIdx ? str : NULL, " ", &last);
    /* ... but then convert tabs to spaces */
    airStrtrans(ws, '\t', ' ');
    // fprintf(stderr, "!%s:\nws[%u]=|%s|\n\n", __func__, wrdIdx, ws);
    size_t slws = strlen(ws);
    if (pos + 1 + slws <= width - !!bslash) {
      /* if this word would still fit on the current line */
      if (wrdIdx) fputc(' ', file);
      fprintf(file, "%s", ws);
      pos += 1 + slws;
    } else {
      /* else we start a new line and print the indent */
      if (bslash) {
        fprintf(file, " \\");
      }
      fputc('\n', file);
      for (size_t ii = 0; ii < indent; ii++) {
        fputc(' ', file);
      }
      fprintf(file, "%s", ws);
      pos = indent + slws;
    }
    /* if the last character of the word was a newline, then indent */
    if ('\n' == ws[slws - 1]) {
      for (size_t ii = 0; ii < indent; ii++) {
        fputc(' ', file);
      }
      pos = indent;
    }
  }
  fputc('\n', file);
  free(str);
}

/* how to const-correctly use hparm or _hparm in an expression */
#define HPARM (_hparm ? _hparm : hparm)

void
hestInfo(FILE *file, const char *argv0, const char *info, const hestParm *_hparm) {
  if (info) {
    if (!argv0) {
      fprintf(file, "%s: ERROR: hestInfo got NULL argv0\n", __func__);
      return;
    }
    hestParm *hparm = _hparm ? NULL : hestParmNew();
    fprintf(file, "\n%s: ", argv0);
    hestPrintStr(file, HPARM->indent, strlen(argv0) + 2, HPARM->columns, info,
                 AIR_FALSE);
    fprintf(file, "\n");
    hestParmFree(hparm); // NULL hparm ok
  }
  return;
}

// error string song and dance
#define DO_ERR                                                                          \
  char *err = biffGetDone(HEST);                                                        \
  fprintf(stderr, "%s: problem with given hestOpt array\n%s", __func__, err);           \
  free(err)

/*
 ******** hestUsage
 *
 * Prints just the usage line to `file`
 * New for TeemV2: `oneline` prevents printing "Usage:" on one line and the usage line on
 * the next
 */
void
hestUsage(FILE *file, const hestOpt *hopt, const char *argv0, const hestParm *_hparm,
          int oneline) {
  /* with a very large number of options, it is possible to overflow buff[].
  Previous to the 2023 revisit, it was for max lenth 2*AIR_STRLEN_HUGE, but
  test/ex6.c blew past that.  May have to increment again in the future :)
  OR, just do the right thing and dynamically allocate it.
  In any case we are safe about string usage; the only problem would be that
  usage info is truncated, this function avoids any buffer overflow. */
#define LIKE_SO_SO_HUGE (64 * AIR_STRLEN_HUGE + 1)
  char _buff[LIKE_SO_SO_HUGE];
  hestParm *hparm = _hparm ? NULL : hestParmNew();

  if (hest__OPCheck(hopt, HPARM)) {
    /* we can't continue; the opt array is botched */
    DO_ERR;
    if (hparm) {
      hestParmFree(hparm);
    }
    return;
  }

  uint numOpts = hestOptNum(hopt);
  char *buff = _buff;
  size_t buffSize = LIKE_SO_SO_HUGE, already, indent;
  if (oneline) {
    SN_COPY(buff, buffSize, "Usage: ");
    already = 7; // = strlen("Usage: ")
    indent = 7;
  } else {
    SN_COPY(buff, buffSize, "Usage:\n ");
    already = 0;
    indent = HPARM->indent;
  }
  SN_COPY(buff, buffSize, argv0 ? argv0 : "");
  if (HPARM->responseFileEnable) {
    SN_PRINTF(buff, buffSize, " [%cfile\t...]", MY_RESPONSE_FILE_FLAG);
  }
  for (uint opi = 0; opi < numOpts; opi++) {
    SN_COPY(buff, buffSize, " ");
    int dobrack = (1 == hopt[opi].kind || (hopt[opi].flag && hopt[opi].dflt));
    if (dobrack) {
      SN_COPY(buff, buffSize, "[");
    }
    hsetBuf(buff, buffSize, hopt + opi, /* */
            AIR_TRUE /* showshort */, AIR_TRUE /* showlong */);
    SN_INCR(buff, buffSize);
    if (dobrack) {
      SN_COPY(buff, buffSize, "]");
    }
  }

  hestPrintStr(file, indent, already, HPARM->columns, _buff, AIR_TRUE);
  if (hparm) {
    hestParmFree(hparm);
  }
#undef LIKE_SO_SO_HUGE
  return;
}

/* formerly airAtod, which is kind of like atof() */
static double
stod(const char *str) {
  double val = 0.0;

  airSingleSscanf(str, "%lf", &val);
  /* return value ignored, but trusting sscanf-like behavior that
     if a value could not be parsed, then &val is not set */
  return val;
}

void
hestGlossary(FILE *file, const hestOpt *hopt, const hestParm *_hparm) {
  /* See note above about buff[] that is LIKE_SO_SO_HUGE
     (tl;dr big size is to avoid truncation not buffer overflow) */
#define LIKE_SO_SO_HUGE (64 * AIR_STRLEN_HUGE + 1)
  char _buff[LIKE_SO_SO_HUGE], tmpS[AIR_STRLEN_HUGE + 1];
  char *buff;
  size_t len, buffSize;
  const size_t _buffSize = LIKE_SO_SO_HUGE;
  hestParm *hparm = _hparm ? NULL : hestParmNew();

  if (hest__OPCheck(hopt, HPARM)) {
    /* we can't continue; the opt array is botched */
    DO_ERR;
    if (hparm) {
      hestParmFree(hparm);
    }
    return;
  }

  unsigned int optNum = hestOptNum(hopt);
  size_t maxlen = 0;
  if (optNum) {
    fprintf(file, "\nOptions:\n");
  }
  for (uint opi = 0; opi < optNum; opi++) {
    hsetBuf(_buff, _buffSize, hopt + opi, /* */
            AIR_TRUE /* showshort */, AIR_FALSE /* showlong */);
    size_t sblen = strlen(_buff);
    maxlen = AIR_MAX(sblen, maxlen);
  }
  /* fprintf(stderr, "\n!%s: maxlen = %zu (responseFileEnable %d)\n", __func__, maxlen,
          HPARM->responseFileEnable); */
  if (HPARM->responseFileEnable) {
    maxlen = AIR_MAX(9, maxlen); /* 9 = strlen("@file ...") */
    snprintf(_buff, _buffSize, "%cfile ...", MY_RESPONSE_FILE_FLAG);
    for (size_t jj = strlen(_buff); jj < maxlen; jj++) {
      fputc(' ', file);
    }
    fprintf(file, "%s = ", _buff);
    airStrcpy(_buff, _buffSize, "response file(s) containing command-line arguments");
    hestPrintStr(file, maxlen + 3, maxlen + 3, HPARM->columns, _buff, AIR_FALSE);
  }
  for (uint opi = 0; opi < optNum; opi++) {
    // fprintf(stderr, "!%s(%u/%u): HELLO\n", __func__, opi, optNum);
    hsetBuf(_buff, _buffSize, hopt + opi, /* */
            AIR_TRUE /* showshort */, AIR_FALSE /* showlong */);
    airOneLinify(_buff);
    len = strlen(_buff);
    for (size_t jj = len; jj < maxlen + HPARM->indent; jj++) {
      fputc(' ', file);
    }
    fprintf(file, "%s", _buff);
    fflush(file);
    // fprintf(stderr, "!%s(%u/%u): _buff=|%s|\n", __func__, opi, optNum, _buff);
    /* now accumulate into `buff` */
    buff = _buff;
    buffSize = _buffSize;
    buff[0] = '\0';
    if (hopt[opi].flag && strchr(hopt[opi].flag, MY_MULTI_FLAG_SEP)) {
      /* there is a long-form flag as well as short */
      hsetBuf(buff, buffSize, hopt + opi, /* */
              AIR_FALSE /* showshort */, AIR_TRUE /* showlong */);
      SN_INCR(buff, buffSize);
      SN_COPY(buff, buffSize, " = ");
      fprintf(file, " , ");
    } else {
      /* there is only a short-form flag */
      fprintf(file, " = ");
    }
    if (hopt[opi].info) {
      SN_COPY(buff, buffSize, hopt[opi].info);
    }
    if ((hopt[opi].min || hest__Max(hopt[opi].max))
        && (!(2 == hopt[opi].kind && airTypeEnum == hopt[opi].type
              && HPARM->elideSingleEnumType))
        && (!(2 == hopt[opi].kind && airTypeOther == hopt[opi].type
              && HPARM->elideSingleOtherType))) {
      /* if there are newlines in the info, then we want to clarify the
         type by printing it on its own line */
      if (hopt[opi].info && strchr(hopt[opi].info, '\n')) {
        SN_COPY(buff, buffSize, "\n ");
      } else {
        SN_COPY(buff, buffSize, " ");
      }
      SN_COPY(buff, buffSize, "(");
      if (hopt[opi].min == 0 && hest__Max(hopt[opi].max) == 1) {
        SN_COPY(buff, buffSize, "optional\t");
      } else {
        if ((int)hopt[opi].min == hest__Max(hopt[opi].max)
            && hest__Max(hopt[opi].max) > 1) { /* HEY scrutinize casts */
          SN_PRINTF(buff, buffSize, "%d\t", hest__Max(hopt[opi].max));
        } else if ((int)hopt[opi].min
                   < hest__Max(hopt[opi].max)) { /* HEY scrutinize casts */
          if (-1 == hopt[opi].max) {
            SN_PRINTF(buff, buffSize, "%u\tor\tmore\t", hopt[opi].min);
          } else {
            SN_PRINTF(buff, buffSize, "%u..%d\t", hopt[opi].min,
                      hest__Max(hopt[opi].max));
          }
        }
      }
      SN_PRINTF(buff, buffSize, "%s%s)",
                (airTypeEnum == hopt[opi].type
                   ? hopt[opi].enm->name
                   : (airTypeOther == hopt[opi].type ? hopt[opi].CB->type
                                                     : hest__TypeStr[hopt[opi].type])),
                (hest__Max(hopt[opi].max) > 1
                   ? (airTypeOther == hopt[opi].type
                          && 'y' == hopt[opi].CB->type[airStrlen(hopt[opi].CB->type) - 1]
                          && HPARM->cleverPluralizeOtherY
                        ? "\bies"
                        : "s")
                   : ""));
    }
    // fprintf(stderr, "!%s: HPARM->elideSingleOtherDefault = %d\n",
    //         "hestGlossary", HPARM->elideSingleOtherDefault);
    if (hopt[opi].dflt                                 // there is a default string
        && (hopt[opi].min || hest__Max(hopt[opi].max)) // opt has values to parse
        /* below is &&-ing together the !-negations of reasons to NOT show the default */
        && (!(2 == hopt[opi].kind
              && (airTypeFloat == hopt[opi].type || airTypeDouble == hopt[opi].type)
              && !AIR_EXISTS(stod(hopt[opi].dflt)) // dflt is nan, inf, or -inf
              && HPARM->elideSingleNonExistFloatDefault))
        && (!(2 == hopt[opi].kind               //
              && airTypeOther == hopt[opi].type //
              && HPARM->elideSingleOtherDefault))
        && (!(2 == hopt[opi].kind                //
              && airTypeString == hopt[opi].type //
              && 0 == airStrlen(hopt[opi].dflt)  //
              && HPARM->elideSingleEmptyStringDefault))
        && (!((3 == hopt[opi].kind || 5 == hopt[opi].kind)
              && (airTypeFloat == hopt[opi].type || airTypeDouble == hopt[opi].type)
              && !AIR_EXISTS(stod(hopt[opi].dflt)) //
              && HPARM->elideMultipleNonExistFloatDefault))
        && (!((3 == hopt[opi].kind || 5 == hopt[opi].kind)
              && airTypeString == hopt[opi].type //
              && 0 == airStrlen(hopt[opi].dflt)
              && HPARM->elideMultipleEmptyStringDefault))) {
      // (so we do show the default)
      /* if there are newlines in the info, then we want to clarify the
         default by printing it on its own line */
      if (hopt[opi].info && strchr(hopt[opi].info, '\n')) {
        SN_COPY(buff, buffSize, "\n ");
      } else {
        SN_COPY(buff, buffSize, "; ");
      }
      snprintf(tmpS, sizeof(tmpS), "default:\t\"%s\"", hopt[opi].dflt);
      if (HPARM->usageDefaultDisplayAvoidsWrapping) {
        airStrtrans(tmpS, ' ', '\t');
      }
      SN_COPY(buff, buffSize, tmpS);
    }
    hestPrintStr(file, maxlen + 3 + HPARM->indent, maxlen + 3, HPARM->columns, _buff,
                 AIR_FALSE);
  }
  if (hparm) {
    hestParmFree(hparm);
  }
#undef LIKE_SO_SO_HUGE
  return;
}

#undef HPARM
