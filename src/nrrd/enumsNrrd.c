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
/* clang-format off */
/*
** Rules of thumb for editing these things.  The airEnum definitions are
** unfortunately EXTREMELY sensitive to small typo errors, and there is
** no good way to detect the errors.  So:
**
** 1) Be awake and undistracted.  Turn down the music.
** 2) When editing the char arrays, make sure that you put commas
**    where you mean them to be.  C's automatic string concatenation
**    is not your friend here.  In fact, EXPLOIT the fact that you can have
**    a comma after the last element of a list (of strings)- it decreases
**    the chances that adding a new element at the end will be thwarted by
**    the lack of a comma at the end of the previous (and previously last)
**    string.
** 3) When editing the *StrEqv and *ValEqv arrays, make absolutely
**    sure that both are changed in parallel.  Use only one enum value
**    per line; putting all equivalents on that line, and make sure that
**    there is one line in both *StrEqv and *ValEqv for all the possible
**    enum values, and that there are as many elements in each line.
** 4) Make sure that additions here are reflected in nrrdEnums.h and
**    vice versa.
*/

/* ------------------------ nrrdFormat ------------------------- */

static const char *
formatTypeStr[NRRD_FORMAT_TYPE_MAX+1] = {
  "(unknown_format)",
  "nrrd",
  "pnm",
  "png",
  "vtk",
  "text",
  "eps",
};

static const char *
formatTypeDesc[NRRD_FORMAT_TYPE_MAX+1] = {
  "unknown_format",
  "native format for nearly raw raster data",
  "Portable aNy Map: includes PGM for grayscale and PPM for color",
  "Portable Network Graphics: lossless compression of 8- and 16-bit data",
  "Visualization ToolKit STRUCTURED_POINTS data",
  "white-space-delimited plain text encoding of 2-D float array",
  "Encapsulated PostScript images",
};

static const char *
formatTypeStrEqv[] = {
  "nrrd",
  "pnm",
  "png",
  "vtk",
  "table", "text", "txt",
  "eps",
  ""
};

static const int
formatTypeValEqv[] = {
  nrrdFormatTypeNRRD,
  nrrdFormatTypePNM,
  nrrdFormatTypePNG,
  nrrdFormatTypeVTK,
  nrrdFormatTypeText, nrrdFormatTypeText, nrrdFormatTypeText,
  nrrdFormatTypeEPS,
};

static const airEnum
formatType_ae = {
  "format",
  NRRD_FORMAT_TYPE_MAX,
  formatTypeStr,  NULL,
  formatTypeDesc,
  formatTypeStrEqv, formatTypeValEqv,
  AIR_FALSE
};
const airEnum *const nrrdFormatType = &formatType_ae;

/* ------------------------ nrrdType ------------------------- */

static const char *
typeStr[NRRD_TYPE_MAX+1] = {
  "(unknown_type)",
  "signed char",
  "unsigned char",
  "short",
  "unsigned short",
  "int",
  "unsigned int",
  "long long int",
  "unsigned long long int",
  "float",
  "double",
  "block",
};

static const char *
typeDesc[NRRD_TYPE_MAX+1] = {
  "unknown type",
  "signed 1-byte integer",
  "unsigned 1-byte integer",
  "signed 2-byte integer",
  "unsigned 2-byte integer",
  "signed 4-byte integer",
  "unsigned 4-byte integer",
  "signed 8-byte integer",
  "unsigned 8-byte integer",
  "4-byte floating point",
  "8-byte floating point",
  "size user-defined at run-time",
};

#define ntCH nrrdTypeChar
#define ntUC nrrdTypeUChar
#define ntSH nrrdTypeShort
#define ntUS nrrdTypeUShort
#define ntIN nrrdTypeInt
#define ntUI nrrdTypeUInt
#define ntLL nrrdTypeLLong
#define ntUL nrrdTypeULLong
#define ntFL nrrdTypeFloat
#define ntDB nrrdTypeDouble
#define ntBL nrrdTypeBlock

static const char *
typeStrEqv[] = {
  "signed char", /* but NOT just "char" */ "int8", "int8_t",
  "uchar", "unsigned char", "uint8", "uint8_t",
  "short", "short int", "signed short", "signed short int", "int16", "int16_t",
  "ushort", "unsigned short", "unsigned short int", "uint16", "uint16_t",
  "int", "signed int", "int32", "int32_t",
  "uint", "unsigned int", "uint32", "uint32_t",
  "longlong", "long long", "long long int", "signed long long",
               "signed long long int", "int64", "int64_t",
  "ulonglong", "unsigned long long", "unsigned long long int",
               "uint64", "uint64_t",
  "float",
  "double",
  "block",
  ""
};

static const int
typeValEqv[] = {
  ntCH, ntCH, ntCH,
  ntUC, ntUC, ntUC, ntUC,
  ntSH, ntSH, ntSH, ntSH, ntSH, ntSH,
  ntUS, ntUS, ntUS, ntUS, ntUS,
  ntIN, ntIN, ntIN, ntIN,
  ntUI, ntUI, ntUI, ntUI,
  ntLL, ntLL, ntLL, ntLL, ntLL, ntLL, ntLL,
  ntUL, ntUL, ntUL, ntUL, ntUL,
  ntFL,
  ntDB,
  ntBL,
};

static const airEnum
type_ae = {
  "type",
  NRRD_TYPE_MAX,
  typeStr, NULL,
  typeDesc,
  typeStrEqv, typeValEqv,
  AIR_FALSE
};
const airEnum *const nrrdType = &type_ae;

/* ------------------------ nrrdEncodingType ------------------------- */

static const char *
encodingTypeStr[NRRD_ENCODING_TYPE_MAX+1] = {
  "(unknown_encoding)",
  "raw",
  "ascii",
  "hex",
  "gz",
  "bz2",
  "zrl"
};

static const char *
encodingTypeDesc[NRRD_ENCODING_TYPE_MAX+1] = {
  "unknown encoding",
  "file is byte-for-byte same as memory representation",
  "values written out in ASCII",
  "case-insenstive hexadecimal encoding (2 chars / byte)",
  "gzip compression of binary encoding",
  "bzip2 compression of binary encoding",
  "simple compression by encoding run-length of zeros",
};

static const char *
encodingTypeStrEqv[] = {
  "raw",
  "txt", "text", "ascii",
  "hex",
  "gz", "gzip",
  "bz2", "bzip2",
  "zrl",
  ""
};

static const int
encodingTypeValEqv[] = {
  nrrdEncodingTypeRaw,
  nrrdEncodingTypeAscii, nrrdEncodingTypeAscii, nrrdEncodingTypeAscii,
  nrrdEncodingTypeHex,
  nrrdEncodingTypeGzip, nrrdEncodingTypeGzip,
  nrrdEncodingTypeBzip2, nrrdEncodingTypeBzip2,
  nrrdEncodingTypeZRL
};

static const airEnum
encodingType_ae = {
  "encoding",
  NRRD_ENCODING_TYPE_MAX,
  encodingTypeStr, NULL,
  encodingTypeDesc,
  encodingTypeStrEqv, encodingTypeValEqv,
  AIR_FALSE
};
const airEnum *const nrrdEncodingType = &encodingType_ae;

/* ------------------------ nrrdCenter ------------------------- */

static const char *
centerStr[NRRD_CENTER_MAX+1] = {
  "(unknown_center)",
  "node",
  "cell",
};

