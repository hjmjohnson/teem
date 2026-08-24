#!/usr/bin/env python3
# This file was first written by an LLM so it is not copyrightable; subsequent human
# tweaks do not change that. Link with source, assuming GLK's account:
# https://claude.ai/chat/753238c4-c4d1-4241-824a-c1a310956286
"""
run_hest_tests.py — build and run hest integration tests.

Each test compiles a C test program (th_*.c), runs it with a specific
argv, and compares the "key=value" stdout against an expected dict.
Integer and floats values are compared exactly (GLK fix: since we
only use reals that floats can store exactly, and will handle special values
separately)

To run, from this directory (GLK fixed):

  python3 run_hest_tests.py \
      --cflags "-DTEEM_NON_CMAKE -I../../../../built/include -std=c99" \
      --ldflags "-L../../../../built/lib -lhest -lbiff -lair"

Because this is used as part of hest development, it precedes building libteem,
and instead uses the non-CMake build outputs, which are in a specific directory
relative to here in hest/test/tht2

Compilation notes:
  - Test program files use C99 (// comments, mid-block declarations, %zu).
  - Link order matters on some platforms: put the library after the .c file.

Usage:
  python3 run_hest_tests.py [options]

Options:
  --cc COMPILER      C compiler (default: cc)
  --cflags FLAGS     Compiler flags string (include paths, -std=..., etc.)
  --ldflags FLAGS    Linker flags string (library paths and -l flags)
  --build-dir DIR    Where to put compiled binaries (default: ./build)
  --only PATTERN     Run only tests whose name contains PATTERN (substring)
  --coverage         Compile with and run llvm source-based code coverage
  --valgrind         Wrap each execution with valgrind --leak-check=full
  --keep-temps       Do not delete temporary response files after the run
  -v, --verbose      Show stdout/stderr of passing tests too
"""

import argparse
import math
import os
import shutil
import subprocess
import sys
import tempfile
import textwrap
import shlex
from pathlib import Path

# ---------------------------------------------------------------------------
# Constants from teem/air.h (not parsed at runtime to avoid a chicken-and-egg
# dependency on the compiled library).
# ---------------------------------------------------------------------------
AIR_ENDIAN_LITTLE = 1234
AIR_ENDIAN_BIG = 4321

# (GLK) anticipating airEnumStr(hestSource, src)
# no need for 'commmand-line': that is the assumed default source
SRC_RF = 'response-file'
SRC_DF = 'default'

# how much to indent multi-line text
INDENT = '       '

