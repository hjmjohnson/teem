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

#include "nrrd.h"
#include "privateNrrd.h"

/*
** these aren't "const"s because the user should be able to change
** default behavior- until a more sophisticated mechanism for this
** kind of control is developed, it seems simple and usable enough to
** have this be global state which we agree to treat nicely, as in,
** threads shouldn't be changing these willy-nilly.
**
** What IS a "default"?  A default is the assertion of a certain
** choice in situations where the user hasn't set it explicitly, but
** COULD.  The pad value in resampling is a good example: it is set by
** a constructor to nrrdDefaultResamplePadValue, but the user can also set it
** explicitly.
*/

int nrrdDefaultWriteEncodingType = nrrdEncodingTypeRaw;
int nrrdDefaultWriteBareText = AIR_TRUE;
int nrrdDefaultWriteMoreThanFloatInText = AIR_FALSE;
unsigned int nrrdDefaultWriteCharsPerLine = 75;
unsigned int nrrdDefaultWriteValsPerLine = 8;
/* ---- BEGIN non-NrrdIO */
int nrrdDefaultResampleBoundary = nrrdBoundaryBleed;
int nrrdDefaultResampleType = nrrdTypeDefault;
int nrrdDefaultResampleRenormalize = AIR_TRUE;
int nrrdDefaultResampleRound = AIR_TRUE;
int nrrdDefaultResampleClamp = AIR_TRUE;
int nrrdDefaultResampleCheap = AIR_FALSE;
double nrrdDefaultResamplePadValue = 0.0;
int nrrdDefaultResampleNonExistent = nrrdResampleNonExistentNoop;
/* creating and by-default enabling this functionality is a change for Teem2 */
int nrrdDefaultDeclineStdioOnTTY = AIR_TRUE;
/* ---- END non-NrrdIO */
int nrrdDefaultCenter = nrrdCenterCell;
/* new for TeemV2; emulate pre-TeemV2 nrrdStateVerboseIO=1 initialization */
int nrrdDefaultIoVerbose = 1; /* NrrdIO-hack-003 */
/* new for TeemV2; pre-TeemV2 behavior equivalent to this being AIR_TRUE */
int nrrdDefaultIoHeaderDoubleExcessPrecision = AIR_FALSE;

/* these aren't really "defaults" because there's no other channel for
   specifying this information.  It is just global state.  Obviously,
   like defaults, they are not thread-safe if different threads ever
   set them differently. */
int nrrdStateKeyValuePairsPropagate = AIR_FALSE;
/* ---- BEGIN non-NrrdIO */
int nrrdStateBlind8BitRange = AIR_TRUE;
unsigned int nrrdStateMeasureModeBins = 1024u;
/* gone in TeemV2: setting output measurement type should not be so sneaky
 * int nrrdStateMeasureType = nrrdTypeFloat;
 * int nrrdStateMeasureHistoType = nrrdTypeFloat; */
int nrrdStateDisallowIntegerNonExist = AIR_TRUE;
/* ---- END non-NrrdIO */
int nrrdStateAlwaysSetContent = AIR_TRUE;
int nrrdStateDisableContent = AIR_FALSE;
const char *nrrdStateUnknownContent = NRRD_UNKNOWN;
int nrrdStateGrayscaleImage3D = AIR_FALSE;
/* there is no sane reason to change this initialization */
int nrrdStateKeyValueReturnInternalPointers = AIR_FALSE;
/* Making the default for this be AIR_TRUE means that nrrd is not only
   completely conservative about updating kind, but purposely stupid.
   Nrrd is only going to implement the most converative kind of logic
   anyway, based on existing sementics nailed down by the format spec. */
int nrrdStateKindNoop = AIR_FALSE;
/* new for TeemV2: if true, ~emulate pre-TeemV2 behavior of ascii encoding
   when reading NRRD files, almost treating commas as whitespace */
int nrrdStateEncodingAsciiCommaAllow = AIR_FALSE;

/* these are helper functions for min/max testing */
airLLong /* Biff: (private) nope */
nrrd__LLongMaxHelp(airLLong val) {
  return val * 2 + 1;
}
airLLong /* Biff: (private) nope */
nrrd__LLongMinHelp(airLLong val) {
  return val * 2;
}
airULLong /* Biff: (private) nope */
nrrd__ULLongMaxHelp(airULLong val) {
  return val + 1;
}

/* should the acceptance (or not) of malformed NRRD header fields
   embedded in PNM or text comments be controlled here? */

/* Are there other assumptions currently built into nrrd which could
   stand to be user-controllable? */

/* ---- BEGIN non-NrrdIO */

const char *const nrrdEnvVarDefaultWriteEncodingType
  = "NRRD_DEFAULT_WRITE_ENCODING_TYPE";
const char *const nrrdEnvVarDefaultWriteBareText = "NRRD_DEFAULT_WRITE_BARE_TEXT";
const char *const nrrdEnvVarDefaultWriteMoreThanFloatInText
  = "NRRD_DEFAULT_WRITE_MORE_THAN_FLOAT_IN_TEXT";