static const char *
centerDesc[NRRD_CENTER_MAX+1] = {
  "unknown centering",
  "samples are at boundaries between elements along axis",
  "samples are at centers of elements along axis",
};

static const airEnum
center_ae = {
  "centering",
  NRRD_CENTER_MAX,
  centerStr, NULL,
  centerDesc,
  NULL, NULL,
  AIR_FALSE
};
const airEnum *const nrrdCenter = &center_ae;

/* ------------------------ nrrdKind ------------------------- */

/*
  nrrdKindUnknown,
  nrrdKindDomain,            *  1: any image domain *
  nrrdKindSpace,             *  2: a spatial domain *
  nrrdKindTime,              *  3: a temporal domain *
  * -------------------------- end domain kinds *
  * -------------------------- begin range kinds *
  nrrdKindList,              *  4: any list of values, non-resample-able *
  nrrdKindPoint,             *  5: coords of a point *
  nrrdKindVector,            *  6: coeffs of (contravariant) vector *
  nrrdKindCovariantVector,   *  7: coeffs of covariant vector (eg gradient) *
  nrrdKindNormal,            *  8: coeffs of unit-length covariant vector *
  * -------------------------- end arbitrary size kinds *
  * -------------------------- begin size-specific kinds *
  nrrdKindStub,              *  9: axis with one sample (a placeholder) *
  nrrdKindScalar,            * 10: effectively, same as a stub *
  nrrdKindComplex,           * 11: real and imaginary components *
  nrrdKind2Vector,           * 12: 2 component vector *
  nrrdKind3Color,            * 13: ANY 3-component color value *
  nrrdKindRGBColor,          * 14: RGB, no colorimetry *
  nrrdKindHSVColor,          * 15: HSV, no colorimetry *
  nrrdKindXYZColor,          * 16: perceptual primary colors *
  nrrdKind4Color,            * 17: ANY 4-component color value *
  nrrdKindRGBAColor,         * 18: RGBA, no colorimetry *
  nrrdKind3Vector,           * 19: 3-component vector *
  nrrdKind3Gradient,         * 20: 3-component covariant vector *
  nrrdKind3Normal,           * 21: 3-component covector, assumed normalized *
  nrrdKind4Vector,           * 22: 4-component vector *
  nrrdKindQuaternion,        * 23: (w,x,y,z), not necessarily normalized *
  nrrdKind2DSymMatrix,       * 24: Mxx Mxy Myy *
  nrrdKind2DMaskedSymMatrix, * 25: mask Mxx Mxy Myy *
  nrrdKind2DMatrix,          * 26: Mxx Mxy Myx Myy *
  nrrdKind2DMaskedMatrix,    * 27: mask Mxx Mxy Myx Myy *
  nrrdKind3DSymMatrix,       * 28: Mxx Mxy Mxz Myy Myz Mzz *
  nrrdKind3DMaskedSymMatrix, * 29: mask Mxx Mxy Mxz Myy Myz Mzz *
  nrrdKind3DMatrix,          * 30: Mxx Mxy Mxz Myx Myy Myz Mzx Mzy Mzz *
  nrrdKind3DMaskedMatrix,    * 31: mask Mxx Mxy Mxz Myx Myy Myz Mzx Mzy Mzz *
*/

static const char *
kindStr[NRRD_KIND_MAX+1] = {
  "(unknown_kind)",
  "domain",
  "space",
  "time",
  "list",
  "point",
  "vector",
  "covariant-vector",
  "normal",
  "stub",
  "scalar",
  "complex",
  "2-vector",
  "3-color",
  "RGB-color",
  "HSV-color",
  "XYZ-color",
  "4-color",
  "RGBA-color",
  "3-vector",
  "3-gradient",
  "3-normal",
  "4-vector",
  "quaternion",
  "2D-symmetric-matrix",
  "2D-masked-symmetric-matrix",
  "2D-matrix",
  "2D-masked-matrix",
  "3D-symmetric-matrix",
  "3D-masked-symmetric-matrix",
  "3D-matrix",
  "3D-masked-matrix",
};

static const char *
kindDesc[NRRD_KIND_MAX+1] = {
  "unknown kind",
  "a domain variable of the function which the nrrd samples",
  "a spatial domain, like the axes of a measured volume image",
  "a temporal domain, as from time-varying measurements",
  "some list of attributes; it makes no sense to resample along these",
  "coordinates of a point",
  "coefficients of a (contravariant) vector",
  "coefficients of a covariant vector, such as a gradient",
  "coefficients of a normalized covariant vector",
  "a place-holder axis with a single sample",
  "axis used to indicate that the nrrd contains a scalar value",
  "real and imaginary parts of a value",
  "a 2-component vector",
  "any 3-component color value",
  "red-green-blue color",
  "hue-saturation-value single hexcone color",
  "perceptual primaries color",
  "any 4-component color value",
  "red-green-blue-alpha color",
  "a 3-element (contravariant) vector",
  "a 3-element gradient (covariant) vector",
  "a 3-element (covariant) vector which is assumed normalized",
  "a 4-element (contravariant) vector",
  "quaternion: x y z w",
  "3 elements of 2D symmetric matrix: Mxx Mxy Myy",
  "mask plus 3 elements of 2D symmetric matrix: mask Mxx Mxy Myy",
  "4 elements of general 2D matrix: Mxx Mxy Myx Myy",
  "mask plus 4 elements of general 2D matrix: mask Mxx Mxy Myx Myy",
  "6 elements of 3D symmetric matrix: Mxx Mxy Mxz Myy Myz Mzz",
  "mask plus 6 elements of 3D symmetric matrix: mask Mxx Mxy Mxz Myy Myz Mzz",
  "9 elements of general 3D matrix: Mxx Mxy Mxz Myx Myy Myz Mzx Mzy Mzz",
  "mask plus 9 elements of general 3D matrix: mask Mxx Mxy Mxz Myx Myy Myz Mzx Mzy Mzz",
};

static const char *
kindStr_Eqv[] = {
  "domain",
  "space",
  "time",
  "list",
  "point",
  "vector", "contravariant-vector",
  "covariant-vector",
  "normal",
  "stub",
  "scalar",
  "complex",
  "2-vector",
  "3-color",
  "RGB-color", "RGBcolor", "RGB",
  "HSV-color", "HSVcolor", "HSV",
  "XYZ-color",
  "4-color",
  "RGBA-color", "RGBAcolor", "RGBA",
  "3-vector",
  "3-gradient",
  "3-normal",
  "4-vector",
  "quaternion",
  "2D-symmetric-matrix", "2D-sym-matrix",
        "2D-symmetric-tensor", "2D-sym-tensor",
  "2D-masked-symmetric-matrix", "2D-masked-sym-matrix",
        "2D-masked-symmetric-tensor", "2D-masked-sym-tensor",
  "2D-matrix",
        "2D-tensor",
  "2D-masked-matrix",
        "2D-masked-tensor",
  "3D-symmetric-matrix", "3D-sym-matrix",
        "3D-symmetric-tensor", "3D-sym-tensor",
  "3D-masked-symmetric-matrix", "3D-masked-sym-matrix",
        "3D-masked-symmetric-tensor", "3D-masked-sym-tensor",
  "3D-matrix",
        "3D-tensor",
  "3D-masked-matrix",
        "3D-masked-tensor",
  ""
};

