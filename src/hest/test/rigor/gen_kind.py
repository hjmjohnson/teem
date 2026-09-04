#!/usr/bin/env python3
#
# Teem: Tools to process and visualize scientific data and images
# Copyright (C) 2009--2026  University of Chicago
# Copyright (C) 2005--2008  Gordon Kindlmann
# Copyright (C) 1998--2004  University of Utah
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
#
"""
gen_kind.py generates th-kind{2,3,4,5}*.c used for exhaustive hest testing.
"""

# pylint: disable=use-dict-literal,invalid-name

import subprocess
import re

# CType: mapping from TY type code in th_kindK.c to the scalar C types
# that are straightforward to handle. Missing only one: XY
CType = dict(
    BB='int',
    SH='short',
    UH='unsigned short',
    SI='int',
    UI='unsigned int',
    SL='long',
    UL='unsigned long',
    SZ='size_t',
    FL='float',
    DB='double',
    CH='char',
    SS='char*',
    EE='int',
    CO='Combo*',
)

# HType: mapping from TY type code in th_kindK.c to hest's name for the type
HType = dict(
    BB='Bool',
    SH='Short',
    UH='UShort',
    SI='Int',
    UI='UInt',
    SL='Long',
    UL='ULong',
    SZ='Size_t',
    FL='Float',
    DB='Double',
    CH='Char',
    SS='String',
    EE='Enum',
    XY='Other',
    CO='Other',
)

TYsNonnumeric = ['CH', 'SS', 'EE', 'XY', 'CO']


def HName(TY):
    """The "name" parm for hestOptAdd for type `TY`"""
    if TY == 'XY':
        return 'ostat'
    if TY == 'CO':
        return 'odyn'
    # else
    return HType[TY].lower()


def HExtraArg(TY):
    """Maps from type `TY` to the final extra arg to the hestOptAdd_ calls that need it"""
    ret = None
    match TY:
        case 'EE':
            ret = ', airEndian'
        case 'XY':
            ret = ', &posCB'
        case 'CO':
            ret = ', &comboCB'
        case _:
            ret = ''
    return ret


def cvar_decl(TY, KI, VN):
    """Create C variable declaration for type `TY` kind `KI` and value number `VN`"""
    decl = None
    if TY in CType:
        ctype = CType[TY]
        match KI:
            case 2 | 4:
                decl = f'{ctype} {TY}'
            case 3:
                decl = f'{ctype} {TY}[{VN}]'
            case 5:
                decl = f'{ctype} *{TY};  unsigned int {TY}Num'
    else:   # TY == XY
        match KI:
            case 2 | 4:
                decl = f'double {TY}[2]'
            case 3:
                decl = f'double {TY}[{2*VN}]'
            case 5:
                decl = f'double *{TY}; unsigned int {TY}Num'
    return decl


def hoa_call(TY, KI, VN):
    """Create the hestOptAdd_ call for type `TY` kind `KI` and value number `VN`"""
    hnm = HName(TY)
    hcall = None
    getenvs = (
        f'const char *{TY}flag = getenv("TH_{TY}_FLAG"); '
        f'const char *{TY}default = getenv("TH_{TY}_DEFAULT"); '
    )
    match KI:
        case 2:
            hcall = (
                f'{TY}opidx = hestOptAdd_1_{HType[TY]}(&hopt, '
                f'{TY}flag ? "{TY}" : NULL, "{hnm}", &{TY}, '
                f'{TY}default, "exactly 1 {hnm}"{HExtraArg(TY)})'
            )
        case 3:
            hname = ' '.join([f'{hnm}{N}' for N in range(0, VN)])
            if VN <= 4:
                hcall = (
                    f'{TY}opidx = hestOptAdd_{VN}_{HType[TY]}(&hopt, '
                    f'{TY}flag ? "{TY}" : NULL, "{hname}", {TY}, '
                    f'{TY}default, "exactly {VN} {hnm}"{HExtraArg(TY)})'
                )
            else:
                hcall = (
                    f'{TY}opidx = hestOptAdd_N_{HType[TY]}(&hopt, '
                    f'{TY}flag ? "{TY}" : NULL, "{hname}", {VN}, {TY}, '
                    f'{TY}default, "exactly {VN} {hnm}"{HExtraArg(TY)})'
                )
        case 4:
            if TY in TYsNonnumeric:
                # we're only including this in order to exercise error messages saying sorry,
                # and we only turn that on with the right env var
                hcall = f'if (getenv("TH_KIND4_{TY}")) {{'
                hcall += (
                    f'{TY}opidx = hestOptAdd_nva(&hopt, '  # hestOpt **optP
                    f'{TY}flag ? "{TY}" : NULL, '  #  const char *flag,
                    f'"{hnm}", airType{HType[TY]}, '  # const char *name, int type,
                    f'0, 1, '  # unsigned int min, int max,
                    f'&{TY}, {TY}default, '  # void *valueP, const char *dflt,
                    f'"exactly 1 {hnm}", NULL, '  # const char *info, unsigned int *sawP,
                )
                match TY:
                    # const airEnum *enm, const hestCB *CB
                    case 'EE':
                        hcall += 'airEndian, NULL'
                    case 'XY':
                        hcall += 'NULL, &posCB'
                    case 'CO':
                        hcall += 'NULL, &comboCB'
                    case _:
                        hcall += 'NULL, NULL'
                hcall += '); }'
            else:
                hcall = (
                    f'{TY}opidx = hestOptAdd_1v_{HType[TY]}(&hopt, '
                    f'{TY}flag ? "{TY}" : NULL, "{hnm}", &{TY}, '
                    f'{TY}default, "exactly 1 {hnm}"{HExtraArg(TY)})'
                )
        case 5:
            getenvs += (
                f'unsigned int {TY}vmin = 1; th_getenvUInt(&{TY}vmin, "TH_{TY}_VMIN"); '
                f'int {TY}vmax = 3; th_getenvInt(&{TY}vmax, "TH_{TY}_VMAX"); '
            )
            hcall = (
                f'{TY}opidx = hestOptAdd_Mv_{HType[TY]}(&hopt, '
                f'{TY}flag ? "{TY}" : NULL, "{hnm}", '
                f'{TY}vmin, {TY}vmax, &{TY}, '
                f'{TY}default, "some number of {hnm}", &{TY}Num{HExtraArg(TY)})'
            )
    # need TYopidx for later printing of option source
    return f'unsigned int {TY}opidx; ' + getenvs + hcall


