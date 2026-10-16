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

/* learned: some header file must declare private functions
** within extern "C" so that others can link with them
*/

#define NO_STRING "."

#define INFO "Create a nrrd (or nrrd header) from scratch"
static const char *unu_makeInfoL
  = (INFO ".  The data can be in one or more files, or coming from stdin. "
          "This provides an easy way of specifying the information about some "
          "data as to wrap it in a NRRD file, either to pass on for further "
          "unu processing, or to save to disk.  Note that with \"-h\", this creates "
          "a detached nrrd header file, without ever reading or writing data files. "
          "\n \n "
          "When using multiple datafiles, the data from each is simply "
          "concatenated in memory (as opposed to interleaving along a faster axis). "
          "Keep in mind that all the options below refer to the finished data segment "
          "resulting from joining all the data pieces together, "
          "except for \"-ls\", \"-bs\", and \"-e\", which apply (uniformly) to the "
          "individual data files. Use the \"-fd\" option when the things being joined "
          "together are not slices of the final result, but slabs or scanlines. "
          "It may be easier to put multiple filenames in a response file; "
          "there can be one or more filenames per line of the response file. "
          "You can also use a sprintf-style format to identify a numbered "
          "range of files, so for example \"-i I.%03d 1 90 1\" "
          "refers to I.001, I.002, ... I.090, using the inclusive range from the first "
          "to the second integer (following the sprintf-style format), in steps of "
          "the third.  Can optionally give a fourth integer to serve same role as "
          "\"-fd\"."
          "\n \n "
          "NOTE: for the \"-l\" (labels), \"-u\" (units), and \"-spu\" (space units) "
          "options below, you can use a single unquoted period (.) to signify "
          "an empty string.  This creates a convenient way to convey something that "
          "the shell doesn't make it easy to convey.  Shell expansion weirdness "
          "also requires the use of quotes around the arguments to \"-orig\" (space "
          "origin), \"-dirs\" (space directions), and \"-mf\" (measurement frame).\n "
          "\n "
          "* Uses various components of file and data IO, but currently there is no "
          "library function that encapsulates the functionality here.");