static int
kindVal_Eqv[] = {
  nrrdKindDomain,
  nrrdKindSpace,
  nrrdKindTime,
  nrrdKindList,
  nrrdKindPoint,
  nrrdKindVector, nrrdKindVector,
  nrrdKindCovariantVector,
  nrrdKindNormal,
  nrrdKindStub,
  nrrdKindScalar,
  nrrdKindComplex,
  nrrdKind2Vector,
  nrrdKind3Color,
  nrrdKindRGBColor, nrrdKindRGBColor, nrrdKindRGBColor,
  nrrdKindHSVColor, nrrdKindHSVColor, nrrdKindHSVColor,
  nrrdKindXYZColor,
  nrrdKind4Color,
  nrrdKindRGBAColor, nrrdKindRGBAColor, nrrdKindRGBAColor,
  nrrdKind3Vector,
  nrrdKind3Gradient,
  nrrdKind3Normal,
  nrrdKind4Vector,
  nrrdKindQuaternion,
  nrrdKind2DSymMatrix, nrrdKind2DSymMatrix,
        nrrdKind2DSymMatrix, nrrdKind2DSymMatrix,
  nrrdKind2DMaskedSymMatrix, nrrdKind2DMaskedSymMatrix,
        nrrdKind2DMaskedSymMatrix, nrrdKind2DMaskedSymMatrix,
  nrrdKind2DMatrix,
        nrrdKind2DMatrix,
  nrrdKind2DMaskedMatrix,
        nrrdKind2DMaskedMatrix,
  nrrdKind3DSymMatrix, nrrdKind3DSymMatrix,
        nrrdKind3DSymMatrix, nrrdKind3DSymMatrix,
  nrrdKind3DMaskedSymMatrix, nrrdKind3DMaskedSymMatrix,
        nrrdKind3DMaskedSymMatrix, nrrdKind3DMaskedSymMatrix,
  nrrdKind3DMatrix,
        nrrdKind3DMatrix,
  nrrdKind3DMaskedMatrix,
        nrrdKind3DMaskedMatrix,
};

static const airEnum
kind_ae = {
  "kind",
  NRRD_KIND_MAX,
  kindStr, NULL,
  kindDesc,
  kindStr_Eqv, kindVal_Eqv,
  AIR_FALSE
};
const airEnum *const nrrdKind = &kind_ae;

/* ------------------------ nrrdField ------------------------- */

static const char *
fieldStr[NRRD_FIELD_MAX+1] = {
  "Ernesto \"Che\" Guevara",
  "#",
  "content",
  "number",
  "type",
  "block size",
  "dimension",
  "space",
  "space dimension",
  "sizes",
  "spacings",
  "thicknesses",
  "axis mins",
  "axis maxs",
  "space directions",
  "centerings",
  "kinds",
  "labels",
  "units",
  "min",
  "max",
  "old min",
  "old max",
  "endian",
  "encoding",
  "line skip",
  "byte skip",
  "key/value", /* this is the one field for which the canonical string
                  here is totally different from the field identifier in the
                  NRRD file format (":=").  We include nrrdField_keyvalue
                  in the enum because it is very useful to have a consistent
                  way of identifying lines in the format */
  "sample units",
  "space units",
  "space origin",
  "measurement frame",
  "data file",
};

static const char *
fieldDesc[NRRD_FIELD_MAX+1] = {
  "unknown field identifier",
  "comment",
  "short description of whole array and/or its provenance",
  "total number of samples in array",
  "type of sample value",
  "number of bytes in one block (for block-type)",
  "number of axes in array",
  "identifier for space in which array grid lies",
  "dimension of space in which array grid lies",
  "list of number of samples along each axis, aka \"dimensions\" of the array",
  "list of sample spacings along each axis",
  "list of sample thicknesses along each axis",
  "list of minimum positions associated with each axis",
  "list of maximum positions associated with each axis",
  "list of direction inter-sample vectors for each axis",
  "list of sample centerings for each axis",
  "list of kinds for each axis",
  "list of short descriptions for each axis",
  "list of units in which each axes' spacing and thickness is measured",
  "supposed minimum array value",
  "supposed maximum array value",
  "minimum array value prior to quantization",
  "maximum array value prior to quantization",
  "endiannes of data as written in file",
  "encoding of data written in file",
  "number of lines to skip prior to byte skip and reading data",
  "number of bytes to skip after line skip and prior to reading data",
  "string-based key/value pairs",
  "units of measurement of (scalar) values inside array itself",
  "list of units for measuring origin and direct vectors' coefficients",
  "location in space of center of first (lowest memory address) sample",
  "maps coords of (non-scalar) values to coords of surrounding space",
  "with detached headers, where is data to be found",
};

static const char *
fieldStrEqv[] = {
  "#",
  "content",
  "number",
  "type",
  "block size", "blocksize",
  "dimension",
  "space",
  "space dimension", "spacedimension",
  "sizes",
  "spacings",
  "thicknesses",
  "axis mins", "axismins",
  "axis maxs", "axismaxs",
  "space directions", "spacedirections",
  "centers", "centerings",
  "kinds",
  "labels",
  "units",
  "min",
  "max",
  "old min", "oldmin",
  "old max", "oldmax",
  "endian",
  "encoding",
  "line skip", "lineskip",
  "byte skip", "byteskip",
  "key/value",  /* bogus, here to keep the airEnum complete */
  "sample units", "sampleunits",
  "space units", "spaceunits",
  "space origin", "spaceorigin",
  "measurement frame", "measurementframe",
  "data file", "datafile",
  ""
};

static const int
fieldValEqv[] = {
  nrrdField_comment,
  nrrdField_content,
  nrrdField_number,
  nrrdField_type,
  nrrdField_block_size, nrrdField_block_size,
  nrrdField_dimension,
  nrrdField_space,
  nrrdField_space_dimension, nrrdField_space_dimension,
  nrrdField_sizes,
  nrrdField_spacings,
  nrrdField_thicknesses,
  nrrdField_axis_mins, nrrdField_axis_mins,
  nrrdField_axis_maxs, nrrdField_axis_maxs,
  nrrdField_space_directions, nrrdField_space_directions,
  nrrdField_centers, nrrdField_centers,
  nrrdField_kinds,
  nrrdField_labels,
  nrrdField_units,
  nrrdField_min,
  nrrdField_max,
  nrrdField_old_min, nrrdField_old_min,
  nrrdField_old_max, nrrdField_old_max,
  nrrdField_endian,
  nrrdField_encoding,
  nrrdField_line_skip, nrrdField_line_skip,
  nrrdField_byte_skip, nrrdField_byte_skip,
  nrrdField_keyvalue,
  nrrdField_sample_units, nrrdField_sample_units,
  nrrdField_space_units, nrrdField_space_units,
  nrrdField_space_origin, nrrdField_space_origin,
  nrrdField_measurement_frame, nrrdField_measurement_frame,
  nrrdField_data_file, nrrdField_data_file,
};

static const airEnum
field_ae = {
  "nrrd_field",
  NRRD_FIELD_MAX,
  fieldStr, NULL,
  fieldDesc,
  fieldStrEqv, fieldValEqv,
  AIR_FALSE  /* field identifiers not case sensitive */
};
const airEnum *const nrrdField = &field_ae;

/* ------------------------ nrrdSpace ------------------------- */

