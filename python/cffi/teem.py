#
# Teem: Tools to process and visualize scientific data and images
# Copyright (C) 2009--2025  University of Chicago
# Copyright (C) 2008, 2007, 2006, 2005  Gordon Kindlmann
# Copyright (C) 2004, 2003, 2002, 2001, 2000, 1999, 1998  University of Utah
#
# This library is free software; you can redistribute it and/or modify it under the terms
# of the GNU Lesser General Public License (LGPL) as published by the Free Software
# Foundation; either version 2.1 of the License, or (at your option) any later version.
# The terms of redistributing and/or modifying this software also include exceptions to
# the LGPL that facilitate static linking.
#
# This library is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, see <https://www.gnu.org/licenses/>.
"""
teem.py: A convenience wrapper around the _teem extension module, which in turn links into
the underlying libteem.{so,dylib} shared library. The main utility of teem.py is wrapping
calls into Teem functions that use biff, so that if the function has an error, the biff error
message is converted into a Python exception.  We also introduce the Tenum object for wrapping
an airEnum, and maybe eventually other ways of making pythonic interfaces to teem
functionality.  See teem/python/cffi/README.md.

teem/python/cffi/exult.py was likely used to both compile the _teem extension module (the
shared library), and to generate this wrapper, which is the result of simple text
transformations of the template wrapper in teem/python/cffi/lliibb.py
"""

# pylint can't see inside CFFI-generated modules
# pylint: disable=c-extension-no-member

import math as _math   # # likely used in _BIFF_DICT, below, for testing function return values
import sys as _sys
import argparse as _argparse

from typing import Any, Union, Iterator

# halt if python2; thanks to https://stackoverflow.com/a/65407535/1465384
_x, *_y = 1, 2  # NOTE: A SyntaxError means you need Python3, not Python2
del _x, _y


def string(bstr):
    """Convenience utility for going from C char* bytes to Python string:
    string(B) is just _teem.ffi.string(B).decode('ascii')"""
    return _teem.ffi.string(bstr).decode('ascii')


class Tenum:
    """A pythonic helper/wrapper around (CFFI pointers to) Teem's airEnums (which convert
    between integer enum values and C strings, with aliases and descriptions). The conversion
    here is between `int`s and Python `str` strings, with an API that mimics Python's `Enum`.
    The C airEnum underlying the Python Tenum `foo` is available as `foo.airEnum`.
    The nested Tenum.TenumValue class is for representing a single enum value in a way that
    can be either an `int` or a `str`, like a value in Python's `IntEnum`."""

    @staticmethod
    def _is_airenum(obj: Any) -> bool:
        try:
            ty = _teem.ffi.typeof(obj)
            # is obj a pointer to an airEnum struct?
            return ty.kind == 'pointer' and ty.item.cname == 'airEnum' and ty.item.kind == 'struct'
        except AttributeError:
            return False

    def __init__(self, aenm, sym_name: str):   # aenm: CData typehint?
        """Constructor takes a Teem airEnum pointer `aenm` (const airEnum *const)."""
        if not self._is_airenum(aenm):
            raise TypeError(f'passed argument {aenm} does not seem to be a Teem airEnum pointer')
        # pylint: disable-next=invalid-name
        self.airEnum = aenm   # Teem C programmers will expect this spelling
        self.cname = _teem.ffi.string(self.airEnum.name).decode('utf8')
        self.sym_name = sym_name  # the symbol name of this airEnum in the C library
        self.me = f'{self.sym_name} ("{self.cname}") enum'  # how I self-identify in error messages
        # following definition of airEnum struct in teem/air.h,
        # self._vals is initialized to all valid (known) enum values
        vlist = list(range(1, self.airEnum.M + 1))
        if self.airEnum.val:
            self._vals = [self.airEnum.val[i] for i in vlist]
        else:
            self._vals = vlist

    class TenumValue:
        """Represents one value in a Tenum, in a way that can be both an int and a string (like
        a value in Python's `IntEnum`), and that remembers which Tenum it is part of.
        For example:
           tv = teem.nrrdKind['2-vector']
           int(tv)
           str(tv)"""

        def __init__(self, tenum: 'Tenum', value: int) -> None:
            # initialize these fields to be ready answers; yes redundant but rapidly
            # accessible without trip over the CFFI bridge
            self.tenum = tenum
            self.value = value
            self.name = tenum.name(value)   # the canonical name for `value`
            self.description = tenum.description(value)

        @classmethod
        def from_value(cls, tenum: 'Tenum', value: int) -> 'Tenum.TenumValue':
            """construct TenumValue from tenum and integer value"""
            if not value in tenum:
                raise ValueError(
                    f'{value} not valid {tenum.me} value (expected one of {list(map(int, tenum))}'
                )
            return cls(tenum, value)

        def __int__(self) -> int:
            return self.value

        def __str__(self) -> str:
            return self.name

        def __repr__(self) -> str:
            return f'<TenumValue({self.tenum.cname}) value={self.value}:name="{str(self)}">'

        def __eq__(self, other: object) -> bool:
            if isinstance(other, Tenum.TenumValue):
                return self.tenum.airEnum is other.tenum.airEnum and self.value == other.value
            if isinstance(other, int):
                return self.value == other
            if isinstance(other, str):
                # `other` could be an alias string; map to int value to compare
                return self.value == self.tenum.value(other)
            # else
            return NotImplemented

        def __hash__(self) -> int:
            return hash((id(self.tenum), self.value))

    class _ArgparseAction(_argparse.Action):
        """Custom argparse action for parsing a TenumValue from the CLI.
        Example usage:
           enm = teem.nrrdKind
           parser = argparse.ArgumentParser(description="...")
           parser.add_argument(
              "--kind",
              action=enm.argparse_action(),
              help=f"Specify a {enm.cname} enum value",
           )
        """

        def __init__(self, option_strings, dest, tenum, **kwargs):
            super().__init__(option_strings, dest, **kwargs)
            self.tenum = tenum

        def __call__(self, parser, namespace, strings, option_string=None):
            try:
                # we first map from string to integer via .value()
                # and then convert to TenumValue via []
                # if any parsing fails we raise an excls
                excls = _argparse.ArgumentTypeError
                if isinstance(strings, list):
                    result = [
                        self.tenum[self.tenum.value(string, True, excls)] for string in strings
                    ]
                else:
                    result = self.tenum[self.tenum.value(strings, True, excls)]
                setattr(namespace, self.dest, result)
            except excls as e:
                opt = option_string or self.option_strings[0]
                parser.error(f'Invalid value for {opt}: {e}')

    def argparse_action(self):
        """Return a preconfigured argparse.Action for this enum."""
        return lambda **kwargs: self._ArgparseAction(tenum=self, **kwargs)

    def __iter__(self) -> Iterator:
        """Provides a way to iterate through the valid integer values of the enum"""
        for val in self._vals:
            yield self.TenumValue(self, val)

    def __contains__(self, item: Union[int, str, 'Tenum.TenumValue']) -> bool:
        """Answers whether given int is a valid value of enum, or whether given string
        is a valid string in enum, depending on incoming type.
        (wraps airEnumValCheck() and airEnumVal())"""
        if isinstance(item, int):
            return item in self._vals
        if isinstance(item, str):
            return self.unknown() != self.value(item)
        if isinstance(item, Tenum.TenumValue):
            return item.tenum is self and item.value in self._vals
        # else
        return False

    def __getitem__(self, ios: Union[int, str]) -> TenumValue:
        if ios not in self:
            raise ValueError(f'{ios} not in {self.me}')
        # else ios is valid, and alas we redo some branching on type
        if isinstance(ios, int):
            return self.TenumValue(self, ios)
        if isinstance(ios, str):
            return self.TenumValue(self, self.value(ios))
        # should be impossible to get here
        raise TypeError(f'expected int or str not {ios}:{type(ios)}')

    def name(self, value: int, picky=False, excls=ValueError) -> str:
        """Converts from integer enum value val to (Python) string identifier.
        If picky, then failure to parse string as enum member generates an exception,
        of class `excls` (defaults to ValueError) (wraps airEnumStr())"""
        assert isinstance(value, int), f'Need an int argument (not {type(value)})'
        if picky and not value in self:
            raise excls(f'{value} not a valid {self.me} value')
        # else whether or not `value` is in enum, use airEnumStr;
        # this may return the string for "unknown"
        return _teem.ffi.string(_teem.lib.airEnumStr(self.airEnum, value)).decode('utf8')

    def value(self, name: str, picky=False, excls=ValueError) -> int:
        """Converts from name `name` to integer value
        (wraps airEnumVal())"""
        assert isinstance(name, str), f'Need an string argument (not {type(name)})'
        ret = _teem.lib.airEnumVal(self.airEnum, name.encode('utf8'))
        if picky and ret == self.unknown():
            raise excls(f'"{name}" not parsable as {self.me} value')
        # else
        return ret

    def description(self, val: int) -> str:
        """Converts from integer value `val` to description string
        (wraps airEnumDesc())"""
        assert isinstance(val, int), f'Need an int argument (not {type(val)})'
        dsc = _teem.lib.airEnumDesc(self.airEnum, val)
        return _teem.ffi.string(dsc).decode('utf8')

    def unknown(self) -> int:
        """Returns value representing unknown
        (wraps airEnumUnknown())"""
        return _teem.lib.airEnumUnknown(self.airEnum)


# class TenumValParseAction(_argparse.Action):
#    """Custom argparse action for parsing a TenumVal from the command-line"""
#
#    def __init__(self, option_strings, dest, tenum, **kwargs):
#        super().__init__(option_strings, dest, **kwargs)
#        # Store the enum we want to parse
#        self.tenum = tenum
#
#    def __call__(self, _parser, namespace, string, _option_string=None):
#        # parse value from string
#        val = self.tenum.val(string, True, _argparse.ArgumentTypeError)
#        tval = TenumVal(self.tenum, val)
#        # Set the parsed result in the namespace
#        setattr(namespace, self.dest, tval)


