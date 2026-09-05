#!/usr/bin/env python3
# This file was first written by an LLM so it is not copyrightable; subsequent human
# tweaks do not change that. Link with source, assuming GLK's account:
# https://claude.ai/chat/753238c4-c4d1-4241-824a-c1a310956286
"""
run_hest_tests.py — build and run hest tests.

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
"""

import argparse
import os
import shutil
import subprocess
import sys
import struct, fcntl, termios   # for ioctl hacking
import tempfile
import textwrap
import shlex
from pathlib import Path

# Constants from teem/air.h (not parsed at runtime to avoid a chicken-and-egg
# dependency on the compiled library).
AIR_ENDIAN_LITTLE = 1234
AIR_ENDIAN_BIG = 4321

# (GLK) anticipating airEnumStr(hestSource, src)
# no need for 'commmand-line': that is the assumed source unless otherwise noted
SRC_RF = 'response-file'
SRC_DF = 'default'

# how much to indent multi-line text
INDENT = '       '

# For GLK-generated th_kind* tests, based on TY-type parameterization
# TY names and canonical order of types within these:
#   BB     SH      UH      SI     UI     SL      UL     SZ       FL      DB       CH      SS      EE     XY      CO
# <bool> <short> <ushort> <int> <uint> <long> <ulong> <size_t> <float> <double> <char> <string> <enum> <ostat> <odyn>
# TYvals: test values for each type
TYvals = {
    'BB': ['true', 'false', 'true'],
    'SH': ['-67', str(-(2**15)), str(2**15 - 1)],
    'UH': ['42', '1', str(2**16 - 1)],
    'SI': ['-68', str(-(2**31)), str(2**31 - 1)],
    'UI': ['43', '2', str(2**32 - 1)],
    'SL': ['-69', str(-(2**63)), str(2**63 - 1)],
    'UL': ['44', '3', str(2**64 - 1)],
    'SZ': ['45', '4', str(2**64 - 1)],
    'FL': ['63.5', '-52.125', '23710'],
    'DB': ['64.5', '-53.125', '33710'],
    'CH': ['c', 'a', 'T'],
    'SS': ['good dog', 'mango', 'dark coffee'],
    'EE': ['big', 'little', 'big'],
    'XY': ['3,4', '0,0', '-49,7.5'],
    'CO': ['6.25', '0.1', '-5.125'],
}
# TYs: all the TY
TYs = list(TYvals.keys())
# FLAGS: all the -TY (e.g. "-SI") flags for th_kind* programs
FLAGS = {f'TH_{TY}_FLAG': '' for TY in TYs}
# all the TY for which kind=4 is not (no longer) supported
TYsNonnumeric = ['CH', 'SS', 'EE', 'XY', 'CO']
# TYsNumeric: all the "numeric" TY, but really: ones valid for kind=4 ("!" makes sense)
TYsNumeric = [TY for TY in TYs if TY not in TYsNonnumeric]


def DFLT(I):
    """build up default env dict from indexed values"""
    return {f'TH_{TY}_DEFAULT': ' '.join(Q(TYvals[TY][i]) for i in I) for TY in TYs}


def EXPT(I):
    """build up expected value dict from indexed value(s)"""
    return {
        TY: ([P(TY, TYvals[TY][i]) for i in I] if isinstance(I, list) else P(TY, TYvals[TY][I]))
        for TY in TYs
    }


def ARGV(I, noflag=False):
    """build up argv to pass indexed values (without the -TY flags if `noflag`)"""
    return [x for TY in TYs for x in ([] if noflag else [f'-{TY}']) + [TYvals[TY][i] for i in I]]


def P(TY, given):   # (GLK added)
    """when a value of type `TY` is passed to hest as `given`, this
    shows how the value should be printed after hestParse"""
    ret = given
    if TY == 'XY':
        # like pos_print() in th_kindK.c
        [xx, yy] = given.split(',')
        ret = f'({xx},{yy})'
    elif TY == 'CO':
        # like combo_print() in th_kindK.c
        ret = f'"{given}"->{str(float(given))}'
    return ret


def Q(dd):   # (GLK added)
    """when a default `dd` (of some type) is being put into a the "dflt" string
    of the hestOpt (or passed into it via an TH_TY_DEFAULT envvar), this handles
    the modest quoting that is needed.  But not in any robust kind of way!  This
    is only smart enough to handle the way things are tested here."""
    if ' ' in dd:
        return "'" + dd + "'"
    return dd