# ---------------------------------------------------------------------------
# Test table
# ---------------------------------------------------------------------------
#
# Each entry is a dict.  Recognised keys:
#
#   exe             str   binary name (without path or extension)
#   argv            list  arguments passed to the binary (no program name)
#   env        (var,val)  (GLK added) prior to running, set environment variable
#                         `var` to value `val`
#   expected        dict  key → Python value to compare against printed output
#                           int, float → compared exactly
#                           (we use reals that can be stored exactly)
#                           str   → compared as string (exact, case-sensitive)
#                         Keys not present in `expected` are ignored.
#   expect_fail     bool  binary should exit non-zero  [default: False]
#   response_files  dict  {filename: content}
#                         Files to create in a temp dir before running.
#                         Any argv element of the form "@FILENAME" where
#                         FILENAME is a key here is rewritten to the actual
#                         absolute path of the temp file.
#   name            str   human-readable label printed in results
#
TESTS = [
    # ==========================================================================
    # th_flag — stand-alone flag (kind 1)
    # ==========================================================================
    dict(
        exe='th_flag',
        name='flag: absent → 0, source=default',
        argv=[],
        expected={'v': 0, 'v.src': SRC_DF},
    ),
    dict(
        exe='th_flag',
        name='flag: present → 1, source=cmdline',
        argv=['-v'],
        expected={'v': 1},
    ),
    # ==========================================================================
    # th_flagged_scalars — kind-2 flagged options of various types
    # Options: -n int(99)  -x double(1.0)  -s string("bingo")
    #          -b bool(false)  -e airEndian("little")
    # ==========================================================================
    dict(
        exe='th_flagged_scalars',
        name='scalars: all defaults (no argv)',
        argv=[],
        expected={
            'n': 99,
            'n.src': SRC_DF,
            'x': 1.0,
            'x.src': SRC_DF,
            's': 'bingo',
            's.src': SRC_DF,
            'b': False,
            'b.src': SRC_DF,
            'e': AIR_ENDIAN_LITTLE,
            'e.src': SRC_DF,
        },
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: all from cmdline',
        argv='-n 42 -x 3.14 -s hello -b true -e big',
        expected={
            'n': 42,
            'x': 3.14,
            's': 'hello',
            'b': True,
            'e': AIR_ENDIAN_BIG,
        },
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: partial override (-n and -s only)',
        argv='-n 7 -s world',
        expected={
            'n': 7,
            'x': 1.0,
            'x.src': SRC_DF,
            's': 'world',
            'b': False,
            'b.src': SRC_DF,
            'e': AIR_ENDIAN_LITTLE,
            'e.src': SRC_DF,
        },
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: flags in reversed definition order',
        argv='-e big -b true -s rev -x 2.0 -n 1',
        expected={
            'n': 1,
            'x': 2.0,
            's': 'rev',
            'b': True,
            'e': AIR_ENDIAN_BIG,
        },
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: last -n wins when repeated',
        argv='-n 10 -n 20',
        expected={'n': 20},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: -b false explicitly',
        argv='-b false',
        expected={'b': False},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: -b 0 synonym',
        argv='-b 0',
        expected={'b': False},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: -b 1 synonym',
        argv='-b 1',
        expected={'b': True},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: -{ }- comment block is ignored',
        # The -{ ... }- encloses -n 100, which should be treated as a comment.
        # The real -n 42 before it should win.
        argv='-n 42 -{ -n 100 }-',
        expected={'n': 42},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: nested -{ -{ }- }- comments',
        argv='-n 5 -{ -n 10 -{ bingo banana }- }-',
        expected={'n': 5},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: badly nested -{ -{ }- }- comments 1',
        argv='-n 5 -{ -n 10 -{ bingo banana }- }- }-',
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: badly nested -{ -{ }- }- comments 2',
        argv='-n 5 -{ -n 10 -{ bingo banana }- }- -{',
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: entire -n setting inside comment → n stays at default',
        argv='-{ -n 77 }-',
        expected={'n': 99, 'n.src': SRC_DF},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: from response file',
        response_files={'opts.txt': '-n 77 -s from-file'},
        argv=['@opts.txt'],
        expected={
            'n': 77,
            'n.src': SRC_RF,
            's': 'from-file',
            's.src': SRC_RF,
            'x': 1.0,
            'x.src': SRC_DF,
            'b': False,
            'b.src': SRC_DF,
        },
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: response file then cmdline override',
        response_files={'base.txt': '-n 77 -s from-file'},
        argv='@base.txt -n 99',
        expected={
            'n': 99,
            's': 'from-file',
            's.src': SRC_RF,
        },
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: response file with # comment line',
        response_files={'commented.txt': '# this whole line is a comment\n-n 55'},
        argv='@commented.txt',
        expected={'n': 55, 'n.src': SRC_RF},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: response file with double-quoted string containing space',
        response_files={'quoted.txt': '-s "hello world"'},
        argv='@quoted.txt',
        expected={'s': 'hello world', 's.src': SRC_RF},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: response file with single-quoted string',
        response_files={'squoted.txt': "-s 'one two'"},
        argv='@squoted.txt',
        expected={'s': 'one two', 's.src': SRC_RF},
    ),
    # ==========================================================================
    # th_fixed_multi — kind-3 fixed-multiple options
    # Options: -f float[2] (default "1.0 2.0")   -i int[3] (default "10 20 30")
    # ==========================================================================
    dict(
        exe='th_fixed_multi',
        name='multi: all defaults',
        argv=[],
        expected={
            'f': [1.0, 2.0],
            'f.src': SRC_DF,
            'i': [10, 20, 30],
            'i.src': SRC_DF,
        },
    ),
    dict(
        exe='th_fixed_multi',
        name='multi: -f from cmdline',
        argv='-f 3.5 7.25',
        expected={
            'f': [3.5, 7.25],
            'i': [10, 20, 30],
            'i.src': SRC_DF,
        },
    ),
    dict(
        exe='th_fixed_multi',
        name='multi: both options from cmdline, flags interleaved',
        argv='-i 1 2 3 -f 0.5 0.25',
        expected={
            'f': [0.5, 0.25],
            'i': [1, 2, 3],
        },
    ),
    dict(
        exe='th_fixed_multi',  # sorry, copy-pasta
        name='multi: both options from cmdline, --long flags interleaved',
        argv='--int 1 2 3 --float 0.5 0.25',
        expected={
            'f': [0.5, 0.25],
            'i': [1, 2, 3],
        },
    ),
    dict(
        exe='th_fixed_multi',
        name='multi: -f with negative values',
        argv='-f -1.0 -2.5',
        expected={'f': [-1.0, -2.5]},
    ),
    dict(
        exe='th_fixed_multi',
        name='multi: -i with zero and negative',
        argv='-i 0 -5 100',
        expected={'i': [0, -5, 100]},
    ),
    # ==========================================================================
    # th_unflagged_fixed — unflagged positional options, no variadic
    # Options: A[2] int (req), B string (req), C float (req)
    # Straight left-to-right assignment in definition order.
    # ==========================================================================
    dict(
        exe='th_unflagged_fixed',
        name='unflagged-fixed: basic left-to-right assignment',
        argv='10 20 hello 3.5',
        expected={
            'A': [10, 20],
            'B': 'hello',
            'C': 3.5,
        },
    ),
    dict(
        exe='th_unflagged_fixed',
        name='unflagged-fixed: "zero" values',
        argv='0 0 zero 0.0',
        expected={'A': [0, 0], 'B': 'zero', 'C': 0.0},
    ),
    dict(
        exe='th_unflagged_fixed',
        name='unflagged-fixed: ones and spaces values',
        argv=['1', '1', 'one and done', '1.0'],
        expected={'A': [1, 1], 'B': 'one and done', 'C': 1.0},
    ),
    # ==========================================================================
    # th_variadic — forward-then-backward assignment
    # Definition order: A[2] int (req) | B[] strings variadic (min=1) | C int (req)
    # ==========================================================================
    dict(
        exe='th_variadic',
        name='unflagged-variadic: minimum B (exactly one string in middle)',
        argv='10 20 only 99',
        expected={
            'A': [10, 20],
            'B': ['only'],
            'C': 99,
        },
    ),
    dict(
        exe='th_variadic',
        name='unflagged-variadic: three strings in middle',
        argv='10 20 p q r 99',
        expected={
            'A': [10, 20],
            'B': ['p', 'q', 'r'],
            'C': 99,
        },
    ),
    dict(
        exe='th_variadic',
        name='unflagged-variadic: five strings in middle',
        argv='1 2 a b c d e 42',
        expected={
            'A': [1, 2],
            'B': [
                'a',
                'b',
                'c',
                'd',
                'e',
            ],
            'C': 42,
        },
    ),
    dict(
        exe='th_variadic',
        name='unflagged-variadic: strings that look like numbers are still strings',
        argv='1 2 100 200 300 999',
        expected={
            'A': [1, 2],
            'B': ['100', '200', '300'],
            'C': 999,
        },
    ),
    dict(
        exe='th_variadic',
        name='unflagged-variadic: negative int for C (backward-consumed from end)',
        # A gets the first two, C gets the last (even though it looks like a flag),
        # B gets the middle.  Tests that hest correctly identifies -7 as the
        # trailing positional arg for C rather than misreading it as a flag.
        argv='3 4 mid -7',
        expected={
            'A': [3, 4],
            'B': ['mid'],
            'C': -7,
        },
    ),
]
# ==========================================================================
# running the GLK-generated th_kind* tests
# ==========================================================================
# TY names and canonical order of types within these:
#   BB     SH      UH      SI     UI     SL      UL     SZ       FL      DB       CH      SS      EE     XY      CO
# <bool> <short> <ushort> <int> <uint> <long> <ulong> <size_t> <float> <double> <char> <string> <enum> <ostat> <odyn>
# TYa and TYb are complementary subsets; maybe useful for testing
TYsA = [
    'BB',
    'UH',
    'UI',
    'UL',
    'FL',
    'CH',
    'EE',
    'CO',
]
TYsB = [
    'SH',
    'SI',
    'SL',
    'SZ',
    'DB',
    'SS',
    'XY',
]
TYs = TYsA + TYsB
TYvals = {
    'BB': ['true', 'true', 'false'],
    'SH': ['-67', str(-(2**15)), str(2**15 - 1)],
    'UH': ['42', '0', str(2**16 - 1)],
    'SI': ['-68', str(-(2**31)), str(2**31 - 1)],
    'UI': ['43', '0', str(2**32 - 1)],
    'SL': ['-69', str(-(2**63)), str(2**63 - 1)],
    'UL': ['44', '0', str(2**64 - 1)],
    'SZ': ['45', '0', str(2**64 - 1)],
    'FL': ['63.5', '-52.125', '23710'],
    'DB': ['64.5', '-53.125', '33710'],
    'CH': ['c', 'a', 'T'],
    'SS': ['good dog', 'bat', 'coffee'],
    'EE': ['big', 'little', 'big'],
    'XY': ['3,4', '0,0', '-49,7.5'],
    'CO': ['6.25', '0.1', '-5.125'],
}
if True:
    TESTS += [
        dict(
            env={f'TH_{TY}_DEFAULT': TYvals[TY][0] for TY in TYs},
            exe='th_kind2',
            name='kind2 (NO TYs flagged) all defaults',
            argv=['--help'],
        ),
        # dict(
        #    env={f'TH_{TY}_FLAG': '' for TY in TYs},
        #    exe='th_kind2',
        #    name='kind2 (all TYs flagged) --help',
        #    argv=['--help'],
        # ),
    ]