def _equals_one(val):   # likely used in _BIFF_DICT, below
    """Returns True iff given val equals 1"""
    return val == 1


def _equals_null(val):   # likely used in _BIFF_DICT, below
    """Returns True iff given val equals NULL"""
    return val == _teem.ffi.NULL


_BIFF_DICT = {  # contents here are filled in by teem/python/cffi/exult.py Tffi.wrap()
    'nrrdArrayCompare': (_equals_one, 0, b'nrrd', 'nrrd/accessors.c:556'),
    'nrrdApply1DLut': (_equals_one, 0, b'nrrd', 'nrrd/apply1D.c:435'),
    'nrrdApplyMulti1DLut': (_equals_one, 0, b'nrrd', 'nrrd/apply1D.c:466'),
    'nrrdApply1DRegMap': (_equals_one, 0, b'nrrd', 'nrrd/apply1D.c:514'),
    'nrrdApplyMulti1DRegMap': (_equals_one, 0, b'nrrd', 'nrrd/apply1D.c:545'),
    'nrrd1DIrregMapCheck': (_equals_one, 0, b'nrrd', 'nrrd/apply1D.c:585'),
    'nrrd1DIrregAclCheck': (_equals_one, 0, b'nrrd', 'nrrd/apply1D.c:682'),
    'nrrd1DIrregAclGenerate': (_equals_one, 0, b'nrrd', 'nrrd/apply1D.c:809'),
    'nrrdApply1DIrregMap': (_equals_one, 0, b'nrrd', 'nrrd/apply1D.c:872'),
    'nrrdApply1DSubstitution': (_equals_one, 0, b'nrrd', 'nrrd/apply1D.c:1042'),
    'nrrdApply2DLut': (_equals_one, 0, b'nrrd', 'nrrd/apply2D.c:294'),
    'nrrdArithGamma': (_equals_one, 0, b'nrrd', 'nrrd/arith.c:50'),
    'nrrdArithSRGBGamma': (_equals_one, 0, b'nrrd', 'nrrd/arith.c:138'),
    'nrrdArithUnaryOp': (_equals_one, 0, b'nrrd', 'nrrd/arith.c:344'),
    'nrrdArithBinaryOp': (_equals_one, 0, b'nrrd', 'nrrd/arith.c:553'),
    'nrrdArithIterBinaryOpSelect': (_equals_one, 0, b'nrrd', 'nrrd/arith.c:639'),
    'nrrdArithIterBinaryOp': (_equals_one, 0, b'nrrd', 'nrrd/arith.c:726'),
    'nrrdArithTernaryOp': (_equals_one, 0, b'nrrd', 'nrrd/arith.c:876'),
    'nrrdArithIterTernaryOpSelect': (_equals_one, 0, b'nrrd', 'nrrd/arith.c:954'),
    'nrrdArithIterTernaryOp': (_equals_one, 0, b'nrrd', 'nrrd/arith.c:1042'),
    'nrrdArithAffine': (_equals_one, 0, b'nrrd', 'nrrd/arith.c:1065'),
    'nrrdArithIterAffine': (_equals_one, 0, b'nrrd', 'nrrd/arith.c:1108'),
    'nrrdAxisInfoCompare': (_equals_one, 0, b'nrrd', 'nrrd/axis.c:935'),
    'nrrdOrientationReduce': (_equals_one, 0, b'nrrd', 'nrrd/axis.c:1223'),
    'nrrdMetaDataNormalize': (_equals_one, 0, b'nrrd', 'nrrd/axis.c:1268'),
    'nrrdCCFind': (_equals_one, 0, b'nrrd', 'nrrd/cc.c:306'),
    'nrrdCCAdjacency': (_equals_one, 0, b'nrrd', 'nrrd/cc.c:577'),
    'nrrdCCMerge': (_equals_one, 0, b'nrrd', 'nrrd/cc.c:675'),
    'nrrdCCRevalue': (_equals_one, 0, b'nrrd', 'nrrd/cc.c:825'),
    'nrrdCCSettle': (_equals_one, 0, b'nrrd', 'nrrd/cc.c:852'),
    'nrrdCCValid': ((lambda rv: 0 == rv), 0, b'nrrd', 'nrrd/ccmethods.c:24'),
    'nrrdCCSize': (_equals_one, 0, b'nrrd', 'nrrd/ccmethods.c:55'),
    'nrrdDeringVerboseSet': (_equals_one, 0, b'nrrd', 'nrrd/deringNrrd.c:100'),
    'nrrdDeringLinearInterpSet': (_equals_one, 0, b'nrrd', 'nrrd/deringNrrd.c:113'),
    'nrrdDeringVerticalSeamSet': (_equals_one, 0, b'nrrd', 'nrrd/deringNrrd.c:126'),
    'nrrdDeringInputSet': (_equals_one, 0, b'nrrd', 'nrrd/deringNrrd.c:139'),
    'nrrdDeringCenterSet': (_equals_one, 0, b'nrrd', 'nrrd/deringNrrd.c:174'),
    'nrrdDeringClampPercSet': (_equals_one, 0, b'nrrd', 'nrrd/deringNrrd.c:193'),
    'nrrdDeringClampHistoBinsSet': (_equals_one, 0, b'nrrd', 'nrrd/deringNrrd.c:214'),
    'nrrdDeringRadiusScaleSet': (_equals_one, 0, b'nrrd', 'nrrd/deringNrrd.c:233'),
    'nrrdDeringThetaNumSet': (_equals_one, 0, b'nrrd', 'nrrd/deringNrrd.c:251'),
    'nrrdDeringRadialKernelSet': (_equals_one, 0, b'nrrd', 'nrrd/deringNrrd.c:269'),
    'nrrdDeringThetaKernelSet': (_equals_one, 0, b'nrrd', 'nrrd/deringNrrd.c:289'),
    'nrrdDeringExecute': (_equals_one, 0, b'nrrd', 'nrrd/deringNrrd.c:751'),
    'nrrdFFTWWisdomRead': (_equals_one, 0, b'nrrd', 'nrrd/fftNrrd.c:32'),
    'nrrdFFT': (_equals_one, 0, b'nrrd', 'nrrd/fftNrrd.c:89'),
    'nrrdFFTWWisdomWrite': (_equals_one, 0, b'nrrd', 'nrrd/fftNrrd.c:286'),
    'nrrdCheapMedian': (_equals_one, 0, b'nrrd', 'nrrd/filt.c:405'),
    'nrrdDistanceL2': (_equals_one, 0, b'nrrd', 'nrrd/filt.c:812'),
    'nrrdDistanceL2Biased': (_equals_one, 0, b'nrrd', 'nrrd/filt.c:824'),
    'nrrdDistanceL2Signed': (_equals_one, 0, b'nrrd', 'nrrd/filt.c:836'),
    'nrrdHisto': (_equals_one, 0, b'nrrd', 'nrrd/histogram.c:38'),
    'nrrdHistoCheck': (_equals_one, 0, b'nrrd', 'nrrd/histogram.c:158'),
    'nrrdHistoDraw': (_equals_one, 0, b'nrrd', 'nrrd/histogram.c:187'),
    'nrrdHistoAxis': (_equals_one, 0, b'nrrd', 'nrrd/histogram.c:323'),
    'nrrdHistoJoint': (_equals_one, 0, b'nrrd', 'nrrd/histogram.c:437'),
    'nrrdHistoThresholdOtsu': (_equals_one, 0, b'nrrd', 'nrrd/histogram.c:644'),
    'nrrdKernelParse': (_equals_one, 0, b'nrrd', 'nrrd/kernel.c:3035'),
    'nrrdKernelSpecParse': (_equals_one, 0, b'nrrd', 'nrrd/kernel.c:3214'),
    'nrrdKernelSpecSprint': (_equals_one, 0, b'nrrd', 'nrrd/kernel.c:3236'),
    'nrrdKernelSprint': (_equals_one, 0, b'nrrd', 'nrrd/kernel.c:3295'),
    'nrrdKernelCompare': (_equals_one, 0, b'nrrd', 'nrrd/kernel.c:3313'),
    'nrrdKernelSpecCompare': (_equals_one, 0, b'nrrd', 'nrrd/kernel.c:3362'),
    'nrrdKernelCheck': (_equals_one, 0, b'nrrd', 'nrrd/kernel.c:3438'),
    'nrrdConvert': (_equals_one, 0, b'nrrd', 'nrrd/map.c:123'),
    'nrrdClampConvert': (_equals_one, 0, b'nrrd', 'nrrd/map.c:143'),
    'nrrdCastClampRound': (_equals_one, 0, b'nrrd', 'nrrd/map.c:169'),
    'nrrdQuantize': (_equals_one, 0, b'nrrd', 'nrrd/map.c:191'),
    'nrrdUnquantize': (_equals_one, 0, b'nrrd', 'nrrd/map.c:363'),
    'nrrdHistoEq': (_equals_one, 0, b'nrrd', 'nrrd/map.c:500'),
    'nrrdProject': (_equals_one, 0, b'nrrd', 'nrrd/measure.c:1134'),
    'nrrdBoundarySpecCheck': (_equals_one, 0, b'nrrd', 'nrrd/methodsNrrd.c:92'),
    'nrrdBoundarySpecParse': (_equals_one, 0, b'nrrd', 'nrrd/methodsNrrd.c:116'),
    'nrrdBoundarySpecSprint': (_equals_one, 0, b'nrrd', 'nrrd/methodsNrrd.c:175'),
    'nrrdBoundarySpecCompare': (_equals_one, 0, b'nrrd', 'nrrd/methodsNrrd.c:198'),
    'nrrdBasicInfoCopy': (_equals_one, 0, b'nrrd', 'nrrd/methodsNrrd.c:548'),
    'nrrdWrap_nva': (_equals_one, 0, b'nrrd', 'nrrd/methodsNrrd.c:827'),
    'nrrdWrap_va': (_equals_one, 0, b'nrrd', 'nrrd/methodsNrrd.c:858'),
    'nrrdCopy': (_equals_one, 0, b'nrrd', 'nrrd/methodsNrrd.c:949'),
    'nrrdAlloc_nva': (_equals_one, 0, b'nrrd', 'nrrd/methodsNrrd.c:979'),
    'nrrdAlloc_va': (_equals_one, 0, b'nrrd', 'nrrd/methodsNrrd.c:1028'),
    'nrrdMaybeAlloc_nva': (_equals_one, 0, b'nrrd', 'nrrd/methodsNrrd.c:1149'),
    'nrrdMaybeAlloc_va': (_equals_one, 0, b'nrrd', 'nrrd/methodsNrrd.c:1166'),
    'nrrdCompare': (_equals_one, 0, b'nrrd', 'nrrd/methodsNrrd.c:1207'),
    'nrrdPPM': (_equals_one, 0, b'nrrd', 'nrrd/methodsNrrd.c:1397'),
    'nrrdPGM': (_equals_one, 0, b'nrrd', 'nrrd/methodsNrrd.c:1417'),
    'nrrdSpaceVectorParse': (_equals_one, 4, b'nrrd', 'nrrd/parseNrrd.c:519'),
    '_nrrdDataFNCheck': (_equals_one, 3, b'nrrd', 'nrrd/parseNrrd.c:1201'),
    'nrrdRangePercentileSet': (_equals_one, 0, b'nrrd', 'nrrd/range.c:108'),
    'nrrdRangePercentileFromStringSet': (_equals_one, 0, b'nrrd', 'nrrd/range.c:210'),
    'nrrdOneLine': (_equals_one, 0, b'nrrd', 'nrrd/read.c:89'),
    'nrrdLineSkip': (_equals_one, 0, b'nrrd', 'nrrd/read.c:262'),
    'nrrdByteSkip': (_equals_one, 0, b'nrrd', 'nrrd/read.c:359'),
    'nrrdRead': (_equals_one, 0, b'nrrd', 'nrrd/read.c:523'),
    'nrrdStringRead': (_equals_one, 0, b'nrrd', 'nrrd/read.c:543'),
    'nrrdLoad': ((lambda rv: 1 == rv or 2 == rv), 0, b'nrrd', 'nrrd/read.c:641'),
    'nrrdLoadMulti': (_equals_one, 0, b'nrrd', 'nrrd/read.c:708'),
    'nrrdInvertPerm': (_equals_one, 0, b'nrrd', 'nrrd/reorder.c:32'),
    'nrrdAxesInsert': (_equals_one, 0, b'nrrd', 'nrrd/reorder.c:84'),
    'nrrdAxesPermute': (_equals_one, 0, b'nrrd', 'nrrd/reorder.c:150'),
    'nrrdShuffle': (_equals_one, 0, b'nrrd', 'nrrd/reorder.c:306'),
    'nrrdAxesSwap': (_equals_one, 0, b'nrrd', 'nrrd/reorder.c:452'),
    'nrrdFlip': (_equals_one, 0, b'nrrd', 'nrrd/reorder.c:488'),
    'nrrdJoin': (_equals_one, 0, b'nrrd', 'nrrd/reorder.c:569'),
    'nrrdAxesSplit': (_equals_one, 0, b'nrrd', 'nrrd/reorder.c:816'),
    'nrrdAxesDelete': (_equals_one, 0, b'nrrd', 'nrrd/reorder.c:878'),
    'nrrdAxesMerge': (_equals_one, 0, b'nrrd', 'nrrd/reorder.c:930'),
    'nrrdReshape_nva': (_equals_one, 0, b'nrrd', 'nrrd/reorder.c:980'),
    'nrrdReshape_va': (_equals_one, 0, b'nrrd', 'nrrd/reorder.c:1050'),
    'nrrdBlock': (_equals_one, 0, b'nrrd', 'nrrd/reorder.c:1087'),
    'nrrdUnblock': (_equals_one, 0, b'nrrd', 'nrrd/reorder.c:1158'),
    'nrrdTile2D': (_equals_one, 0, b'nrrd', 'nrrd/reorder.c:1257'),
    'nrrdUntile2D': (_equals_one, 0, b'nrrd', 'nrrd/reorder.c:1371'),
    'nrrdResampleDefaultCenterSet': (_equals_one, 0, b'nrrd', 'nrrd/resampleContext.c:169'),
    'nrrdResampleNonExistentSet': (_equals_one, 0, b'nrrd', 'nrrd/resampleContext.c:190'),
    'nrrdResampleRangeSet': (_equals_one, 0, b'nrrd', 'nrrd/resampleContext.c:311'),
    'nrrdResampleOverrideCenterSet': (_equals_one, 0, b'nrrd', 'nrrd/resampleContext.c:330'),
    'nrrdResampleBoundarySet': (_equals_one, 0, b'nrrd', 'nrrd/resampleContext.c:387'),
    'nrrdResamplePadValueSet': (_equals_one, 0, b'nrrd', 'nrrd/resampleContext.c:408'),
    'nrrdResampleBoundarySpecSet': (_equals_one, 0, b'nrrd', 'nrrd/resampleContext.c:425'),
    'nrrdResampleRenormalizeSet': (_equals_one, 0, b'nrrd', 'nrrd/resampleContext.c:446'),
    'nrrdResampleTypeOutSet': (_equals_one, 0, b'nrrd', 'nrrd/resampleContext.c:463'),
    'nrrdResampleRoundSet': (_equals_one, 0, b'nrrd', 'nrrd/resampleContext.c:488'),
    'nrrdResampleClampSet': (_equals_one, 0, b'nrrd', 'nrrd/resampleContext.c:505'),
    'nrrdResampleExecute': (_equals_one, 0, b'nrrd', 'nrrd/resampleContext.c:1440'),
    'nrrdSimpleResample': (_equals_one, 0, b'nrrd', 'nrrd/resampleNrrd.c:49'),
    'nrrdSpatialResample': (_equals_one, 0, b'nrrd', 'nrrd/resampleNrrd.c:518'),
    'nrrdSpaceSet': (_equals_one, 0, b'nrrd', 'nrrd/simple.c:81'),
    'nrrdSpaceDimensionSet': (_equals_one, 0, b'nrrd', 'nrrd/simple.c:118'),
    'nrrdSpaceOriginSet': (_equals_one, 0, b'nrrd', 'nrrd/simple.c:170'),
    'nrrdContentSet_va': (_equals_one, 0, b'nrrd', 'nrrd/simple.c:472'),
    '_nrrdCheck': (_equals_one, 3, b'nrrd', 'nrrd/simple.c:1296'),
    'nrrdCheck': (_equals_one, 0, b'nrrd', 'nrrd/simple.c:1333'),
    'nrrdSameSize': ((lambda rv: 0 == rv), 3, b'nrrd', 'nrrd/simple.c:1354'),
    'nrrdSanity': ((lambda rv: 0 == rv), 0, b'nrrd', 'nrrd/simple.c:1541'),
    'nrrdSlice': (_equals_one, 0, b'nrrd', 'nrrd/subset.c:37'),
    'nrrdCrop': (_equals_one, 0, b'nrrd', 'nrrd/subset.c:182'),
    'nrrdSliceSelect': (_equals_one, 0, b'nrrd', 'nrrd/subset.c:366'),
    'nrrdSample_nva': (_equals_one, 0, b'nrrd', 'nrrd/subset.c:578'),
    'nrrdSample_va': (_equals_one, 0, b'nrrd', 'nrrd/subset.c:617'),
    'nrrdSimpleCrop': (_equals_one, 0, b'nrrd', 'nrrd/subset.c:646'),
    'nrrdCropAuto': (_equals_one, 0, b'nrrd', 'nrrd/subset.c:667'),
    'nrrdSplice': (_equals_one, 0, b'nrrd', 'nrrd/superset.c:30'),
    'nrrdInset': (_equals_one, 0, b'nrrd', 'nrrd/superset.c:155'),
    'nrrdPad_va': (_equals_one, 0, b'nrrd', 'nrrd/superset.c:270'),
    'nrrdPad_nva': (_equals_one, 0, b'nrrd', 'nrrd/superset.c:471'),
    'nrrdSimplePad_va': (_equals_one, 0, b'nrrd', 'nrrd/superset.c:499'),
    'nrrdSimplePad_nva': (_equals_one, 0, b'nrrd', 'nrrd/superset.c:537'),
    'nrrdIoStateSet': (_equals_one, 0, b'nrrd', 'nrrd/write.c:24'),
    'nrrdIoStateEncodingSet': (_equals_one, 0, b'nrrd', 'nrrd/write.c:97'),
    'nrrdIoStateFormatSet': (_equals_one, 0, b'nrrd', 'nrrd/write.c:117'),
    'nrrdWrite': (_equals_one, 0, b'nrrd', 'nrrd/write.c:994'),
    'nrrdStringWrite': (_equals_one, 0, b'nrrd', 'nrrd/write.c:1010'),
    'nrrdSave': (_equals_one, 0, b'nrrd', 'nrrd/write.c:1031'),
    'nrrdSaveMulti': (_equals_one, 0, b'nrrd', 'nrrd/write.c:1097'),
    'ell_Nm_check': (_equals_one, 0, b'ell', 'ell/genmat.c:23'),
    'ell_Nm_tran': (_equals_one, 0, b'ell', 'ell/genmat.c:57'),
    'ell_Nm_mul': (_equals_one, 0, b'ell', 'ell/genmat.c:102'),
    'ell_Nm_inv': (_equals_one, 0, b'ell', 'ell/genmat.c:336'),
    'ell_Nm_pseudo_inv': (_equals_one, 0, b'ell', 'ell/genmat.c:377'),
    'ell_Nm_wght_pseudo_inv': (_equals_one, 0, b'ell', 'ell/genmat.c:411'),
    'ell_q_avg4_d': (_equals_one, 0, b'ell', 'ell/quat.c:469'),
    'ell_q_avgN_d': (_equals_one, 0, b'ell', 'ell/quat.c:537'),
    'mossImageCheck': (_equals_one, 0, b'moss', 'moss/methodsMoss.c:73'),
    'mossImageAlloc': (_equals_one, 0, b'moss', 'moss/methodsMoss.c:94'),
    'mossSamplerImageSet': (_equals_one, 0, b'moss', 'moss/sampler.c:24'),
    'mossSamplerKernelSet': (_equals_one, 0, b'moss', 'moss/sampler.c:76'),
    'mossSamplerUpdate': (_equals_one, 0, b'moss', 'moss/sampler.c:98'),
    'mossSamplerSample': (_equals_one, 0, b'moss', 'moss/sampler.c:193'),
    'mossLinearTransform': (_equals_one, 0, b'moss', 'moss/xform.c:138'),
    'mossFourPointTransform': (_equals_one, 0, b'moss', 'moss/xform.c:217'),
    'alanUpdate': (_equals_one, 0, b'alan', 'alan/coreAlan.c:58'),
    'alanInit': (_equals_one, 0, b'alan', 'alan/coreAlan.c:97'),
    'alanRun': (_equals_one, 0, b'alan', 'alan/coreAlan.c:451'),
    'alanDimensionSet': (_equals_one, 0, b'alan', 'alan/methodsAlan.c:103'),
    'alan2DSizeSet': (_equals_one, 0, b'alan', 'alan/methodsAlan.c:118'),
    'alan3DSizeSet': (_equals_one, 0, b'alan', 'alan/methodsAlan.c:138'),
    'alanTensorSet': (_equals_one, 0, b'alan', 'alan/methodsAlan.c:160'),
    'alanParmSet': (_equals_one, 0, b'alan', 'alan/methodsAlan.c:207'),
    'gageContextCopy': (_equals_null, 0, b'gage', 'gage/ctx.c:86'),
    'gageKernelSet': (_equals_one, 0, b'gage', 'gage/ctx.c:198'),
    'gagePerVolumeAttach': (_equals_one, 0, b'gage', 'gage/ctx.c:393'),
    'gagePerVolumeDetach': (_equals_one, 0, b'gage', 'gage/ctx.c:452'),
    'gageDeconvolve': (_equals_one, 0, b'gage', 'gage/deconvolve.c:24'),
    'gageDeconvolveSeparable': (_equals_one, 0, b'gage', 'gage/deconvolve.c:206'),
    'gageKindCheck': (_equals_one, 0, b'gage', 'gage/kind.c:31'),
    'gageKindVolumeCheck': (_equals_one, 0, b'gage', 'gage/kind.c:216'),
    'gageOptimSigSet': (_equals_one, 0, b'gage', 'gage/optimsig.c:215'),
    'gageOptimSigContextNew': (_equals_null, 0, b'gage', 'gage/optimsig.c:309'),
    'gageOptimSigCalculate': (_equals_one, 0, b'gage', 'gage/optimsig.c:1089'),
    'gageOptimSigErrorPlot': (_equals_one, 0, b'gage', 'gage/optimsig.c:1161'),
    'gageOptimSigErrorPlotSliding': (_equals_one, 0, b'gage', 'gage/optimsig.c:1252'),
    'gageVolumeCheck': (_equals_one, 0, b'gage', 'gage/pvl.c:34'),
    'gagePerVolumeNew': (_equals_null, 0, b'gage', 'gage/pvl.c:55'),
    'gageQueryReset': (_equals_one, 0, b'gage', 'gage/pvl.c:259'),
    'gageQuerySet': (_equals_one, 0, b'gage', 'gage/pvl.c:285'),
    'gageQueryAdd': (_equals_one, 0, b'gage', 'gage/pvl.c:341'),
    'gageQueryItemOn': (_equals_one, 0, b'gage', 'gage/pvl.c:359'),
    'gageShapeSet': (_equals_one, 0, b'gage', 'gage/shape.c:402'),
    'gageShapeEqual': ((lambda rv: 0 == rv), 0, b'gage', 'gage/shape.c:465'),
    'gageStructureTensor': (_equals_one, 0, b'gage', 'gage/st.c:81'),
    'gageStackPerVolumeNew': (_equals_one, 0, b'gage', 'gage/stack.c:96'),
    'gageStackPerVolumeAttach': (_equals_one, 0, b'gage', 'gage/stack.c:125'),
    'gageStackBlurParmCompare': (_equals_one, 0, b'gage', 'gage/stackBlur.c:124'),
    'gageStackBlurParmCopy': (_equals_one, 0, b'gage', 'gage/stackBlur.c:228'),
    'gageStackBlurParmSigmaSet': (_equals_one, 0, b'gage', 'gage/stackBlur.c:265'),
    'gageStackBlurParmScaleSet': (_equals_one, 0, b'gage', 'gage/stackBlur.c:359'),
    'gageStackBlurParmKernelSet': (_equals_one, 0, b'gage', 'gage/stackBlur.c:383'),
    'gageStackBlurParmRenormalizeSet': (_equals_one, 0, b'gage', 'gage/stackBlur.c:396'),
    'gageStackBlurParmBoundarySet': (_equals_one, 0, b'gage', 'gage/stackBlur.c:408'),
    'gageStackBlurParmBoundarySpecSet': (_equals_one, 0, b'gage', 'gage/stackBlur.c:427'),
    'gageStackBlurParmOneDimSet': (_equals_one, 0, b'gage', 'gage/stackBlur.c:444'),
    'gageStackBlurParmNeedSpatialBlurSet': (_equals_one, 0, b'gage', 'gage/stackBlur.c:456'),
    'gageStackBlurParmVerboseSet': (_equals_one, 0, b'gage', 'gage/stackBlur.c:468'),
    'gageStackBlurParmDgGoodSigmaMaxSet': (_equals_one, 0, b'gage', 'gage/stackBlur.c:480'),
    'gageStackBlurParmCheck': (_equals_one, 0, b'gage', 'gage/stackBlur.c:496'),
    'gageStackBlurParmParse': (_equals_one, 0, b'gage', 'gage/stackBlur.c:543'),
    'gageStackBlurParmSprint': (_equals_one, 0, b'gage', 'gage/stackBlur.c:808'),
    'gageStackBlur': (_equals_one, 0, b'gage', 'gage/stackBlur.c:1388'),
    'gageStackBlurCheck': (_equals_one, 0, b'gage', 'gage/stackBlur.c:1491'),
    'gageStackBlurGet': (_equals_one, 0, b'gage', 'gage/stackBlur.c:1599'),
    'gageStackBlurManage': (_equals_one, 0, b'gage', 'gage/stackBlur.c:1701'),
    'gageUpdate': (_equals_one, 0, b'gage', 'gage/update.c:311'),
    'dyeConvert': (_equals_one, 0, b'dye', 'dye/convertDye.c:349'),
    'dyeColorParse': (_equals_one, 0, b'dye', 'dye/methodsDye.c:186'),
    'baneClipNew': (_equals_null, 0, b'bane', 'bane/clip.c:100'),
    'baneClipAnswer': (_equals_one, 0, b'bane', 'bane/clip.c:150'),
    'baneClipCopy': (_equals_null, 0, b'bane', 'bane/clip.c:165'),
    'baneFindInclusion': (_equals_one, 0, b'bane', 'bane/hvol.c:85'),
    'baneMakeHVol': (_equals_one, 0, b'bane', 'bane/hvol.c:246'),
    'baneGKMSHVol': (_equals_null, 0, b'bane', 'bane/hvol.c:445'),
    'baneIncNew': (_equals_null, 0, b'bane', 'bane/inc.c:249'),
    'baneIncAnswer': (_equals_one, 0, b'bane', 'bane/inc.c:358'),
    'baneIncCopy': (_equals_null, 0, b'bane', 'bane/inc.c:373'),
    'baneMeasrNew': (_equals_null, 0, b'bane', 'bane/measr.c:31'),
    'baneMeasrCopy': (_equals_null, 0, b'bane', 'bane/measr.c:150'),
    'baneRangeNew': (_equals_null, 0, b'bane', 'bane/rangeBane.c:87'),
    'baneRangeCopy': (_equals_null, 0, b'bane', 'bane/rangeBane.c:128'),
    'baneRangeAnswer': (_equals_one, 0, b'bane', 'bane/rangeBane.c:142'),
    'baneRawScatterplots': (_equals_one, 0, b'bane', 'bane/scat.c:24'),
    'baneOpacInfo': (_equals_one, 0, b'bane', 'bane/trnsf.c:27'),
    'bane1DOpacInfoFrom2D': (_equals_one, 0, b'bane', 'bane/trnsf.c:142'),
    'baneSigmaCalc': (_equals_one, 0, b'bane', 'bane/trnsf.c:220'),
    'banePosCalc': (_equals_one, 0, b'bane', 'bane/trnsf.c:251'),
    'baneOpacCalc': (_equals_one, 0, b'bane', 'bane/trnsf.c:401'),
    'baneInputCheck': (_equals_one, 0, b'bane', 'bane/valid.c:24'),
    'baneHVolCheck': (_equals_one, 0, b'bane', 'bane/valid.c:62'),
    'baneInfoCheck': (_equals_one, 0, b'bane', 'bane/valid.c:104'),
    'banePosCheck': (_equals_one, 0, b'bane', 'bane/valid.c:142'),
    'baneBcptsCheck': (_equals_one, 0, b'bane', 'bane/valid.c:177'),
    'limnCameraUpdate': (_equals_one, 0, b'limn', 'limn/cam.c:32'),
    'limnCameraAspectSet': (_equals_one, 0, b'limn', 'limn/cam.c:129'),
    'limnCameraPathMake': (_equals_one, 0, b'limn', 'limn/cam.c:188'),
    'limnEnvMapFill': (_equals_one, 0, b'limn', 'limn/envmap.c:23'),
    'limnEnvMapCheck': (_equals_one, 0, b'limn', 'limn/envmap.c:117'),
    'limnObjectWriteOFF': (_equals_one, 0, b'limn', 'limn/io.c:78'),
    'limnPolyDataWriteIV': (_equals_one, 0, b'limn', 'limn/io.c:137'),
    'limnObjectReadOFF': (_equals_one, 0, b'limn', 'limn/io.c:263'),
    'limnPolyDataWriteLMPD': (_equals_one, 0, b'limn', 'limn/io.c:454'),
    'limnPolyDataReadLMPD': (_equals_one, 0, b'limn', 'limn/io.c:574'),
    'limnPolyDataWriteVTK': (_equals_one, 0, b'limn', 'limn/io.c:948'),
    'limnPolyDataReadOFF': (_equals_one, 0, b'limn', 'limn/io.c:1038'),
    'limnPolyDataSave': (_equals_one, 0, b'limn', 'limn/io.c:1143'),
    'limnLightUpdate': (_equals_one, 0, b'limn', 'limn/light.c:65'),
    'limnPolyDataAlloc': (_equals_one, 0, b'limn', 'limn/polydata.c:148'),
    'limnPolyDataCopy': (_equals_one, 0, b'limn', 'limn/polydata.c:227'),
    'limnPolyDataCopyN': (_equals_one, 0, b'limn', 'limn/polydata.c:259'),
    'limnPolyDataPrimitiveVertexNumber': (_equals_one, 0, b'limn', 'limn/polydata.c:550'),
    'limnPolyDataPrimitiveArea': (_equals_one, 0, b'limn', 'limn/polydata.c:572'),
    'limnPolyDataRasterize': (_equals_one, 0, b'limn', 'limn/polydata.c:630'),
    'limnPolyDataSpiralTubeWrap': (_equals_one, 0, b'limn', 'limn/polyfilter.c:24'),
    'limnPolyDataSmoothHC': ((lambda rv: -1 == rv), 0, b'limn', 'limn/polyfilter.c:334'),
    'limnPolyDataVertexWindingFix': (_equals_one, 0, b'limn', 'limn/polymod.c:1228'),
    'limnPolyDataCCFind': (_equals_one, 0, b'limn', 'limn/polymod.c:1247'),
    'limnPolyDataPrimitiveSort': (_equals_one, 0, b'limn', 'limn/polymod.c:1378'),
    'limnPolyDataVertexWindingFlip': (_equals_one, 0, b'limn', 'limn/polymod.c:1461'),
    'limnPolyDataPrimitiveSelect': (_equals_one, 0, b'limn', 'limn/polymod.c:1490'),
    'limnPolyDataClipMulti': (_equals_one, 0, b'limn', 'limn/polymod.c:1705'),
    'limnPolyDataCompress': (_equals_null, 0, b'limn', 'limn/polymod.c:1992'),
    'limnPolyDataJoin': (_equals_null, 0, b'limn', 'limn/polymod.c:2082'),
    'limnPolyDataEdgeHalve': (_equals_one, 0, b'limn', 'limn/polymod.c:2151'),
    'limnPolyDataNeighborList': ((lambda rv: -1 == rv), 0, b'limn', 'limn/polymod.c:2328'),
    'limnPolyDataNeighborArray': ((lambda rv: -1 == rv), 0, b'limn', 'limn/polymod.c:2424'),
    'limnPolyDataNeighborArrayComp': ((lambda rv: -1 == rv), 0, b'limn', 'limn/polymod.c:2464'),
    'limnPolyDataCube': (_equals_one, 0, b'limn', 'limn/polyshapes.c:25'),
    'limnPolyDataCubeTriangles': (_equals_one, 0, b'limn', 'limn/polyshapes.c:135'),
    'limnPolyDataOctahedron': (_equals_one, 0, b'limn', 'limn/polyshapes.c:345'),
    'limnPolyDataCylinder': (_equals_one, 0, b'limn', 'limn/polyshapes.c:459'),
    'limnPolyDataCone': (_equals_one, 0, b'limn', 'limn/polyshapes.c:633'),
    'limnPolyDataSuperquadric': (_equals_one, 0, b'limn', 'limn/polyshapes.c:732'),
    'limnPolyDataSpiralBetterquadric': (_equals_one, 0, b'limn', 'limn/polyshapes.c:857'),
    'limnPolyDataSpiralSuperquadric': (_equals_one, 0, b'limn', 'limn/polyshapes.c:1014'),
    'limnPolyDataPolarSphere': (_equals_one, 0, b'limn', 'limn/polyshapes.c:1032'),
    'limnPolyDataSpiralSphere': (_equals_one, 0, b'limn', 'limn/polyshapes.c:1044'),
    'limnPolyDataIcoSphere': (_equals_one, 0, b'limn', 'limn/polyshapes.c:1095'),
    'limnPolyDataPlane': (_equals_one, 0, b'limn', 'limn/polyshapes.c:1339'),
    'limnPolyDataSquare': (_equals_one, 0, b'limn', 'limn/polyshapes.c:1394'),
    'limnPolyDataSuperquadric2D': (_equals_one, 0, b'limn', 'limn/polyshapes.c:1437'),
    'limnQNDemo': (_equals_one, 0, b'limn', 'limn/qn.c:890'),
    'limnObjectRender': (_equals_one, 0, b'limn', 'limn/renderLimn.c:23'),
    'limnObjectPSDraw': (_equals_one, 0, b'limn', 'limn/renderLimn.c:182'),
    'limnObjectPSDrawConcave': (_equals_one, 0, b'limn', 'limn/renderLimn.c:312'),
    'limnSplineNrrdEvaluate': (_equals_one, 0, b'limn', 'limn/splineEval.c:321'),
    'limnSplineSample': (_equals_one, 0, b'limn', 'limn/splineEval.c:359'),
    'limnCbfPointsNew': (_equals_null, 0, b'limn', 'limn/splineFit.c:175'),
    'limnCbfPointsCheck': (_equals_one, 0, b'limn', 'limn/splineFit.c:248'),
    'limnCbfCtxPrep': (_equals_one, 0, b'limn', 'limn/splineFit.c:521'),
    'limnCbfTVT': (_equals_one, 0, b'limn', 'limn/splineFit.c:789'),
    'limnCbfSingle': (_equals_one, 0, b'limn', 'limn/splineFit.c:1529'),
    'limnCbfCorners': (_equals_one, 0, b'limn', 'limn/splineFit.c:1580'),
    'limnCbfMulti': (_equals_one, 0, b'limn', 'limn/splineFit.c:1782'),
    'limnCbfGo': (_equals_one, 0, b'limn', 'limn/splineFit.c:1900'),
    'limnSplineTypeSpecNew': (_equals_null, 0, b'limn', 'limn/splineMethods.c:23'),
    'limnSplineNew': (_equals_null, 0, b'limn', 'limn/splineMethods.c:123'),
    'limnSplineNrrdCleverFix': (_equals_one, 0, b'limn', 'limn/splineMethods.c:249'),
    'limnSplineCleverNew': (_equals_null, 0, b'limn', 'limn/splineMethods.c:394'),
    'limnSplineUpdate': (_equals_one, 0, b'limn', 'limn/splineMethods.c:422'),
    'limnSplineTypeSpecParse': (_equals_null, 0, b'limn', 'limn/splineMisc.c:221'),
    'limnSplineParse': (_equals_null, 0, b'limn', 'limn/splineMisc.c:277'),
    'limnObjectWorldHomog': (_equals_one, 0, b'limn', 'limn/transform.c:23'),
    'limnObjectFaceNormals': (_equals_one, 0, b'limn', 'limn/transform.c:45'),
    'limnObjectSpaceTransform': (_equals_one, 0, b'limn', 'limn/transform.c:208'),
    'limnObjectFaceReverse': (_equals_one, 0, b'limn', 'limn/transform.c:333'),
    'echoThreadStateInit': (_equals_one, 0, b'echo', 'echo/renderEcho.c:24'),
    'echoRTRenderCheck': (_equals_one, 0, b'echo', 'echo/renderEcho.c:132'),
    'echoRTRender': (_equals_one, 0, b'echo', 'echo/renderEcho.c:407'),
    'hooverContextCheck': (_equals_one, 0, b'hoover', 'hoover/methodsHoover.c:54'),
    'hooverRender': ((lambda rv: _teem.lib.hooverErrInit == rv), 0, b'hoover', 'hoover/rays.c:358'),
    'seekExtract': (_equals_one, 0, b'seek', 'seek/extract.c:934'),
    'seekDataSet': (_equals_one, 0, b'seek', 'seek/setSeek.c:54'),
    'seekSamplesSet': (_equals_one, 0, b'seek', 'seek/setSeek.c:114'),
    'seekTypeSet': (_equals_one, 0, b'seek', 'seek/setSeek.c:147'),
    'seekLowerInsideSet': (_equals_one, 0, b'seek', 'seek/setSeek.c:171'),
    'seekNormalsFindSet': (_equals_one, 0, b'seek', 'seek/setSeek.c:191'),
    'seekStrengthUseSet': (_equals_one, 0, b'seek', 'seek/setSeek.c:206'),
    'seekStrengthSet': (_equals_one, 0, b'seek', 'seek/setSeek.c:221'),
    'seekItemScalarSet': (_equals_one, 0, b'seek', 'seek/setSeek.c:283'),
    'seekItemStrengthSet': (_equals_one, 0, b'seek', 'seek/setSeek.c:302'),
    'seekItemHessSet': (_equals_one, 0, b'seek', 'seek/setSeek.c:321'),
    'seekItemGradientSet': (_equals_one, 0, b'seek', 'seek/setSeek.c:341'),
    'seekItemNormalSet': (_equals_one, 0, b'seek', 'seek/setSeek.c:362'),
    'seekItemEigensystemSet': (_equals_one, 0, b'seek', 'seek/setSeek.c:383'),
    'seekIsovalueSet': (_equals_one, 0, b'seek', 'seek/setSeek.c:412'),
    'seekEvalDiffThreshSet': (_equals_one, 0, b'seek', 'seek/setSeek.c:438'),
    'seekVertexStrength': (_equals_one, 0, b'seek', 'seek/textract.c:1882'),
    'seekUpdate': (_equals_one, 0, b'seek', 'seek/updateSeek.c:671'),
    'tenAnisoPlot': (_equals_one, 0, b'ten', 'ten/aniso.c:1066'),
    'tenAnisoVolume': (_equals_one, 0, b'ten', 'ten/aniso.c:1125'),
    'tenAnisoHistogram': (_equals_one, 0, b'ten', 'ten/aniso.c:1197'),
    'tenEvecRGBParmCheck': (_equals_one, 0, b'ten', 'ten/aniso.c:1312'),
    'tenEMBimodal': (_equals_one, 0, b'ten', 'ten/bimod.c:411'),
    'tenBVecNonLinearFit': (_equals_one, 0, b'ten', 'ten/bvec.c:97'),
    'tenDWMRIKeyValueParse': (_equals_one, 0, b'ten', 'ten/chan.c:58'),
    'tenBMatrixCalc': (_equals_one, 0, b'ten', 'ten/chan.c:346'),
    'tenEMatrixCalc': (_equals_one, 0, b'ten', 'ten/chan.c:387'),
    'tenEstimateLinear3D': (_equals_one, 0, b'ten', 'ten/chan.c:581'),
    'tenEstimateLinear4D': (_equals_one, 0, b'ten', 'ten/chan.c:628'),
    'tenSimulate': (_equals_one, 0, b'ten', 'ten/chan.c:870'),
    'tenEpiRegister3D': (_equals_one, 0, b'ten', 'ten/epireg.c:1042'),
    'tenEpiRegister4D': (_equals_one, 0, b'ten', 'ten/epireg.c:1193'),
    'tenEstimateMethodSet': (_equals_one, 0, b'ten', 'ten/estimate.c:282'),
    'tenEstimateSigmaSet': (_equals_one, 0, b'ten', 'ten/estimate.c:304'),
    'tenEstimateValueMinSet': (_equals_one, 0, b'ten', 'ten/estimate.c:322'),
    'tenEstimateGradientsSet': (_equals_one, 0, b'ten', 'ten/estimate.c:340'),
    'tenEstimateBMatricesSet': (_equals_one, 0, b'ten', 'ten/estimate.c:367'),
    'tenEstimateSkipSet': (_equals_one, 0, b'ten', 'ten/estimate.c:394'),
    'tenEstimateSkipReset': (_equals_one, 0, b'ten', 'ten/estimate.c:412'),
    'tenEstimateThresholdFind': (_equals_one, 0, b'ten', 'ten/estimate.c:427'),
    'tenEstimateThresholdSet': (_equals_one, 0, b'ten', 'ten/estimate.c:495'),
    'tenEstimateUpdate': (_equals_one, 0, b'ten', 'ten/estimate.c:801'),
    'tenEstimate1TensorSimulateSingle_f': (_equals_one, 0, b'ten', 'ten/estimate.c:975'),
    'tenEstimate1TensorSimulateSingle_d': (_equals_one, 0, b'ten', 'ten/estimate.c:1003'),
    'tenEstimate1TensorSimulateVolume': (_equals_one, 0, b'ten', 'ten/estimate.c:1034'),
    'tenEstimate1TensorSingle_f': (_equals_one, 0, b'ten', 'ten/estimate.c:1741'),
    'tenEstimate1TensorSingle_d': (_equals_one, 0, b'ten', 'ten/estimate.c:1769'),
    'tenEstimate1TensorVolume4D': (_equals_one, 0, b'ten', 'ten/estimate.c:1806'),
    'tenExperSpecGradSingleBValSet': (_equals_one, 0, b'ten', 'ten/experSpec.c:61'),
    'tenExperSpecGradBValSet': (_equals_one, 0, b'ten', 'ten/experSpec.c:102'),
    'tenExperSpecFromKeyValueSet': (_equals_one, 0, b'ten', 'ten/experSpec.c:171'),
    'tenDWMRIKeyValueFromExperSpecSet': (_equals_one, 0, b'ten', 'ten/experSpec.c:327'),
    'tenFiberTraceSet': (_equals_one, 0, b'ten', 'ten/fiber.c:826'),
    'tenFiberTrace': (_equals_one, 0, b'ten', 'ten/fiber.c:846'),
    'tenFiberDirectionNumber': ((lambda rv: 0 == rv), 0, b'ten', 'ten/fiber.c:866'),
    'tenFiberSingleTrace': (_equals_one, 0, b'ten', 'ten/fiber.c:915'),
    'tenFiberMultiNew': (_equals_null, 0, b'ten', 'ten/fiber.c:958'),
    'tenFiberMultiTrace': (_equals_one, 0, b'ten', 'ten/fiber.c:1024'),
    'tenFiberMultiPolyData': (_equals_one, 0, b'ten', 'ten/fiber.c:1244'),
    'tenFiberMultiProbeVals': (_equals_one, 0, b'ten', 'ten/fiber.c:1255'),
    'tenFiberContextDwiNew': (_equals_null, 0, b'ten', 'ten/fiberMethods.c:209'),
    'tenFiberContextNew': (_equals_null, 0, b'ten', 'ten/fiberMethods.c:223'),
    'tenFiberTypeSet': (_equals_one, 0, b'ten', 'ten/fiberMethods.c:247'),
    'tenFiberStopSet': (_equals_one, 0, b'ten', 'ten/fiberMethods.c:377'),
    'tenFiberStopAnisoSet': (_equals_one, 0, b'ten', 'ten/fiberMethods.c:553'),
    'tenFiberStopDoubleSet': (_equals_one, 0, b'ten', 'ten/fiberMethods.c:565'),
    'tenFiberStopUIntSet': (_equals_one, 0, b'ten', 'ten/fiberMethods.c:589'),
    'tenFiberAnisoSpeedSet': (_equals_one, 0, b'ten', 'ten/fiberMethods.c:636'),
    'tenFiberAnisoSpeedReset': (_equals_one, 0, b'ten', 'ten/fiberMethods.c:701'),
    'tenFiberKernelSet': (_equals_one, 0, b'ten', 'ten/fiberMethods.c:716'),
    'tenFiberProbeItemSet': (_equals_one, 0, b'ten', 'ten/fiberMethods.c:735'),
    'tenFiberIntgSet': (_equals_one, 0, b'ten', 'ten/fiberMethods.c:747'),
    'tenFiberUpdate': (_equals_one, 0, b'ten', 'ten/fiberMethods.c:790'),
    'tenGlyphParmCheck': (_equals_one, 0, b'ten', 'ten/glyph.c:71'),
    'tenGlyphGen': (_equals_one, 0, b'ten', 'ten/glyph.c:172'),
    'tenGradientCheck': (_equals_one, 0, b'ten', 'ten/grads.c:66'),
    'tenGradientRandom': (_equals_one, 0, b'ten', 'ten/grads.c:105'),
    'tenGradientJitter': (_equals_one, 0, b'ten', 'ten/grads.c:150'),
    'tenGradientBalance': (_equals_one, 0, b'ten', 'ten/grads.c:372'),
    'tenGradientDistribute': (_equals_one, 0, b'ten', 'ten/grads.c:457'),
    'tenGradientGenerate': (_equals_one, 0, b'ten', 'ten/grads.c:650'),
    'tenEvecRGB': (_equals_one, 0, b'ten', 'ten/miscTen.c:24'),
    'tenEvqVolume': (_equals_one, 0, b'ten', 'ten/miscTen.c:149'),
    'tenBMatrixCheck': (_equals_one, 0, b'ten', 'ten/miscTen.c:210'),
    '_tenFindValley': (_equals_one, 0, b'ten', 'ten/miscTen.c:254'),
    'tenSizeNormalize': (_equals_one, 0, b'ten', 'ten/mod.c:219'),
    'tenSizeScale': (_equals_one, 0, b'ten', 'ten/mod.c:235'),
    'tenAnisoScale': (_equals_one, 0, b'ten', 'ten/mod.c:253'),
    'tenEigenvalueClamp': (_equals_one, 0, b'ten', 'ten/mod.c:273'),
    'tenEigenvaluePower': (_equals_one, 0, b'ten', 'ten/mod.c:292'),
    'tenEigenvalueAdd': (_equals_one, 0, b'ten', 'ten/mod.c:310'),
    'tenEigenvalueMultiply': (_equals_one, 0, b'ten', 'ten/mod.c:328'),
    'tenLog': (_equals_one, 0, b'ten', 'ten/mod.c:346'),
    'tenExp': (_equals_one, 0, b'ten', 'ten/mod.c:363'),
    'tenInterpParmBufferAlloc': (_equals_one, 0, b'ten', 'ten/path.c:62'),
    'tenInterpParmCopy': (_equals_null, 0, b'ten', 'ten/path.c:121'),
    'tenInterpN_d': (_equals_one, 0, b'ten', 'ten/path.c:304'),
    'tenInterpTwoDiscrete_d': (_equals_one, 0, b'ten', 'ten/path.c:803'),
    'tenInterpMulti3D': (_equals_one, 0, b'ten', 'ten/path.c:953'),
    'tenDwiGageKindSet': (_equals_one, 0, b'ten', 'ten/tenDwiGage.c:1037'),
    'tenDwiGageKindCheck': (_equals_one, 0, b'ten', 'ten/tenDwiGage.c:1177'),
    'tenModelParse': (_equals_one, 0, b'ten', 'ten/tenModel.c:61'),
    'tenModelFromAxisLearn': (_equals_one, 0, b'ten', 'ten/tenModel.c:122'),
    'tenModelSimulate': (_equals_one, 0, b'ten', 'ten/tenModel.c:160'),
    'tenModelSqeFit': (_equals_one, 0, b'ten', 'ten/tenModel.c:408'),
    'tenModelConvert': (_equals_one, 0, b'ten', 'ten/tenModel.c:683'),
    'tenTensorCheck': (_equals_one, 4, b'ten', 'ten/tensor.c:52'),
    'tenMeasurementFrameReduce': (_equals_one, 0, b'ten', 'ten/tensor.c:85'),
    'tenExpand2D': (_equals_one, 0, b'ten', 'ten/tensor.c:155'),
    'tenExpand': (_equals_one, 0, b'ten', 'ten/tensor.c:229'),
    'tenShrink': (_equals_one, 0, b'ten', 'ten/tensor.c:285'),
    'tenMake': (_equals_one, 0, b'ten', 'ten/tensor.c:527'),
    'tenSlice': (_equals_one, 0, b'ten', 'ten/tensor.c:629'),
    'tenTripleCalc': (_equals_one, 0, b'ten', 'ten/triple.c:413'),
    'tenTripleConvert': (_equals_one, 0, b'ten', 'ten/triple.c:471'),
    'pullEnergyPlot': (_equals_one, 0, b'pull', 'pull/actionPull.c:230'),
    'pullBinProcess': (_equals_one, 0, b'pull', 'pull/actionPull.c:1104'),
    'pullGammaLearn': (_equals_one, 0, b'pull', 'pull/actionPull.c:1139'),
    'pullBinsPointAdd': (_equals_one, 0, b'pull', 'pull/binningPull.c:181'),
    'pullBinsPointMaybeAdd': (_equals_one, 0, b'pull', 'pull/binningPull.c:203'),
    'pullCCFind': (_equals_one, 0, b'pull', 'pull/ccPull.c:28'),
    'pullCCMeasure': (_equals_one, 0, b'pull', 'pull/ccPull.c:112'),
    'pullCCSort': (_equals_one, 0, b'pull', 'pull/ccPull.c:207'),
    'pullOutputGetFilter': (_equals_one, 0, b'pull', 'pull/contextPull.c:380'),
    'pullOutputGet': (_equals_one, 0, b'pull', 'pull/contextPull.c:576'),
    'pullPropGet': (_equals_one, 0, b'pull', 'pull/contextPull.c:589'),
    'pullPositionHistoryNrrdGet': (_equals_one, 0, b'pull', 'pull/contextPull.c:767'),
    'pullPositionHistoryPolydataGet': (_equals_one, 0, b'pull', 'pull/contextPull.c:839'),
    'pullStart': (_equals_one, 0, b'pull', 'pull/corePull.c:111'),
    'pullFinish': (_equals_one, 0, b'pull', 'pull/corePull.c:166'),
    'pullRun': (_equals_one, 0, b'pull', 'pull/corePull.c:333'),
    'pullEnergySpecParse': (_equals_one, 0, b'pull', 'pull/energy.c:626'),
    'pullInfoSpecAdd': (_equals_one, 0, b'pull', 'pull/infoPull.c:131'),
    'pullInfoGet': (_equals_one, 0, b'pull', 'pull/infoPull.c:403'),
    'pullInfoSpecSprint': (_equals_one, 0, b'pull', 'pull/infoPull.c:448'),
    'pullInitRandomSet': (_equals_one, 0, b'pull', 'pull/initPull.c:107'),
    'pullInitHaltonSet': (_equals_one, 0, b'pull', 'pull/initPull.c:125'),
    'pullInitPointPerVoxelSet': (_equals_one, 0, b'pull', 'pull/initPull.c:144'),
    'pullInitGivenPosSet': (_equals_one, 0, b'pull', 'pull/initPull.c:172'),
    'pullInitLiveThreshUseSet': (_equals_one, 0, b'pull', 'pull/initPull.c:186'),
    'pullInitUnequalShapesAllowSet': (_equals_one, 0, b'pull', 'pull/initPull.c:199'),
    'pullIterParmSet': (_equals_one, 0, b'pull', 'pull/parmPull.c:102'),
    'pullSysParmSet': (_equals_one, 0, b'pull', 'pull/parmPull.c:191'),
    'pullFlagSet': (_equals_one, 0, b'pull', 'pull/parmPull.c:270'),
    'pullVerboseSet': (_equals_one, 0, b'pull', 'pull/parmPull.c:345'),
    'pullThreadNumSet': (_equals_one, 0, b'pull', 'pull/parmPull.c:370'),
    'pullRngSeedSet': (_equals_one, 0, b'pull', 'pull/parmPull.c:382'),
    'pullProgressBinModSet': (_equals_one, 0, b'pull', 'pull/parmPull.c:394'),
    'pullCallbackSet': (_equals_one, 0, b'pull', 'pull/parmPull.c:406'),
    'pullInterEnergySet': (_equals_one, 0, b'pull', 'pull/parmPull.c:431'),
    'pullLogAddSet': (_equals_one, 0, b'pull', 'pull/parmPull.c:492'),
    'pullPointNew': (_equals_null, 0, b'pull', 'pull/pointPull.c:31'),
    'pullProbe': (_equals_one, 0, b'pull', 'pull/pointPull.c:357'),
    'pullPointInitializePerVoxel': (_equals_one, 0, b'pull', 'pull/pointPull.c:636'),
    'pullPointInitializeRandomOrHalton': (_equals_one, 0, b'pull', 'pull/pointPull.c:821'),
    'pullPointInitializeGivenPos': (_equals_one, 0, b'pull', 'pull/pointPull.c:992'),
    'pullTraceSet': (_equals_one, 0, b'pull', 'pull/trace.c:243'),
    'pullTraceMultiAdd': (_equals_one, 0, b'pull', 'pull/trace.c:672'),
    'pullTraceMultiPlotAdd': (_equals_one, 0, b'pull', 'pull/trace.c:702'),
    'pullTraceMultiWrite': (_equals_one, 0, b'pull', 'pull/trace.c:1013'),
    'pullTraceMultiRead': (_equals_one, 0, b'pull', 'pull/trace.c:1118'),
    'pullVolumeSingleAdd': (_equals_one, 0, b'pull', 'pull/volumePull.c:210'),
    'pullVolumeStackAdd': (_equals_one, 0, b'pull', 'pull/volumePull.c:236'),
    'pullVolumeLookup': (_equals_null, 0, b'pull', 'pull/volumePull.c:473'),
    'pullConstraintScaleRange': (_equals_one, 0, b'pull', 'pull/volumePull.c:492'),
    'coilStart': (_equals_one, 0, b'coil', 'coil/coreCoil.c:285'),
    'coilIterate': (_equals_one, 0, b'coil', 'coil/coreCoil.c:360'),
    'coilFinish': (_equals_one, 0, b'coil', 'coil/coreCoil.c:405'),
    'coilVolumeCheck': (_equals_one, 0, b'coil', 'coil/methodsCoil.c:25'),
    'coilContextAllSet': (_equals_one, 0, b'coil', 'coil/methodsCoil.c:69'),
    'coilOutputGet': (_equals_one, 0, b'coil', 'coil/methodsCoil.c:200'),
    'pushOutputGet': (_equals_one, 0, b'push', 'push/action.c:69'),
    'pushBinProcess': (_equals_one, 0, b'push', 'push/action.c:159'),
    'pushBinPointAdd': (_equals_one, 0, b'push', 'push/binning.c:178'),
    'pushRebin': (_equals_one, 0, b'push', 'push/binning.c:195'),
    'pushStart': (_equals_one, 0, b'push', 'push/corePush.c:181'),
    'pushIterate': (_equals_one, 0, b'push', 'push/corePush.c:231'),
    'pushRun': (_equals_one, 0, b'push', 'push/corePush.c:304'),
    'pushFinish': (_equals_one, 0, b'push', 'push/corePush.c:394'),
    'pushEnergySpecParse': (_equals_one, 0, b'push', 'push/forces.c:303'),
    'miteSample': (_math.isnan, 0, b'mite', 'mite/ray.c:150'),
    'miteRenderBegin': (_equals_one, 0, b'mite', 'mite/renderMite.c:61'),
    'miteShadeSpecParse': (_equals_one, 0, b'mite', 'mite/shade.c:68'),
    'miteThreadNew': (_equals_null, 0, b'mite', 'mite/thread.c:24'),
    'miteThreadBegin': (_equals_one, 0, b'mite', 'mite/thread.c:91'),
    'miteVariableParse': (_equals_one, 0, b'mite', 'mite/txf.c:99'),
    'miteNtxfCheck': (_equals_one, 0, b'mite', 'mite/txf.c:230'),
    'meetAirEnumAllCheck': (_equals_one, 0, b'meet', 'meet/enumall.c:216'),
    'meetNrrdKernelAllCheck': (_equals_one, 0, b'meet', 'meet/meetNrrd.c:235'),
    'meetPullVolCopy': (_equals_null, 0, b'meet', 'meet/meetPull.c:43'),
    'meetPullVolParse': (_equals_one, 0, b'meet', 'meet/meetPull.c:99'),
    'meetPullVolLeechable': (_equals_one, 0, b'meet', 'meet/meetPull.c:314'),
    'meetPullVolStackBlurParmFinishMulti': (_equals_one, 0, b'meet', 'meet/meetPull.c:428'),
    'meetPullVolLoadMulti': (_equals_one, 0, b'meet', 'meet/meetPull.c:473'),
    'meetPullVolAddMulti': (_equals_one, 0, b'meet', 'meet/meetPull.c:554'),
    'meetPullInfoParse': (_equals_one, 0, b'meet', 'meet/meetPull.c:637'),
    'meetPullInfoAddMulti': (_equals_one, 0, b'meet', 'meet/meetPull.c:767'),
}