/*
  nrrdSpaceUnknown,
  nrrdSpaceRightUp,                   *  1: 2-D, oriented like upper right
                                         Cartesian quadrant, number I *
  nrrdSpaceRightDown,                 *  2: 2-D, oriented like raster
                                         coordinates *
  nrrdSpaceRightAnteriorSuperior,     *  3: NIFTI-1 (right-handed) *
  nrrdSpaceLeftAnteriorSuperior,      *  4: standard Analyze (left-handed) *
  nrrdSpaceLeftPosteriorSuperior,     *  5: DICOM 3.0 (right-handed) *
  nrrdSpaceRightAnteriorSuperiorTime, *  6: *
  nrrdSpaceLeftAnteriorSuperiorTime,  *  7: *
  nrrdSpaceLeftPosteriorSuperiorTime, *  8: *
  nrrdSpaceScannerXYZ,                *  9: ACR/NEMA 2.0 (pre-DICOM 3.0) *
  nrrdSpaceScannerXYZTime,            * 10: *
  nrrdSpace3DRightHanded,             * 11: *
  nrrdSpace3DLeftHanded,              * 12: *
  nrrdSpace3DRightHandedTime,         * 13: *
  nrrdSpace3DLeftHandedTime,          * 14: *
  nrrdSpaceLast
*/

static const char *
spaceStr[NRRD_SPACE_MAX+1] = {
  "(unknown_space)",
  "right-up",
  "right-down",
  "right-anterior-superior",
  "left-anterior-superior",
  "left-posterior-superior",
  "right-anterior-superior-time",
  "left-anterior-superior-time",
  "left-posterior-superior-time",
  "scanner-xyz",
  "scanner-xyz-time",
  "3D-right-handed",
  "3D-left-handed",
  "3D-right-handed-time",
  "3D-left-handed-time",
};

static const char *
spaceDesc[NRRD_SPACE_MAX+1] = {
  "unknown space",
  "right-up (like Cartesian quadrant I)",
  "right-down (like raster coordinates)",
  "right-anterior-superior (used in NIFTI-1 and SPL's 3D Slicer)",
  "left-anterior-superior (used in Analyze 7.5)",
  "left-posterior-superior (used in DICOM 3)",
  "right-anterior-superior-time",
  "left-anterior-superior-time",
  "left-posterior-superior-time",
  "scanner-xyz (used in ACR/NEMA 2.0)",
  "scanner-xyz-time",
  "3D-right-handed",
  "3D-left-handed",
  "3D-right-handed-time",
  "3D-left-handed-time",
};

static const char *
spaceStrEqv[] = {
  "right-up", "right up",
  "right-down", "right down",
  "right-anterior-superior", "right anterior superior",
      "rightanteriorsuperior", "RAS",
  "left-anterior-superior", "left anterior superior",
      "leftanteriorsuperior", "LAS",
  "left-posterior-superior", "left posterior superior",
      "leftposteriorsuperior", "LPS",
  "right-anterior-superior-time", "right anterior superior time",
      "rightanteriorsuperiortime", "RAST",
  "left-anterior-superior-time", "left anterior superior time",
      "leftanteriorsuperiortime", "LAST",
  "left-posterior-superior-time", "left posterior superior time",
      "leftposteriorsuperiortime", "LPST",
  "scanner-xyz",
  "scanner-xyz-time", "scanner-xyzt",
  "3D-right-handed", "3D right handed", "3Drighthanded",
  "3D-left-handed", "3D left handed", "3Dlefthanded",
  "3D-right-handed-time", "3D right handed time",
      "3Drighthandedtime",
  "3D-left-handed-time", "3D left handed time",
      "3Dlefthandedtime",
  ""
};

static const int
spaceValEqv[] = {
  nrrdSpaceRightUp, nrrdSpaceRightUp,
  nrrdSpaceRightDown, nrrdSpaceRightDown,
  nrrdSpaceRightAnteriorSuperior, nrrdSpaceRightAnteriorSuperior,
     nrrdSpaceRightAnteriorSuperior, nrrdSpaceRightAnteriorSuperior,
  nrrdSpaceLeftAnteriorSuperior, nrrdSpaceLeftAnteriorSuperior,
     nrrdSpaceLeftAnteriorSuperior, nrrdSpaceLeftAnteriorSuperior,
  nrrdSpaceLeftPosteriorSuperior, nrrdSpaceLeftPosteriorSuperior,
     nrrdSpaceLeftPosteriorSuperior, nrrdSpaceLeftPosteriorSuperior,
  nrrdSpaceRightAnteriorSuperiorTime, nrrdSpaceRightAnteriorSuperiorTime,
     nrrdSpaceRightAnteriorSuperiorTime, nrrdSpaceRightAnteriorSuperiorTime,
  nrrdSpaceLeftAnteriorSuperiorTime, nrrdSpaceLeftAnteriorSuperiorTime,
     nrrdSpaceLeftAnteriorSuperiorTime, nrrdSpaceLeftAnteriorSuperiorTime,
  nrrdSpaceLeftPosteriorSuperiorTime, nrrdSpaceLeftPosteriorSuperiorTime,
     nrrdSpaceLeftPosteriorSuperiorTime, nrrdSpaceLeftPosteriorSuperiorTime,
  nrrdSpaceScannerXYZ,
  nrrdSpaceScannerXYZTime, nrrdSpaceScannerXYZTime,
  nrrdSpace3DRightHanded, nrrdSpace3DRightHanded, nrrdSpace3DRightHanded,
  nrrdSpace3DLeftHanded, nrrdSpace3DLeftHanded, nrrdSpace3DLeftHanded,
  nrrdSpace3DRightHandedTime, nrrdSpace3DRightHandedTime,
     nrrdSpace3DRightHandedTime,
  nrrdSpace3DLeftHandedTime, nrrdSpace3DLeftHandedTime,
     nrrdSpace3DLeftHandedTime
};

static const airEnum
space_ae = {
  "space",
  NRRD_SPACE_MAX,
  spaceStr, NULL,
  spaceDesc,
  spaceStrEqv, spaceValEqv,
  AIR_FALSE
};
const airEnum *const nrrdSpace = &space_ae;

/* ------------------------ nrrdSpacingStatus ------------------------- */

static const char *
spacingStatusStr[NRRD_SPACING_STATUS_MAX+1] = {
  "(unknown_status)",
  "none",
  "scalarNoSpace",
  "scalarWithSpace",
  "direction",
};

static const char *
spacingStatusDesc[NRRD_SPACING_STATUS_MAX+1] = {
  "unknown spacing status behavior",
  "neither axis->spacing nor axis->spaceDirection set",
  "axis->spacing set normally",
  "axis->spacing set, with surround space (?)",
  "axis->spaceDirection set normally",
};

static const airEnum
spacingStatus_ae = {
  "spacing status",
  NRRD_SPACING_STATUS_MAX,
  spacingStatusStr, NULL,
  spacingStatusDesc,
  NULL, NULL,
  AIR_FALSE
};
const airEnum *const nrrdSpacingStatus = &spacingStatus_ae;

/* ---- BEGIN non-NrrdIO */

/* ---------------------- nrrdFormatPNGsRGBIntent ------------------------- */