static int
unu_makeMain(int argc, const char **argv, const char *me, const hestParm *_hparm) {
  hestOpt *hopt = NULL;
  int headerOnly;
  hestOptAdd_Flag(&hopt, "h,header", &headerOnly,
                  "Generate header ONLY: don't write out the whole nrrd, "
                  "don't even bother reading the input data, just output the "
                  "detached nrrd header file (usually with a \".nhdr\" "
                  "extension) determined by the options below. The single "
                  "constraint is that detached headers are incompatible with "
                  "using stdin as the data source.");
  char **dfns;
  uint dfnsLen;
  uint dfnsIdx = hestOptAdd_Mv_String(
    &hopt, "i,input", "file", 1, -1, &dfns, "-",
    "Filename(s) of data file(s); use \"-\" for stdin. *OR*, can use sprintf-style "
    "format for identifying a range of numbered files, see above for details.",
    &dfnsLen);
  int type;
  hestOptAdd_1_Enum(&hopt, "t,type", "type", &type, NULL,
                    "type of data (e.g. \"uchar\", \"int\", \"float\", "
                    "\"double\", etc.)",
                    nrrdType);
  size_t *size;
  uint sizeLen;
  hestOptAdd_Mv_Size_t(&hopt, "s,size", "sz0 sz1", 1, -1, &size, NULL,
                       "number of samples along each axis (and implicit indicator "
                       "of dimension of nrrd)",
                       &sizeLen);
  uint dataFileDim;
  hestOptAdd_1_UInt(&hopt, "fd,filedim", "dim", &dataFileDim, "0",
                    "When using *multiple* input data files (to \"-i\"), what is "
                    "the dimension of the array data in each individual file. By "
                    "default (not using this option), this dimension is assumed "
                    "to be one less than the whole data dimension. ");
  double *spacing;
  uint spacingLen;
  hestOptAdd_Mv_Double(&hopt, "sp,spacing", "sp0 sp1", 0, -1, &spacing, "",
                       "spacing between samples on each axis.  Use \"nan\" for "
                       "any non-spatial axes (e.g. spacing between red, green, and blue "
                       "along axis 0 of interleaved RGB image data)",
                       &spacingLen);
  /* NB: these are like unu jhisto's -min -max, not like unu crop's */
  double *axmin, *axmax, *thickness;
  uint minLen, maxLen, thicknessLen;
  hestOptAdd_Mv_Double(&hopt, "min,axismin", "min0 min1", 0, -1, &axmin, "",
                       "When each axis has a distinct meaning (as in a joint "
                       "histogram), the per-axis min is the smallest \"position\" "
                       "associated with the first sample on the axis. Use \"nan\" for "
                       "\"no value to set\" when other axes do have axis min",
                       &minLen);
  hestOptAdd_Mv_Double(&hopt, "max,axismax", "max0 max1", 0, -1, &axmax, "",
                       "Goes with -min: the per-axis maximum \"position\". "
                       "-max and -min should probably be used together, and having "
                       "this information logically supersedes the -sp spacing on those "
                       "axes.",
                       &maxLen);
  hestOptAdd_Mv_Double(&hopt, "th,thickness", "th0 th1", 0, -1, &thickness, "",
                       "thickness of region represented by one sample along each axis. "
                       "  As with -sp spacing, use \"nan\" for "
                       "any non-spatial axes.",
                       &thicknessLen);
  char **kinds;
  uint kindsLen;
  uint kindIdx = hestOptAdd_Mv_String(
    &hopt, "k,kind", "k0 k1", 0, -1, &kinds, "",
    "what \"kind\" is each axis, from the nrrdKind airEnum (e.g. space, time, "
    "3-vector, 3D-masked-symmetric-matrix, or \"none\" to signify no kind)",
    &kindsLen);
  char **centers;
  uint centersLen;
  uint centersIdx = hestOptAdd_Mv_String(
    &hopt, "cn,centering", "c0 c1", 0, -1, &centers, "",
    "kind of centering (node or cell) for each axis, or \"none\" to signify "
    "no centering",
    &centersLen);
  char **label;
  uint labelLen;
  hestOptAdd_Mv_String(&hopt, "l,label", "lb0 lb1", 0, -1, &label, "",
                       "short string labels for each of the axes", &labelLen);
  char **units;
  uint unitsLen;
  hestOptAdd_Mv_String(&hopt, "u,unit", "un0 un1", 0, -1, &units, "",
                       "short strings giving units for each of the axes", &unitsLen);
  char *content;
  hestOptAdd_1_String(&hopt, "c,content", "content", &content, "",
                      "Specifies the content string of the nrrd, which is built upon "
                      "by many nrrd function to record a history of operations");
  uint lineSkip;
  hestOptAdd_1_UInt(&hopt, "ls,lineskip", "num", &lineSkip, "0",
                    "number of ascii lines to skip before reading data");
  long int byteSkip; /* NOT unsigned: -1 means "from the end" */
  hestOptAdd_1_Long(&hopt, "bs,byteskip", "num", &byteSkip, "0",
                    "number of bytes to skip (after skipping ascii lines, if any) "
                    "before reading data.  Can use \"-bs -1\" to skip a binary "
                    "header of unknown length in raw-encoded data");
  char encInfo[AIR_STRLEN_LARGE + 1];
  char *enc = encInfo;
  size_t encSize = sizeof(encInfo);
  SN_COPY(enc, encSize,
          "encoding of input data. Possibilities include:"
          "\n \b\bo \"raw\": raw encoding"
          "\n \b\bo \"ascii\": ascii values, one scanline per line of text, "
          "values within line are delimited by space, tab, or comma"
          "\n \b\bo \"hex\": two hex digits per byte");
  if (nrrdEncodingGzip->available()) {
    SN_COPY(enc, encSize, "\n \b\bo \"gzip\", \"gz\": gzip compressed raw data");
  }
  if (nrrdEncodingBzip2->available()) {
    SN_COPY(enc, encSize, "\n \b\bo \"bzip2\", \"bz2\": bzip2 compressed raw data");
  }
  int encodingType;
  hestOptAdd_1_Enum(&hopt, "e,encoding", "enc", &encodingType, "raw", encInfo,
                    nrrdEncodingType);
  int endian;
  hestOptAdd_1_Enum(&hopt, "en,endian", "end", &endian,
                    airEnumStr(airEndian, airMyEndian()),
                    "Endianness of data; relevent for any data with value "
                    "representation bigger than 8 bits, with a non-ascii encoding: "
                    "\"little\" for Intel and friends "
                    "(least significant byte first, at lower address); "
                    "\"big\" for everyone else (most significant byte first). "
                    "Defaults to endianness of this machine",
                    airEndian);
  char *spcStr;
  hestOptAdd_1_String(&hopt, "spc,space", "space", &spcStr, "",
                      "identify the space (e.g. \"RAS\", \"LPS\") in which the array "
                      "conceptually lives, from the nrrdSpace airEnum, which in turn "
                      "determines the dimension of the space.  Or, use an integer>0 to"
                      "give the dimension of a space that nrrdSpace doesn't know "
                      "about. "
                      "By default (not using this option), the enclosing space is "
                      "set as unknown.");
  char *_origStr;
  hestOptAdd_1_String(&hopt, "orig,origin", "origin", &_origStr, "",
                      "(NOTE: must quote vector) the origin in space of the array: "
                      "the location of the center "
                      "of the first sample, of the form \"(x,y,z)\" (or however "
                      "many coefficients are needed for the chosen space). Quoting the "
                      "vector is needed to stop interpretation from the shell");
  char *_dirStr;
  hestOptAdd_1_String(&hopt, "dirs,directions", "v0 v1 ...", &_dirStr, "",
                      "(NOTE: must quote whole vector list) The \"space directions\": "
                      "the vectors in space spanned by incrementing (by one) each "
                      "axis index (the column vectors of the index-to-world "
                      "matrix transform), OR, \"none\" for non-spatial axes. Give "
                      "one vector per axis. Using a space direction logically "
                      "supersedes both per-axis -sp spacing and -min,-max. "
                      "(Quoting around whole vector list, not "
                      "individually, prevents the shell from interpreting parentheses)");
  char *_mframeStr;
  hestOptAdd_1_String(&hopt, "mf,measurementframe", "v0 v1 ...", &_mframeStr, "",
                      "(NOTE: must quote whole vector list). Each vector is a *column* "
                      "vector of the matrix which transforms from coordinates in "
                      "measurement frame (in which the coefficients of vectors and "
                      "tensors are given) to coordinates of world space (given with "
                      "\"-spc\"). This is not a per-axis field: the column vectors "
                      "comprise a D-by-D square matrix, where D is the dimension of "
                      "world space.");
  char **spunits;
  uint spunitsLen;
  uint spunitsIdx = hestOptAdd_Mv_String(
    &hopt, "spu,spaceunit", "su0 su1", 0, -1, &spunits, "",
    "short strings giving units with which the coefficients of the "
    "space origin and direction vectors are measured.",
    &spunitsLen);
  char **kvp;
  uint kvpLen;
  hestOptAdd_Mv_String(&hopt, "kv,keyvalue", "key/val", 0, -1, &kvp, "",
                       "key/value string pairs to be stored in nrrd.  Each key/value "
                       "pair must be a single string (put it in \"\"s "
                       "if the key or the value contain spaces).  The format of each "
                       "pair is \"<key>:=<value>\", with no spaces before or after "
                       "\":=\".",
                       &kvpLen);
  char **cmnt;
  uint cmntLen;
  hestOptAdd_Mv_String(&hopt, "cm,comment", "comment", 0, -1, &cmnt, "",
                       "comments to be stored in nrrd.  Each one must be a single "
                       "string (put it in \"\"s if it contain spaces).",
                       &cmntLen);
  char *out;
  hestOptAdd_1_String(&hopt, "o,output", "nout", &out, "-",
                      "output filename.  If \"-h\" has been used, the output file is "
                      "always a detached header.  Otherwise, use extension "
                      "\".nrrd\" to signal creation of self-contained nrrd, and "
                      "\".nhdr\" to signal creating of a detached header with "
                      "(single) data file.");
  char *outData;
  hestOptAdd_1_String(&hopt, "od,outputdata", "name", &outData, "",
                      "when *not* using \"-h\" and saving to a \".nhdr\" file, using "
                      "this option allows you to explicitly name the data file, "
                      "instead of (by default, not using this option) having it be "
                      "the same filename base as the header file.");
  int hdrDblXPrec;
  hestOptAdd_1_Bool(&hopt, "hdep", "bool", &hdrDblXPrec,
                    nrrdDefaultIoHeaderDoubleExcessPrecision ? "true" : "false",
                    "how to set NrrdIoState->headerDoubleExcessPrecision. If true, "
                    "`double` values in the NRRD header are printed with conversion "
                    "%.17g, which means the resulting strings may be longer than "
                    "necessary to recover all 64 bits of the `double`. This emulates "
                    "some under-informed choices that affected pre-TeemV2 code.");
  int verboseBump;
  hestOptAdd_1_Int(&hopt, "vb,verbump", "bump", &verboseBump, "1",
                   "just before calling nrrdFormatNRRD->read to read data files, "
                   "bump the NrrdIoState->verbose by this value, to get more diagnostic "
                   "feedback than would otherwise be shown");

  // (boilerplate for YES some hparm modification)
  hestParm *hparm = hestParmCopy(_hparm);
  // Gone in TeemV2: hparm->greedySingleString = AIR_TRUE;
  // Gone in TeemV2: airStrtokQuoting = AIR_TRUE;
  /* so that long lists of filenames can be read from file */
  hparm->responseFileEnable = AIR_TRUE;
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_makeInfoL);
  airArray *mop = unrrduMopNew(hopt, hparm);
  char *err;

  /* ---------------- BEGIN ERROR CHECKING --------------- */
  if (headerOnly) {
    for (uint ii = 0; ii < dfnsLen; ii++) {
      if (!strcmp("-", dfns[ii])) {
        fprintf(stderr,
                "%s: can't use detached headers (\"-h\") "
                "with stdin (\"-\") as data source "
                "(filename %u of %u)\n",
                me, ii + 1, dfnsLen);
        airMopError(mop);
        return 1;
      }
    }
  }
  /* given the information we have, we set the fields in the nrrdIoState
     so as to simulate having read the information from a header */
  if (!(AIR_IN_CL(1, sizeLen, NRRD_DIM_MAX))) {
    fprintf(stderr,
            "%s: # axis sizes (%u) not in currently valid nrrd dimension "
            "range [1,NRRD_DIM_MAX] = [1,%u]\n",
            me, sizeLen, NRRD_DIM_MAX);
    airMopError(mop);
    return 1;
  }