def _biffer(func, func_name: str, blob):
    """
    generates function wrappers that turn C biff errors into Python exceptions
    """
    (
        rvtf,  # C-function return value test function
        mubi,  # Maybe useBiff index (1-based) into function arguments
        bkey,  # bytes for biff key to retrieve biff error
        fnln,  # filename and line number of C function
    ) = blob

    def wrapper(*args):
        """
        function wrapper that turns C biff errors into Python exceptions
        """
        ret_val = func(*args)
        # we have to get biff error if rvtf(ret_val) indicates error, and,
        # either: this function definitely uses biff (0 == mubi)
        #     or: (this function maybe uses biff and) "useBiff" args[mubi-1] is True
        if rvtf(ret_val) and (0 == mubi or args[mubi - 1]):
            err = _teem.lib.biffGetDone(bkey)
            estr = string(err).rstrip()
            _teem.lib.free(err)
            raise RuntimeError(
                f'return value {ret_val} from C function "{func_name}" ({fnln}):\n{estr}'
            )
        return ret_val

    wrapper.__name__ = func_name
    wrapper.__doc__ = f"""
error-checking wrapper around C function "{func_name}" ({fnln}):
{func.__doc__}
"""
    return wrapper


# NOTE: this is copy-pasta from GLK's SciVis class code, and the python wrappers there
class _teem_Module:
    """An object that exists just to "become" the imported module, an old hack[1,2]
    that is still needed because even though there is now module-level __getattr__[3],
    module-level __setattr__ has been sadly rejected[4,5]. Using __setattr__ here
    solves two problems: (1) supporting true read-only variables, which avoids easy but
    confusing mistakes (should NOT be able to over-write the C functions we need to use,
    and accurately reflecting C names (of enums, functions) matters more than hewing to
    the mere convention that ALL-CAPS Python variables are constants), and, (2) creating
    useful aliases for global variables so that they are actually set in the underlying
    C library instead of just modifying a Python-only variable.
    [1] https://groups.google.com/g/comp.lang.python/c/2H53WSCXhoM
    [2] https://mail.python.org/pipermail/python-ideas/2012-May/014969.html
    [3] https://peps.python.org/pep-0562/
    [4] https://discuss.python.org/t/extend-pep-562-with-setattr-for-modules/25506
    [5] https://discuss.python.org/t/pep-726-module-setattr-and-delattr/32640
    """

    # _done being False indicates to __setattr__ that __init__ is in progress
    _done = False

    def __init__(self):
        """Set up all the extension module wrapping"""
        # we init ourself with the globals() to best emulate being a module
        for kk, vv in globals().items():
            setattr(self, kk, vv)
        # set various things that simplify using CFFI and this module
        # for slight convenience, e.g. when calling nrrdLoad with NULL (default) NrrdIoState
        self.NULL = _teem.ffi.NULL
        # The value of this ffi, as opposed to "from cffi import FFI; ffi = FFI()" is that it knows
        # about the various typedefs that were learned to build the CFFI wrapper, which may in turn
        # be useful for setting up calls into libteem
        self.ffi = _teem.ffi
        # enable access to original un-wrapped things, straight from cffi
        self.lib = _teem.lib
        # for non-const things, self._alias maps from exported name to CFFI object
        # in the underlying library
        self._alias = {}
        # go through everything in underlying C library, and process accordingly
        for sym_name in dir(_teem.lib):
            if 'free' == sym_name:
                # don't export C runtime's free(), though we use it above in the biff wrapper
                continue
            # sym is the symbol with name sym_name
            # (not __lib_.lib[sym_name] since '_cffi_backend.Lib' object is not subscriptable)
            sym = getattr(_teem.lib, sym_name)
            # string useful for distinguishing different kinds of CFFI objects
            strsym = str(sym)
            # The exported symbol xprt will be ...
            if sym_name in _BIFF_DICT:
                # ... or: a Python wrapper around a function known to use biff.
                setattr(self, sym_name, _biffer(sym, sym_name, _BIFF_DICT[sym_name]))
            # else either a function known to not use biff, or not a function,
            elif strsym.startswith("<cdata 'airEnum *' "):
                # _sym is name of an airEnum, wrap it as such
                setattr(self, sym_name, Tenum(sym, sym_name))
            elif (
                # Annoyingly, functions in _teem.lib can either look like
                # <cdata 'int(*)(char *, ...)' 0x10af91330> or like
                # <built-in method _lib_Foo of _cffi_backend.Lib object at 0x10b0cd210>
                (strsym.startswith('<cdata') and '(*)(' in strsym)  # some functions
                or strsym.startswith('<built-in method')  # other functions
                # ridiculous list of wacky types of things in Teem
                or strsym.startswith("<cdata 'void(*[")
                or strsym.startswith("<cdata 'char *")
                or strsym.startswith("<cdata 'char[")
                or strsym.startswith("<cdata 'unsigned int *")
                or strsym.startswith("<cdata 'unsigned int(*")
                or strsym.startswith("<cdata 'unsigned int[")
                or strsym.startswith("<cdata 'int[")
                or strsym.startswith("<cdata 'int(*[")
                or strsym.startswith("<cdata 'float(*[")
                or strsym.startswith("<cdata 'double[")
                or strsym.startswith("<cdata 'double(*[")
                or strsym.startswith("<cdata 'size_t[")
                or strsym.startswith("<cdata 'size_t(*[")
                or strsym.startswith("<cdata 'struct ")
                or strsym.startswith("<cdata 'airFloat &'")
                or strsym.startswith("<cdata 'hestCB &'")
                or strsym.startswith("<cdata 'airRandMTState *'")
                or strsym.startswith("<cdata 'hestCB *'")
                or strsym.startswith("<cdata 'unrrduCmd * *'")
                or strsym.startswith("<cdata 'gageItemPack *'")
                or strsym.startswith("<cdata 'NrrdFormat *")
                or strsym.startswith("<cdata 'NrrdKernel *")
                or strsym.startswith("<cdata 'coilKind &'")
                or strsym.startswith("<cdata 'coilKind *")
                or strsym.startswith("<cdata 'coilMethod *")
                or strsym.startswith("<cdata 'pushEnergy *")
                or strsym.startswith("<cdata 'pullEnergy *")
            ):
                # with C strings, it might be cute to instead export a real Python string, but
                # then its value would NOT be useful as is for the underlying C library.
                setattr(self, sym_name, sym)
            else:
                # More special cases; see if sym is an integer constant: enum or #define
                cval = None  # value of symbol as integer const
                try:
                    cval = _teem.ffi.integer_const(sym_name)
                except _teem.ffi.error:
                    # sym_name wasn't actually an integer const; ignore the complaint.
                    pass
                if cval is sym:
                    # so sym_name *is* an integer const, export that (integer) value
                    setattr(self, sym_name, sym)
                elif isinstance(sym, (bytes, float, int)):
                    # sym_name is a NON-CONST scalar, do not export, instead alias it
                    self._alias[sym_name] = sym_name
                    # HEY in the python wrappers for GLK's SciVis, this aliasing is
                    # non-trivial e.g. `foo.Verbose` aliases _foo.lib.fooVerbose`.
                    # Here, `_alias` instead becomes a badly-named mechanism to indicate
                    # which exports might be mutable
                else:
                    raise ValueError(
                        f'Libary item {sym_name} is something ({strsym}) unexpected; sorry'
                    )
        # done looping through symbols
        # Fake out the name of this class to be name of wannabe module
        self.__class__.__name__ = __name__
        # Prevent further changes
        self._done = True

    def __setattr__(self, name, value):
        """Allow new attributes during __init__, then be read-only except for aliased variables"""
        if not self._done:
            # self.__init__ is still in progress; pass through
            self.__dict__[name] = value
            return
        # else __init__ is done; have turned mostly read-only
        if name in self._alias:
            # the aliases are the one thing we allow changing
            setattr(self.lib, self._alias[name], value)
            # and we're done
            return
        # else try to give informative exceptions to documenting being read-only
        if not name in self.__dict__:
            raise ValueError(
                f'"{name}" not already in {self.__name__} wrapper module '
                'and cannot add new elements.'
            )
        raise ValueError(
            f'Cannot change "{name}" in {self.__name__} wrapper module'
            # not saying this because somethings like _lib_.NULL or .NAN
            # are not actually in the underlying C library
            # 'corresponding element of underlying C library is const.'
        )

    def __getattr__(self, name):
        """Handle requests for attributes that don't really exist;
        currently just the aliased variables"""
        if name in self._alias:
            return getattr(self.lib, self._alias[name])
        # else not an alias
        raise KeyError(f'"{name}" not in {self.__name__} wrapper module')

    def __dir__(self):
        """Directory of self, hacked to include the aliased variables"""
        lst = list(self.__dict__.keys())
        for name in self._alias:
            lst.append(name)
        return lst

    # other utility functions that come in handy
    def str(self, cstr):
        """Utility function from C char* to Python string; nothing more than
        str(cstr) = _teem.ffi.string(cstr).decode('utf8')"""
        return _teem.ffi.string(cstr).decode('utf8')

    def cs(self, pstr: str):
        """Utility function from Python string to something compatible with C char*.
        Has such a short name ("cs" could stand for "C string" or "char star") to be
        shorter than its simple implementation: cs(pstr) = pstr.encode('utf8')"""
        return pstr.encode('utf8')


if 'teem' == __name__:  # being imported
    try:
        import _teem
    except ModuleNotFoundError:
        print('\n*** teem.py: failed to "import _teem", the _teem extension ')
        print('*** module stored in a file named something like: ')
        print('*** _teem.cpython-platform.so.')
        print('*** Is there a build_teem.py script you can run to recompile it?\n')
        raise
    # Finally, the object-instance-becomes-the-module fake-out workaround described in the
    # __lib_Module docstring above and the links therein.
    _sys.modules[__name__] = _teem_Module()
