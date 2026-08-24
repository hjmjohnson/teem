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
    vname = TY.lower()
    decl = None
    if TY in CType:
        ctype = CType[TY]
        match KI:
            case 2 | 4:
                decl = f'{ctype} {vname}'
            case 3:
                decl = f'{ctype} {vname}[{VN}]'
            case 5:
                decl = f'{ctype} *{vname};  unsigned int {vname}Num'
    else:   # TY == XY
        match KI:
            case 2 | 4:
                decl = f'double {vname}[2]'
            case 3:
                decl = f'double {vname}[2][{VN}]'
            case 5:
                decl = f'double *{vname}; unsigned int {vname}Num'
    return decl


def hoa_call(TY, KI, VN):
    """Create the hestOptAdd_ call for type `TY` kind `KI` and value number `VN`"""
    vname = TY.lower()
    hnm = HName(TY)
    hcall = None
    getenvs = (
        f'const char *{TY}flag = getenv("TH_{TY}_FLAG"); '
        f'const char *{TY}default = getenv("TH_{TY}_DEFAULT"); '
    )
    match KI:
        case 2:
            hcall = (
                f'hestOptAdd_1_{HType[TY]}(&hopt, '
                f'{TY}flag ? "{vname}" : NULL, "{hnm}", &{vname}, '
                f'{TY}default, "exactly 1 {hnm}"{HExtraArg(TY)})'
            )
        case 3:
            hname = ' '.join([f'{hnm}{N}' for N in range(0, VN)])
            if VN <= 4:
                hcall = (
                    f'hestOptAdd_{VN}_{HType[TY]}(&hopt, '
                    f'{TY}flag ? "{vname}" : NULL, "{hname}", {vname}, '
                    f'{TY}default, "exactly {VN} {hnm}"{HExtraArg(TY)})'
                )
            else:
                hcall = (
                    f'hestOptAdd_N_{HType[TY]}(&hopt, '
                    f'{TY}flag ? "{vname}" : NULL, "{hname}", {VN}, {vname}, '
                    f'{TY}default, "exactly {VN} {hnm}"{HExtraArg(TY)})'
                )
        case 4:
            hcall = (
                f'hestOptAdd_1v_{HType[TY]}(&hopt, '
                f'{TY}flag ? "{vname}" : NULL, "{hnm}", &{vname}, '
                f'{TY}default, "exactly 1 {hnm}"{HExtraArg(TY)})'
            )
        case 5:
            hcall = (
                f'unsigned int {TY}vmin = 1; th_getenvUInt(&{TY}vmin, "TH_{TY}_VMIN");'
                f'int {TY}vmax = 3; th_getenvInt(&{TY}vmax, "TH_{TY}_VMAX");'
                f'hestOptAdd_Nv_{HType[TY]}(&hopt, '
                f'{TY}flag ? "{vname}" : NULL, "{hnm}", '
                f'{TY}vmin, {TY}vmax, &{vname}, '
                f'{TY}default, "some number of {hnm}", &{TY.lower()}Num{HExtraArg(TY)})'
            )
    return getenvs + hcall


def procline(line, KI, VN, fname):
    """process one line of file `fname` for kind `KI` and value number `VN`"""
    if '(GLK)' in line:
        return f'// {fname}: AUTOMATICALLY GENERATED; EDITING POINTLESS; SEE gen_kind.py'
    if '(K)' in line:
        return ''
    if match := re.search('_DA_(..)_', line):
        # for *D*eclaring and *A*dding option
        TY = match.group(1)
        decl = cvar_decl(TY, KI, VN)
        hoac = hoa_call(TY, KI, VN)
        line = f'{decl}; {hoac};'
    if match := re.search('_PR_(..)_', line):
        # for *PR*inting what was learned by hest
        TY = match.group(1)
        line = f' // print({TY})'
    return line


def generate(ilines, KI, VN):
    """generate and format the output file for kind `KI` and value number `VN`"""
    fname = f'th_kind{KI}_n{VN}.c' if KI == 3 else f'th_kind{KI}.c'
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
                generate(lines, 5, 6)