static const char *
formatPNGsRGBIntentStr[NRRD_FORMAT_PNG_SRGB_INTENT_NUM+1] = {
  "(unknown_sRGB_intent)",
  "perceptual",
  "relative",
  "saturation",
  "absolute",
  "none"
};

static int
formatPNGsRGBIntentVal[NRRD_FORMAT_PNG_SRGB_INTENT_NUM+1] = {
  nrrdFormatPNGsRGBIntentUnknown,    /* -1: nobody knows */
  /* it is true and perhaps odd that we aren't using the values like
     PNG_sRGB_INTENT_PERCEPTUAL, PNG_sRGB_INTENT_RELATIVE, etc. Why not:
     1) we need this enum to be valid and consistent regardless of whether
     this Teem is compiled with PNG support
     2) The values of those constants are apparently pegged to constants set
     in ICC profiles, not libpng itself, libpng isn't the authority on them
     3) The values seem to be set in stone */
  nrrdFormatPNGsRGBIntentPerceptual, /* 0 */
  nrrdFormatPNGsRGBIntentRelative,   /* 1 */
  nrrdFormatPNGsRGBIntentSaturation, /* 2 */
  nrrdFormatPNGsRGBIntentAbsolute,   /* 3 */
  nrrdFormatPNGsRGBIntentNone        /* 4 */
};

static const char *
formatPNGsRGBIntentStrEqv[] = {
  "perceptual", "perc", "p",
  "relative",   "rel",  "r",
  "saturation", "sat",  "s",
  "absolute",   "abs",  "a",
  "none",       "no",   "n",
  ""
};

static int
formatPNGsRGBIntentValEqv[] = {
  nrrdFormatPNGsRGBIntentPerceptual, nrrdFormatPNGsRGBIntentPerceptual, nrrdFormatPNGsRGBIntentPerceptual,
  nrrdFormatPNGsRGBIntentRelative,   nrrdFormatPNGsRGBIntentRelative,   nrrdFormatPNGsRGBIntentRelative,
  nrrdFormatPNGsRGBIntentSaturation, nrrdFormatPNGsRGBIntentSaturation, nrrdFormatPNGsRGBIntentSaturation,
  nrrdFormatPNGsRGBIntentAbsolute,   nrrdFormatPNGsRGBIntentAbsolute,   nrrdFormatPNGsRGBIntentAbsolute,
  nrrdFormatPNGsRGBIntentNone,       nrrdFormatPNGsRGBIntentNone,       nrrdFormatPNGsRGBIntentNone
};

static const char *
formatPNGsRGBIntentDesc[NRRD_FORMAT_PNG_SRGB_INTENT_NUM+1] = {
  "unknown sRGB rendering intent",
  /* see http://www.libpng.org/pub/png/book/chapter10.html and
     https://en.wikipedia.org/wiki/Color_management#Rendering_intent */
  "perceptual: expand/compress/shift gamut to fit within output",
  "relative colorimetric: true colors, but shifted by media white point",
  "saturation: gamut remapping that preserves saturation",
  "absolute colorimetric: true colors were possible, else clipped",
  "none: do not store any intent in the sRGB chunk"
};

static const airEnum
formatPNGsRGBIntent_ae = {
  "sRGB intent",
  NRRD_FORMAT_PNG_SRGB_INTENT_NUM,
  formatPNGsRGBIntentStr, formatPNGsRGBIntentVal,
  formatPNGsRGBIntentDesc,
  formatPNGsRGBIntentStrEqv, formatPNGsRGBIntentValEqv,
  AIR_FALSE
};
const airEnum *const nrrdFormatPNGsRGBIntent = &formatPNGsRGBIntent_ae;

/* -------------------- nrrdOrientationHave --------------------- */

static const char *
orientationHaveStr[NRRD_ORIENTATION_HAVE_MAX+1] = {
  "(unknown_orientation_have)",
  "nothing",
  "spacing",
  "min+spacing",
  "min+max",
  "directions",
  "origin+directions"
};

static const char *
orientationHaveDesc[NRRD_ORIENTATION_HAVE_MAX+1] = {
  "unknown orientation have",
  "know nothing",
  "know per-axis spacing",
  "know per-axis min and spacing",
  "know per-axis min and max",
  "know space directions",
  "know space origin and directions"
};

static const char *
orientationHaveStrEqv[] = {
  "nothing",
  "spacing", "spc",
  "min+spacing", "min+spc", "minspacing", "minspc",
  "min+max", "minmax",
  "directions", "dirs",
  "origin+directions", "origindirections", "oridirs", "full",
  ""
};

static const int
orientationHaveValEqv[] = {
  nrrdOrientationHaveNothing,
  nrrdOrientationHaveSpacing,
  nrrdOrientationHaveSpacing,
  nrrdOrientationHaveMinSpacing,
  nrrdOrientationHaveMinSpacing,
  nrrdOrientationHaveMinSpacing,
  nrrdOrientationHaveMinSpacing,
  nrrdOrientationHaveMinMax,
  nrrdOrientationHaveMinMax,
  nrrdOrientationHaveDirections,
  nrrdOrientationHaveDirections,
  nrrdOrientationHaveOriginDirections,
  nrrdOrientationHaveOriginDirections,
  nrrdOrientationHaveOriginDirections,
  nrrdOrientationHaveOriginDirections
};

static const airEnum
orientationHave_ae = {
  "orientation have",
  NRRD_ORIENTATION_HAVE_MAX,
  orientationHaveStr, NULL,
  orientationHaveDesc,
  orientationHaveStrEqv, orientationHaveValEqv,
  AIR_FALSE
};
const airEnum *const nrrdOrientationHave = &orientationHave_ae;

/* ---- BEGIN non-NrrdIO */
/* ------------------------ nrrdBoundary ------------------------- */

static const char *
boundaryStr[NRRD_BOUNDARY_MAX+1] = {
  "(unknown_boundary)",
  "pad",
  "bleed",
  "wrap",
  "weight",
  "mirror"
};

static const char *
boundaryDesc[NRRD_BOUNDARY_MAX+1] = {
  "unknown boundary behavior",
  "pad with some specified value",
  "copy values from edge outward as needed",
  "wrap around to other end of axis",
  "re-weight (by normalization) samples within axis range",
  "mirror folding"
};

static const airEnum
boundary_ae = {
  "boundary behavior",
  NRRD_BOUNDARY_MAX,
  boundaryStr, NULL,
  boundaryDesc,
  NULL, NULL,
  AIR_FALSE
};
const airEnum *const nrrdBoundary = &boundary_ae;

/* ------------------------ nrrdMeasure ------------------------- */

static const char *
measureStr[NRRD_MEASURE_MAX+1] = {
  "(unknown_measure)",
  "min",
  "max",
  "mean",
  "median",
  "mode",
  "product",
  "sum",
  "L1",
  "L2",
  "L4",
  "normalizedL2",
  "RMS",
  "Linf",
  "variance",
  "stdv",
  "CoV",
  "skew",
  "line-slope",
  "line-intercept",
  "line-error",
  "histo-min",
  "histo-max",
  "histo-mean",
  "histo-median",
  "histo-mode",
  "histo-product",
  "histo-sum",
  "histo-L2",
  "histo-variance",
  "histo-stdv",
};