# ==========================================================================
# expected parse failures
# ==========================================================================
TESTS += [
    dict(
        env=dict(TH_NULL_HPARM='2'),
        exe='th_variadic',
        name='unflagged-variadic: --help (after valid args) wont work without hparm->respectDashDashHelp',
        argv='1 2 a b c d e 42 --help'.split(),
        expect_fail=True,
    ),
    dict(
        env=dict(TH_NULL_HPARM='2'),
        exe='th_variadic',
        name='unflagged-variadic: bare --help wont work without hparm->respectDashDashHelp',
        argv=['--help'],
        expect_fail=True,
    ),
    dict(
        env=dict(TH_VMIN='0'),
        exe='th_variadic',
        name='unflagged-variadic: cant have min=0 amidst other unflagged',
        argv=['10', '20', '99'],
        expect_fail=True,
    ),
    dict(
        env=dict(TH_TWOVAR='true'),
        exe='th_variadic',
        name='unflagged-variadic: cant have two unflagged variadic',
        argv=['10', '20', '99'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: bogus option',
        argv=['-q', 'bingo'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: missing argument',
        argv=['-n'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: unexpected non-flag',
        argv=['unexpected'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: too many args',
        argv=['-n', '6', '7'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: bad integer argument',
        argv=['-n', 'bingo'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: bad double argument',
        argv=['-x', 'bingo'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: bad boolean argument',
        argv=['-b', 'bingo'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: bad airEnum argument',
        argv=['-e', 'bingo'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: exercising ~string quoting (bad extra arg)',
        argv=['-s', '~/quote/me', 'oopsie'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: exercising !string quoting (bad extra arg)',
        argv=['-s', '!bingo', 'oopsie'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: exercising string! quoting (bad extra arg)',
        argv=['-s', 'bingo!', 'oopsie'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: exercising ! quoting (bad extra arg)',
        argv=['-s', '!', 'oopsie'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: exercising single quoting (bad extra arg)',
        argv=['-s', '""\\""\\""`""`""$""$', 'oopsie'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: exercising quoting quoting (bad extra arg)',
        argv=['-s', "'", 'oopsie'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: exercising str ing quoting (bad extra arg)',
        argv=['-s', 'bingo bob', 'oopsie'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: exercising ,~#^+:=@% quoting (bad extra arg)',
        argv=['-s', 'b,i~n#g^o+b:o=b@i%m', 'oopsie'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='error: exercising empty string quoting (bad extra arg)',
        argv=['-s', '', 'oopsie'],
        expect_fail=True,
    ),
]  # end TESTS

# ---------------------------------------------------------------------------
# Determine which test binaries are needed
# ---------------------------------------------------------------------------
EXES = sorted({t['exe'] for t in TESTS})

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------


def build_exes(args, exe_dir: Path, build_dir: Path) -> bool:
    """Compile every test binary.  Returns True if all succeeded."""
    build_dir.mkdir(parents=True, exist_ok=True)
    cflags = args.cflags
    cc = args.cc
    if args.coverage:
        cflags += ' -fprofile-instr-generate -fcoverage-mapping'
        cc = 'clang'
    all_ok = True
    for exe in EXES:
        src = exe_dir / f'{exe}.c'
        out = build_dir / exe
        if not src.exists():
            print(f'  [SKIP]  {exe}: source {src} not found')
            continue
        # Build the compile command.  ldflags comes last so that on Linux, the
        # linker sees the object file before the library that satisfies it.
        cmd = [cc] + cflags.split() + [str(src), '-o', str(out)] + args.ldflags.split()
        print(f'  compiling via: {shlex.join(cmd)}')
        print(f'  compiling {exe} ...', end='', flush=True)
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode == 0:
            print(' ok')
        else:
            print(' FAILED')
            sys.stderr.write(result.stderr)
            all_ok = False
    return all_ok


# ---------------------------------------------------------------------------
# Output parsing
# ---------------------------------------------------------------------------


def parse_kv(stdout: str) -> dict:
    """Parse 'key=value\\n' lines from stdout into {str: str}.

    Only the first '=' on each line is treated as the separator, so values
    that happen to contain '=' would survive.
    Lines that contain no '=', or (GLK) which have spaces prior to '=' are silently ignored.
    """
    result = {}
    for line in stdout.splitlines():
        if '=' in line:
            key, _, val = line.partition('=')
            if ' ' in key:
                continue
            if '[' == val[0]:
                if ']' != val[-1]:
                    raise RuntimeError(f'in {line=} {val=} starts with [ but does not end with ]')
                # ignore [], split on |
                val = val[1:-1].split('|')
            result[key] = val
    return result


# ---------------------------------------------------------------------------
# Comparison
# ---------------------------------------------------------------------------


def _values_match(key: str, actual_val, expected_val) -> bool:
    """Return True if actual_str is equivalent to expected_val for this key."""
    try:
        # bool must be checked before int because bool is a subclass of int.
        if isinstance(expected_val, bool):
            print(f'#########  {expected_val=} {type(expected_val)=} ')
            print(f'#########  {actual_val=} {type(actual_val)=} ')
            print(f'#########  returning {bool(actual_val) == expected_val}')
            ret = ('true' == actual_val and expected_val) or (
                'false' == actual_val and not expected_val
            )
            print(f'#########  returning {ret}')
            return ret
        if isinstance(expected_val, int):
            return int(actual_val) == expected_val
        if isinstance(expected_val, float):
            actual_f = float(actual_val)
            return actual_f == expected_val
        if isinstance(expected_val, str):
            return actual_val == expected_val
        if isinstance(expected_val, list):
            if not isinstance(actual_val, list):
                raise TypeError(
                    f'expected list {expected_val} but got {type(actual_val).__name__!r} {actual_val=} for key {key!r}'
                )
            vnum = len(expected_val)
            if len(actual_val) != vnum:
                raise TypeError(
                    f'len({expected_val=}) = {vnum} != {len(actual_val)} = len({actual_val=}) for key {key!r}'
                )
            return all(_values_match(key, actual_val[ii], expected_val[ii]) for ii in range(vnum))
        raise TypeError(
            f'unsupported expected type {type(expected_val).__name__!r} for key {key!r}'
        )
    except (ValueError, ZeroDivisionError):
        return False


# ---------------------------------------------------------------------------
# Running one test
# ---------------------------------------------------------------------------


def run_one(tnum: int, test: dict, args, build_dir: Path) -> tuple:
    """
    Execute one test case.  Returns (expected: bool, detail: str).
    where "expected" means that the command ran or failed as expected.
    """
    exe = test['exe']
    envdict = test.get('env', None)   # GLK added
    argv = test.get('argv', [])
    # GLK simplification: accept string, turn into list with naive splitting on ' '
    if isinstance(argv, str):
        argv = argv.split(' ')
    expected = test.get('expected', {})
    expect_fail = test.get('expect_fail', False)
    rf_specs = test.get('response_files', {})

    tmp_dir = None
    try:
        # ---- materialise response files ----
        if rf_specs:
            tmp_dir = tempfile.mkdtemp(prefix='hest_test_')
            # Rewrite "@FILENAME" argv elements where FILENAME is a known rf key.
            for idx, arg in enumerate(argv):
                if arg.startswith('@'):
                    fname = arg[1:]
                    if fname in rf_specs:
                        fpath = os.path.join(tmp_dir, fname)
                        with open(fpath, 'w') as fh:
                            fh.write(rf_specs[fname])
                        print(f'with response file {fpath}:')
                        for line in rf_specs[fname].splitlines():
                            print(f'{INDENT}{line}')
                        argv[idx] = f'@{fpath}'

        # ---- assemble the argv for running the program ----
        binary = str(build_dir / exe)
        cmd = [binary] + argv
        if args.valgrind:
            cmd = [
                'valgrind',
                '--leak-check=full',
                '--show-leak-kinds=all',
                '--error-exitcode=42',
                '--quiet',
            ] + cmd

        # ---- execute ----
        print(f'[{tnum}] running: ', end='')
        if envdict:
            print(f'')
            MYENV = ENV.copy()
            for envvar, envval in envdict.items():
                print(f'{envvar}={envval}  ', end='')
                MYENV[envvar] = envval
        else:
            MYENV = ENV
        print(f'{shlex.join(cmd)}')
        result = subprocess.run(cmd, capture_output=True, text=True, env=MYENV)
        exit_code = result.returncode
        errlog = f'stderr:\n{textwrap.indent(result.stderr.strip(), INDENT)}'

        # ---- If supposed to not run, verify that
        if expect_fail:
            if exit_code == 0:
                return False, 'expected failure/non-zero exit but got 0'
            return True, errlog

        # ---- Else supposed to run, verify that
        if exit_code:
            return False, f'unexpected failure/non-zero exit code {exit_code}' + errlog

        # ---- Supposed to run and did run; did it run correctly?
        print(f'stdout:\n{textwrap.indent(result.stdout.strip(), INDENT)}')
        actual = parse_kv(result.stdout)
        failures = []
        for key, exp_val in expected.items():
            if key not in actual:
                failures.append(f'  missing key {key!r}')
                continue
            act_val = actual[key]
            if not _values_match(key, act_val, exp_val):
                failures.append(f'  {key}: got {act_val!r}, expected {exp_val!r}')

        if failures:
            detail = 'value mismatch(es):\n' + '\n'.join(failures)
            outlog = f'stdout:\n{textwrap.indent(result.stdout.strip(), INDENT)}'
            detail += f'\nfull binary output:' + outlog
            return False, detail

        # if all went well, we do not include stdout: in the case of turning up hparm->verbose,
        # via setting envvar TH_VERBOSE, stdout is very long and tedious
        # (but still generated for the sake of coverage)
        return True, ''

    finally:
        if tmp_dir and not args.keep_temps:
            shutil.rmtree(tmp_dir, ignore_errors=True)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main() -> int:
    ap = argparse.ArgumentParser(
        description='Build and run hest integration tests.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='See module docstring for full usage and examples.',
    )
    ap.add_argument('--cc', default='cc', metavar='COMPILER', help='C compiler (default: cc)')
    ap.add_argument(
        '--cflags',
        default='',
        metavar='FLAGS',
        help='compiler flags (include paths, -std=c99, etc.)',
    )
    ap.add_argument(
        '--ldflags',
        default='',
        metavar='FLAGS',
        help='linker flags (-L paths, -l libs, or path to .a)',
    )
    ap.add_argument(
        '--build-dir',
        default='build',
        metavar='DIR',
        help='directory for compiled binaries (default: ./build)',
    )
    ap.add_argument(
        '--only',
        default=None,
        metavar='PATTERN',
        help='run only tests whose name contains PATTERN',
    )
    ap.add_argument(
        '--coverage',
        action='store_true',
        help='compile with and run llvm source-based code coverage',
    )
    ap.add_argument(
        '--valgrind', action='store_true', help='run each test under valgrind --leak-check=full'
    )
    ap.add_argument(
        '--keep-temps', action='store_true', help='do not delete temporary response files'
    )
    ap.add_argument(
        '-v', '--verbose', action='store_true', help='show output of passing tests too'
    )
    args = ap.parse_args()

    script_dir = Path(__file__).parent
    exe_dir = script_dir
    build_dir = Path(args.build_dir)

    # ---- build ----
    if not args.cflags and not args.ldflags:
        print(
            'Warning: --cflags and --ldflags are both empty.  Compilation will',
            file=sys.stderr,
        )
        print('  probably fail without -I paths for hest.h and a link target.', file=sys.stderr)
    print('Building exes ...')
    if not build_exes(args, exe_dir, build_dir):
        print('One or more builds failed; aborting.', file=sys.stderr)
        return 2

    # ---- run --help on everything
    # GLK added: helper function for running --help
    def runhelp(thebinary, thenv, stdin=None):
        result = subprocess.run(
            [str(thebinary), '--help'], capture_output=True, text=True, stdin=stdin, env=thenv
        )
        print(f'./{build_dir}/{exe} --help:')
        print(textwrap.indent(result.stdout, INDENT))

    for exe in EXES:
        binary = build_dir / exe
        print('--help with no env')
        runhelp(binary, ENV)
        # again with NULL hparm for coverage
        print('--help with TH_NULL_HPARM=1')
        runhelp(binary, dict(TH_NULL_HPARM='1', **ENV))
        if 'th_variadic' == exe:
            # this one has extra switches set by envvar
            print('--help with TH_FLAGFIX=true')
            runhelp(binary, dict(TH_FLAGFIX='true', **ENV))
            print('--help with TH_FLAGFIX=true, TH_VMIN=0')
            runhelp(binary, dict(TH_FLAGFIX='true', TH_VMIN='0', **ENV))
        elif 'th_kind2' == exe:
            print('--help with all defaults on')
            runhelp(binary, {f'TH_{TY}_DEFAULT': TYvals[TY][0] for TY in TYs})
            print('--help with thwarted ioctl and 65 #columns')
            runhelp(binary, dict(TH_COLUMNS='65', **ENV), stdin=subprocess.DEVNULL)

    # ---- filter ----
    active = TESTS
    if args.only:
        pat = args.only
        active = [t for t in TESTS if pat in t.get('name', '') or pat in t['exe']]
        if not active:
            print(f'No tests match pattern {pat!r}.', file=sys.stderr)
            print('Available test names:', file=sys.stderr)
            for t in TESTS:
                print(f"  {t.get('name', t['exe'])}", file=sys.stderr)
            return 1

    # ---- run ----
    total = passed = failed = 0
    ## Actually, don't.  The real runner script 0-go.sh has to handle this, since there
    ## will be other tests, not run from this .py, that contribute to coverage
    # if args.coverage:
    #    # once, set filename pattern for .profraw file
    #    ENV['LLVM_PROFILE_FILE'] = 'test-%p.profraw'
    for t in active:
        total += 1
        label = t.get('name', f"{t['exe']} {t.get('argv', [])}")
        expect_fail = t.get('expect_fail', False)
        # "ok" == got what we expected
        ok, detail = run_one(total, t, args, build_dir)
        # (GLK) HERE is where sense of "pass" changes
        # from: "hestParse() and everything else worked ok"
        # to: "the program worked or failed as we intended it to"
        if ok:
            tag = '[good:' + ('fail]' if expect_fail else 'pass]')
            passed += 1
        else:
            tag = '[OOPS:' + ('PASS]' if expect_fail else 'FAIL]')
            failed += 1
        print(f'{tag} {label}')
        if detail:
            print(textwrap.indent(detail, INDENT))
        print('')   # to better visually separate tests

    # ---- summary ----
    print(f'\n{passed} passed, {failed} failed (of {total})')
    return 0 if failed == 0 else 1


if __name__ == '__main__':
    ENV = os.environ.copy()
    # crank up verbosity, not that we will actually read the resulting output
    # but just for better coverage
    ENV['TH_VERBOSE'] = '11'
    sys.exit(main())