#define LEN_CHECK(VAR, WUTS)                                                            \
  if ((VAR) && (VAR) != sizeLen) {                                                      \
    fprintf(stderr,                                                                     \
            "%s: number of %s (%u) not same as dimension (%u) "                         \
            "(number of axis sizes)\n",                                                 \
            me, (WUTS), (VAR), sizeLen);                                                \
    airMopError(mop);                                                                   \
    return 1;                                                                           \
  }
  LEN_CHECK(spacingLen, "spacings");
  LEN_CHECK(thicknessLen, "thicknesses");
  LEN_CHECK(minLen, "mins");
  LEN_CHECK(maxLen, "maxs");
  LEN_CHECK(labelLen, "labels");
  LEN_CHECK(unitsLen, "units");
  LEN_CHECK(kindsLen, "kinds");
  LEN_CHECK(centersLen, "centerings");
#undef LEN_CHECK

  /* ----------------- END ERROR CHECKING ---------------- */
  /* ----------------- BEGIN SETTING INFO ---------------- */
  NrrdIoState *nio = nrrdIoStateNew();
  airMopAdd(mop, nio, nrrdIoStateNix_mop, airMopAlways);
  Nrrd *nrrd = nrrdNew();
  airMopAdd(mop, nrrd, nrrdNuke_mop, airMopAlways);
  unsigned int spaceDim;

  nrrd->type = type;
  nrrd->dim = sizeLen;
  nrrdAxisInfoSet_nva(nrrd, nrrdAxisInfoSize, size);
  /* have to simulate having parsed this line for error checking in
     nrrdIoDataFNCheck() to not cause problems */
  nio->seen[nrrdField_sizes] = AIR_TRUE;
  if (nrrdContainsPercentThisAndMore(dfns[0], 'd')) {
    /* trying to do a formatted filename list */
    if (!(3 == dfnsLen || 4 == dfnsLen)) {
      fprintf(stderr,
              "%s: formatted list of filenames needs either "
              "3 or 4 ints after the format (not %u)\n",
              me, dfnsLen - 1);
      airMopError(mop);
      return 1;
    }
    nio->line = hopt[dfnsIdx].parmStr;
    nio->pos = 0;
    if (nrrdFieldInfoParse[nrrdField_data_file](NULL, nrrd, nio, AIR_TRUE)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble with formatted filename list \"%s\":\n%s", me,
              nio->line, err);
      nio->line = NULL;
      airMopError(mop);
      return 1;
    }
    nio->line = NULL;
  } else {
    /* single or regular LIST of files */
    if (dfnsLen > 1) {
      nio->dataFileDim = dataFileDim ? dataFileDim : nrrd->dim - 1;
    } else {
      nio->dataFileDim = nrrd->dim;
    }
    airArrayLenSet(nio->dataFNArr, dfnsLen);
    for (uint ii = 0; ii < dfnsLen; ii++) {
      nio->dataFN[ii] = airStrdup(dfns[ii]);
    }
  }
  if (nrrdIoDataFNCheck(nio, nrrd, AIR_TRUE)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: trouble with input datafiles:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  if (spacingLen) {
    nrrdAxisInfoSet_nva(nrrd, nrrdAxisInfoSpacing, spacing);
  }
  if (thicknessLen) {
    nrrdAxisInfoSet_nva(nrrd, nrrdAxisInfoThickness, thickness);
  }
  if (minLen) {
    nrrdAxisInfoSet_nva(nrrd, nrrdAxisInfoMin, axmin);
  }
  if (maxLen) {
    nrrdAxisInfoSet_nva(nrrd, nrrdAxisInfoMax, axmax);
  }
  if (labelLen) {
    for (uint ii = 0; ii < nrrd->dim; ii++) {
      if (!strcmp(NO_STRING, label[ii])) {
        label[ii][0] = '\0';
      }
    }
    nrrdAxisInfoSet_nva(nrrd, nrrdAxisInfoLabel, label);
  }
  if (unitsLen) {
    for (uint ii = 0; ii < nrrd->dim; ii++) {
      if (!strcmp(NO_STRING, units[ii])) {
        units[ii][0] = '\0';
      }
    }
    nrrdAxisInfoSet_nva(nrrd, nrrdAxisInfoUnits, units);
  }
  if (airStrlen(content)) {
    nrrd->content = airStrdup(content);
  }
  if (kvpLen) {
    for (uint ii = 0; ii < kvpLen; ii++) {
      /* a hack: have to use NrrdIoState->line as the channel to communicate
         the key/value pair, since we have to emulate it having been
         read from a NRRD header.  But because nio doesn't own the
         memory, we must be careful to unset the pointer prior to
         NrrdIoStateNix being called by the mop. */
      nio->line = kvp[ii];
      nio->pos = 0;
      if (nrrdFieldInfoParse[nrrdField_keyvalue](NULL, nrrd, nio, AIR_TRUE)) {
        airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
        fprintf(stderr, "%s: trouble with key/value %u \"%s\":\n%s", me, ii, kvp[ii],
                err);
        nio->line = NULL;
        airMopError(mop);
        return 1;
      }
      nio->line = NULL;
    }
  }
  if (cmntLen) {
    /* copy-pasta of hack above */
    for (uint ii = 0; ii < cmntLen; ii++) {
      nio->line = cmnt[ii];
      nio->pos = 0;
      if (nrrdFieldInfoParse[nrrdField_comment](NULL, nrrd, nio, AIR_TRUE)) {
        airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
        fprintf(stderr, "%s: trouble with comment %u \"%s\":\n%s", me, ii, cmnt[ii],
                err);
        nio->line = NULL;
        airMopError(mop);
        return 1;
      }
      nio->line = NULL;
    }
  }
  /* pre-TeemV2, we allocated buffers here for printing space-separated kinds and
     centers, and then passed that on to nrrdFieldInfoParse[], but now there's no need:
     hest gives us a string that already serves that purpose */
  if (kindsLen) {
    nio->line = hopt[kindIdx].parmStr;
    nio->pos = 0;
    if (nrrdFieldInfoParse[nrrdField_kinds](NULL, nrrd, nio, AIR_TRUE)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble with kinds \"%s\":\n%s", me, nio->line, err);
      nio->line = NULL;
      airMopError(mop);
      return 1;
    }
    nio->line = NULL;
  }
  if (centersLen) {
    nio->line = hopt[centersIdx].parmStr;
    nio->pos = 0;
    if (nrrdFieldInfoParse[nrrdField_centers](NULL, nrrd, nio, AIR_TRUE)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble with centerings \"%s\":\n%s", me, nio->line, err);
      nio->line = NULL;
      airMopError(mop);
      return 1;
    }
    nio->line = NULL;
  }
  int spaceSet;
  if (airStrlen(spcStr)) {
    int space = airEnumVal(nrrdSpace, spcStr);
    if (!space) {
      /* couldn't parse it as space, perhaps its a uint */
      if (1 != sscanf(spcStr, "%u", &spaceDim)) {
        fprintf(stderr, "%s: couldn't parse \"%s\" as a nrrdSpace or as a uint", me,
                spcStr);
        airMopError(mop);
        return 1;
      }
      /* else we did parse it as a uint */
      nrrd->space = nrrdSpaceUnknown;
      nrrd->spaceDim = spaceDim;
    } else {
      /* we did parse a known space */
      nrrdSpaceSet(nrrd, space);
    }
    spaceSet = AIR_TRUE;
  } else {
    /* we got no space information at all */
    nrrdSpaceSet(nrrd, nrrdSpaceUnknown);
    spaceSet = AIR_FALSE;
  }
  if (airStrlen(_origStr)) {
    char *origStr = NULL;
    /* why this is necessary is a bit confusing to me, both the check for
       enclosing quotes, and the need to use to a separate variable (isn't
       hest doing memory management of addresses, not variables?) */
    if ('\"' == _origStr[0] && '\"' == _origStr[strlen(_origStr) - 1]) {
      _origStr[strlen(_origStr) - 1] = 0;
      origStr = _origStr + 1;
    } else {
      origStr = _origStr;
    }
    /* same hack about using NrrdIoState->line as basis for parsing */
    nio->line = origStr;
    nio->pos = 0;
    if (nrrdFieldInfoParse[nrrdField_space_origin](NULL, nrrd, nio, AIR_TRUE)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble with origin \"%s\":\n%s", me, origStr, err);
      nio->line = NULL;
      airMopError(mop);
      return 1;
    }
    nio->line = NULL;
  }
  char *dirStr = NULL;
  if (airStrlen(_dirStr)) {
    /* same confusion as above */
    if ('\"' == _dirStr[0] && '\"' == _dirStr[strlen(_dirStr) - 1]) {
      _dirStr[strlen(_dirStr) - 1] = 0;
      dirStr = _dirStr + 1;
    } else {
      dirStr = _dirStr;
    }
    /* same hack about using NrrdIoState->line as basis for parsing */
    nio->line = dirStr;
    nio->pos = 0;
    if (nrrdFieldInfoParse[nrrdField_space_directions](NULL, nrrd, nio, AIR_TRUE)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble with space directions \"%s\":\n%s", me, dirStr, err);
      nio->line = NULL;
      airMopError(mop);
      return 1;
    }
    nio->line = NULL;
  }
  char *mframeStr = NULL;
  if (airStrlen(_mframeStr)) {
    /* same confusion as above */
    if ('\"' == _mframeStr[0] && '\"' == _mframeStr[strlen(_mframeStr) - 1]) {
      _mframeStr[strlen(_mframeStr) - 1] = 0;
      mframeStr = _mframeStr + 1;
    } else {
      mframeStr = _mframeStr;
    }
    /* same hack about using NrrdIoState->line as basis for parsing */
    nio->line = mframeStr;
    nio->pos = 0;
    if (nrrdFieldInfoParse[nrrdField_measurement_frame](NULL, nrrd, nio, AIR_TRUE)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble with measurement frame \"%s\":\n%s", me, mframeStr,
              err);
      nio->line = NULL;
      airMopError(mop);
      return 1;
    }
    nio->line = NULL;
  }
  if (spunitsLen) {
    if (!spaceSet) {
      fprintf(stderr, "%s: can't have space units with no space set\n", me);
      airMopError(mop);
      return 1;
    }
    if (nrrd->spaceDim != spunitsLen) {
      fprintf(stderr,
              "%s: number of space units (%u) "
              "not same as space dimension (%u)\n",
              me, spunitsLen, nrrd->spaceDim);
      airMopError(mop);
      return 1;
    }
    for (uint ii = 0; ii < nrrd->spaceDim; ii++) {
      if (!strcmp(NO_STRING, spunits[ii])) {
        spunits[ii][0] = '\0';
      }
    }
    /* pre-TeemV2 explicitly "-quoted these units into a new buffer prior to
       nrrdFieldInfoParse[nrrdField_space_units]; WHY?
       (now just using the string hest gives us) */
    nio->line = hopt[spunitsIdx].parmStr;
    nio->pos = 0;
    if (nrrdFieldInfoParse[nrrdField_space_units](NULL, nrrd, nio, AIR_TRUE)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble with space units \"%s\":\n%s", me, nio->line, err);
      nio->line = NULL;
      airMopError(mop);
      return 1;
    }
    nio->line = NULL;
  }
  if (nrrdCheckMore(nrrd, AIR_FALSE /* checkData */, AIR_TRUE /* useBiff */)) {
    airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
    fprintf(stderr, "%s: problems with nrrd as set up:\n%s", me, err);
    airMopError(mop);
    return 1;
  }

  /* ----------------- END SETTING INFO ---------------- */
  /* -------------------- BEGIN I/O -------------------- */

  nio->lineSkip = lineSkip;
  nio->byteSkip = byteSkip;
  nio->encoding = nrrdEncodingArray[encodingType];
  nio->endian = endian;
  /* for the sake of reading in data files, this is as good a guess as any of what the
     header-relative path to them is.  This assuages concerns that come up even with
     headerOnly */
  nio->path = airStrdup(".");
  nio->headerDoubleExcessPrecision = hdrDblXPrec;
  if (headerOnly) {
    /* we open and hand off the output FILE* to the nrrd writer, which
       will not write any data, because of nio->skipData = AIR_TRUE */
    FILE *fileOut;
    if (!(fileOut = airFopen(out, stdout, "wb"))) {
      fprintf(stderr, "%s: couldn't fopen(\"%s\",\"wb\"): %s\n", me, out,
              strerror(errno));
      airMopError(mop);
      return 1;
    }
    airMopAdd(mop, fileOut, airFclose_mop, airMopAlways);
    /* whatever line and byte skipping is required will be simply
       recorded in the header, and done by the next reader */
    nio->detachedHeader = AIR_TRUE;
    nio->skipData = AIR_TRUE;
    if (nrrdFormatNRRD->write(fileOut, nrrd, nio)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble writing header:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
  } else {
    /* all this does is read the data from the files.  We up the verbosity
       because of all places this is probably where we really want it */
    nio->verbose += verboseBump;
    if (nrrdFormatNRRD->read(NULL, nrrd, nio)) {
      airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
      fprintf(stderr, "%s: trouble reading data files:\n%s", me, err);
      airMopError(mop);
      return 1;
    }
    nio->verbose -= verboseBump;
    /* then save normally */
    nrrdIoStateInit(nio);
    if (strlen(outData)) {
      airArrayLenSet(nio->dataFNArr, 1);
      nio->dataFN[0] = airStrdup(outData);
    }
    MY_SAVE(out, nrrd, nio);
  }

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(make, INFO);
