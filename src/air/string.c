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

#include "air.h"

/* removed for TeemV2:
 *  / * this has to default to false in order for airStrtok to be a
 *    functional substitute for strtok() * /
 *  int airStrtokQuoting = AIR_FALSE;
 */

/*
******** airStrdup()
**
** because they didn't put strdup() in ANSI.
** This will return NULL if given NULL.
*/
char *
airStrdup(const char *s) {
  char *ret;

  if (!s) {
    ret = NULL;
  } else {
    ret = (char *)malloc(strlen(s) + 1);
    if (ret) {
      strcpy(ret, s);
    }
  }
  return ret;
}

/*
******** airStrlen()
**
** just like strlen, but safe to call on NULL (for which return is 0)
*/
size_t
airStrlen(const char *s) {
  size_t ret;

  if (!s) {
    ret = 0;
  } else {
    ret = strlen(s);
  }
  return ret;
}

/* ---- BEGIN non-NrrdIO */
/*
(possibly) truncates a given string `str` stored in a buffer of size `bsize`
to ensure that its strlen does not exceed bsize-1-drop.  But main purpose is
to hide compile-time info about static buffer sizes to avoid warnings about:
snprintf ...'%s' directive output may be truncated writing up to ... */
char *
airStrunc(char *str, size_t bsize, size_t drop) {
  if (str) {
    if (1 + drop < bsize) {
      str[bsize - (1 + drop)] = '\0';
    }
  }
  return str;
}

/*
******** airStrcmp
**
** like strcmp, but:
** safe to call with NULL strings,
** and, a NULL string is treated same as an empty string
*/
int
airStrcmp(const char *_s1, const char *_s2) {
  static const char empty[] = "";
  const char *s1, *s2;
  int ret;

  s1 = _s1 ? _s1 : empty;
  s2 = _s2 ? _s2 : empty;
  /*
  fprintf(stderr, "airStrcmp: _s1=|%s|->s1=|%s| _s2=|%s|->s2=|%s|\n",
          _s1 ? _s1 : "(NULL)", s1,
          _s2 ? _s2 : "(NULL)", s2);
  */
  ret = strcmp(s1, s2);
  return ret;
}

/* ---- END non-NrrdIO */
/*
******** airStrtok()
*
* thread-safe strtok() replacement.  Use just like strtok(), but on each call to parse a
* given string, pass as the last argument the address of a char*, to be used for saving
* state while the string is traversed.  Like strtok(), this will alter the "s" array
* passed to it on the first call, and like strtok(), this returns pointers into this
* string (rather than allocating new strings for each token).
*
* NOTE: TeemV2 removed global airStrtokQuoting and the code in here that used it.
*/
char *
airStrtok(char *s, const char *ct, char **last) {
  char *h, *e;

  if (!(ct && last)) {
    /* can't do any work, bail */
    return NULL;
  }
  h = s ? s : *last;
  if (!airStrlen(h)) return NULL;
  h += strspn(h, ct);
  e = h + strcspn(h, ct);
  if ('\0' == *e) {
    *last = e;
  } else {
    *e = '\0';
    *last = e + 1;
  }
  return h;
}

/*
******** airStrntok()
**
** returns the number of tokens parsable by airStrtok(), but does
** NOT alter the given string
*/
unsigned int
airStrntok(const char *_s, const char *ct) {
  char *s, *t, *l = NULL;
  unsigned int n = 0;

  if (_s && ct) {
    s = airStrdup(_s);
    t = airStrtok(s, ct, &l);
    while (t) {
      n++;
      t = airStrtok(NULL, ct, &l);
    }
    airFree(s); /* no NULL assignment to s, else compile warnings */
  }
  return n;
}

char *
airStrtrans(char *s, char from, char to) {
  size_t i, l;

  if (s) {
    l = strlen(s);
    for (i = 0; i < l; i++) {
      if (s[i] == from) {
        s[i] = to;
      }
    }
  }
  return s;
}

