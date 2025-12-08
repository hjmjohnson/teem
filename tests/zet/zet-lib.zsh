#
# Teem: Tools to process and visualize scientific data and images
# Copyright (C) 2009--2025  University of Chicago
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

####### Zet = Zsh-Enabled Testing
#
# New/minimal zsh-based machinery for testing command-line tools. This is roughly for
# <https://en.wikipedia.org/wiki/Characterization_test>. While the tools being tested are
# not "legacy", it important to have some way of ensuring that updates do not change the
# code output. Determining whether the output is correct is likely via human reasoning
# that can be done as part of generating the "reference file" ("reffile") that contains
# descriptions of the expected outputs (sometimes called a "golden file").
#
# Written by GLK for TeemV2 with recognition that once you have a command-line parser
# (like hest) and once you have command-line wrappers around your C programs (like unu
# around nrrd) shell scripts will be much faster to whip up, and more probably concise,
# than CTest tests. CTest is still obviously very important, but maybe there are easier
# ways to test some things. The acute need for creating Zet was the uncertainty around
# the correctness of the modernization to teem/src/nrrd/filt.c and its myriad ways of
# filtering data 1D, 2D, 3D, and 4D data.  Now, for the first time, everything there is
# actually tested. Creating Zet was also an excuse to learn more about zsh-specific
# scripting. Many technical zsh details were learned thanks to ChatGPT, but the structure
# of files and functions here is all hand-crafted.
#
# No relation to all the other software already named Zet:
# http://zaytsev.com/
# https://github.com/yarrow/zet
# https://www.zoho.com/developer/help/extensions/zappscli.html
# https://www.zetuniverse.com/
# https://zetsoftware.com/
#
# useful zsh resources:
# https://zsh.sourceforge.io/Doc/zsh_us.pdf
# https://gist.github.com/ClementNerma/1dd94cb0f1884b9c20d1ba0037bdcde2
# https://gist.github.com/ChristopherA/562c2e62d01cf60458c5fa87df046fbd
#
# learned:
#
# • with 1 character change from `typeset -g -A ZET_FAIL_COMD`, with
# `typeset -g A ZET_FAIL_COMD` you actually get a regular array, and then indexing like
# `ZET_FAIL_COMD[$fname]` (e.g. fname=hght-d13-th0.5-b0.nrrd) will produce cryptic errors
# like: `d13: parameter not set` because zsh is dutifully trying to interpret $fname as
# arithmetic as it (sensibly) always done for regular arrays). Use `typeset -p` to check
# what kind of type a thing currently is.
#
# • as "man zshparam" says: "In cases where there are two parameters with an upper- and
# lowercase form of the same name, such as path and PATH, the lowercase form is an array
# and the uppercase form is a scalar with the elements of the array joined together by
# colons."  So pity the fool who thinks after `local path` (which generates zero
# warnings) that path is a vanilla variable. Hence the defensive `readonly` commands
# below.

setopt no_unset           # error on unbound variables
setopt warn_create_global # warn if a function leaks variables
setopt warn_nested_var    # warns if looks like non-array/hash used as such
setopt bad_pattern        # error on invalid glob pattern (don' treat as literal)
setopt typeset_silent     # make `local` (aka `typeset`) inside loop silent
# Protect against accidental clobbering of tied parameters; all of these lower-case names
# are connected to special upper-case varible. Copied from man zshparam.
readonly fignore fpath histchars mailpath manpath
readonly module_path path prompt psvar reply
# these cannot be readonly'd because they're implicitly set by [[ $var =~ ... ]]
#   readonly match mbegin mend
# other things to avoid changing by accident; maybe should be longer
# (we do need to meddle with IFS)
readonly PWD OLDPWD RANDOM SECONDS

########################################################################
##
##  Top-level global variables
##

####### ZET_ROOT: absolute directory containing this Zet library file
if (( ${+ZET_ROOT} )); then
    print -u2 "ZET_ROOT already set to $ZET_ROOT"
    print -u2 "ZET_ROOT/zet-lib.zsh should be source'd once"
    exit 1
