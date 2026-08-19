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
  --no-build         Skip compilation; assume binaries already exist
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
from pathlib import Path

# ---------------------------------------------------------------------------
# Constants from teem/air.h (not parsed at runtime to avoid a chicken-and-egg
# dependency on the compiled library).
# ---------------------------------------------------------------------------
AIR_ENDIAN_LITTLE = 1234
AIR_ENDIAN_BIG = 4321

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
        expected={'v': 0, 'v.src': 'default'},
    ),
    dict(
        exe='th_flag',
        name='flag: present → 1, source=cmdline',
        argv=['-v'],
        expected={'v': 1, 'v.src': 'cmdline'},
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
            'n.src': 'default',
            'x': 1.0,
            'x.src': 'default',
            's': 'bingo',
            's.src': 'default',
            'b': 0,
            'b.src': 'default',
            'e': AIR_ENDIAN_LITTLE,
            'e.src': 'default',
        },
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: all from cmdline',
        argv=['-n', '42', '-x', '3.14', '-s', 'hello', '-b', 'true', '-e', 'big'],
        expected={
            'n': 42,
            'n.src': 'cmdline',
            'x': 3.14,
            'x.src': 'cmdline',
            's': 'hello',
            's.src': 'cmdline',
            'b': 1,
            'b.src': 'cmdline',
            'e': AIR_ENDIAN_BIG,
            'e.src': 'cmdline',
        },
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: partial override (-n and -s only)',
        argv=['-n', '7', '-s', 'world'],
        expected={
            'n': 7,
            'n.src': 'cmdline',
            'x': 1.0,
            'x.src': 'default',
            's': 'world',
            's.src': 'cmdline',
            'b': 0,
            'b.src': 'default',
            'e': AIR_ENDIAN_LITTLE,
            'e.src': 'default',
        },
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: flags in reversed definition order',
        argv=['-e', 'big', '-b', 'true', '-s', 'rev', '-x', '2.0', '-n', '1'],
        expected={
            'n': 1,
            'x': 2.0,
            's': 'rev',
            'b': 1,
            'e': AIR_ENDIAN_BIG,
        },
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: last -n wins when repeated',
        argv=['-n', '10', '-n', '20'],
        expected={'n': 20, 'n.src': 'cmdline'},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: -b false explicitly',
        argv=['-b', 'false'],
        expected={'b': 0, 'b.src': 'cmdline'},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: -b 0 and -b 1 synonyms',
        argv=['-b', '1'],
        expected={'b': 1, 'b.src': 'cmdline'},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: -{ }- comment block is ignored',
        # The -{ ... }- encloses -n 100, which should be treated as a comment.
        # The real -n 42 before it should win.
        argv=['-n', '42', '-{', '-n', '100', '}-'],
        expected={'n': 42, 'n.src': 'cmdline'},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: nested -{ -{ }- }- comments',
        argv=['-n', '5', '-{', '-n', '10', '-{', 'bingo', 'banana', '}-', '}-'],
        expected={'n': 5, 'n.src': 'cmdline'},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: badly nested -{ -{ }- }- comments 1',
        argv=['-n', '5', '-{', '-n', '10', '-{', 'bingo', 'banana', '}-', '}-', '}-'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: badly nested -{ -{ }- }- comments 2',
        argv=['-n', '5', '-{', '-n', '10', '-{', 'bingo', 'banana', '}-', '}-', '-{'],
        expect_fail=True,
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: entire -n setting inside comment → n stays at default',
        argv=['-{', '-n', '77', '}-'],
        expected={'n': 99, 'n.src': 'default'},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: from response file',
        response_files={'opts.txt': '-n 77 -s from-file'},
        argv=['@opts.txt'],
        expected={
            'n': 77,
            'n.src': 'responsefile',
            's': 'from-file',
            's.src': 'responsefile',
            'x': 1.0,
            'x.src': 'default',
            'b': 0,
            'b.src': 'default',
        },
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: response file then cmdline override',
        response_files={'base.txt': '-n 77 -s from-file'},
        argv=['@base.txt', '-n', '99'],
        expected={
            'n': 99,
            'n.src': 'cmdline',
            's': 'from-file',
            's.src': 'responsefile',
        },
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: response file with # comment line',
        response_files={'commented.txt': '# this whole line is a comment\n-n 55'},
        argv=['@commented.txt'],
        expected={'n': 55, 'n.src': 'responsefile'},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: response file with double-quoted string containing space',
        response_files={'quoted.txt': '-s "hello world"'},
        argv=['@quoted.txt'],
        expected={'s': 'hello world', 's.src': 'responsefile'},
    ),
    dict(
        exe='th_flagged_scalars',
        name='scalars: response file with single-quoted string',
        response_files={'squoted.txt': "-s 'one two'"},
        argv=['@squoted.txt'],
        expected={'s': 'one two', 's.src': 'responsefile'},
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
            'f0': 1.0,
            'f1': 2.0,
            'f.src': 'default',
            'i0': 10,
            'i1': 20,
            'i2': 30,
            'i.src': 'default',
        },
    ),
    dict(
        exe='th_fixed_multi',
        name='multi: -f from cmdline',
        argv=['-f', '3.5', '7.25'],
        expected={
            'f0': 3.5,
            'f1': 7.25,
            'f.src': 'cmdline',
            'i0': 10,
            'i1': 20,
            'i2': 30,
            'i.src': 'default',
        },
    ),
    dict(
        exe='th_fixed_multi',
        name='multi: both options from cmdline, flags interleaved',
        argv=['-i', '1', '2', '3', '-f', '0.5', '0.25'],
        expected={
            'f0': 0.5,
            'f1': 0.25,
            'f.src': 'cmdline',
            'i0': 1,
            'i1': 2,
            'i2': 3,
            'i.src': 'cmdline',
        },
    ),
    dict(
        exe='th_fixed_multi',
        name='multi: -f with negative values',
        argv=['-f', '-1.0', '-2.5'],
        expected={'f0': -1.0, 'f1': -2.5, 'f.src': 'cmdline'},
    ),
    dict(
        exe='th_fixed_multi',
        name='multi: -i with zero and negative',
        argv=['-i', '0', '-5', '100'],
        expected={'i0': 0, 'i1': -5, 'i2': 100, 'i.src': 'cmdline'},
    ),
    # ==========================================================================
    # th_unflagged_fixed — unflagged positional options, no variadic
    # Options: A[2] int (req), B string (req), C float (req)
    # Straight left-to-right assignment in definition order.
    # ==========================================================================
    dict(
        exe='th_unflagged_fixed',
        name='unflagged-fixed: basic left-to-right assignment',
        argv=['10', '20', 'hello', '3.5'],
        expected={
            'A0': 10,
            'A1': 20,
            'A.src': 'cmdline',
            'B': 'hello',
            'B.src': 'cmdline',
            'C': 3.5,
            'C.src': 'cmdline',
        },
    ),
    dict(
        exe='th_unflagged_fixed',
        name='unflagged-fixed: string with no whitespace (is parsed as one token)',
        argv=['1', '2', 'no-spaces', '0.5'],
        expected={'A0': 1, 'A1': 2, 'B': 'no-spaces', 'C': 0.5},
    ),
    dict(
        exe='th_unflagged_fixed',
        name='unflagged-fixed: zero values',
        argv=['0', '0', 'zero', '0.0'],
        expected={'A0': 0, 'A1': 0, 'C': 0.0},
    ),
    # ==========================================================================
    # th_unflagged_variadic — forward-then-backward assignment
    # Definition order: A[2] int (req) | B[] strings variadic (min=1) | C int (req)
    # ==========================================================================
    dict(
        exe='th_unflagged_variadic',
        name='unflagged-variadic: minimum B (exactly one string in middle)',
        argv=['10', '20', 'only', '99'],
        expected={
            'A0': 10,
            'A1': 20,
            'A.src': 'cmdline',
            'B.len': 1,
            'B0': 'only',
            'B.src': 'cmdline',
            'C': 99,
            'C.src': 'cmdline',
        },
    ),
    dict(
        exe='th_unflagged_variadic',
        name='unflagged-variadic: three strings in middle',
        argv=['10', '20', 'p', 'q', 'r', '99'],
        expected={
            'A0': 10,
            'A1': 20,
            'B.len': 3,
            'B0': 'p',
            'B1': 'q',
            'B2': 'r',
            'C': 99,
        },
    ),
    dict(
        exe='th_unflagged_variadic',
        name='unflagged-variadic: five strings in middle',
        argv=['1', '2', 'a', 'b', 'c', 'd', 'e', '42'],
        expected={
            'A0': 1,
            'A1': 2,
            'B.len': 5,
            'B0': 'a',
            'B1': 'b',
            'B2': 'c',
            'B3': 'd',
            'B4': 'e',
            'C': 42,
        },
    ),
    dict(
        exe='th_unflagged_variadic',
        name='unflagged-variadic: strings that look like numbers are still strings',
        argv=['1', '2', '100', '200', '300', '999'],
        expected={
            'A0': 1,
            'A1': 2,
            'B.len': 3,
            'B0': '100',
            'B1': '200',
            'B2': '300',
            'C': 999,
        },
    ),
    dict(
        exe='th_unflagged_variadic',
        name='unflagged-variadic: negative int for C (backward-consumed from end)',
        # A gets the first two, C gets the last (even though it looks like a flag),
        # B gets the middle.  Tests that hest correctly identifies -7 as the
        # trailing positional arg for C rather than misreading it as a flag.
        argv=['3', '4', 'mid', '-7'],
        expected={
            'A0': 3,
            'A1': 4,
            'B.len': 1,
            'B0': 'mid',
            'C': -7,
        },
    ),
    # ==========================================================================
    # expected parse failures
    # ==========================================================================
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
        argv=['-n', '6 7'],  # HEY should be '6', '7', but this usefully exposes a problem
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
        print(f'  compiling via: {' '.join(cmd)}')
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
    that happen to contain '=' would survive (though none of our test values do).
    Lines that contain no '=' are silently ignored.
    """
    result = {}
    for line in stdout.splitlines():
        if '=' in line:
            key, _, val = line.partition('=')
            result[key.strip()] = val
    return result


# ---------------------------------------------------------------------------
# Comparison
# ---------------------------------------------------------------------------


def _values_match(key: str, actual_str: str, expected_val) -> bool:
    """Return True if actual_str is equivalent to expected_val for this key."""
    try:
        # bool must be checked before int because bool is a subclass of int.
        if isinstance(expected_val, bool):
            return int(actual_str) == int(expected_val)
        elif isinstance(expected_val, int):
            return int(actual_str) == expected_val
        elif isinstance(expected_val, float):
            actual_f = float(actual_str)
            return actual_f == expected_val
        elif isinstance(expected_val, str):
            return actual_str == expected_val
        else:
            raise TypeError(
                f'unsupported expected type {type(expected_val).__name__!r} for key {key!r}'
            )
    except (ValueError, ZeroDivisionError):
        return False


# ---------------------------------------------------------------------------
# Running one test
# ---------------------------------------------------------------------------


def run_one(test: dict, args, build_dir: Path) -> tuple:
    """
    Execute one test case.  Returns (expected: bool, detail: str).
    where "expected" means that the command ran or failed as expected.
    """
    exe = test['exe']
    argv = list(test.get('argv', []))
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

        # ---- build the command ----
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
        print(f"running: {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True, env=ENV)
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
        actual = parse_kv(result.stdout)
        failures = []
        for key, exp_val in expected.items():
            if key not in actual:
                failures.append(f'  missing key {key!r}')
                continue
            act_str = actual[key]
            if not _values_match(key, act_str, exp_val):
                failures.append(f'  {key}: got {act_str!r}, expected {exp_val!r}')

        if failures:
            detail = 'value mismatch(es):\n' + '\n'.join(failures)
            outlog = f'stdout:\n{textwrap.indent(result.stdout.strip(), INDENT)}'
            detail += f'\nfull binary output:' + outlog
            return False, detail

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
    ap.add_argument('--no-build', action='store_true', help='skip compilation step')
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
    if not args.no_build:
        if not args.cflags and not args.ldflags:
            print(
                'Warning: --cflags and --ldflags are both empty.  Compilation will',
                file=sys.stderr,
            )
            print(
                '  probably fail without -I paths for hest.h and a link target.', file=sys.stderr
            )
        print('Building exes ...')
        if not build_exes(args, exe_dir, build_dir):
            print('One or more builds failed; aborting.', file=sys.stderr)
            return 2
        print()

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
    if args.coverage:
        # once, set filename pattern for .profraw file
        ENV['LLVM_PROFILE_FILE'] = 'test-%p.profraw'
    for t in active:
        total += 1
        label = t.get('name', f"{t['exe']} {t.get('argv', [])}")
        expect_fail = t.get('expect_fail', False)
        # "ok" == got what we expected
        ok, detail = run_one(t, args, build_dir)
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
    sys.exit(main())