/*
 ******** airStrcpy
 *
 * Like strncpy but logic is different and more useful:
 *    `dst` is allocated for `dstSize` chars. Copy as much of src as can
 *    fit in `dst`, while always 0-terminating the result.
 * instead of strncpy's "Copy at most n characters, blah blah blah,
 * and you still have to 0-terminate the rest yourself".
 * Thus, instead of:
 *    snprintf(dst, dstSize, "%s", src)   or   snprintf(dst, dstSize, src)
 * you can just
 *    airStrcpy(dst, dstSize, src)
 * This is intended to be like the (non-standard) strlcpy, except that we
 * return the given `dst` pointer, rather than the # of chars written.
 *
 * With declaration `buff[AIR_STRLEN_SMALL+1]`, you call
 *    airStrcpy(buff, AIR_STRLEN_SMALL+1, src),
 * and know that this will not run off the end of `buff`,
 * and that strlen(buff) <= AIR_STRLEN_SMALL.
 *
 * Returns NULL if there was a problem (NULL dst or dstSize zero),
 * otherwise returns dst
 */
char *
airStrcpy(char *dst, size_t dstSize, const char *src) {

  if (!(dst && dstSize > 0)) {
    /* don't have usable `dst` to copy into */
    return NULL;
  }
  /* else dst && dstSize > 0 */
  if (!src || dstSize == 1) {
    /* got either NULL src (?) or have only space for null-termination */
    dst[0] = '\0';
  } else {
    /* have non-NULL src AND dstSize > 1 */
    size_t ii;
    for (ii = 0; src[ii]             /* have non-null character to copy */
                 && ii < dstSize - 1 /* can save to dst[ii] w/ space for final '\0' */
         ;
         ii++) {
      dst[ii] = src[ii];
    }
    dst[ii] = '\0';
  }
  return dst;
}

/*
******** airEndsWith
**
** if "s" ends with "suff", then returns 1, 0 otherwise
*/
int
airEndsWith(const char *s, const char *suff) {

  if (!(s && suff)) return 0;
  if (!(strlen(s) >= strlen(suff))) return 0;
  if (!strncmp(s + strlen(s) - strlen(suff), suff, strlen(suff)))
    return 1;
  else
    return 0;
}

/*
******** airUnescape()
**
** unescapes \\ and \n in place in a given string, which is the only form of
** escaping supported by key/value pairs in NRRD headers.
** Always returns the same pointer as given
*/
char *
airUnescape(char *s) {
  size_t i, j, len;
  int found = 0;

  len = airStrlen(s);
  if (!len) return s;

  for (i = 1, j = 0; i < len; i++, j++) {
    if (s[i - 1] == '\\' && s[i] == '\\') {
      s[j] = '\\';
      i++;
      found = 1;
    } else if (s[i - 1] == '\\' && s[i] == 'n') {
      s[j] = '\n';
      i++;
      found = 1;
    } else {
      s[j] = s[i - 1];
      found = 0;
    }
  }
  if (i == len || !found) s[j++] = s[len - 1];
  s[j] = 0;

  return s;
}

/*
******** airOneLinify()
**
** converts all contiguous white space (as determined by isspace()) to
** a single ' ', entirely removes non-printable (as determined by
** isprint()) characters, and entirely removes white space contiguous
** with the end of the string, even if that means shrinking the string
** to "".
**
** Useful for cleaning up lines of text to be saved as strings in
** fields of other structs.
**
** Whatever happens, this returns the pointer passed to it
*/
char *
airOneLinify(char *s) {
  size_t i, j, len;

  len = airStrlen(s);
  if (!len) return s;

  /* convert white space to space (' '), and delete unprintables */
  for (i = 0; i < len; i++) {
    if (isspace(AIR_INT(s[i]))) {
      s[i] = ' ';
      continue;
    }
    if (!isprint(AIR_INT(s[i]))) {
      for (j = i; j < len; j++) {
        /* this will copy the '\0' at the end */
        s[j] = s[j + 1];
      }
      i--;
      /* string got shorter (prevent ininite loop) */
      len--;
      continue;
    }
  }

  /* compress all contiguous spaces into one */
  for (i = 0; i < len; i++) {
    while (' ' == s[i] && ' ' == s[i + 1]) {
      for (j = i + 1; j < len; j++) {
        s[j] = s[j + 1];
      }
    }
  }

  /* lose trailing white space */
  i = airStrlen(s);
  if (' ' == s[i - 1]) {
    s[i - 1] = '\0';
  }

  return s;
}