static const char *
measureDesc[NRRD_MEASURE_MAX+1] = {
  "unknown measure",
  "minimum of values",
  "maximum of values",
  "mean of values",
  "median of values",
  "mode of values",
  "product of values",
  "sum of values",
  "L1 norm of values",
  "L2 norm of values",
  "L4 norm of values",
  "L2 norm of values divided by # of values",
  "Root of Mean of Squares",
  "Linf norm of values",
  "variance of values",
  "standard deviation of values",
  "coefficient of variation of values",
  "skew of values",
  "slope of line of best fit",
  "y-intercept of line of best fit",
  "error of line fitting",
  "minimum of histogrammed values",
  "maximum of histogrammed values",
  "mean of histogrammed values",
  "median of histogrammed values",
  "mode of histogrammed values",
  "product of histogrammed values",
  "sum of histogrammed values",
  "L2 norm of histogrammed values",
  "variance of histogrammed values",
  "standard deviation of histogrammed values",
};

static const char *
measureStrEqv[] = {
  "min",
  "max",
  "mean",
  "median",
  "mode",
  "product", "prod",
  "sum",
  "L1",
  "L2",
  "L4",
  "normalizedL2", "normL2", "nL2",
  "rootmeansquare", "rms",
  "Linf",
  "variance", "var",
  "SD", "stdv",
  "cov",
  "skew", "skewness",
  "slope", "line-slope",
  "intc", "intercept", "line-intc", "line-intercept",
  "error", "line-error",
  "histo-min",
  "histo-max",
  "histo-mean",
  "histo-median",
  "histo-mode",
  "histo-product",
  "histo-sum",
  "histo-l2",
  "histo-variance", "histo-var",
  "histo-sd", "histo-stdv",
  ""
};

static const int
measureValEqv[] = {
  nrrdMeasureMin,
  nrrdMeasureMax,
  nrrdMeasureMean,
  nrrdMeasureMedian,
  nrrdMeasureMode,
  nrrdMeasureProduct, nrrdMeasureProduct,
  nrrdMeasureSum,
  nrrdMeasureL1,
  nrrdMeasureL2,
  nrrdMeasureL4,
  nrrdMeasureNormalizedL2, nrrdMeasureNormalizedL2, nrrdMeasureNormalizedL2,
  nrrdMeasureRootMeanSquare, nrrdMeasureRootMeanSquare,
  nrrdMeasureLinf,
  nrrdMeasureVariance, nrrdMeasureVariance,
  nrrdMeasureSD, nrrdMeasureSD,
  nrrdMeasureCoV,
  nrrdMeasureSkew, nrrdMeasureSkew,
  nrrdMeasureLineSlope, nrrdMeasureLineSlope,
  nrrdMeasureLineIntercept, nrrdMeasureLineIntercept,
     nrrdMeasureLineIntercept, nrrdMeasureLineIntercept,
  nrrdMeasureLineError, nrrdMeasureLineError,
  nrrdMeasureHistoMin,
  nrrdMeasureHistoMax,
  nrrdMeasureHistoMean,
  nrrdMeasureHistoMedian,
  nrrdMeasureHistoMode,
  nrrdMeasureHistoProduct,
  nrrdMeasureHistoSum,
  nrrdMeasureHistoL2,
  nrrdMeasureHistoVariance, nrrdMeasureHistoVariance,
  nrrdMeasureHistoSD, nrrdMeasureHistoSD,
};

static const airEnum
measure_ae = {
  "measure",
  NRRD_MEASURE_MAX,
  measureStr, NULL,
  measureDesc,
  measureStrEqv, measureValEqv,
  AIR_FALSE
};
const airEnum *const nrrdMeasure = &measure_ae;

/* ------------------------ nrrdUnaryOp ---------------------- */

#define nuNeg nrrdUnaryOpNegative
#define nuRcp nrrdUnaryOpReciprocal
#define nuSin nrrdUnaryOpSin
#define nuCos nrrdUnaryOpCos
#define nuTan nrrdUnaryOpTan
#define nuAsn nrrdUnaryOpAsin
#define nuAcs nrrdUnaryOpAcos
#define nuAtn nrrdUnaryOpAtan
#define nuExp nrrdUnaryOpExp
#define nuLge nrrdUnaryOpLog
#define nuLg2 nrrdUnaryOpLog2
#define nuLgt nrrdUnaryOpLog10
#define nuL1p nrrdUnaryOpLog1p
#define nuEm1 nrrdUnaryOpExpm1
#define nuSqt nrrdUnaryOpSqrt
#define nuCbt nrrdUnaryOpCbrt
#define nuErf nrrdUnaryOpErf
#define nuNerf nrrdUnaryOpNerf
#define nuCil nrrdUnaryOpCeil
#define nuFlr nrrdUnaryOpFloor
#define nuRup nrrdUnaryOpRoundUp
#define nuRdn nrrdUnaryOpRoundDown
#define nuAbs nrrdUnaryOpAbs
#define nuSgn nrrdUnaryOpSgn
#define nuExs nrrdUnaryOpExists
#define nuRnd nrrdUnaryOpRand
#define nuNrn nrrdUnaryOpNormalRand
#define nuoIf nrrdUnaryOpIf
#define nuZer nrrdUnaryOpZero
#define nuOne nrrdUnaryOpOne

static const char *
unaryOpStr[NRRD_UNARY_OP_MAX+1] = {
  "(unknown_unary_op)",
  "-",
  "r",
  "sin",
  "cos",
  "tan",
  "asin",
  "acos",
  "atan",
  "exp",
  "log",
  "log2",
  "log10",
  "log1p",
  "expm1",
  "sqrt",
  "cbrt",
  "erf",
  "nerf",
  "ceil",
  "floor",
  "roundup",
  "rounddown",
  "abs",
  "sgn",
  "exists",
  "rand",
  "normrand",
  "if",
  "zero",
  "one",
  "tauofsig",
  "sigoftau"
};

static const char *
unaryOpDesc[NRRD_UNARY_OP_MAX+1] = {
  "unknown unary op",
  "negative; additive inverse",
  "reciprocal; multiplicative inverse",
  "sin",
  "cos",
  "tan",
  "arcsin",
  "arccos",
  "arctan",
  "e raised to something",
  "natural (base e) logarithm",
  "base 2 logarithm",
  "base 10 logarithm",
  "accurate ln(1+x)",
  "accurate exp(x)-1",
  "square root",
  "cube root",
  "error function (integral of gaussian)",
  "erf, mapped to range (0,1)",
  "smallest integer greater than or equal",
  "largest integer less than or equal",
  "round to closest integer (0.5 rounded to 1)",
  "round to closest integer (0.5 rounded to 0)",
  "absolute value",
  "sign of value (-1, 0, or 1)",
  "value is not infinity or NaN",
  "uniformly distributed random value between 0 and 1",
  "normally distributed random value, mean 0, stdv 1",
  "if nonzero, 1, else 0",
  "always zero",
  "always one",
  "Lindeberg effective scale Tau as function of Sigma",
  "Sigma as function of Lindeberg effective scale Tau"
};

static const char *
unaryOpStrEqv[] = {
  "-", "neg", "negative", "minus",
  "r", "recip",
  "sin",
  "cos",
  "tan",
  "asin", "arcsin",
  "acos", "arccos",
  "atan", "arctan",
  "exp",
  "ln", "log",
  "log2",
  "log10",
  "ln1p", "log1p",
  "expm1",
  "sqrt",
  "cbrt",
  "erf",
  "nerf",
  "ceil",
  "floor",
  "roundup", "rup",
  "rounddown", "rdown", "rdn",
  "abs", "fabs",
  "sgn", "sign",
  "exists",
  "rand",
  "normalrand", "normrand", "nrand",
  "if",
  "zero", "0",
  "one", "1",
  "tauofsig", "tos",
  "sigoftau", "sot",
  ""
};