const char *const nrrdEnvVarDefaultCenter = "NRRD_DEFAULT_CENTER";
const char *const nrrdEnvVarDefaultWriteCharsPerLine
  = "NRRD_DEFAULT_WRITE_CHARS_PER_LINE";
const char *const nrrdEnvVarDefaultWriteValsPerLine = "NRRD_DEFAULT_WRITE_VALS_PER_LINE";
/* over-writes nrrd__StateVerboseIO for setting nrrdDefaultIoVerbose */
const char *const nrrdEnvVarDefaultIoVerbose = "NRRD_DEFAULT_IO_VERBOSE";
const char *const nrrdEnvVarDefaultIoHeaderDoubleExcessPrecision
  = "NRRD_DEFAULT_IO_HEADER_DOUBLE_EXCESS_PRECISION";

const char *const nrrdEnvVarStateKindNoop = "NRRD_STATE_KIND_NOOP";
/* nrrdStateVerboseIO gone but still respect this to set nrrdDefaultIoVerbose */
static const char *const nrrd__StateVerboseIO = "NRRD_STATE_VERBOSE_IO";
const char *const nrrdEnvVarStateKeyValuePairsPropagate
  = "NRRD_STATE_KEYVALUEPAIRS_PROPAGATE";
const char *const nrrdEnvVarStateBlind8BitRange = "NRRD_STATE_BLIND_8_BIT_RANGE";
const char *const nrrdEnvVarStateAlwaysSetContent = "NRRD_STATE_ALWAYS_SET_CONTENT";
const char *const nrrdEnvVarStateDisableContent = "NRRD_STATE_DISABLE_CONTENT";
const char *const nrrdEnvVarStateMeasureModeBins = "NRRD_STATE_MEASURE_MODE_BINS";
const char *const nrrdEnvVarStateGrayscaleImage3D = "NRRD_STATE_GRAYSCALE_IMAGE_3D";
const char *const nrrdEnvVarStateEncodingAsciiCommaAllow
  = "NRRD_STATE_ENCODING_ASCII_COMMA_ALLOW";

/*
**    return
**     value:
**        -1: unset, or bad args    ==> *val NOT set
**  AIR_TRUE: set in a valid way    ==> *val set (to something)
** AIR_FALSE: set in an invalid way ==> *val NOT set
*/

int /* Biff: nope */
nrrdGetenvBool(int *val, const char **envStr, const char *envVar) {
  const char *env;
  int tmp;

  if (!(val && envVar)) {
    return -1;
  }
  env = getenv(envVar);
  if (envStr) {
    *envStr = env;
  }
  if (!env) {
    return -1;
  }
  if (!strlen(env)) {
    /* for bools, being merely set (but not to any string) means "true" */
    *val = AIR_TRUE;
    return AIR_TRUE;
  }
  tmp = airEnumVal(airBool, env);
  if (airEnumUnknown(airBool) == tmp) {
    return AIR_FALSE;
  } else {
    *val = tmp;
    return AIR_TRUE;
  }
}

int /* Biff: nope */
nrrdGetenvEnum(int *val, const char **envStr, const airEnum *enm, const char *envVar) {
  const char *env;
  int tmp;

  if (!(val && envVar)) {
    return -1;
  }
  env = getenv(envVar);
  if (envStr) {
    *envStr = env;
  }
  if (!env) {
    return -1;
  }
  tmp = airEnumVal(enm, env);
  if (airEnumUnknown(enm) == tmp) {
    return AIR_FALSE;
  } else {
    *val = tmp;
    return AIR_TRUE;
  }
}

int /* Biff: nope */
nrrdGetenvUInt(unsigned int *val, const char **envStr, const char *envVar) {
  const char *env;
  unsigned int tmp;

  if (!(val && envVar)) {
    return -1;
  }
  env = getenv(envVar);
  if (envStr) {
    *envStr = env;
  }
  if (!env) {
    return -1;
  }
  if (1 != sscanf(env, "%u", &tmp)) {
    return AIR_FALSE;
  } else {
    *val = tmp;
    return AIR_TRUE;
  }
}

int /* Biff: nope */
nrrdGetenvInt(int *val, const char **envStr, const char *envVar) {
  const char *env;
  int tmp;

  if (!(val && envVar)) {
    return -1;
  }
  env = getenv(envVar);
  if (envStr) {
    *envStr = env;
  }
  if (!env) {
    return -1;
  }
  if (1 != sscanf(env, "%d", &tmp)) {
    return AIR_FALSE;
  } else {
    *val = tmp;
    return AIR_TRUE;
  }
}