def print_scalar(TY, VI=-1):
    """return C code to print a single value of type TY, perhaps from an array (index VI>=0)"""
    # cnvseq: printf conversion sequence for TY, if it is that simple
    cnvseq = dict(
        # BB: airEnumStr(airBool)
        SH='%hd',
        UH='%hu',
        SI='%d',
        UI='%u',
        SL='%ld',
        UL='%lu',
        SZ='%zu',
        # FL: airSinglePrintf(%g)
        # DB: airSinglePrintf(%lg)
        CH='%c',
        SS='%s',
        # EE: airEnumStr(airEndian)
        # XY: pos_print
        # CO: combo_print
    )
    var = TY
    if isinstance(VI, str) or VI >= 0:
        if 'XY' == TY:
            var += f'+(2*{VI})'
        else:
            var += f'[{VI}]'
    match TY:
        # first all the non-printf cases
        case 'BB':
            code = f'printf("%s", airEnumStr(airBool, {var})); '
        case 'FL':
            code = f'airSinglePrintf(stdout, NULL, 0, "%g", {var}); '
        case 'DB':
            code = f'airSinglePrintf(stdout, NULL, 0, "%lg", {var}); '
        case 'EE':
            code = f'printf("%s", airEnumStr(airEndian, {var})); '
        case 'XY':
            code = f'pos_print(stdout, {var}); '
        case 'CO':
            code = f'combo_print(stdout, {var}); '
        case _:
            # else we have a simple printf case
            code = f'printf("{cnvseq[TY]}", {var}); '
    return code


def print_val(TY, KI, VN):
    """Generate code for printing type `TY` kind `KI` and value number `VN`
    The means of generating this code is simplistic, and the output is
    not any kind of good example of human-written C code, unfortunately."""
    code = f'printf("{TY}="); '
    match KI:
        case 2 | 4:
            code += print_scalar(TY, -1)
        case 3:
            # number of values known at code generation time
            code += f'printf("["); '
            for vi in range(VN):
                code += print_scalar(TY, vi)
                code += 'printf(";"); '
            code += f'printf("]"); '
        case 5:
            # number of values known only at run time
            code += f'printf("["); '
            code += f'for (unsigned int vi=0; vi<{TY}Num; vi++) {{'  # escaping {
            code += print_scalar(TY, 'vi')
            code += f'   printf(";");'
            code += '}'
            code += f'printf("]"); '
    code += 'printf("\\n"); '
    code += f'THPRINT_SOURCE("{TY}", hopt, {TY}opidx); '
    return code


def procline(line, KI, VN, fname):
    """process one line of file `fname` for kind `KI` and value number `VN`"""
    if '(GLK)' in line:
        return (
            f'// {fname}: THIS FILE IS AUTOMATICALLY GENERATED.\n'
            '// EDITING THIS FILE IS POINTLESS.\n'
            '// INSTEAD, READ gen_kind.py, WHICH GENERATED THIS.\n'
        )
    if '(K)' in line:
        return ''
    if match := re.search('_DA_(..)_', line):
        # for *D*eclaring and *A*dding option
        # special test for non-orthogonal non-existent functions
        TY = match.group(1)
        decl = cvar_decl(TY, KI, VN)
        hoac = hoa_call(TY, KI, VN)
        line = f'{decl}; {hoac};'
    if match := re.search('_PR_(..)_', line):
        # for *PR*inting what was learned by hest
        # special test for non-orthogonal non-existent functions
        TY = match.group(1)
        if KI == 4 and TY in TYsNonnumeric:
            # the point of including these was for coverage of error messages,
            # we will never be able to parse these values,
            # so there's no point in trying to print them
            line = ''
        else:
            line = print_val(TY, KI, VN)
    return line


def generate(ilines, KI, VN):
    """generate and format the output file for kind `KI` and value number `VN`"""
    fname = f'th_kind{KI}_n{VN}.c' if KI == 3 else f'th_kind{KI}.c'
    print(f'generating {fname} ...')
    olines = [procline(iline, KI, VN, fname) for iline in ilines]
    with open(fname, 'w', encoding='utf-8') as ff:
        for oline in olines:
            ff.write(f'{oline}\n')
    # clang-format output so it looks normal
    cmd = f'clang-format -i {fname}'
    result = subprocess.run(cmd.split(), check=False)
    if result.returncode == 0:
        print(f'{cmd}: done')
    else:
        print(f'{cmd}: FAILED')


if __name__ == '__main__':
    with open('th_kindK.c', encoding='utf-8') as file:
        lines = [line.rstrip() for line in file]
    for kind in [2, 3, 4, 5]:
        match kind:
            case 2 | 4:
                generate(lines, kind, 1)
            case 3:
                generate(lines, 3, 2)
                generate(lines, 3, 3)
                generate(lines, 3, 4)
                generate(lines, 3, 5)
            case 5:
                generate(lines, 5, 0)