static const int
unaryOpValEqv[] = {
  nuNeg, nuNeg, nuNeg, nuNeg,
  nuRcp, nuRcp,
  nuSin,
  nuCos,
  nuTan,
  nuAsn, nuAsn,
  nuAcs, nuAcs,
  nuAtn, nuAtn,
  nuExp,
  nuLge, nuLge,
  nuLg2,
  nuLgt,
  nuL1p, nuL1p,
  nuEm1,
  nuSqt,
  nuCbt,
  nuErf,
  nuNerf,
  nuCil,
  nuFlr,
  nuRup, nuRup,
  nuRdn, nuRdn, nuRdn,
  nuAbs, nuAbs,
  nuSgn, nuSgn,
  nuExs,
  nuRnd,
  nuNrn, nuNrn, nuNrn,
  nuoIf,
  nuZer, nuZer,
  nuOne, nuOne,
  nrrdUnaryOpTauOfSigma, nrrdUnaryOpTauOfSigma,
  nrrdUnaryOpSigmaOfTau, nrrdUnaryOpSigmaOfTau
};

static const airEnum
unaryOp_ae = {
  "unary op",
  NRRD_UNARY_OP_MAX,
  unaryOpStr, NULL,
  unaryOpDesc,
  unaryOpStrEqv, unaryOpValEqv,
  AIR_FALSE
};
const airEnum *const nrrdUnaryOp = &unaryOp_ae;

/* ------------------------ nrrdBinaryOp ---------------------- */

static const char *
binaryOpStr[NRRD_BINARY_OP_MAX+1] = {
  "(unknown_binary_op)",
  "+",
  "-",
  "x",
  "/",
  "^",
  "spow",
  "fpow",
  "%",
  "fmod",
  "atan2",
  "min",
  "max",
  "lt",
  "lte",
  "gt",
  "gte",
  "comp",
  "eq",
  "neq",
  "exists",
  "if",
  "nrand",
  "rrand",
  "+c",
  "-c",
  "xc",
  "dulp",
};

static const char *
binaryOpDesc[NRRD_BINARY_OP_MAX+1] = {
  "unknown binary op",
  "add",
  "subtract",
  "multiply",
  "divide",
  "power",
  "signed power",
  "one minus power of one minus",
  "integer modulo",
  "fractional modulo",
  "two-argment arctangent based on atan2()",
  "miniumum",
  "maximum",
  "less then",
  "less then or equal",
  "greater than",
  "greater than or equal",
  "compare (resulting in -1, 0, or 1)",
  "equal",
  "not equal",
  "if exists(a), then a, else b",
  "if a, then a, else b",
  "a + b*gaussianNoise",
  "sample of Rician with mu a and sigma b",
  "add, but clamp to integer representation range",
  "subtract, but clamp to integer representation range",
  "multiply, but clamp to integer representation range",
  "32-bit FP distance in ULPs",
};

#define nbAdd nrrdBinaryOpAdd
#define nbSub nrrdBinaryOpSubtract
#define nbMul nrrdBinaryOpMultiply
#define nbDiv nrrdBinaryOpDivide
#define nbPow nrrdBinaryOpPow
#define nbSpw nrrdBinaryOpSgnPow
#define nbFpw nrrdBinaryOpFlippedSgnPow
#define nbMod nrrdBinaryOpMod
#define nbFmd nrrdBinaryOpFmod
#define nbAtn nrrdBinaryOpAtan2
#define nbMin nrrdBinaryOpMin
#define nbMax nrrdBinaryOpMax
#define nbLt  nrrdBinaryOpLT
#define nbLte nrrdBinaryOpLTE
#define nbGt  nrrdBinaryOpGT
#define nbGte nrrdBinaryOpGTE
#define nbCmp nrrdBinaryOpCompare
#define nbEq  nrrdBinaryOpEqual
#define nbNeq nrrdBinaryOpNotEqual
#define nbExt nrrdBinaryOpExists
#define nbIf  nrrdBinaryOpIf
#define nbUlp nrrdBinaryOpULPDistance

static const char *
binaryOpStrEqv[] = {
  "+", "plus", "add",
  "-", "minus", "subtract", "sub",
  "x", "*", "times", "multiply", "product",
  "/", "divide", "quotient",
  "^", "pow", "power",
  "spow", "sgnpow", "sgnpower",
  "fpow",
  "%", "mod", "modulo",
  "fmod",
  "atan2",
  "min", "minimum",
  "max", "maximum",
  "lt", "<", "less", "lessthan",
  "lte", "<=", "lessthanorequal",
  "gt", ">", "greater", "greaterthan",
  "gte", ">=", "greaterthanorequal",
  "comp", "compare",
  "eq", "=", "==", "equal",
  "neq", "ne", "!=", "notequal",
  "exists",
  "if",
  "nrand",
  "rrand",
  "+c", "addclamp",
  "-c", "subtractclamp",
  "xc", "multiplyclamp",
  "-f", "-fp", "dulp", "ulpd", "ulpdistance",
  ""
};

static const int
binaryOpValEqv[] = {
  nbAdd, nbAdd, nbAdd,
  nbSub, nbSub, nbSub, nbSub,
  nbMul, nbMul, nbMul, nbMul, nbMul,
  nbDiv, nbDiv, nbDiv,
  nbPow, nbPow, nbPow,
  nbSpw, nbSpw, nbSpw,
  nbFpw,
  nbMod, nbMod, nbMod,
  nbFmd,
  nbAtn,
  nbMin, nbMin,
  nbMax, nbMax,
  nbLt, nbLt, nbLt, nbLt,
  nbLte, nbLte, nbLte,
  nbGt, nbGt, nbGt, nbGt,
  nbGte, nbGte, nbGte,
  nbCmp, nbCmp,
  nbEq, nbEq, nbEq, nbEq,
  nbNeq, nbNeq, nbNeq, nbNeq,
  nbExt,
  nbIf,
  nrrdBinaryOpNormalRandScaleAdd,
  nrrdBinaryOpRicianRand,
  nrrdBinaryOpAddClamp, nrrdBinaryOpAddClamp,
  nrrdBinaryOpSubtractClamp, nrrdBinaryOpSubtractClamp,
  nrrdBinaryOpMultiplyClamp, nrrdBinaryOpMultiplyClamp,
  nbUlp, nbUlp, nbUlp, nbUlp, nbUlp,
};

static const airEnum
binaryOp_ae = {
  "binary op",
  NRRD_BINARY_OP_MAX,
  binaryOpStr, NULL,
  binaryOpDesc,
  binaryOpStrEqv, binaryOpValEqv,
  AIR_FALSE
};
const airEnum *const nrrdBinaryOp = &binaryOp_ae;

/* ------------------------ nrrdTernaryOp ---------------------- */

static const char *
ternaryOpStr[NRRD_TERNARY_OP_MAX+1] = {
  "(unknown_ternary_op)",
  "add",
  "multiply",
  "min",
  "min_sm",
  "max",
  "max_sm",
  "lt_sm",
  "gt_sm",
  "clamp",
  "ifelse",
  "lerp",
  "exists",
  "in_op",
  "in_cl",
  "gauss",
  "rician"
};

