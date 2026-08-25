#!/usr/bin/env bash
set -o errexit
set -o nounset
shopt -s expand_aliases
JUNK=""
function junk { JUNK="$JUNK $@"; }
function cleanup { rm -rf $JUNK; }
trap cleanup err exit int term
# https://devmanual.gentoo.org/tools-reference/bash/
unset UNU_QUIET_QUIT
unset UNU_HISTORY_TRACE

# put together based on info from (GLK account assumed)
# https://claude.ai/chat/753238c4-c4d1-4241-824a-c1a310956286

# NOTE: this is very much in-progress: should have a command-line
# option to control coverage, and, which kind of reporting to do

# where is root of Teem source tree
SRCROOT=$(cd "$(dirname "$0")/../../../.." && pwd)
echo "####### $0: SRCROOT=$SRCROOT"

#COVERAGE='' #not doing coverage testing
COVERAGE=yup # YES doing coverage testing

WITHCOV=""
COVIS=""
COVFLAG=
if [[ $COVERAGE ]]; then
    WITHCOV=" WITH COVERAGE"
    COVFLAG=--coverage # flag for python runner script
    COVIS=covis        # directory for generated html coverage report
    # now handled here, instead of in run_hest_tests.py
    export 'LLVM_PROFILE_FILE'='test-%p.profraw'
fi

echo -e "\n####### $0: cleaning up previous build of air, biff, hest"
(
    cd ../../..
    pwd
    make air.clean biff.clean hest.clean -j8
)

echo -e "\n####### $0: recompiling air, biff, hest${WITHCOV}"
if [[ $COVERAGE ]]; then
    # yes doing coverage testing
    (
        cd ../..
        # The coverage= prefix tell Teem's make to compile with coverage testing
        coverage= make test -j8
    )
    # clean up everything from previous run(s)
    echo "############## doing COVERAGE in $(pwd):"
    rm -rf *.profraw *.profdata $COVIS build *.json ncov.txt
    ls -alR
else # not doing coverage testing
    (
        cd ../..
        make test -j8
    )
fi

runme() {
    echo "#######" >&2
    echo "####### $1" >&2
    echo "#######" >&2
    eval "$1"
}
runme ./gen_kind.py # regenerate source code of programs generated from th_kindK.c

echo -e "\n####### $0: running tests${WITHCOV}"
# correctness of ../decls is indirectly via the compiler: could hest compile with the wrong header?
runme "../decls" # for coverage of hestOptAddDeclsPrint
junk decls.h     # just-created
## work on th_kindK.c should create the coverage from this, and more
## (still working on "and more", but ex6 uniquely triggers growing the opt dynamic array
## because of the crazy number of options it uses)
runme "../ex6"
runme "../ex6 --help"
runme "../ex6 --bogus"
# correctness of ../tht1 results is checked internally: will exit(1) on error
runme "../tht1" # simpler "tier 1" tests, also for coverage
runme "python3 run_hest_tests.py $COVFLAG \
    --cflags '-DTEEM_NON_CMAKE -I../../../../built/include -std=c99' \
    --ldflags '-L../../../../built/lib -lhest -lbiff -lair'"
if [[ $COVERAGE ]]; then
    # at end, clean up the raw profiles just generated
    junk *.profraw
fi
# but keep the build/ directory of executables for later debugging

if [[ $COVERAGE ]]; then
    # make the indexed profile
    runme "llvm-profdata merge -sparse test-*.profraw -o test.profdata"
    junk test.profdata
    # different tests used different binaries, have to list them there
    CARGS="build/th_fixed_multi \
        -object build/th_flag \
        -object build/th_flagged_scalars \
        -object build/th_unflagged_fixed \
        -object build/th_variadic \
        -object build/th_kind2 \
        -object build/th_kind3_n2 \
        -object build/th_kind3_n3 \
        -object build/th_kind3_n4 \
        -object build/th_kind3_n5 \
        -object build/th_kind4 \
        -object build/th_kind5 \
        -object ../ex6 \
        -instr-profile=test.profdata" # plus the indexed profile
    if true; then
        # using llvm-cov
        # name the hest source files (but nothing below in test/ or test/tht2)
        HEST_C=$(
            cd ../..
            echo *.c | xargs -n 1 echo | sed -e 's|^|$SRCROOT/src/hest/|' | xargs echo
        )
        # expand $SRCROOT
        HEST_C=$(eval echo $HEST_C)
        # what directories/files to show profiling results for
        SHOWWHAT="$SRCROOT/src/air $SRCROOT/src/biff $HEST_C"
        ## textual report on functions in hest source files
        runme "llvm-cov report --show-functions $CARGS ../../*.c"
        ## for diagnosing issue with uncovered region 0 characters long -> invisible in web view
        runme "llvm-cov export $CARGS > cov.json"
        # GLK went on a goose chase to figure out an unexecuted zero-length branch region
        # in argvHest.c, identified here, but there other such in other files
        runme "cat cov.json | python3 cov-json-check.py | wc -l"
        runme "cat cov.json | python3 cov-json-check.py | grep argvHest"
        runme "llvm-cov show --format=html \
            $CARGS $SHOWWHAT \
            --show-branches=count --show-expansions \
            -output-dir=${COVIS}/"
        # generate simplified textual representation of coverage in source, to facilitate
        # doing textual diffs to discover what changed in coverage between two runs
        # (the sed commands reduce the |count| region to just | | or |0| or |1|)
        runme "llvm-cov show --format=text $CARGS $SHOWWHAT | sed -E 's/\|[[:space:]]+([0-9])/|\1/g' | sed -E 's/\|[1-9][0-9]*(\.[0-9]+)?k?\|/|1|/g' | sed -E 's/\|[[:space:]]+\|/| |/g' > ncov.txt"
        open -a Firefox $COVIS/index.html
    else
        # using lcov tools, after: brew install lcov
        # NOTE: this does not know anything about macro expansion,
        # because .lcov format is entirely line-based
        IGNORE="--ignore-filename-regex='.*tht2/.*'"
        runme "llvm-cov export -format=lcov $CARGS $IGNORE >test.lcov"
        junk test.lcov
        runme "genhtml test.lcov --branch-coverage \
            --ignore-errors unsupported,unsupported,inconsistent,corrupt \
            --output-directory ${COVIS}/"
        open -a Firefox $COVIS/hest/index.html
        # ... actually, gave up on using gcovr ...
        ## after: pip3 install gcovr
        #gcovr --lcov test.lcov --html-details ${COVIS}/
    fi
fi