/*
******** airToLower()
**
** calls tolower() on all characters in a string, and returns the same
** pointer that it was given
*/
char *
airToLower(char *str) {
  char *c;

  if (str) {
    c = str;
    while (*c) {
      *c = AIR_CAST(char, tolower(AIR_INT(*c)));
      c++;
    }
  }
  return str;
}

/*
******** airToUpper()
**
** calls toupper() on all characters in a string, and returns the same
** pointer that it was given
*/
char *
airToUpper(char *str) {
  char *c;

  if (str) {
    c = str;
    while (*c) {
      *c = AIR_CAST(char, toupper(AIR_INT(*c)));
      c++;
    }
  }
  return str;
}

/*
 ******** airOneLine()
 *
 * gets one line from `file`, putting it into an array of given size. `size` must be the
 * size of line buffer `line`: the size which `line` was allocated for, not the number of
 * non-null characters it was meant to hold.  `size` must be at least 3.  This always
 * null-terminates the contents of the array (except if the arguments are invalid).  The
 * idea is that the null-termination replaces the (text) line termination.
 *
 * Return values:
 * 0 and line[0]=='\0': immediately hit EOF, or arguments are invalid
 * 0 and line[0]!='\0': read something into `line` but hit EOF before seeing newline;
 *                      caller will need strlen() to find out its length
 *                   1: line was a single newline (or other line termination)
 *      N with N<=size: line was N-1 characters followed by newline
 *              size+1: didn't see a newline within size-1 characters
 *
 * So in normal cases, the return is the number of characters making up the line,
 * including the line termination
 *
 * In case of DOS\Windows\Cygwin: this will quietly pretend that "\r\n" pair is really
 * just "\n", including the way that characters making up the line are counted. However,
 * there is no pretension that on those platforms, "\n" by itself does not actually count
 * as a newline.
 *
 * Or, if trafficking in legacy Mac text files (for which the line termination is
 * only "\r", the same applies- these are also effectively treated the same as a newline.
 *
 * Uses ungetc(), respecting "man ungetc" info that one character of push-back is
 * guaranteed
 */
size_t
airOneLine(FILE *file, char *line, size_t size) {
  int cc = 0;
  size_t ii, ret;

  if (!(size >= 3 /* need room for a character and a Windows newline */
        && line && file)) {
    if (line) line[0] = '\0';
    return 0;
  }
  /* cc is always set at least once, but not so for any char in line[]  */
  for (ii = 0; (ii < size - 1               /* save line[size-1] for '\0' */
                && EOF != (cc = getc(file)) /* didn't hit EOF trying to read char */
                && cc != '\n'               /* char isn't newline */
                && cc != '\r');             /* char isn't carriage return */
       ++ii) {
    line[ii] = AIR_CAST(char, cc);
  }

  if (EOF == cc) {
    /* for-loop terminated because we hit EOF */
    line[ii] = '\0'; /* TeemV2: changed from line[0] = '\0' */
    ret = 0;
  } else if ('\r' == cc || '\n' == cc) {
    /* for-loop terminated because we hit '\n' or '\r' */
    /* if it was '\r', see if next character is '\n' */
    if ('\r' == cc) {
      cc = getc(file);
      if (EOF != cc && '\n' != cc) {
        /* oops, we got something, and it was not a '\n'
           ==> this was a legacy Mac line termination; put `cc` back for later */
        ungetc(cc, file);
      }
    }
    line[ii] = '\0';
    ret = ii + 1;
  } else {
    /* for-loop terminated because we got to end of buffer (ii == size-1) */
    cc = getc(file);
    /* but see if we were about to get '\r', "\r\n", or '\n' */
    if ('\r' == cc) {
      int dd;
      dd = getc(file);
      if (EOF != dd && '\n' != dd) {
        /* oops, put it back */
        ungetc(dd, file);
      }
      line[ii] = '\0';
      ret = ii + 1;
    } else if ('\n' == cc) {
      line[ii] = '\0';
      ret = ii + 1;
    } else {
      /* weren't about to get a line termination,
         we really did run out of buffer */
      if (EOF != cc) {
        ungetc(cc, file);
      }
      line[size - 1] = '\0';
      ret = size + 1;
    }
  }
  return ret;
}