def test_list(args):
    # Test table
    #
    # Each entry is a dict.  Recognised keys:
    #
    #   exe             str   binary name (without path or extension)
    #   argv            list  arguments passed to the binary (no program name)
    #   env        (var,val)  (GLK added) prior to running, set environment variable
    #                         `var` to value `val`
    #   expected        dict  key -> Python value to compare against printed output
    #                           int, float -> compared exactly
    #                           (we use reals that can be stored exactly)
    #                           str   -> compared as string (exact, case-sensitive)
    #                         Keys not present in `expected` are ignored.
    #   expect_fail     bool  binary should exit non-zero  [default: False]
    #   response_files  dict  {filename: content}
    #                         Files to create in a temp dir before running.
    #                         Any argv element of the form "@FILENAME" where
    #                         FILENAME is a key here is rewritten to the actual
    #                         absolute path of the temp file.
    #   name            str   human-readable label printed in results
    #   num             int   added by later code: a stable numbering of each test
    #
    # these three are used with th_variadic below
    flagABCD = {f'TH_FLAG_{L}': 'true' for L in ['A', 'B', 'C', 'D']}
    flagA_CD = {f'TH_FLAG_{L}': 'true' for L in ['A', 'C', 'D']}
    expACD_XOX = {'A': [101, 202], 'C': 303, 'D': [404, 505]}
    dfltACD_XOX = {'TH_A_DFLT': '101 202', 'TH_C_DFLT': '303', 'TH_D_DFLT': '404 505'}

    tlist = [
        # ==========================================================================
        # th_flag — stand-alone flag (kind 1)
        # ==========================================================================
        dict(
            exe='th_flag',
            name='flag: absent -> 0, source=default',
            argv=[],
            expected={'v': 0, 'v.src': SRC_DF},
        ),
        dict(
            exe='th_flag',
            name='flag: present -> 1, source=cmdline',
            argv=['-v'],
            expected={'v': 1},
        ),
        dict(
            env={'TH_THEATER': ''},
            exe='th_flag',
            name='flag: present -> 1, source=cmdline but with coverage theater',
            argv=['-v'],
            expected={'v': 1},
        )
        if args.coverage
        else dict(),
        dict(
            env={'TH_MISSINGFLAG': ''},
            exe='th_flag',
            name='flag: (kind2) missing the required flag',
            argv=[],
            expect_fail=True,
        ),
        # ==========================================================================
        # th_flagged_scalars — kind=2 single fixed parm
        dict(
            exe='th_flagged_scalars',
            name='scalars: all defaults (no argv)',
            argv=[],
            expected={
                'n': 99,
                'n.src': SRC_DF,
                'x': 1.0,
                'x.src': SRC_DF,
                # note: The default string value _is_ the default string,
                # without any tokenization or quoting
                's': 'sunk cost',
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
            name='scalars: nested -{ }- comments',
            argv='-n 5 -{ -n 10 -{ bingo banana }- }-',
            expected={'n': 5},
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: badly nested -{ }- comments 1',
            argv='-n 5 -{ -n 10 -{ bingo banana }- }- }-',
            expect_fail=True,
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: badly nested -{ }- comments 2',
            argv='-n 5 -{ -n 10 -{ bingo banana }- }- -{',
            expect_fail=True,
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: badly nested -{ }- comments 3',
            argv='-n 5 -{ -n 10 -{ bingo banana',
            expect_fail=True,
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: entire -n setting inside comment -> n stays at default',
            argv='-{ -n 77 }-',
            expected={'n': 99, 'n.src': SRC_DF},
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file from stdin',
            stdin_content='-n 222 -{ that should stick }-\n',
            argv='-n 111 @-',
            expected={'n': 222, 'n.src': SRC_RF},
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file from stdin: cant read stdin twice',
            stdin_content='-n 222 -{ that should stick }-\n',
            argv='-n 111 @- @-',
            expect_fail=True,
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: from response file',
            response_files={'opts.txt': '-n 77 -s from-file\n'},
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
            response_files={'base.txt': '-n 77 -s from-file\n'},
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
            response_files={'commented.txt': '# this whole line is a comment\n-n 55\n'},
            argv='@commented.txt',
            expected={'n': 55, 'n.src': SRC_RF},
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file w/ balanced -{ }- comments',
            response_files={'bracecmt.txt': '-n -{ goober }- 789\n'},
            argv='@bracecmt.txt',
            expected={'n': 789, 'n.src': SRC_RF},
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file w/ UNbalanced -{ }- comments',
            response_files={'bad-bracecmt.txt': '-n -{ goober 789\n'},
            argv='@bad-bracecmt.txt',
            expect_fail=True,
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: malformed response file invocation',
            argv='@ resp/doesnotexist.txt',
            expect_fail=True,
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: missing response file',
            argv='@doesnotexist.txt',
            expect_fail=True,
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file cant ask for --help',
            response_files={'bad-help.txt': '--help'},
            argv='@bad-help.txt',
            expect_fail=True,
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file cant ask for --usage',
            response_files={'bad-usage.txt': '--usage'},
            argv='@bad-usage.txt',
            expect_fail=True,
        ),
        dict(
            env=dict(TH_N_DFLT='--help'),
            exe='th_flagged_scalars',
            name='scalars: default string file cant ask for --help',
            argv=[],
            expect_fail=True,
        ),
        dict(
            env=dict(TH_N_DFLT='--usage'),
            exe='th_flagged_scalars',
            name='scalars: default string file cant ask for --usage',
            argv=[],
            expect_fail=True,
        ),
        dict(
            env=dict(TH_N_DFLT='@bogus.txt'),
            exe='th_flagged_scalars',
            name='scalars: default string file cant invoke response file',
            argv=[],
            expect_fail=True,
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file with double-quoted string containing space',
            response_files={'dquoted.txt': '-s "hello world"\n'},
            argv=['-s', 'good morning', '@dquoted.txt'],
            expected={'s': 'hello world', 's.src': SRC_RF},
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file with double-quoted string with line continuation and other escapes',
            response_files={'dquoted-lc.txt': '-s "hello \\\nworld \$ \\` \\" \\\\ \\x "\n'},
            argv=['-s', 'good morning', '@dquoted-lc.txt'],
            expected={'s': 'hello world $ ` " \\ \\x ', 's.src': SRC_RF},
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file with single-quoted string',
            response_files={'squoted.txt': "-s 'one two'"},  # not using newline, for variety
            argv=['-s', 'not this', '@squoted.txt'],
            expected={'s': 'one two', 's.src': SRC_RF},
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file cannot directly recurse',
            # unfortunately here have to hard-code response file directory
            response_files={'recurse.txt': '-n 999 @resp/recurse.txt'},
            argv=['-s', 'hi there', '@recurse.txt'],
            expect_fail=True,
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file cannot indirectly recurse',
            # unfortunately here have to hard-code response file directory
            response_files={
                'recurse2a.txt': '-n 999 @resp/recurse2b.txt\n',
                'recurse2b.txt': '-n 888 @resp/recurse2a.txt\n',
            },
            argv=['-s', 'hi there', '@recurse2a.txt'],
            expect_fail=True,
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file can nest some',
            # unfortunately here have to hard-code response file directory
            response_files={
                'nest1a.txt': '-n 901 @resp/nest2a.txt',
                'nest2a.txt': '-n 902 @resp/nest3a.txt\n',
                'nest3a.txt': '-n 903 #no more nesting',
            },
            argv=['-n', '900', '@nest1a.txt'],
            expected={'n': 903, 'n.src': SRC_RF},
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file cannot nest too much',
            # unfortunately here have to hard-code response file directory
            response_files={
                'nest1b.txt': '-n 901 @resp/nest2b.txt',
                'nest2b.txt': '-n 902 @resp/nest3b.txt\n',
                'nest3b.txt': '-n 903 @resp/nest4b.txt',
                'nest4b.txt': '-n 904 @resp/nest5b.txt\n',
                'nest5b.txt': '-n 905 @resp/nest6b.txt',
                'nest6b.txt': '-n 906 #no more nesting',
            },
            argv=['-n', '900', '@nest1b.txt'],
            expect_fail=True,
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file doesnt close double-quote string',
            response_files={'bad-sq.txt': '-s "we have unfinished'},
            argv=['-s', 'hi there', '@bad-sq.txt'],
            expect_fail=True,
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file doesnt close single-quote string',
            response_files={'bad-sq.txt': "-s 'we have unfinished"},
            argv=['-s', 'hi there', '@bad-sq.txt'],
            expect_fail=True,
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file escapes space and concats quotes',
            response_files={'escape-space.txt': '-s \\ th"i"s\\ wo\'r\'ks'},
            argv=['-s', 'hi there', '@escape-space.txt'],
            expected={'s': ' this works', 's.src': SRC_RF},
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file doesnt finish escape',
            response_files={'bad-escape.txt': '-s this\\ bad\\'},
            argv=['-s', 'hi there', '@bad-escape.txt'],
            expect_fail=True,
        ),
        dict(
            exe='th_flagged_scalars',
            name='scalars: response file doesnt finish double-quoted escape',
            response_files={'bad-dqescape.txt': '-s "this bad\\'},
            argv=['-s', 'hi there', '@bad-dqescape.txt'],
            expect_fail=True,
        ),
        # ==========================================================================
        # th_fixed_multi — kind=3 fixed-multiple parms
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
        dict(
            exe='th_unflagged_fixed',
            name='unflagged-fixed: basic left-to-right assignment',
            argv='10 20 hello 3.5,4.24',
            expected={
                'A': [10, 20],
                'B': 'hello',
                'C': '(3.5,4.24)',
            },
        ),
        dict(
            exe='th_unflagged_fixed',
            name='unflagged-fixed: "zero" values',
            argv='0 0 zero 0.0,00',
            expected={'A': [0, 0], 'B': 'zero', 'C': '(0,0)'},
        ),
        dict(
            exe='th_unflagged_fixed',
            name='unflagged-fixed: ones and spaces values plus bad extra',
            argv=['1', '1', 'one and done', '1.0,1.00', 'unwanted'],
            expect_fail=True,
        ),
        dict(
            exe='th_unflagged_fixed',
            name='unflagged-fixed: ones and spaces values',
            argv=['1', '1', 'one and done', '1.0,1.00'],
            expected={'A': [1, 1], 'B': 'one and done', 'C': '(1,1)'},
        ),
        ## (GLK) exercising self-mop-enabled hest functions
        dict(
            env={'TH_SELFMOP': ''},
            exe='th_unflagged_fixed',
            name='unflagged-fixed: self-mopped ones and spaces values',
            argv=['1', '1', 'one and done', '1.0,1.00'],
            expected={'A': [1, 1], 'B': 'one and done', 'C': '(1,1)'},
        ),
        dict(
            env={'TH_SELFMOP': ''},
            exe='th_unflagged_fixed',
            name='unflagged-fixed: self-mopped --version',
            argv=['--version'],
        ),
        dict(
            env={'TH_SELFMOP': ''},
            exe='th_unflagged_fixed',
            name='unflagged-fixed: self-mopped --usage',
            argv=['--usage'],
        ),
        dict(
            env={'TH_SELFMOP': ''},
            exe='th_unflagged_fixed',
            name='unflagged-fixed: self-mopped --help',
            argv=['--help'],
        ),
        # ==========================================================================
        # th_variadic — A:two fixed, B:one variadic, C:single fixed, D:two fixed
        # GLK notes that this test program (originally LLM-generated) has since gained
        # lots of envvar-configurable features, so it has become the vehicle to test
        # many more things than it was originally meant for
        *[
            {**weed, 'exe': 'th_variadic'}  # weed = wee (smaller) dictionary
            for weed in [
                dict(
                    name='variadic: three strings in middle (all unflagged)',
                    argv=['10', '20', 'p', 'happy little tree', 'r', '33', '44', '55'],
                    expected={
                        'A': [10, 20],
                        'B': ['p', 'happy little tree', 'r'],
                        'C': 33,
                        'D': [44, 55],
                    },
                ),
                dict(
                    env={'TH_DROP_B': 'true'},
                    # weirdly this is (still?) the ONLY test with unflagged opts but none variadic
                    name='variadic: no B so zero strings in middle (all unflagged)',
                    argv=['10', '20', '33', '44', '55'],
                    expected={
                        'A': [10, 20],
                        'C': 33,
                        'D': [44, 55],
                    },
                ),
                dict(
                    env=flagABCD,
                    name='variadic: one string in middle (all flagged)',
                    argv='-D 44 55 -C 33 -B happy -A 10 20',
                    expected={
                        'A': [10, 20],
                        'B': ['happy'],
                        'C': 33,
                        'D': [44, 55],
                    },
                ),
                dict(
                    env={
                        'TH_A_DFLT': '111 222',
                        **flagABCD,
                    },
                    name='variadic: one string in middle (all flagged) with -A default',
                    argv='-B happy -C 33 -D 44 55',
                    expected={
                        'A': [111, 222],
                        'B': ['happy'],
                        'C': 33,
                        'D': [44, 55],
                    },
                ),
                dict(
                    env={
                        'TH_A_DFLT': '111',
                        **flagABCD,
                    },
                    name='variadic: one string in middle (all flagged) with -A bad default (only 1 of 2 ints)',
                    argv='-B happy -C 33 -D 44 55',
                    expect_fail=True,
                ),
                dict(
                    env={
                        'TH_A_DFLT': '119 229',
                        'TH_C_DFLT': '339',
                        'TH_D_DFLT': '449 559',
                        **flagA_CD,
                    },
                    name='variadic: repeated strings in middle from repeated response file',
                    response_files={'bongo.txt': 'bongo -{ a word to repeat }- '},
                    argv='@bongo.txt @bongo.txt',
                    expected={
                        'A': [119, 229],
                        'B': ['bongo', 'bongo'],
                        'C': 339,
                        'D': [449, 559],
                    },
                ),
                dict(
                    env={
                        'TH_A_DFLT': '111 oops',
                        **flagABCD,
                    },
                    name='variadic: one string in middle (all flagged) with -A bad default (bad 2nd int)',
                    argv='-B happy -C 33 -D 44 55',
                    expect_fail=True,
                ),
                dict(
                    name='variadic: minimum B (exactly vmin=1 string in middle) (all unflagged)',
                    argv='10 20 only 33 44 55',
                    expected={
                        'A': [10, 20],
                        'B': ['only'],
                        'C': 33,
                        'D': [44, 55],
                    },
                ),
                dict(
                    env={
                        'TH_B_VMIN': '0',
                        'TH_B_DFLT': '',  # the default is the empty string (not NULL)
                        **flagA_CD,
                    },
                    # from 0-README.md:
                    # For `kind`=5 (multiple variadic) options with `min`=0, a non-NULL empty default string `""` means: 0 arguments, _even for string-type options_.
                    name='variadic: no (unflagged) B vmin=0 strings given, has empty string default to say: zero args (all else flagged)',
                    argv='-A 101 202 -C 303 -D 404 505',
                    expected={'B': [], **expACD_XOX},
                ),
                dict(
                    env={
                        'TH_B_VMIN': '0',
                        'TH_B_DFLT': "''",  # the default is a quoted empty string
                        **flagA_CD,
                    },
                    # from 0-README.md:
                    # If the type is string, use `"''"` to indicate a default value of a single empty string.
                    name='variadic: no (unflagged) B vmin=0 strings given, has single empty as default (all else flagged)',
                    argv='-A 101 202 -C 303 -D 404 505',
                    expected={
                        'B': [''],
                        **expACD_XOX,
                    },
                ),
                dict(
                    env={
                        'TH_B_VMIN': '0',
                        'TH_B_DFLT': 'coffee grounds',
                        **flagA_CD,
                    },
                    # from 0-README.md:
                    # For an unflagged multiple variadic option with a default and with min=0 (i.e. the option can take zero or more parms),
                    # a lone “-.” argument is how the user explicitly says “I am passing zero arguments, and overriding the default”.
                    name='variadic: (unflagged) B vmin=0 getting only -. (all else flagged)',
                    argv='-A 101 202 -C 303 -D 404 505 -.',
                    expected={'B': [], **expACD_XOX},
                ),
                dict(
                    env={
                        'TH_B_VMIN': '0',
                        'TH_B_DFLT': 'coffee grounds',
                        **flagA_CD,
                    },
                    # from 0-README.md:
                    # On the other hand, not giving any args for the [unflagged multiple variadic _with_ a default and with `min`=0]
                    # option means “I am not setting this option, and want the default”
                    name='variadic: (unflagged) B vmin=0 getting nothing (to use default) (all else flagged)',
                    argv='-A 101 202 -C 303 -D 404 505',
                    expected={'B': ['coffee', 'grounds'], **expACD_XOX},
                ),
                dict(
                    env={
                        'TH_B_VMIN': '0',
                        **flagA_CD,
                    },
                    # from 0-README.md:
                    # For an unflagged multiple variadic option with no default and with min=0,
                    # giving no arguments also works to say “I am passing zero arguments”, rather than being an error.
                    name='variadic: exactly B (unflagged defaultless) vmin=0 string, 0 given (all else flagged)',
                    argv='-A 101 202 -C 303 -D 404 505',
                    expected={'B': [], **expACD_XOX},
                ),
                dict(
                    env={
                        'TH_B_VMIN': '1',
                        **flagA_CD,
                    },
                    # in constrast to above, now vmin=1, but none given; not okay
                    name='variadic: B (unflagged defaultless) vmin=1 but 0 given (all else flagged)',
                    argv='-A 101 202 -C 303 -D 404 505',
                    expect_fail=True,
                ),
                dict(
                    env={
                        'TH_B_VMIN': '0',
                        'TH_B_DFLT': 'hydrogen helium',
                        **flagA_CD,
                    },
                    name='variadic: no (unflagged) B vmin=0 strings given but has default w/ 2 (all else flagged)',
                    argv='-A 101 202 -C 303 -D 404 505',
                    expected={'B': ['hydrogen', 'helium'], **expACD_XOX},
                ),
                dict(
                    env={
                        'TH_B_VMIN': '2',
                        'TH_B_DFLT': 'hydrogen helium',
                        **flagA_CD,
                    },
                    name='variadic: no (unflagged) B vmin=2 strings given but has default w/ 2 (all else flagged)',
                    argv='-A 101 202 -C 303 -D 404 505',
                    expected={'B': ['hydrogen', 'helium'], **expACD_XOX},
                ),
                dict(
                    env={
                        'TH_B_VMIN': '3',
                        'TH_B_DFLT': 'hydrogen helium',
                        **flagA_CD,
                    },
                    name='variadic: no (unflagged) B vmin=3 strings given but has default w/ ONLY 2 (all else flagged)',
                    argv='-A 101 202 -C 303 -D 404 505',
                    expect_fail=True,
                ),
                dict(
                    env={
                        'TH_B_VMIN': '3',
                        'TH_B_DFLT': 'hydrogen helium lithium',
                        **flagA_CD,
                    },
                    name='variadic: (unflagged) B vmin=3 but ONLY 2 strings given (all else flagged)',
                    argv='-A 101 202 cats dogs -C 303 -D 404 505',
                    expect_fail=True,
                ),
                dict(
                    env={
                        'TH_B_VMIN': '2',
                        **flagA_CD,
                    },
                    name='variadic: (unflagged) B vmin=2 with 2 strings and -. (all else flagged)',
                    argv='-A 101 202 cats dogs -. -C 303 -D 404 505',
                    expected={'B': ['cats', 'dogs'], **expACD_XOX},
                ),
                dict(
                    env={
                        'TH_B_VMIN': '2',
                        **flagA_CD,
                    },
                    name='variadic: (unflagged) B vmin=2 with 3 strings (all else flagged)',
                    argv='-A 101 202 cats dogs fish -C 303 -D 404 505',
                    expected={'B': ['cats', 'dogs', 'fish'], **expACD_XOX},
                ),
                dict(
                    env={
                        'TH_B_VMIN': '2',
                        **flagA_CD,
                    },
                    name='variadic: (unflagged) B vmin=2 with 3 strings and -. (all else flagged)',
                    argv='-A 101 202 cats dogs fish -. -C 303 -D 404 505',
                    expected={'B': ['cats', 'dogs', 'fish'], **expACD_XOX},
                ),
                dict(
                    env={
                        'TH_B_VMIN': '3',
                        **flagA_CD,
                    },
                    name='variadic: (unflagged defaultless) B vmin=3 but ONLY 2 strings given (all else flagged)',
                    argv='-A 101 202 cats dogs -C 303 -D 404 505',
                    expect_fail=True,
                ),
                dict(
                    env={
                        'TH_B_VMIN': '3',
                        **flagA_CD,
                    },
                    name='variadic: (unflagged defaultless) B vmin=3 but ONLY 2 strings w/ -. given (all else flagged)',
                    argv='-A 101 202 cats dogs -. -C 303 -D 404 505',
                    expect_fail=True,
                ),
                dict(
                    env={
                        'TH_B_VMIN': '0',
                        'TH_B_DFLT': '"hydrogen" "hel i um"',
                        **flagA_CD,
                    },
                    name='variadic: exactly (unflagged) B vmin=0 string but has default w/ 2 quoted (all else flagged)',
                    argv='-A 101 202 -C 303 -D 404 505',
                    expected={'B': ['hydrogen', 'hel i um'], **expACD_XOX},
                ),
                dict(
                    env={
                        'TH_B_VMIN': '0',
                        'TH_B_DFLT': '"hydrogen" -{ move_along }- "helium"',
                        **flagA_CD,
                    },
                    name='variadic: exactly B (unflagged w/default) vmin=0 string, got 2 quoted w/comments (all else flagged)',
                    argv='-A 101 202 -C 303 -D 404 505',
                    expected={'B': ['hydrogen', 'helium'], **expACD_XOX},
                ),
                dict(
                    env={
                        'TH_B_VMIN': '0',
                        'TH_B_DFLT': '"hydrogen" -{ move_along "helium"',
                        **flagA_CD,
                    },
                    name='variadic: exactly B (unflagged w/default) vmin=0 string, got 2 quoted w/ busted comment (all else flagged)',
                    argv='-A 101 202 -C 303 -D 404 505',
                    expect_fail=True,
                ),
                dict(
                    env={
                        'TH_B_VMIN': '0',
                        'TH_B_DFLT': 'hydrogen\\ helium',
                        **flagA_CD,
                    },
                    name='variadic: exactly B (unflagged w/default) vmin=0 string, 1 given w/space (all else flagged)',
                    argv='-A 101 202 -C 303 -D 404 505',
                    expected={'B': ['hydrogen helium'], **expACD_XOX},
                ),
                dict(
                    env={
                        'TH_B_VMIN': '0',
                        'TH_B_DFLT': 'hydrogen\\ helium \\',
                        **flagA_CD,
                    },
                    name='variadic: exactly B (unflagged w/default) vmin=0 string, busted 1 given (all else flagged)',
                    argv='-A 101 202 -C 303 -D 404 505',
                    expect_fail=True,
                ),
                dict(
                    env=dict(TH_B_VMIN='0'),
                    name='variadic: minimum B (exactly vmin=0 string in middle) (all unflagged)',
                    argv='10 20 33 44 55',
                    expected={
                        'A': [10, 20],
                        'B': [],
                        'C': 33,
                        'D': [44, 55],
                    },
                ),
                # ---------- from 0-README.md
                # For an flagged multiple variadic option with min=0, the presence of the flag alone.
                # with no following parms, is what communicates “I am passing zero arguments
                # (overriding the default if there is one)”.
                # --------------------------- first with default
                # more from 0-README.md:
                # Thus for multiple variadic option with min=0 with flag “f”,
                # both “-f -.” and “-f” (with no non-flag arg following)
                # both mean “I am passing zero arguments”.
                dict(  # withOUT -.
                    env={
                        'TH_B_VMIN': '0',
                        'TH_B_DFLT': 'mister rogers',
                        **flagABCD,
                    },
                    name='variadic: vmin=0 B getting 0, over-riding default (all flagged)',
                    argv='-A 101 202 -B -C 303 -D 404 505',
                    expected={'B': [], **expACD_XOX},
                ),
                dict(  # WITH -.
                    env={
                        'TH_B_VMIN': '0',
                        'TH_B_DFLT': 'mister rogers',
                        **flagABCD,
                    },
                    name='variadic: vmin=0 B getting just -., over-riding default (all flagged)',
                    argv='-A 101 202 -B -. -C 303 -D 404 505',
                    expected={'B': [], **expACD_XOX},
                ),
                # ---- then without default
                dict(  # withOUT -.
                    env={
                        'TH_B_VMIN': '0',
                        **flagABCD,
                    },
                    name='variadic: vmin=0 B getting 0 (no default) (all flagged)',
                    argv='-A 101 202 -B -C 303 -D 404 505',
                    expected={'B': [], **expACD_XOX},
                ),
                dict(  # WITH -.
                    env={
                        'TH_B_VMIN': '0',
                        **flagABCD,
                    },
                    name='variadic: vmin=0 B getting just -. (no default) (all flagged)',
                    argv='-A 101 202 -B -. -C 303 -D 404 505',
                    expected={'B': [], **expACD_XOX},
                ),
                # --------------------------- end demo
                dict(
                    env=flagABCD,
                    name='variadic: not giving strings to default-less -B (all flagged)',
                    argv='-A 11 22 -C 33 -D 44 55',
                    expect_fail=True,
                ),
                dict(
                    env=flagABCD,
                    name='variadic: not giving ints to default-less -D (all flagged)',
                    argv='-A 11 22 -B eat food -C 33',
                    expect_fail=True,
                ),
                dict(
                    env={
                        'TH_B_VMIN': '2',
                        'TH_B_DFLT': 'mister rogers',
                        **flagABCD,
                    },
                    name='variadic: vmin=2 B not appearing, accepting default (all flagged)',
                    argv='-A 11 22 -C 33 -D 44 55',
                    expected={
                        'A': [11, 22],
                        'B': ['mister', 'rogers'],
                        'C': 33,
                        'D': [44, 55],
                    },
                ),
                dict(
                    env={
                        'TH_B_VMIN': '2',
                        'TH_B_DFLT': 'mister rogers',
                        **flagABCD,
                    },
                    name='variadic: vmin=2 B appearing with only 1 arg (all flagged)',
                    argv='-A 11 22 -B Pastore -C 33 -D 44 55',
                    expect_fail=True,
                ),
                dict(
                    env={
                        'TH_B_VMIN': '2',
                        'TH_B_DFLT': 'mister rogers',
                        **flagABCD,
                    },
                    name='variadic: vmin=2 B appearing with only 1 arg and then -. (all flagged)',
                    argv='-A 11 22 -B Pastore -. -C 33 -D 44 55',
                    expect_fail=True,
                ),
                # ------------- From 0-README.md:
                # For any multiple variadic option, flagged or unflagged,
                # "-." can be used to explicitly mark the end of args for the option.
                #  ------ so: first two with B flagged
                dict(
                    env={
                        'TH_B_VMIN': '2',
                        'TH_B_DFLT': 'mister rogers',
                        **flagABCD,
                    },
                    name='variadic: vmin=2 B appearing with enough args and then -. (all flagged)',
                    argv='-A 101 202 -B Pastore was won over -. -C 303 -D 404 505',
                    expected={'B': ['Pastore', 'was', 'won', 'over'], **expACD_XOX},
                ),
                dict(
                    env={
                        'TH_B_VMIN': '2',
                        'TH_B_DFLT': 'mister rogers',
                        **flagABCD,
                    },
                    name='variadic: vmin=2 B appearing with enough args and then -C (all flagged)',
                    argv='-A 101 202 -B Pastore was won over -C 303 -D 404 505',
                    expected={'B': ['Pastore', 'was', 'won', 'over'], **expACD_XOX},
                ),
                #  ------ then, two with B unflagged
                dict(
                    env={
                        'TH_B_VMIN': '2',
                        'TH_B_DFLT': 'mister rogers',
                        **flagA_CD,
                    },
                    name='variadic: vmin=2 B appearing with enough args and then -. (all flagged)',
                    argv='-A 101 202 Pastore was won over -. -C 303 -D 404 505',
                    expected={'B': ['Pastore', 'was', 'won', 'over'], **expACD_XOX},
                ),
                dict(
                    env={
                        'TH_B_VMIN': '2',
                        'TH_B_DFLT': 'mister rogers',
                        **flagA_CD,
                    },
                    name='variadic: vmin=2 B appearing with enough args and then -C (all flagged)',
                    argv='-A 101 202 Pastore was won over -C 303 -D 404 505',
                    expected={'B': ['Pastore', 'was', 'won', 'over'], **expACD_XOX},
                ),
                # ----------- end verification of -. behavior
                #
                # -------------------------- from 0-README.md:
                # Within the consumption order just described, each unflagged option asks:
                # "are there remaining command-line arguments for me to consume?"
                # ---------- WITH variadic
                dict(  ##### give A B C D   WITH variadic
                    env={
                        'TH_B_DFLT': 'mister rogers',
                        **dfltACD_XOX,
                    },
                    name='variadic: unflagged option consumption order w/ variadic: give ABCD',
                    argv='119 229 micro tonal 339 449 559',
                    expected={
                        'A': [119, 229],
                        'B': ['micro', 'tonal'],
                        'C': 339,
                        'D': [449, 559],
                    },
                ),
                dict(  ##### give A _ C D   WITH variadic
                    env={
                        'TH_B_DFLT': 'mister rogers',
                        **dfltACD_XOX,
                    },
                    name='variadic: unflagged option consumption order w/ variadic: give A_CD',
                    argv='119 229 339 449 559',
                    expected={
                        'A': [119, 229],
                        'B': ['mister', 'rogers'],
                        'B.src': SRC_DF,
                        'C': 339,
                        'D': [449, 559],
                    },
                ),
                dict(  ##### give A _ _ D   WITH variadic
                    env={
                        'TH_B_DFLT': 'mister rogers',
                        **dfltACD_XOX,
                    },
                    name='variadic: unflagged option consumption order w/ variadic: give A__D',
                    argv='119 229 449 559',
                    expected={
                        'A': [119, 229],
                        'B': ['mister', 'rogers'],
                        'B.src': SRC_DF,
                        'C': 303,
                        'C.src': SRC_DF,
                        'D': [449, 559],
                    },
                ),
                dict(  ##### give A _ _ _   WITH variadic
                    env={
                        'TH_B_DFLT': 'mister rogers',
                        **dfltACD_XOX,
                    },
                    name='variadic: unflagged option consumption order w/ variadic: give A___',
                    argv='119 229',
                    expected={
                        'A': [119, 229],
                        'B': ['mister', 'rogers'],
                        'B.src': SRC_DF,
                        'C': 303,
                        'C.src': SRC_DF,
                        'D': [404, 505],
                        'D.src': SRC_DF,
                    },
                ),
                dict(  ##### give _ _ _ _   WITH variadic
                    env={
                        'TH_B_DFLT': 'mister rogers',
                        **dfltACD_XOX,
                    },
                    name='variadic: unflagged option consumption order w/ variadic: give ____',
                    argv=[],
                    expected={
                        'A': [101, 202],
                        'A.src': SRC_DF,
                        'B': ['mister', 'rogers'],
                        'B.src': SRC_DF,
                        'C': 303,
                        'C.src': SRC_DF,
                        'D': [404, 505],
                        'D.src': SRC_DF,
                    },
                ),
                # ---------- withOUT variadic
                # HEY put back in here
                # -------------- end of test of unflagged consumption order
                dict(
                    env=dict(TH_TWO_VARI='true'),
                    name='variadic: cant have two unflagged variadic',
                    argv='1 2 100 200 300 -33 -44 -55',
                    expect_fail=True,
                ),
                dict(
                    env=dict(TH_TWO_VARI='true', TH_SELFMOP='true'),
                    name='variadic: (self-mopping) cant have two unflagged variadic',
                    argv='1 2 100 200 300 -33 -44 -55',
                    expect_fail=True,
                ),
                dict(
                    env=dict(TH_SELFMOP='true'),
                    name='variadic: (self-mopping) --usage',
                    argv='--usage',
                ),
                dict(
                    env=dict(TH_SELFMOP='true'),
                    name='variadic: (self-mopping) --help',
                    argv='--help',
                ),
            ]
        ],
        ## NOTE: all of the above have been with all 3 options being *unflagged*
        # dict(
        #    env=dict(TH_B_VMIN='0', TH_FLAG_A='true', TH_FLAG_C='true'),
        #    exe='th_variadic',
        #    name='variadic: CAN have vmin=0 with other two flagged',
        #    argv=['10', '20', '99'],
        #    expect_fail=True,
        # ),
    ]
    tlist += [
        # th_kind2 (NO flags, with defaults) --help did not increase coverage
        dict(
            env={
                **{f'TH_{TY}_DEFAULT': TYvals[TY][0] for TY in TYs},
                **FLAGS,
            },
            exe='th_kind2',
            name='kind2 (with flags, with defaults) --help',
            argv=['--help'],
        ),
        # (does not seem to increase coverage)
        # dict(
        #    env={
        #        **{f'TH_{TY}_DEFAULT': TYvals[TY][0] for TY in TYs},
        #        **FLAGS,
        #    },
        #    exe='th_kind2',
        #    name='kind2 (with flags, with defaults) set from default',
        #    argv=[],
        #    expected={TY: p(TY, TYvals[TY][0]) for TY in TYs},
        # ),
        dict(
            exe='th_kind2',
            name='kind2 (NO flags, NO defaults) set from args',
            argv=[TYvals[TY][1] for TY in TYs],
            expected={TY: P(TY, TYvals[TY][1]) for TY in TYs},
        ),
        dict(
            env=FLAGS,
            exe='th_kind2',
            name='kind2 (with flags, NO defaults) set from args',
            argv=[wut for TY in TYs for wut in [f'-{TY}', TYvals[TY][1]]],
            expected={TY: P(TY, TYvals[TY][1]) for TY in TYs},
        ),
        # (does not seem to increase coverage)
        # dict(
        #    env={
        #        **{f'TH_{TY}_DEFAULT': TYvals[TY][0] for TY in TYs},
        #        **FLAGS,
        #    },
        #    exe='th_kind2',
        #    name='kind2 (with flags, with defaults) set from args',
        #    argv=[wut for TY in TYs for wut in [f'-{TY}', TYvals[TY][1]]],
        #    expected={TY: p(TY, TYvals[TY][1]) for TY in TYs},
        # ),
        ###################### kind3_n2
        dict(
            env={
                **{f'TH_{TY}_DEFAULT': f'{Q(TYvals[TY][0])} {Q(TYvals[TY][1])}' for TY in TYs},
                **FLAGS,
            },
            exe='th_kind3_n2',
            name='kind3_n2 (with flags, with defaults) --help',
            argv=['--help'],
        ),
        dict(
            env={f'TH_{TY}_DEFAULT': f'{Q(TYvals[TY][0])} {Q(TYvals[TY][1])}' for TY in TYs},
            exe='th_kind3_n2',
            name='kind3_n2 (NO flags, with defaults) set from defaults',
            argv=[],
            expected={TY: [P(TY, TYvals[TY][0]), P(TY, TYvals[TY][1])] for TY in TYs},
        ),
        dict(
            exe='th_kind3_n2',
            name='kind3_n2 (NO flags, NO defaults) set from args',
            argv=[x for TY in TYs for x in [TYvals[TY][1], TYvals[TY][2]]],
            expected={TY: [P(TY, TYvals[TY][1]), P(TY, TYvals[TY][2])] for TY in TYs},
        ),
        # this increases air/array.c coverage but not hest coverage?
        dict(
            env=FLAGS,
            exe='th_kind3_n2',
            name='kind3_n2 (with flags, NO defaults) set from args',
            argv=[x for TY in TYs for x in [f'-{TY}', TYvals[TY][1], TYvals[TY][2]]],
            expected={TY: [P(TY, TYvals[TY][1]), P(TY, TYvals[TY][2])] for TY in TYs},
        ),
    ]
    ###################### kind3_n3
    tlist += [
        dict(
            env={
                **DFLT([0, 1, 2]),
                **FLAGS,
            },
            exe='th_kind3_n3',
            name='kind3_n3 (with flags, with defaults) --help',
            argv=['--help'],
        ),
        ## These do not seem to increase coverage but seems right to test the round-trips
        dict(
            env=DFLT([0, 1, 2]),
            exe='th_kind3_n3',
            name='kind3_n3 (NO flags, with defaults) set from defaults',
            argv=[],
            expected=EXPT([0, 1, 2]),
        ),
        dict(
            env=FLAGS,
            exe='th_kind3_n3',
            name='kind3_n3 (with flags, NO defaults) set from args',
            argv=ARGV([0, 1, 2]),
            expected=EXPT([0, 1, 2]),
        ),
    ]
    ###################### kind3_n4 (copy-pasta!)
    tlist += [
        dict(
            env={
                **DFLT([0, 1, 2, 0]),
                **FLAGS,
            },
            exe='th_kind3_n4',
            name='kind3_n4 (with flags, with defaults) --help',
            argv=['--help'],
        ),
        ## These do not seem to increase coverage but seems right to test the round-trips
        dict(
            env=DFLT([0, 1, 2, 0]),
            exe='th_kind3_n4',
            name='kind3_n4 (NO flags, with defaults) set from defaults',
            argv=[],
            expected=EXPT([0, 1, 2, 0]),
        ),
        dict(
            env=FLAGS,
            exe='th_kind3_n4',
            name='kind3_n4 (with flags, NO defaults) set from args',
            argv=ARGV([0, 1, 2, 0]),
            expected=EXPT([0, 1, 2, 0]),
        ),
    ]
    ###################### kind3_n5 (copy-pasta!)
    tlist += [
        dict(
            env={
                **DFLT([0, 1, 2, 1, 0]),
                **FLAGS,
            },
            exe='th_kind3_n5',
            name='kind3_n5 (with flags, with defaults) --help',
            argv=['--help'],
        ),
        ## These do not seem to increase coverage but seems right to test the round-trips
        dict(
            env=DFLT([0, 1, 2, 1, 0]),
            exe='th_kind3_n5',
            name='kind3_n5 (NO flags, with defaults) set from defaults',
            argv=[],
            expected=EXPT([0, 1, 2, 1, 0]),
        ),
        dict(
            exe='th_kind3_n5',
            name='kind3_n5 (NO flags, NO defaults) set from args',
            argv=ARGV([0, 1, 2, 1, 0], noflag=True),
            expected=EXPT([0, 1, 2, 1, 0]),
        ),
    ]
    ###################### kind4
    tlist += [
        dict(
            env={
                **DFLT([0]),
                **FLAGS,
            },
            exe='th_kind4',
            name='kind4 (with required flags+defaults) --help',
            argv=['--help'],
        ),
        dict(
            env={
                **DFLT([0]),  # harmlessly sets defaults for non-existent options
                **FLAGS,  # harmlessly turns on flags for non-existent options
            },
            exe='th_kind4',
            name='kind4 (with required flags+defaults) set from defaults',
            argv=[],
            expected={TY: P(TY, TYvals[TY][0]) for TY in TYsNumeric},
        ),
        *[
            dict(
                env={
                    f'TH_KIND4_{TZ}': '',
                    **DFLT([0]),  # harmlessly sets defaults for non-existent options
                    **FLAGS,  # harmlessly turns on flags for non-existent options
                },
                exe='th_kind4',
                name=f'kind4 (with req flags+defaults) set from dflt, plus bad TY={TZ}',
                argv=[],
                expect_fail=True,
            )
            for TZ in TYsNonnumeric
        ],
        dict(
            env={
                **DFLT([0]),
                **FLAGS,
            },
            exe='th_kind4',
            name='kind4 (with required flags+defaults) set by kind4-specific inversion 1->0',
            argv=[f'-{TY}' for TY in TYsNumeric],
            expected={f'{TY}': ('false' if TY == 'BB' else '0') for TY in TYsNumeric},
        ),
        dict(
            env={
                **{f'TH_{TY}_DEFAULT': ('false' if TY == 'BB' else '0') for TY in TYsNumeric},
                **FLAGS,
            },
            exe='th_kind4',
            name='kind4 (with required flags+defaults) set by kind4-specific inversion 0->1',
            argv=[f'-{TY}' for TY in TYsNumeric],
            expected={f'{TY}': ('true' if TY == 'BB' else '1') for TY in TYsNumeric},
        ),
        dict(
            env={
                **DFLT([0]),
                **FLAGS,
            },
            exe='th_kind4',
            name='kind4 (with required flags+defaults) set args',
            argv=[x for TY in TYsNumeric for x in [f'-{TY}', TYvals[TY][1]]],
            expected={f'{TY}': TYvals[TY][1] for TY in TYsNumeric},
        ),
        *[
            dict(
                env={
                    **DFLT([0]),
                    **FLAGS,
                },
                exe='th_kind4',
                name=f'kind4 (with required flags+defaults) set args but bad {TZ}',
                argv=[
                    x
                    for TY in TYsNumeric
                    # perturb only the passed TZ value
                    for x in [f'-{TY}', 'sewage' if TY == TZ else TYvals[TY][1]]
                ],
                expect_fail=True,
            )
            for TZ in TYsNumeric
        ],
    ]
    ###################### kind5
    tlist += [
        dict(
            env={
                **DFLT([0, 1]),
                **FLAGS,
            },
            exe='th_kind5',
            name='kind5 (with flags, with defaults) --help',
            argv=['--help'],
        ),
        dict(
            env={
                **DFLT([0, 1]),
                **FLAGS,
            },
            exe='th_kind5',
            name='kind5 (with flags, with defaults) set from defaults',
            argv=[],
            expected=EXPT([0, 1]),
        ),
        # HEY would be nicer if we actually exercised having a variable number of parms
        # i.e. with different number of parms for different options; it would demonstrate
        # correctness but would not increase coverage
        dict(
            env=FLAGS,
            exe='th_kind5',
            name='kind5 (with flags, NO defaults) set from args',
            argv=ARGV([1, 2]),
            expected=EXPT([1, 2]),
        ),
    ]
    # ==========================================================================
    # expected parse failures
    # ==========================================================================
    tlist += [
        dict(
            env={'TH_THEATER': ''},
            exe='th_unflagged_fixed',
            name='unflagged-fixed: coverage theater (with busted invocation)',
            argv=[],
            expect_fail=True,
        )
        if args.coverage
        else dict(),
        dict(
            env={'TH_SELFMOP': ''},  # make hestParseOrExit fail
            exe='th_unflagged_fixed',
            name='unflagged-fixed: hestParseOrExit failing on bad args',
            argv='10 20 hello 3,nope',
            expect_fail=True,
        ),
        dict(
            env={'TH_SELFMOP': ''},  # make hestParseOrExit fail
            exe='th_unflagged_fixed',
            name='unflagged-fixed: hestParseOrExit with no args',
            argv=[],
            expect_fail=True,
        ),
        *[  # make hestParseOrExit fail in different ways
            dict(
                env={'TH_SELFMOP': '', 'TH_BREAK_POE': str(POE)},
                exe='th_unflagged_fixed',
                name=f'unflagged-fixed: hestParseOrExit failing on busted hestOpt ({POE=})',
                argv='10 20 hello 3,4',
                expect_fail=True,
            )
            for POE in [1, 2, 3, 4]
        ],
        dict(
            env={'TH_SELFMOP': '', 'TH_QUIET_QUIT': ''},  # make hestParseOrExit quiet quit
            exe='th_unflagged_fixed',
            name='unflagged-fixed: busted "other", with quiet quitting',
            argv='10 20 hello 3,pets',
            expect_fail=True,
        ),
        dict(
            env={'TH_DSOEM': ''},  # Dont Sprint Other Error Message
            exe='th_unflagged_fixed',
            name='unflagged-fixed: busted "other", without error message',
            argv='10 20 hello 3,pets',
            expect_fail=True,
        ),
        dict(
            env=FLAGS,  # oops no defaults (harmlessly turns on flags for non-existent options)
            exe='th_kind4',
            name='kind=4 (single variadic) missing required defaults',
            argv=[],
            expect_fail=True,
        ),
        dict(
            env={
                **DFLT([0]),  # harmlessly sets defaults for non-existent options
                **{kk: vv for kk, vv in FLAGS.items() if kk != 'TH_SI_FLAG'},
            },
            exe='th_kind4',
            name='kind4 with required defaults but missing SI flag',
            argv=[],
            expect_fail=True,
        ),
        *[  # each dict has one flag passed a bad value, to test parsing error handling
            dict(
                env=FLAGS,
                exe='th_kind2',
                name=f'kind=2 with a non-parsable {TY=} value',
                argv=ARGV([0]) + [f'-{TY}', 'entropy'],
                expect_fail=True,
            )
            for TY in TYs
            if TY != 'SS'  # (any string is a parsable string: can't force string parse failure)
        ],
        dict(
            env={'TH_NULL_SAW': ''},
            exe='th_variadic',
            name='variadic: NULL sawP bad',
            argv='1 2 a b c d e 42'.split(),
            expect_fail=True,
        ),
        dict(
            env=dict(TH_NULL_HPARM='2'),
            exe='th_variadic',
            name='variadic: --help (after valid args) wont work without hparm->respectDashDashHelp',
            argv='1 2 a b c d e 42 --help'.split(),
            expect_fail=True,
        ),
        dict(
            env=dict(TH_NULL_HPARM='2'),
            exe='th_variadic',
            name='variadic: bare --help wont work without hparm->respectDashDashHelp',
            argv=['--help'],
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
    ]
    # stable numbering (even for ones not included because !args.coverage)
    tlist = [{'num': ii + 1, **tt} for ii, tt in enumerate(tlist)]
    tlist = [tt for tt in tlist if 'name' in tt]   # entirely drop those not included
    return tlist


def build_exes(args, exes: list, exe_dir: Path, build_dir: Path) -> bool:
    """Compile every test binary.  Returns True if all succeeded."""
    build_dir.mkdir(parents=True, exist_ok=True)
    cflags = args.cflags
    cc = args.cc
    if args.coverage:
        cflags += ' -fprofile-instr-generate -fcoverage-mapping'
        cc = 'clang'
    all_ok = True
    for exe in exes:
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


# Output parsing


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
                # ignore [], split on ;, ignore the final one
                val = val[1:-1].split(';')[0:-1]
            result[key] = val
    return result


# Comparison


def _values_match(key: str, actual_val, expected_val) -> bool:
    """Return True if actual_str is equivalent to expected_val for this key."""
    try:
        # bool must be checked before int because bool is a subclass of int.
        if isinstance(expected_val, bool):
            ret = ('true' == actual_val and expected_val) or (
                'false' == actual_val and not expected_val
            )
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


# Running one test


def run_one(test: dict, args, build_dir: Path) -> tuple:
    """
    Execute one test case.  Returns (expected: bool, detail: str).
    where "expected" means that the command ran or failed as expected.
    """
    exe = test['exe']
    envdict = test.get('env', None)   # GLK added
    argv = test.get('argv', [])
    # GLK added: accept string, turn into list with naive splitting on ' '
    if isinstance(argv, str):
        argv = argv.split(' ')
    expected = test.get('expected', {})
    expect_fail = test.get('expect_fail', False)
    rf_specs = test.get('response_files', {})
    stdin_content = test.get('stdin_content', None)   # GLK added

    rfdir = None
    #### materialise response files
    if rf_specs:
        rfdir = 'resp'   # pre-existing directory for this purpose
        # write the files
        for fname in rf_specs:
            fpath = os.path.join(rfdir, fname)
            with open(fpath, 'w') as fh:
                fh.write(rf_specs[fname])
            print(f'with response file {fpath}:')
            for line in rf_specs[fname].splitlines():
                print(f'{INDENT}{line}')
        # Rewrite "@FILENAME" argv elements where FILENAME is a known rf key.
        for idx, arg in enumerate(argv):
            if arg.startswith('@'):
                # inject the 'resp/' into the @arg identifying the response file
                fname = arg[1:]
                if fname in rf_specs:
                    fpath = os.path.join(rfdir, fname)
                    argv[idx] = f'@{fpath}'

    #### assemble the argv for running the program
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

    #### execute
    print('running: ', end='')
    # generate a copy-paste-able command-line by using shell syntax
    # for building up an environment in which a command is run
    if envdict:
        MYENV = ENV.copy()
        for envvar, envval in envdict.items():
            print(f'{envvar}="{envval}"  ', end='')
            MYENV[envvar] = envval
    else:
        MYENV = ENV
    # then print the command itself
    print(f'{shlex.join(cmd)}')
    # run it
    # added encoding='latin1' (h/t https://stackoverflow.com/a/76384840)
    # because otherwise got surprising error message (where previously it worked fine):
    # UnicodeDecodeError: 'utf-8' codec can't decode byte 0xbb in position 85: invalid start byte
    result = subprocess.run(
        cmd, capture_output=True, text=True, encoding='latin1', input=stdin_content, env=MYENV
    )
    exit_code = result.returncode
    errlog = f'stderr:\n{textwrap.indent(result.stderr.strip(), INDENT)}'
    outlog = f'stdout:\n{textwrap.indent(result.stdout.strip(), INDENT)}'

    #### If supposed to not run, verify that
    if expect_fail:
        if exit_code == 0:
            return False, 'expected failure/non-zero exit but got 0\n' + outlog
        return True, errlog

    #### Else supposed to run, verify that
    if exit_code:
        return False, f'unexpected failure/non-zero exit code {exit_code}\n' + errlog

    #### Supposed to run and did run; did it run correctly?
    print(outlog)
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
        detail = (
            'X X X X X X X X X X X X X X X\n'
            + ' value mismatch(es):\n'  #
            + 'X X X X X X X X X X X X X X X\n'  #
            + '\n'.join(failures)  #
        )
        outlog = f'stdout:\n{textwrap.indent(result.stdout.strip(), INDENT)}'
        detail += f'\nfull binary output:' + outlog
        return False, detail

    # if all went well, we do not include stdout: in the case of turning up hparm->verbose,
    # via setting envvar TH_VERBOSE, stdout is very long and tedious
    # (but still generated for the sake of coverage)
    return True, ''


# Main


def main() -> int:
    ap = argparse.ArgumentParser(
        description='Build and run hest integration tests.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='See module docstring for full usage and examples.',
    )
    ap.add_argument('--cc', default='cc', metavar='COMPILER', help='C compiler (default: cc)')
    ap.add_argument(
        '--cflags',
        default='-DTEEM_NON_CMAKE -I../../../../built/include -std=c99',
        metavar='FLAGS',
        help='compiler flags (include paths, -std=c99, etc.)',
    )
    ap.add_argument(
        '--ldflags',
        default='-L../../../../built/lib -lhest -lbiff -lair',
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
    args = ap.parse_args()

    # for better coverage (it makes a big difference in parsest.c)
    # crank up verbosity (not that we actually check the resulting output)
    if args.coverage:
        ENV['TH_VERBOSE'] = '11'

    script_dir = Path(__file__).parent
    exe_dir = script_dir
    build_dir = Path(args.build_dir)

    tests_all = test_list(args)
    if args.only:
        if args.only.isdigit():   # GLK added
            tnum = int(args.only)
            tests_active = [tt for tt in tests_all if tnum == tt['num']]
            if not tests_active:
                print(f'No test has number {tnum}', file=sys.stderr)
        else:
            pat = args.only
            tests_active = [t for t in tests_all if pat in t.get('name', '') or pat in t['exe']]
            if not tests_active:
                print(f'No tests match pattern "{pat!r}"', file=sys.stderr)
        if not tests_active:
            print('Available test numbers and names:', file=sys.stderr)
            for tt in tests_all:
                print(f"#{tt['num']}  {tt.get('name', tt['exe'])}", file=sys.stderr)
            return 1
    else:
        tests_active = tests_all

    # Determine which test binaries are needed
    exes = sorted({t['exe'] for t in tests_active})

    #### build
    if not args.cflags and not args.ldflags:
        print(
            'Warning: --cflags and --ldflags are both empty.  Compilation will',
            file=sys.stderr,
        )
        print('  probably fail without -I paths for hest.h and a link target.', file=sys.stderr)
    print('Building exes ...')
    if not build_exes(args, exes, exe_dir, build_dir):
        print(
            (
                'One or more builds failed; aborting. Perhaps try again after:\n'
                '  (cd ../../../; make clean); (cd ../../; make )'
            ),
            file=sys.stderr,
        )
        return 2

    if args.coverage:
        #### run --help on everything
        # GLK added: helper function for running --help
        def run_help(thebinary, thenv, stdin=None, usage=False):
            ddwut = '--usage' if usage else '--help'
            result = subprocess.run(
                [str(thebinary), ddwut], capture_output=True, text=True, stdin=stdin, env=thenv
            )
            print(f'./{build_dir}/{exe} {ddwut}:')
            if result.returncode == 0:
                print(textwrap.indent(result.stdout, INDENT))
            else:
                print(textwrap.indent(result.stderr, INDENT))

        for exe in exes:
            binary = build_dir / exe
            print('--help with no env')
            run_help(binary, ENV)
            # again with NULL hparm for coverage
            print('--help with TH_NULL_HPARM=1')
            run_help(binary, dict(TH_NULL_HPARM='1', **ENV))
            if 'th_variadic' == exe:
                # this one has extra switches set by envvar
                run_help(binary, ENV, usage=True)
                myenv = dict(**{f'TH_FLAG_{L}': 'true' for L in ['A', 'B', 'C', 'D']}, **ENV)
                print('--help with TH_FLAG_{A,B,C,D}=true')
                run_help(binary, myenv)
                print('--help with TH_FLAG_{A,B,C,D}=true, TH_B_VMIN=0')
                run_help(binary, myenv)
            elif 'th_kind2' == exe:
                print('--help with all defaults on')
                run_help(binary, {f'TH_{TY}_DEFAULT': TYvals[TY][0] for TY in TYs})
                if args.coverage:
                    print('--help with thwarted ioctl and 65 columns')
                    run_help(binary, dict(TH_COLUMNS='65', **ENV), stdin=subprocess.DEVNULL)
                    print('--help with fake ioctl thinking it has only 20 columns')
                    master_fd, slave_fd = os.openpty()
                    # Set the pty's window size: rows=20, cols=20, xpixels=0, ypixels=0
                    winsize = struct.pack('HHHH', 20, 20, 0, 0)
                    fcntl.ioctl(master_fd, termios.TIOCSWINSZ, winsize)
                    try:
                        run_help(binary, ENV, stdin=slave_fd)
                    finally:
                        os.close(master_fd)
                        os.close(slave_fd)
            elif 'th_flagged_scalars' == exe:
                # from the --help output it is clear this is working as intended,
                # BUT HEY SHOULD REALLY BE EXPLICITLY TESTING THAT HERE
                # but GLK cannot figure out why these tests do not increase coverage
                print(
                    '--help with TH_ENEFD 1: nonfinite dflt and !elideSingleNonExistFloatDefault'
                )
                run_help(binary, {'TH_ENEFD': '1'})
                print('--help with TH_ENEFD 2: nonfinite dflt and elideSingleNonExistFloatDefault')
                run_help(binary, {'TH_ENEFD': '2'})
            elif 'th_fixed_multi' == exe:
                # from the --help output it is clear this is working as intended,
                # but GLK cannot figure out why these tests do not increase coverage
                print(
                    '--help with TH_ENEFD 1: nonfinite dflt and !elideMultipleNonExistFloatDefault'
                )
                run_help(binary, {'TH_ENEFD': '1'})
                print(
                    '--help with TH_ENEFD 2: nonfinite dflt and elideMultipleNonExistFloatDefault'
                )
                run_help(binary, {'TH_ENEFD': '2'})

    #### run
    total = passed = failed = 0
    # if args.coverage:
    #    # once, set filename pattern for .profraw file
    #    ENV['LLVM_PROFILE_FILE'] = 'test-%p.profraw'
    ## Actually not doing that here, since there will be other tests, run from ./0-go.sh
    # but not from this .py, that usefully contribute to coverage
    for tt in tests_active:
        total += 1
        expect_fail = tt.get('expect_fail', False)
        print(f'[test# {tt["num"]}] === {tt["name"]}')
        # "ok" == got what we expected
        ok, detail = run_one(tt, args, build_dir)
        # (GLK) HERE is where sense of "pass" changes
        # from: "hestParse() and everything else worked ok"
        # to: "the program worked or failed as we intended it to"
        if ok:
            tag = 'good:' + ('fail' if expect_fail else 'pass')
            passed += 1
        else:
            tag = 'OOPS:' + ('PASS' if expect_fail else 'FAIL')
            failed += 1
        if detail:
            print(textwrap.indent(detail, INDENT))
        print(f'[test# {tt["num"]}] ==> {tag}')
        print('')   # to better visually separate tests

    #### summary
    print(f'\n{passed} passed, {failed} failed (of {total})')
    return 0 if failed == 0 else 1


if __name__ == '__main__':
    ENV = os.environ.copy()
    sys.exit(main())
