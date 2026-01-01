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

#include "unrrdu.h"
#include "privateUnrrdu.h"

#define INFO "Write nrrd with specific format, encoding, or endianness"
static const char *unu_saveInfoL
  = (INFO ". Use \"unu\tsave\t-f\tpnm\t|\txv\t-\" to view PPM- or "
          "PGM-compatible nrrds on unix.  EPS output is a EPSF-3.0 file with "
          "BoundingBox and HiResBoundingBox DSC comments, and is suitable for "
          "inclusion into other PostScript documents.  As a stand-alone file, the "
          "image is conveniently centered on an 8.5x11 inch page, with 0.5 "
          "inch margins.\n "
          "* Uses various fields in the NrrdIOState passed to nrrdSave");

static int
unu_saveMain(int argc, const char **argv, const char *me, const hestParm *hparm) {
  hestOpt *hopt = NULL;
  char fmtInfo[AIR_STRLEN_HUGE + 1];
  char *fmt = fmtInfo;
  size_t fmtSize = sizeof(fmtInfo);
  SN_COPY(fmt, fmtSize,
          "output file format. Possibilities include:\n "
          "\b\bo \"nrrd\": standard nrrd format\n "
          "\b\bo \"pnm\": PNM image; PPM for color, PGM for grayscale\n "
          "\b\bo \"text\": ASCII text for 1-D and 2-D data\n "
          "\b\bo \"ptext\": like \"text\" but enforcing real plain text,\n "
          "        i.e., no NRRD header fields on lines (prior to data) \n "
          "        starting with \"#\"\n "
          "\b\bo \"vtk\": VTK \"STRUCTURED_POINTS\" dataset");
  if (nrrdFormatPNG->available()) {
    SN_COPY(fmt, fmtSize, "\n \b\bo \"png\": PNG image");
  }
  SN_COPY(fmt, fmtSize, "\n \b\bo \"eps\": EPS file");
  int frmt[2];
  hestOptAdd_1_Other(&hopt, "f,format", "form", frmt, NULL, fmtInfo,
                     &unrrduUnuHestFormatCB);
  char encInfo[AIR_STRLEN_HUGE + 1];
  char *ens = encInfo;
  size_t ensSize = sizeof(encInfo);
  SN_COPY(ens, ensSize,
          "encoding of data in file.  Not all encodings are supported in "
          "a given format. Possibilities include:"
          "\n \b\bo \"raw\": raw encoding"
          "\n \b\bo \"ascii\": print data in ascii"
          "\n \b\bo \"hex\": two hex digits per byte");
  if (nrrdEncodingGzip->available()) {
    SN_COPY(ens, ensSize, "\n \b\bo \"gzip\", \"gz\": gzip compressed raw data");
  }
  if (nrrdEncodingBzip2->available()) {
    SN_COPY(ens, ensSize, "\n \b\bo \"bzip2\", \"bz2\": bzip2 compressed raw data");
  }
  if (nrrdEncodingGzip->available() || nrrdEncodingBzip2->available()) {
    SN_COPY(ens, ensSize,
            "\n The specifiers for compressions may be followed by a colon "
            "\":\", followed by an optional digit giving compression \"level\" "
            "(for gzip) or \"block size\" (for bzip2).  For gzip, this can be "
            "followed by an optional character for a compression strategy:\n "
            "\b\bo \"d\": default, Huffman with string match\n "
            "\b\bo \"h\": Huffman alone\n "
            "\b\bo \"f\": specialized for filtered data\n "
            "For example, \"gz\", \"gz:9\", \"gz:9f\" are all valid");
  }
  int enc[3];
  hestOptAdd_1_Other(&hopt, "e,encoding", "enc", enc, "raw", encInfo,
                     &unrrduUnuHestEncodingCB);
  int nioEndian;
  hestOptAdd_1_Enum(&hopt, "en,endian", "end", &nioEndian,
                    airEnumStr(airEndian, airMyEndian()),
                    "Endianness to save data out as; \"little\" for Intel and "
                    "friends; \"big\" for everyone else. "
                    "Defaults to endianness of this machine",
                    airEndian);
  Nrrd *nin;
  MY_OPT_ADD_NIN(nin, "input nrrd");
  char *out;
  MY_OPT_ADD_NOUT(out, "output nrrd");
  char *outData;
  hestOptAdd_1_String(&hopt, "od,ouputdata", "name", &outData, "",
                      "when saving to a \".nhdr\" file, "
                      "this option allows you to explicitly name the data file, "
                      "instead of (by default, not using this option) having it be "
                      "the same filename base as the header file.");

  // (boilerplate for no hparm modification)
  hestParseOrExit(hopt, argc, argv, hparm, me, unu_saveInfoL);
  airArray *mop = unrrduMopNew(hopt, NULL);
  char *err;

  NrrdIoState *nio = nrrdIoStateNew();
  airMopAdd(mop, nio, nrrdIoStateNix_mop, airMopAlways);
  nio->format = nrrdFormatArray[frmt[0]];
  nio->endian = nioEndian;
  if (nrrdFormatTypeText == frmt[0] && frmt[1]) {
    nio->bareText = AIR_TRUE;
  }
  nio->encoding = nrrdEncodingArray[enc[0]];
  if (nrrdEncodingTypeGzip == enc[0]) {
    nio->zlibLevel = enc[1];
    nio->zlibStrategy = enc[2];
  } else if (nrrdEncodingTypeBzip2 == enc[0]) {
    nio->bzip2BlockSize = enc[1];
  }

  Nrrd *nout = nrrdNew();
  airMopAdd(mop, nout, nrrdNuke_mop, airMopAlways);
  nrrdCopy(nout, nin);
  if (airMyEndian() != nio->endian) {
    nrrdSwapEndian(nout);
  }
  if (airEndsWith(out, NRRD_EXT_NHDR)) {
    if (nio->format != nrrdFormatNRRD) {
      fprintf(stderr, "%s: WARNING: will use %s format\n", me, nrrdFormatNRRD->name);
      nio->format = nrrdFormatNRRD;
    }
    if (strlen(outData)) {
      airArrayLenSet(nio->dataFNArr, 1);
      nio->dataFN[0] = airStrdup(outData);
    }
  }

  MY_HISTORY(nout, nin);
  MY_SAVE(out, nout, nio);

  airMopOkay(mop);
  return 0;
}

MY_UNRRDU_CMD(save, INFO);