fi
ZET_ROOT=${(%):-%x}       # get the path of this file
#               %x  = path of the current script file
#        ${(%):-  } = expands %x to string we can save
ZET_ROOT=${ZET_ROOT:A:h}
#                  :A resolves to absolute
#                    :h takes directory

####### ZET_PFX: way of marking lines originating from these functions
ZET_PFX="(zet):"
print "$ZET_PFX Starting with ZET_ROOT=$ZET_ROOT"

####### ZET_DLM: visually distinctive things to delimit output
ZET_DLM="≡≡==≡≡≡-≡≡=≡≡==-≡≡=≡=≡≡-=≡≡≡≡==-=≡≡==≡≡-≡≡≡≡=≡=-≡≡==≡=≡-≡≡≡=≡=="

####### ZET_DCMD, ZET_TRACE: track where we are in scripts
# used by _zet_debug and _zet_cleanup
typeset -g ZET_DCMD
typeset -g ZET_TRACE

####### ZET_TSET: top-level description of current test set
# (only one tset at a time can be actively running)
# used with zet_{begin,test,end} functions below
typeset -g -A ZET_TSET  # -A : associative array
# (Ideally all the following arrays could also hang off of ZET_TSET
#  e.g. ZET_TSET[comd] instead of ZET_COMD, but sadly zsh cannot nest
#  associative arrays ...)
####### ZET_COMD, ZET_CHEK, ZET_WANT: maps from test/file names to more details
# see _zet_read_reff description
typeset -g -A ZET_COMD
typeset -g -A ZET_CHEK
typeset -g -A ZET_WANT
####### ZET_PASS, ZET_FAIL: arrays of passing/failing test/file names
typeset -g -a ZET_PASS # -a: regular array
if (( ${+ZET_FAIL} )); then unset ZET_FAIL; fi
typeset -g -a ZET_FAIL
####### ZET_FAIL_COMD, ZET_FAIL_RSLT, ZET_FAIL_WANT
# maps from test that failed to command `comd` to re-run,
# what it produces `rslt` versus what it should have produce `want`
typeset -g -A ZET_FAIL_COMD
typeset -g -A ZET_FAIL_RSLT
typeset -g -A ZET_FAIL_WANT

#__#__# _zet_arrays_reset
# a helper function to (re)initialize these global arrays. Despite the
# uniform syntax, this preserves the type (regular vs associative) of the array
_zet_arrays_reset() {
    setopt localoptions no_warn_nested_var # we're here to modify globals
    ZET_TSET=()
    ZET_COMD=()
    ZET_CHEK=()
    ZET_WANT=()
    ZET_PASS=()
    ZET_FAIL=()
    ZET_FAIL_COMD=()
    ZET_FAIL_RSLT=()
    ZET_FAIL_WANT=()
}

########################################################################
##
##  Functions and state for remembering transient ("junk") files,
##  what to do when finishing, and ways of finishing
##

