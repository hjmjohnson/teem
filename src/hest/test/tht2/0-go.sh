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

#COVERAGE='' #not doing coverage testing
COVERAGE=yup # YES doing coverage testing

WITHCOV=""
COVIS=""
COVFLAG=
if [[ $COVERAGE ]]; then
    WITHCOV=" WITH COVERAGE"
    COVFLAG=--coverage # flag for python runner script
    COVIS=covis        # directory for generated html coverage report
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
        coverage= make -j8
    )
    # clean up all .profraw from previous run
    rm -f *.profraw
else # not doing coverage testing
    (
        cd ../..
        make -j8
    )
fi

runme() {
    echo "#######"
    echo "####### $*"
    echo "#######"
    "$@"
}

echo -e "\n####### $0: running tests${WITHCOV}"
runme python3 run_hest_tests.py $COVFLAG \
    --cflags "-DTEEM_NON_CMAKE -I../../../../built/include -std=c99" \
    --ldflags "-L../../../../built/lib -lhest -lbiff -lair"

if [[ $COVERAGE ]]; then
    # analyzing coverage results
    runme llvm-profdata merge -sparse *.profraw -o test.profdata
    runme rm -rf $COVIS
    CARGS="build/th_fixed_multi \
        -object build/th_flag \
        -object build/th_flagged_scalars \
        -object build/th_unflagged_fixed \
        -object build/th_unflagged_variadic \
        -instr-profile=test.profdata \
        -ignore-filename-regex='.*tht2/.*'"
    if true; then
        # using llvm-cov
        runme llvm-cov report --show-functions $CARGS ../../*.c
        runme llvm-cov show --format=html \
            $CARGS \
            --show-branches=count --show-expansions \
            -output-dir=${COVIS}/
    else
        # using lcov tools, after: brew install lcov
        # NOTE: this does not know anything about macro expansion,
        # because .lcov format is entirely line-based
        runme llvm-cov export -format=lcov $CARGS >test.lcov
        runme genhtml test.lcov --branch-coverage \
            --ignore-errors unsupported,unsupported,inconsistent,corrupt \
            --output-directory covis/
        # ... actually, gave up on using gcovr ...
        ## after: pip3 install gcovr
        #gcovr --lcov test.lcov --html-details covis/
    fi
    open -a Firefox $COVIS/index.html
fi
