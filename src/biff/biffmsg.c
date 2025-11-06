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

#include "biff.h"
#include "privateBiff.h"

/*
With the Nov'09 re-write of biff, this sourcefile becomes the only place where a static
buffer is used for message handling; this should eventually be avoided by using things
like asprintf and vasprintf which allocated the string as needed.  However (writing here
in 2025), vasprintf is not apparently part of any C standard (as per
https://en.cppreference.com/w/c/experimental/dynamic/asprintf), so we make do with
snprintf and vsnprintf, used with _HACK_STRLEN.  Note that we are NOT merely hoping
that incoming error messages are shorter than _HACK_STRLEN. If they are longer, they
will be truncated safely to avoid any buffer overflow.
*/
#define _HACK_STRLEN AIR_STRLEN_HUGE
#define _MSG_INCR    2

/*
 * _biffMsgNoop
 *
 * pass this instead of a real biffMsg (allocated by biffMsgNew) as a
 * flag to say, "don't bother, really".  This turns all the biffMsg
 * functions into no-ops (except that var-args are still consumed
 * where they are used)
 */
static const biffMsg _biffMsgNoopStruct = {NULL, NULL, 0, NULL};
const biffMsg *const _biffMsgNoop = &_biffMsgNoopStruct;

biffMsg *
_biffMsgNew(const char *key) {
  static const char me[] = "_biffMsgNew";
  biffMsg *msg;

  if (!key) {
    fprintf(stderr, "%s: PANIC got NULL key\n", me);
    return NULL; /* exit(1); */
  }
  msg = AIR_CALLOC(1, biffMsg);
  if (msg) {
    airPtrPtrUnion appu;

    msg->key = airStrdup(key);
    msg->err = NULL;
    msg->errNum = 0;
    appu.cp = &(msg->err);
    msg->errArr = airArrayNew(appu.v, &(msg->errNum), sizeof(char *), _MSG_INCR);
    if (msg->errArr) {
      airArrayPointerCB(msg->errArr, NULL, airFree);
    }
  }
  if (!(msg && msg->key && msg->errArr)) {
    fprintf(stderr, "%s: PANIC couldn't calloc new msg\n", me);
    return NULL; /* exit(1); */
  }
  return msg;
}

biffMsg *
_biffMsgNix(biffMsg *msg) {

  if (msg && msg != _biffMsgNoop) {
    airFree(msg->key);
    airArrayLenSet(msg->errArr, 0); /* frees all msg->err[i] */
    airArrayNuke(msg->errArr);
    airFree(msg);
  }
  return NULL;
}

/*
 * adds a given message to the given entry.  The message is processed to
 * convert all whitespace into ' ', and to eliminate whitespace at the
 * end of the message.
 */
void
_biffMsgAdd(biffMsg *msg, const char *err) {
  static const char me[] = "_biffMsgAdd";
  unsigned int idx;

  if (_biffMsgNoop == msg) {
    return;
  }
  if (!(msg && err)) {
    fprintf(stderr, "%s: PANIC got NULL msg (%p) or err (%p)\n", me, AIR_VOIDP(msg),
            AIR_CVOIDP(err));
    return; /* exit(1); */
  }
  idx = airArrayLenIncr(msg->errArr, 1);
  if (!msg->err) {
    fprintf(stderr, "%s: PANIC: couldn't add message to %s\n", me, msg->key);
    return; /* exit(1); */
  }
  if (!(msg->err[idx] = airOneLinify(airStrdup(err)))) {
    fprintf(stderr, "%s: PANIC: couldn't alloc message to %s\n", me, msg->key);
    return; /* exit(1); */
  }
  return;
}

void
_biffMsgAddVL(biffMsg *msg, const char *errfmt, va_list args) {
  char errstr[_HACK_STRLEN + 1];

  /* h/t Jorik Blaas for highlighting need for vsnprintf over vsprintf */
  vsnprintf(errstr, _HACK_STRLEN + 1, errfmt, args);
  _biffMsgAdd(msg, errstr);
  return;
}

static void
_biffMsgClear(biffMsg *msg) {

  if (_biffMsgNoop == msg) {
    return;
  }
  airArrayLenSet(msg->errArr, 0); /* frees all msg->err[i] */
  /* but msg->key stays allocated */
  return;
}

/*
** max length of line formatted "[<key>] <err>\n"
*/
static size_t
_biffMsgLineLenMax(const biffMsg *msg) {
  unsigned int ii;
  size_t len, maxlen;

  if (_biffMsgNoop == msg) {
    return 0;
  }
  maxlen = 0;
  for (ii = 0; ii < msg->errNum; ii++) {
    len = strlen(msg->err[ii]) + strlen(msg->key) + strlen("[] \n");
    maxlen = AIR_MAX(maxlen, len);
  }
  return maxlen;
}

/*
 * _biffMsgMove
 *
 * "src" is not const because we clear it after moving things out
 */
void
_biffMsgMove(biffMsg *dest, biffMsg *src, const char *err) {
  static const char me[] = "_biffMsgMove";
  unsigned int ii;
  size_t buffSize;
  char *buff;

  if (_biffMsgNoop == dest || _biffMsgNoop == src) {
    return;
  }
  if (!(dest && src)) {
    fprintf(stderr, "%s: PANIC got NULL msg (%p %p)\n", me, AIR_VOIDP(dest),
            AIR_VOIDP(src));
    return; /* exit(1); */
  }
  /* if src and dest are same, this degenerates to _biffMsgAdd */
  if (dest == src && airStrlen(err)) {
    _biffMsgAdd(dest, err);
    return;
  }

  buffSize = _biffMsgLineLenMax(src) + 1;
  buff = AIR_CALLOC(buffSize, char);
  if (!buff) {
    fprintf(stderr, "%s: PANIC: can't allocate buffer\n", me);
    return; /* exit(1); */
  }
  for (ii = 0; ii < src->errNum; ii++) {
    snprintf(buff, buffSize, "[%s] %s", src->key, src->err[ii]);
    _biffMsgAdd(dest, buff);
  }
  free(buff);
  _biffMsgClear(src);
  if (airStrlen(err)) {
    _biffMsgAdd(dest, err);
  }
  return;
}

void
_biffMsgMoveVL(biffMsg *dest, biffMsg *src, const char *errfmt, va_list args) {
  char errstr[_HACK_STRLEN + 1];

  vsnprintf(errstr, _HACK_STRLEN + 1, errfmt, args);
  _biffMsgMove(dest, src, errstr);
  return;
}