static const char *
ternaryOpDesc[NRRD_TERNARY_OP_MAX+1] = {
  "unknown ternary op",
  "add three values",
  "multiply three values",
  "minimum of three values",
  "smooth minimum of 1st and 3rd value, starting at 2nd",
  "maximum of three values",
  "smooth maximum of 1st and 3rd value, starting at 2nd",
  "1st less than 3rd, smoothed by 2nd",
  "1st greater than 3rd, smoothed by 2nd",
  "clamp 2nd value to closed interval between 1st and 3rd",
  "if 1st value is non-zero, then 2nd value, else 3rd value",
  "linearly interpolate between 2nd value (1st = 0.0) and 3rd (1st = 1.0)",
  "if 1st value exists, the 2nd value, else the 3rd",
  "2nd value is inside OPEN interval range between 1st and 3rd",
  "2nd value is inside CLOSED interval range between 1st and 3rd",
  "evaluate (at 1st value) Gaussian with mean=2nd and stdv=3rd value",
  "evaluate (at 1st value) Rician with mean=2nd and stdv=3rd value"
};

#define ntAdd nrrdTernaryOpAdd
#define ntMul nrrdTernaryOpMultiply

static const char *
ternaryOpStrEqv[] = {
  "+", "plus", "add",
  "x", "*", "times", "multiply", "product",
  "min",
  "min_sm", "minsm",  /* HEY want to deprecate minsm */
  "max",
  "max_sm",
  "lt_sm",
  "gt_sm",
  "clamp",
  "ifelse", "if",
  "lerp",
  "exists",
  "in_op",
  "in_cl",
  "gauss",
  "rician",
  ""
};

static const int
ternaryOpValEqv[] = {
  ntAdd, ntAdd, ntAdd,
  ntMul, ntMul, ntMul, ntMul, ntMul,
  nrrdTernaryOpMin,
  nrrdTernaryOpMinSmooth, nrrdTernaryOpMinSmooth,
  nrrdTernaryOpMax,
  nrrdTernaryOpMaxSmooth,
  nrrdTernaryOpLTSmooth,
  nrrdTernaryOpGTSmooth,
  nrrdTernaryOpClamp,
  nrrdTernaryOpIfElse, nrrdTernaryOpIfElse,
  nrrdTernaryOpLerp,
  nrrdTernaryOpExists,
  nrrdTernaryOpInOpen,
  nrrdTernaryOpInClosed,
  nrrdTernaryOpGaussian,
  nrrdTernaryOpRician,
};

static const airEnum
ternaryOp_ae = {
  "ternary op",
  NRRD_TERNARY_OP_MAX,
  ternaryOpStr, NULL,
  ternaryOpDesc,
  ternaryOpStrEqv, ternaryOpValEqv,
  AIR_FALSE
};
const airEnum *const nrrdTernaryOp = &ternaryOp_ae;

/* ------------------------ nrrdFFTWPlanRigor ---------------------- */

static const char *
FFTWPlanRigorStr[NRRD_FFTW_PLAN_RIGOR_MAX+1] = {
  "(unknown_rigor)",
  "estimate",
  "measure",
  "patient",
  "exhaustive"
};

static const char *
FFTWPlanRigorDesc[NRRD_FFTW_PLAN_RIGOR_MAX+1] = {
  "unknown rigor",
  "no machine-specific measurements, just estimate a plan",
  "do several machine-specific measurements to determine plan",
  "performs more measurements to find \"more optimal\" plan",
  "considers even wider range of algorithms to find most optimal plan"
};

static const char *
FFTWPlanRigorStrEqv[] = {
  "e", "est", "estimate",
  "m", "meas", "measure",
  "p", "pat", "patient",
  "x", "ex", "exhaustive",
  ""
};

static const int
FFTWPlanRigorValEqv[] = {
  nrrdFFTWPlanRigorEstimate, nrrdFFTWPlanRigorEstimate, nrrdFFTWPlanRigorEstimate,
  nrrdFFTWPlanRigorMeasure, nrrdFFTWPlanRigorMeasure, nrrdFFTWPlanRigorMeasure,
  nrrdFFTWPlanRigorPatient, nrrdFFTWPlanRigorPatient, nrrdFFTWPlanRigorPatient,
  nrrdFFTWPlanRigorExhaustive, nrrdFFTWPlanRigorExhaustive, nrrdFFTWPlanRigorExhaustive
};

static const airEnum
FFTWPlanRigor_ae = {
  "fftw plan rigor",
  NRRD_FFTW_PLAN_RIGOR_MAX,
  FFTWPlanRigorStr, NULL,
  FFTWPlanRigorDesc,
  FFTWPlanRigorStrEqv, FFTWPlanRigorValEqv,
  AIR_FALSE
};
const airEnum *const nrrdFFTWPlanRigor = &FFTWPlanRigor_ae;

/* ---------------------- nrrdResampleNonExistent -------------------- */

static const char *
resampleNonExistentStr[NRRD_RESAMPLE_NON_EXISTENT_MAX+1] = {
  "(unknown_resample_non_existent)",
  "noop",
  "renormalize",
  "weight"
};

static const char *
resampleNonExistentDesc[NRRD_RESAMPLE_NON_EXISTENT_MAX+1] = {
  "unknown resample non-existent",
  "no-op; include non-existent values in convolution sum",
  "use only existent values in kernel support and renormalize weights",
  "use only existent values in kernel support and use weights as is"
};

static const char *
resampleNonExistentStrEqv[] = {
  "noop",
  "renorm", "renormalize",
  "wght", "weight",
  ""
};

static const int
resampleNonExistentValEqv[] = {
  nrrdResampleNonExistentNoop,
  nrrdResampleNonExistentRenormalize, nrrdResampleNonExistentRenormalize,
  nrrdResampleNonExistentWeight, nrrdResampleNonExistentWeight
};

static const airEnum
resampleNonExistent_ae = {
  "resample non-existent",
  NRRD_RESAMPLE_NON_EXISTENT_MAX,
  resampleNonExistentStr, NULL,
  resampleNonExistentDesc,
  resampleNonExistentStrEqv, resampleNonExistentValEqv,
  AIR_FALSE
};
const airEnum *const nrrdResampleNonExistent = &resampleNonExistent_ae;

/* ---------------------- nrrdMetaDataCanonicalVersion -------------------- */

static const char *
metaDataCanonicalVersionStr[NRRD_META_DATA_CANONICAL_VERSION_MAX+1] = {
  "(unknown_meta_data_canonical_version)",
  "alpha",
};

static const char *
metaDataCanonicalVersionDesc[NRRD_META_DATA_CANONICAL_VERSION_MAX+1] = {
  "unknown meta data canonical version",
  "initial version, used for Diderot until at least 2016",
};

static const airEnum
metaDataCanonicalVersion_ae = {
  "canonical meta-data version",
  NRRD_META_DATA_CANONICAL_VERSION_MAX,
  metaDataCanonicalVersionStr, NULL,
  metaDataCanonicalVersionDesc,
  NULL, NULL,
  AIR_FALSE
};
const airEnum *const nrrdMetaDataCanonicalVersion = &metaDataCanonicalVersion_ae;
/* clang-format on */
/* ---- END non-NrrdIO */