int /* Biff: nope */
nrrdGetenvDouble(double *val, const char **envStr, const char *envVar) {
  const char *env;
  double tmp;

  if (!(val && envVar)) {
    return -1;
  }
  env = getenv(envVar);
  if (envStr) {
    *envStr = env;
  }
  if (!env) {
    return -1;
  }
  if (1 != airSingleParse(env, "%lg", &tmp)) {
    return AIR_FALSE;
  } else {
    *val = tmp;
    return AIR_TRUE;
  }
}

/*
** This function is not used in the same way within nrrd the same way
** as the other nrrdGetenv functions; it was added just to have a more
** convenient wrapper around getenv for strings.
*/
int /* Biff: nope */
nrrdGetenvString(const char **envStr, const char *envVar) {

  if (!(envStr && envVar)) {
    return -1;
  }
  *envStr = getenv(envVar);
  if (!(*envStr)) {
    return AIR_FALSE;
  } else {
    return AIR_TRUE;
  }
}

void
nrrdDefaultGetenv(void) {
  static const char me[] = "nrrdDefaultGetenv";
  int setOldWay, oldVal, setNewWay, newVal;

  /*
   * TeemV2 gone: nrrdEnvVarDefaultWriteBareTextOld = "NRRD_DEF_WRITE_BARE_TEXT"
   * TeemV2 gone: nrrdEnvVarDefaultCenterOld = "NRRD_DEF_CENTER";
   */
  nrrdGetenvBool(/**/ &nrrdDefaultWriteBareText, NULL, nrrdEnvVarDefaultWriteBareText);
  nrrdGetenvEnum(/**/ &nrrdDefaultCenter, NULL, nrrdCenter, nrrdEnvVarDefaultCenter);

  nrrdGetenvBool(/**/ &nrrdDefaultWriteMoreThanFloatInText, NULL,
                 nrrdEnvVarDefaultWriteMoreThanFloatInText);
  nrrdGetenvEnum(/**/ &nrrdDefaultWriteEncodingType, NULL, nrrdEncodingType,
                 nrrdEnvVarDefaultWriteEncodingType);
  nrrdGetenvUInt(/**/ &nrrdDefaultWriteCharsPerLine, NULL,
                 nrrdEnvVarDefaultWriteCharsPerLine);
  nrrdGetenvUInt(/**/ &nrrdDefaultWriteValsPerLine, NULL,
                 nrrdEnvVarDefaultWriteValsPerLine);

  /* for TeemV2 nrrdStateVerboseIO --> nrrdDefaultIoVerbose but we'll
   still honor the old env var name if new env var name is not given */
  setOldWay = (AIR_TRUE == nrrdGetenvInt(&oldVal, NULL, nrrd__StateVerboseIO));
  setNewWay = (AIR_TRUE == nrrdGetenvInt(&newVal, NULL, nrrdEnvVarDefaultIoVerbose));
  if (setOldWay) {
    if (!setNewWay) {
      fprintf(stderr,
              "%s: WARNING environment variable \"%s\" deprecated, "
              "use \"%s\" instead\n",
              me, nrrd__StateVerboseIO, nrrdEnvVarDefaultIoVerbose);
      nrrdDefaultIoVerbose = oldVal;
    } else if (setNewWay) {
      fprintf(stderr,
              "%s: WARNING environment variable \"%s\" "
              "over-riding deprecated \"%s\"\n",
              me, nrrdEnvVarDefaultIoVerbose, nrrd__StateVerboseIO);
      nrrdDefaultIoVerbose = newVal;
    }
  } else if (setNewWay) {
    nrrdDefaultIoVerbose = newVal;
  }
  nrrdGetenvBool(&nrrdDefaultIoHeaderDoubleExcessPrecision, NULL,
                 nrrdEnvVarDefaultIoHeaderDoubleExcessPrecision);
  return;
}

void
nrrdStateGetenv(void) {

  nrrdGetenvBool(/**/ &nrrdStateKindNoop, NULL, nrrdEnvVarStateKindNoop);
  nrrdGetenvBool(/**/ &nrrdStateKeyValuePairsPropagate, NULL,
                 nrrdEnvVarStateKeyValuePairsPropagate);
  nrrdGetenvBool(/**/ &nrrdStateBlind8BitRange, NULL, nrrdEnvVarStateBlind8BitRange);
  nrrdGetenvBool(/**/ &nrrdStateAlwaysSetContent, NULL, nrrdEnvVarStateAlwaysSetContent);
  nrrdGetenvBool(/**/ &nrrdStateDisableContent, NULL, nrrdEnvVarStateDisableContent);
  nrrdGetenvUInt(/**/ &nrrdStateMeasureModeBins, NULL, nrrdEnvVarStateMeasureModeBins);
  nrrdGetenvBool(/**/ &nrrdStateGrayscaleImage3D, NULL, nrrdEnvVarStateGrayscaleImage3D);
  nrrdGetenvBool(/**/ &nrrdStateEncodingAsciiCommaAllow, NULL,
                 nrrdEnvVarStateEncodingAsciiCommaAllow);
  return;
}

/* ---- END non-NrrdIO */