typeset -ga ZET_JUNK
####### zet_junk file1 file2 ...
# says that these are transient files, to be cleaned up later
zet_junk() {
    setopt localoptions no_warn_nested_var # we're here to modify globals
    ZET_JUNK+=("$@");
}
####### zet_unjunk file1 file2 ...
# says that (oops) actually these files should stick around
zet_unjunk() {
    # For each argument, remove matching entries from the array
    local f
    for f in "$@"; do
        ZET_JUNK=(${ZET_JUNK:#${~f}})
    done
}
#__#__# _zet_debug
# records commands prior to being run here, for trap DEBUG
_zet_debug() {
    setopt localoptions no_warn_nested_var # we're here to modify globals
    local dcmd=${${(z)ZSH_DEBUG_CMD}[1]}
    #              (z) shell-like word-splitting of ZSH_DEBUG_CMD
    #            ${  ... array ... }[1]  1st element
    #          ${ more wrapping braces } because contents contain {}
    if [[ $(whence -w -- "$dcmd" 2>/dev/null) == *builtin* ]]; then
        # built-in commands aren't worth remembering
        ZET_DCMD=""
        ZET_TRACE=""
    else
        ZET_DCMD="$ZSH_DEBUG_CMD"
        ZET_TRACE="$funcfiletrace[1]"
    fi
}
trap _zet_debug DEBUG
#__#__# _zet_cleanup
# how we speak for ourselves on our way out the door
_zet_cleanup() {
    trap '' DEBUG
    if [[ -n $ZET_DCMD ]]; then
        local fileline=("${(@s/:/)ZET_TRACE}")
        print -u2 "\n$ZET_PFX $fileline[1] line $fileline[2] error running: $ZET_DCMD"
        ZET_DCMD=""
    fi
    # we're done if have no junk
    (( ${#ZET_JUNK[@]} == 0 )) && return 0
    if (( ! ${+ZET_UNJUNK} )); then
        # else remove it (unless ZET_UNJUNK)
        print "(_zet_cleanup rm'ing $#ZET_JUNK files)"
        rm -rf -- "${ZET_JUNK[@]}"
        # clear junk list in case of repeated calls
        setopt localoptions no_warn_nested_var
        ZET_JUNK=()
    fi
}
# Ensure trap works for ordinary exits and signals
# Note: trapping on "ERR" allows failing commands (in addition to
# failure to start running a command) to generate diagnostic context
# (e.g. to help show which unu command had busted options)
trap _zet_cleanup EXIT TERM ERR
# Respond to a control-C by cleaning up and then `exit`ing
trap '_zet_cleanup; exit 130' INT
# Ensure cleanup even when script is sourced or run in a subshell
autoload -Uz add-zsh-hook
add-zsh-hook zshexit _zet_cleanup

####### zet_die line1 line2 ...
# verbose way to bail from script
zet_die() {
    local where  # where are we being called from?
    if (( ${#funcstack[@]} > 1 )); then
        # inside a function (funcstack[1] = zet_die itself)
        where="ERROR in ${funcstack[2]}() called from ${funcfiletrace[2]}:"
    else
        # called from top-level, not inside a function
        # can still use $funcfiletrace[1]
        where="ERROR in script ${funcfiletrace[1]}:"
    fi
    print -u2 "\n$ZET_PFX $where"
    local line
    for line in "$@"; do
        print -u2 "$line"
    done
    exit 1
}

####### zet_stop
# way to stop a script while preventing any junk cleanup
# (for debugging of any transient files)
zet_stop() {
    setopt localoptions no_warn_nested_var # okay to modify ZET_JUNK
    ZET_JUNK=()
    exit 0
}

########################################################################
##
##  String utility functions
##

#__#__# _zet_unspace str
# Collapse internal multiple spaces in `str` to single space,
# and entirely remove leading and trailing spaces
_zet_unspace() {
    emulate -L zsh
    unsetopt WARN_CREATE_GLOBAL
    unsetopt ERR_EXIT
    setopt extended_glob  # ensure extended globbing for pattern matching
    local str="$1"
    # collapse 2+ internal spaces to a single space
    str="${str//  +/ }"
    # remove trailing spaces/tabs
    str="${str%%[ \t]##}"
    # remove leading spaces/tabs
    str="${str##[ \t]##}"
    # return
    print -n "$str"
}

#__#__# _zet_enbrace rest fname
# replace occurances of `fname` in `rest` with `{}`
_zet_enbrace() {
    emulate -L zsh
    setopt NO_BRACE_EXPAND
    local rest="$1" fname="$2"
    # why } but not { needs \-escaping is totally mysterious
    rest="${rest//$fname/{\}}"
    print -n "$rest"
}

########################################################################
##
##  File utility functions
##

####### zet_assert_cksum want fname
# runs (effectively) "cat fname | cksum", asserts that its output is same as `want`
zet_assert_cksum() {
    local want="$1" fname="$2"
    local rslt="$(cksum $fname)" # actually 3 words, ending with filename
    rslt=${${=rslt}[1,2]} # get first two words: cksum and size
    if [[ "$rslt" != "$want" ]]; then
        zet_unjunk $fname
        zet_die "problem at ${funcfiletrace[1]}" \
                "\"cat $fname | cksum\" supposed to say: $want" \
                "but actually got: $rslt\n"
    fi
    # else rslt == want; continue
}

####### zet_assert_out want comd
# runs command `comd`, asserts that its output is same as `want`
# (HEY this is currently unused? but seems useful)
zet_assert_out() {
    local want comd rslt
    want="$1"
    shift
    comd=("$@")       # store the command and all its arguments in an array
    rslt="$("${comd[@]}")"  # run the command safely and capture stdout
    if [[ "$rslt" != "$want" ]]; then
        zet_die "problem at ${funcfiletrace[1]}" \
                "\"$comd\" supposed to say: $want" \
                "but actually got: $rslt"
    fi
    # else rslt == want; continue
}

########################################################################
##
##  Functions for setting up and running a test set
##  and the various test commands therein
##

#__#__# _zet_read_reff reffile
# Fills in associative arrays `ZET_COMD`, `ZET_CHEK`, `ZET_WANT`
# from reference data in file `reffile`, each line of which is formatted as:
#       fname :: comd ;; chek => want
# The `::`, `;;`, and `=>` delimit the `fname`, `comd`, `chek`, and `want`
# fields, with or without any spaces around the delimiters.
# NOTE: the only expected white-space within the line is just ` ` space.
# * `fname` is a file generated by a test; effectively this is the test name.
#   NOTE: `fname` must not have any spaces (any spaces are removed)
# * `comd` is nominally command that generates `fname`, often a string ending
#   with "-o $fname". NOTE: this is for documentation and context rather
#   than for actually re-generating `fname`. The calling test script has to
#   already know from context exactly how to generate `fname`.
#   `comd` is saved in `ZET_COMD[$fname]`
# * `chek` is the actual command(s) to run to asses something about `fname`,
#   to see if it looks like the expected output (does it match `want`).
#   `chek` is saved to `ZET_CHEK[$fname]`.
#   NOTE: any extra spaces around delimeters `::`, `;;`, and `=>` may end
#   in `comd` or `chek`, and the effect of these commands is expected to be
#   invariant w.r.t. changes in the presence of this whitespace
#   (as it is on the unix command-line)
# * `want` is the expected output of `chek`
#   NOTE: upon reading, any leading spaces in `want` are removed;
#   no other transformations are done.
# NOTE: Within `comd`, `chek`, and `want`, just like with unix `find`,
# the token "{}" works as an abbreviation for the filename `fname`.
# Thus, if $chek is "cksum {}" that is expanded to "cksum $fname" prior to
# saving in to ZET_CHEK[$fname].
_zet_read_reff() {
    emulate -L zsh
    unsetopt ERR_EXIT
    setopt extendedglob
    local reffile="$1"
    if [[ ! -f $reffile ]]; then
        zet_die "reffile \"$reffile\" not found"
    fi
    local line
    local lineno=0
    local fname rest0 rest1 rest2 rest3 comd chek want
    while IFS= read -r line; do
        (( ++lineno ))
        # Skip blank lines and comment lines
        [[ -z $line || $line == '#'* ]] && continue
        line=${line%%\#*} # handle #-comments within line
        # (trying to be faster with less =~ pattern matching)
        # Expected format: `fname :: comd ;; chek => want`
        fname=${${line%%::*}// /}  # what's before ::, / / killing all space
        rest0=${line#*::} # after ::  -->  rest0 = comd ;; chek => want
        # print "\nfname=|$fname|    rest0=|$rest0|"
        if (( ${#fname} == 0 || ${#rest0} == ${#line} )); then
            zet_die "line $lineno of ${reffile:A}:" \
                    "   $line" \
                    "missing \"::\" separating \`fname\` from \`rest\`" \
                    "in expected: \"fname :: comd ;; chek => want\""
        fi
        rest1=${${(S)rest0}//\{\}/$fname} # change `{}` --> $fname
        comd=${rest1%%;;*} # (all before ;;)
        rest2=${rest1#*;;} # (all after ;;) -->  rest2 = chek => want
        # print "comd=|$comd|   rest2=|$rest2|"
        if (( ${#comd} == 0 || ${#rest2} == ${#rest1} )); then
            zet_die "line $lineno of ${reffile:A}:" \
                    "   $line" \
                    "missing \";;\" separating \`comd\` from \`chek\`" \
                    "in expected: \"fname :: comd ;; chek => want\""
        fi
        chek=${rest2%%=>*} # (all before =>)
        rest3=${rest2#*=>} # (all after =>) -->  rest2 = want
        #print "\nchek=|$chek|   rest3=|$rest3|"
        if (( ${#chek} == 0 || ${#rest3} == ${#rest2} )); then
            zet_die "line $lineno of ${reffile:A}:" \
                    "   $line" \
                    "missing \"=>\" separating \`chek\` from \`want\`" \
                    "in expected: \"fname :: comd ;; chek => want\""
        fi
        want=${rest3##[ ]#}  # removing any leading spaces
        if (( ${#want} == 0 )); then
            zet_die "line $lineno of ${reffile:A}:" \
                    "   $line" \
                    "has empty \`want\`" \
                    "in expected: \"fname :: comd ;; chek => want\""
        fi
        # finally, assign recovered vals to global arrays
        if [[ -v "ZET_COMD[$fname]" ]]; then
            zet_die "Line $lineno of ${reffile:A} repeats \"$fname ::\""
        fi
        ZET_COMD[$fname]="$comd"
        ZET_CHEK[$fname]="$chek"
        ZET_WANT[$fname]="$want"
    done < "$reffile"
}

#__#__# _zet_init_reff
# Either starts writing $ZET_TSET[reff] if ZET_GEN_REF,
# else reads it with _zet_read_reff
_zet_init_reff() {
    if (( ! ${#ZET_TSET[@]} )); then
        zet_die "Not currently running any test set"
    fi
    local reffile=$ZET_TSET[reff]
    if [[ $ZET_TSET[mode] == "write" ]]; then
        local verb
        if [[ -f $reffile ]]; then
            verb="OVER-writing existing"
        else
            verb="writing"
        fi
        # given how we're called, we don't need ZET_PFX because we're actually
        # finishing a line that was started with "print -n" by the caller
        print "$verb reference file \"$reffile\" ..."
        rm -f "$reffile"
        touch "$reffile"
    else
        # not writing reffile: we're *reading* it
        print -n "reading existing reference file \"$reffile\" ... "
        _zet_read_reff "$reffile"
        print "done"
    fi
}

####### zet_begin [subname]
# Declares that we are starting a new test set,
# which will have full name `subpath/subname`
# where `subpath` is the path to caller script from here
# and `subname` is either given, else is the filename base of caller
zet_begin() {
    setopt localoptions no_warn_nested_var # we're here to modify globals
    if (( ${#ZET_TSET[@]} )); then
        zet_die "$ZET_PFX Seem to be already running test set \"${ZET_TSET[name]}\""
    fi
    # From what file are we being called
    local given subname caller_file caller_file_raw=${funcfiletrace[1]%:*}
    if (( ${+1} )); then
        given="$1"  # remember how zet_begin was called
        subname="$1"  # use given argument as subname
        # subname can't be crazy
        setopt localoptions extended_glob
        if [[ ! "$subname" == ([A-Za-z][A-Za-z0-9._-]##) ]]; then
            # `##` is zsh for "match the preceding pattern one or more times"
            zet_die "$ZET_PFX Given set subname=\"$subname\" looks unlikely"
        fi
    else
        given=""  # remember how zet_begin was called
        # default: use base filename (without directory and extension)
        subname=${${caller_file_raw:t}%.*}
    fi
    # This is a wildly convoluted way to get a relative path; via trial+error
    # with ChatGPT, which explained the challenge as "ensuring the path from
    # funcfiletrace[1] is absolute before stripping the library root, rather
    # than trusting :A on a raw relative path."   **Improvements welcome!**
    # Make caller_file absolute; combine with $PWD if relative
    if [[ "$caller_file_raw" == /* ]]; then
        caller_file="$caller_file_raw"
    else
        caller_file="$PWD/$caller_file_raw"
    fi
    # Finally, an absolute path to the caller
    caller_file=${caller_file:A}
    # Find `subpath` relative to ZET_ROOT
    local subpath=${caller_file#$ZET_ROOT/}  # strip ZET_ROOT prefix
    subpath=${subpath##/}                    # remove leading slash if any
    subpath=${subpath:h}                     # keep directory only
    # hard reset on all the global arrays
    _zet_arrays_reset
    # Initialize test set associative array
    ZET_TSET[name]=$subpath/$subname
    ZET_TSET[given]=$given
    ZET_TSET[reff]="${caller_file_raw%.*}-ref.txt"
    if (( ${+ZET_GEN_REF} )); then
        print -n "$ZET_PFX Variable ZET_GEN_REF is set ==> "
        # *Writing* reffile
        ZET_TSET[mode]="write"
        ZET_PASS=("(moot! writing reference file)")
        ZET_FAIL=("(moot! writing reference file)")
    else
        print -n "$ZET_PFX Variable ZET_GEN_REF *not* set ==> "
        ZET_TSET[mode]="read"
        # we will be recording test results in these:
        ZET_PASS=() # regular array
        ZET_FAIL=() # regular array
    fi
    _zet_init_reff # will start by finishing "print -n" line above
    print "$ZET_PFX $ZET_DLM"
    print -n "$ZET_PFX Running test set $ZET_TSET[name]: "
    # -n so that progress indication can continue same line
}

####### zet_run_comd comd fname
# run command `comd` that should create (write to) file `fname`
# but do so cautiously, and call junk on `fname`
_zet_run_comd() {
    local comd="$1" fname="$2"
    if [[ -f $fname ]]; then
        zet_die "problem with ${funcfiletrace[1]}:" \
                "File \"$fname\" already exists so will not run:" \
                "$comd"
    fi
    # junk file first (more reliably erases it upon interrupt)
    zet_junk $fname
    # run the command and lose stdout (but not stderr)
    # Explains ChatGPT:
    #    When you call zsh -c "$comd" inside a function, and that command fails
    #    while ERR_EXIT is set, the function aborts in a context where zsh does
    #    not run EXIT hooks.
    # hence the next line:
    setopt localoptions no_err_exit
    # now run the command
    zsh -c "$comd" -- > /dev/null
    # re-enable manually for the remainder
    setopt err_exit
    if [[ ! -f $fname ]]; then
        zet_die "problem with ${funcfiletrace[1]}:" \
                "File \"$fname\" not created by:" \
                "$comd"
    fi
}

##### zet_test fout comd fin chek
# Runs command `comd` with input(s) `fin` and output `fout`, then:
# If ZET_TSET[mode]=="write", runs `chek` (e.g. "cksum $fout") to get `want`
#    and then saves "fname :: comd ;; chek => want" line to reffile
# Else (read reffile to compare with it), compares result of $ZET_CHEK[$fout]
#    to $ZET_WANT[$fout] to see if test passes or fails, and records the
#    circumstances of a failure.
zet_test() {
    setopt localoptions no_warn_nested_var # we're here (in part) to modify globals
    if (( ! ${#ZET_TSET[@]} )); then
        zet_die "Not currently running any test set"
    fi
    local fout="$1" comd="$2" fin="$3" chek="$4" reffile=$ZET_TSET[reff]
    _zet_run_comd "$comd" "$fout"
    if [[ $ZET_TSET[mode] == "write" ]]; then
        # Ensure ouput checker $chek includes $fout, or end it with $fout if not
        if [[ $chek != *" $fout"* ]]; then
            chek="${chek} ${fout}"
        fi
        local want=$(eval $chek) # run the output checker
        local rest=$(_zet_enbrace "$comd ;; $chek => $want" "$fout")
        echo "$fout :: $rest" >> $reffile
        print -n "o"
    else
        if [[ -z ${ZET_CHEK[$fout]+_} ]]; then
            # $fout is not a key in ZET_CHECK
            zet_die "Testing $fout but no reference known (incomplete ref file?)"
        fi
        local rslt=$(eval $ZET_CHEK[$fout])
        local want=$ZET_WANT[$fout]
        if [[ "$rslt" = "$want" ]]; then
            # test passes!
            print -n "."
            ZET_PASS+=($fout)
        else
            # test fails!
            print -n "X"
            ZET_FAIL+=($fout)
            ZET_FAIL_COMD[$fout]="$comd"
            ZET_FAIL_RSLT[$fout]="$rslt"
            ZET_FAIL_WANT[$fout]="$want"
            # preserve the input to the failed command
            zet_unjunk $fin # HEY should make this handle multiple inputs
        fi
    fi
}

####### zet_end name
# Declares that we are done with test set `name`,
# summarizes all the failing tests
zet_end() {
    if (( ! ${#ZET_TSET[@]} )); then
        zet_die "Not currently running any test set"
    fi
    print "" # end line of progress indication
    # we use `given` to avoid replicating the heinous logic of setting $ZET_TSET[name]
    local given
    if (( ${+1} )); then
        given="$1"
    else
        given=""
    fi
    if [[ "${ZET_TSET[given]}" != "$given" ]]; then
        zet_die "Current test set \"${ZET_TSET[name]}\" inconsistent with arg \"$given\""
    fi
    print "$ZET_PFX Ending test set $ZET_TSET[name]"
    if [[ $ZET_TSET[mode] == "read" ]]; then
        print -n "$ZET_PFX $ZET_TSET[name] results: $#ZET_PASS passed, $#ZET_FAIL failed"
        if (( $#ZET_FAIL )); then
            #( # of elements in ZET_FAIL array )
            print ", as follows:" # finishing previous line
            local fout
            for fout in $ZET_FAIL; do
                print "\n$ZET_PFX $fout ::"
                print "${ZET_FAIL_COMD[$fout]} ;;"
                print "$ZET_CHEK[$fout] =>"
                print "   gave: ${ZET_FAIL_RSLT[$fout]}"
                print "!= want: ${ZET_FAIL_WANT[$fout]}"
            done
        else
            print "." # still have to finish previous line
        fi
    fi
    print "$ZET_PFX $ZET_DLM"
    _zet_arrays_reset
}

# TODO:
# lintish ideas:
# - in important helper functions:
#       emulate -L zsh -o pipefail -o no_unset  # explicitly list options
# - always add a type with local (local -a array; local -i count; local -F value)
# - use typeset to create all globals, not just the arrays
# Make some way of automatically generating the input file Ckc array,
# (to speed creation of new test sets), and apply that to nrrd/heq.zsh
# pre-req to parallel: add zet_file to declare that this testset writes this file
#   and have a variable ZET_FILE_LOG that writes out all these files,
#   to (one-time) detect collisions that would confuse parallel runs
# allow tests to run in parallel
# set a key-value pair that documents how the output nrrd was created
# create a system of naming and reading architecture-specific (arm vs x86) reffiles
# enable running specific tests instead of all of them
# Figure out top-level framework for running w/ recompiling unu with UBSAN
#   or running with valgrind
#   ---> how to report errors? from ubsan or valgrind, but not from outputs being wrong
#                              or from test command crashing out
#  e.g. we should have a way of testing that unu commands fail when expected
