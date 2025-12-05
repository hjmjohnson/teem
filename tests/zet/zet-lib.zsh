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

###### Zet = Zsh-Enabled Testing
#
# New/minimal/experimental zsh-based machinery to test command-line tools
#
# Written by GLK for TeemV2 with recognition that shell scripts will be much
# faster to whip up than CTest programs. This was also an excuse to learn more
# about zsh-specific scripting. Many technical zsh details were learned thanks
# to ChatGPT, but the structure of files and functions here is all hand-crafted.
#
# No relation to all the other software already named zet:
# http://zaytsev.com/
# https://github.com/yarrow/zet
# https://www.zoho.com/developer/help/extensions/zappscli.html
# https://www.zetuniverse.com/
# https://zetsoftware.com/
#
# useful zsh resources:
# https://zsh.sourceforge.io/Doc/zsh_us.pdf
# https://gist.github.com/ChristopherA/562c2e62d01cf60458c5fa87df046fbd

setopt NO_UNSET           # error on unbound variables
setopt WARN_CREATE_GLOBAL # warn if a function leaks variables

########################################################################
##
##  Top-level global variables
##

###### ZET_ROOT: absolute directory containing this Zet library file
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

###### ZET_PFX: way of marking lines originating from these functions
ZET_PFX="(zet):"
print "$ZET_PFX Starting with ZET_ROOT=$ZET_ROOT"

###### ZET_DLM and ZET_DLM_LINE: visually distinctive things to delimit output
ZET_DLM_LINE="≡≡==≡≡≡-≡≡=≡≡==-≡≡=≡=≡≡-=≡≡≡≡==-=≡≡==≡≡-≡≡≡≡=≡=-≡≡==≡=≡-≡≡≡=≡=="
ZET_DLM="≡≡≡≡=≡=-≡≡==≡=≡-≡≡≡=≡=="

##### ZET_TSET: top-level description of current test set
# (only one tset at a time can be actively running)
# used with zet_{begin,test,end} functions below
typeset -g -A ZET_TSET  # -A : associative array
# (Ideally all the following arrays could also hang off of ZET_TSET
#  e.g. ZET_TSET[info] instead of ZET_INFO, but sadly zsh cannot nest
#  associative arrays ...)
##### ZET_INFO, ZET_CHEK ZET_WANT: maps from test/file names to more details
# see _zet_read_reff description
typeset -g -A ZET_INFO
typeset -g -A ZET_CHEK
typeset -g -A ZET_WANT
##### ZET_PASS, ZET_FAIL: arrays of passing/failing test/file names
typeset -g -a ZET_PASS # -a: regular array
if (( ${+ZET_FAIL} )); then unset ZET_FAIL; fi
typeset -g -a ZET_FAIL
##### ZET_FAIL_CMD, ZET_FAIL_GOT, ZET_FAIL_WANT
# maps from test that failed to command `cmd` to re-run,
# what it produces `got` versus what it should have produce `want`
typeset -g -A ZET_FAIL_CMD
typeset -g -A ZET_FAIL_GOT
typeset -g -A ZET_FAIL_WANT
# (learned: with one character difference `typeset -g A ZET_FAIL_CMD`
#  you actually get a regular array, and then indexing like
#  `ZET_FAIL_CMD[$fname]` (for e.g. fname=hght-d13-th0.5-b0.nrrd)
#  will produce cryptic errors like: `d13: parameter not set`
#  because zsh is dutifully trying to interpret $fname as arithmetic
#  as it (sensibly) always done for regular arrays)

#__#__# _zet_arrays_reset
# a helper function to (re)initialize these global arrays. Despite the
# uniform syntax, this preserves the type (regular vs associative) of the array
_zet_arrays_reset() {
    ZET_TSET=()
    ZET_INFO=()
    ZET_CHEK=()
    ZET_WANT=()
    ZET_PASS=()
    ZET_FAIL=()
    ZET_FAIL_CMD=()
    ZET_FAIL_GOT=()
    ZET_FAIL_WANT=()
}

########################################################################
##
##  Functions and state for remembering transient ("junk") files,
##  what to do when finishing, and ways of finishing
##

typeset -ga ZET_JUNK
###### zet_junk file1 file2 ...
# says that these are transient files, to be cleaned up later
zet_junk() {
    ZET_JUNK+=("$@");
}
###### zet_unjunk file1 file2 ...
# says that (oops) actually these files should stick around
zet_unjunk() {
    # For each argument, remove matching entries from the array
    local f
    for f in "$@"; do
        ZET_JUNK=(${ZET_JUNK:#${~f}})
    done
}
# keep track of where we are, for display during zet_cleanup
ZET_CMD=""
ZET_TRACE=""
zet_debug() {
    local cmd=${${(z)ZSH_DEBUG_CMD}[1]}
    #             (z) shell-like word-splitting of ZSH_DEBUG_CMD
    #           ${  ... array ... }[1]  1st element
    #         ${ more wrapping braces } because contents contain {}
    if [[ $(whence -w -- "$cmd" 2>/dev/null) == *builtin* ]]; then
        # built-in commands aren't worth remembering
        ZET_CMD=""
        ZET_TRACE=""
    else
        ZET_CMD="$ZSH_DEBUG_CMD"
        ZET_TRACE="$funcfiletrace[1]"
    fi
}
trap zet_debug DEBUG
zet_cleanup() {
    trap '' DEBUG
    if [[ -n $ZET_CMD ]]; then
        local fileline=("${(@s/:/)ZET_TRACE}")
        print -u2 "\n$ZET_PFX $fileline[1] line $fileline[2] error running: $ZET_CMD"
        ZET_CMD=""
    fi
    # we're done if have no junk
    (( ${#ZET_JUNK[@]} == 0 )) && return 0
    # else remove it
    rm -rf -- "${ZET_JUNK[@]}"
    # clear junk list in case of repeated calls
    ZET_JUNK=()
}
# Ensure trap works for ordinary exits and signals
trap zet_cleanup EXIT INT TERM
# Ensure cleanup even when script is sourced or run in a subshell
autoload -Uz add-zsh-hook
add-zsh-hook zshexit zet_cleanup
###### zet_die line1 line2 ...
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
    print -u2 "$ZET_PFX $where"
    local line
    for line in "$@"; do
        print -u2 "$line"
    done
    exit 1
}
###### zet_stop
# way to stop a script while preventing any junk cleanup
# (for debugging of any transient files)
zet_stop() {
    ZET_JUNK=()
    exit 0
}


########################################################################
##
##  String utility functions
##

###### zet_unspace str
# Collapse internal multiple spaces in `str` to single space,
# and entirely remove leading and trailing spaces
zet_unspace() {
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

###### zet_enbrace rest fname
# replace occurances of `fname` in `rest` with `{}`
zet_enbrace() {
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

###### zet_assert_cksum want fname
# runs (effectively) "cat fname | cksum", asserts that its output is same as `want`
zet_assert_cksum() {
    local want="$1" fname="$2"
    local got="$(cksum $fname)" # actually 3 words, ending with filename
    got=${${=got}[1,2]} # get first two words: cksum and size
    if [[ "$got" != "$want" ]]; then
        zet_die "problem at ${funcfiletrace[1]}" \
                "\"cat $fname | cksum\" supposed to say: $want" \
                "but actually got: $got\n"
    fi
    # else got == want; continue
}

###### zet_assert_out want cmd
# runs command `cmd`, asserts that its output is same as `want`
# (HEY this is currently unused? but seems useful)
zet_assert_out() {
    local want cmd got
    want="$1"
    shift
    cmd=("$@")       # store the command and all its arguments in an array
    got="$("${cmd[@]}")"  # run the command safely and capture stdout
    if [[ "$got" != "$want" ]]; then
        zet_die "problem at ${funcfiletrace[1]}" \
                "\"$cmd\" supposed to say: $want" \
                "but actually got: $got"
    fi
    # else got == want; continue
}

########################################################################
##
##  Functions for setting up and running a test set
##  and the various test commands therein
##

#__#__# _zet_read_reff reffile
# Fills in associative arrays `ZET_INFO`, `ZET_CHEK`, `ZET_WANT`
# from reference data in file `reffile`, each line of which is formatted as:
#       fname :: info ;; chek => want
# The `::`, `;;`, and `=>` delimit the `fname`, `info`, `chek`, and `want`
# fields (with or without any white-space between them).
# * `fname` is a file generated by a test; effectively this is the test name.
# * `info` is info about how `fname` was generated, often a command ending
#   with "-o $fname". NOTE: this is for documentation and context rather
#   than for actually re-generating `fname`. The calling test script has to
#   already know from context exactly how to generate `fname`.
#   `info` is saved in `ZET_INFO["$fname"]`
# * `chek` is the actual command(s) to run to asses something about `fname`,
#   to see if it looks like the expected output (does it match `want`).
#   `chek` is saved to `ZET_CHEK["$fname"]`, but if $chek does not include $fname
#   then $fname is added as a final word before saving to `ZET_CHEK["$fname"]`.
# * `want` is the expected output of `chek`
# NOTE: Within `info`, `chek`, and `want`, just like with unix `find`,
# the token "{}" works as an abbreviation for the filename `fname`.
# Thus, if $chek is "cksum {}" that is expanded to "cksum $fname" prior to
# saving in to ZET_CHEK["$fname"]. Also, if $chek is just "cksum" that is also
# expanded to "cksum $fname" because of the rule above.
_zet_read_reff() {
    emulate -L zsh
    local reffile="$1"
    unsetopt ERR_EXIT
    setopt extendedglob
    if [[ ! -f $reffile ]]; then
        zet_die "reffile \"$reffile\" not found"
    fi
    local line lineno=0
    local fname rest info chek want
    while IFS= read -r line; do
        (( lineno++ ))
        # Skip blank lines and comment lines
        [[ -z $line || $line == '#'* ]] && continue
        # handle #-comments within line
        line=${line%%\#*}
        # Expected format: `fname :: info ;; chek => want`
        # Split off fname :: rest
        if [[ $line =~ '^(.*[^[:space:]])[[:space:]]*::[[:space:]]*(.*)$' ]]; then
            #                fname                   ::            rest
            fname=${match[1]}
            rest=${match[2]}
        else
            zet_die "Missing \"::\" on line $lineno of ${reffile:A}:" \
                    "   $line"
        fi
        # Replace {} with filename in the rest of the line
        rest=${${(S)rest}//\{\}/$fname}
        # Parse `rest` as `info ;; chek => want`
        if [[ $rest =~ '^(.*)[[:space:]]*;;[[:space:]]*(.*)[[:space:]]*=>[[:space:]]*(.*)$' ]]; then
            #            info            ;;            chek            =>            want
            info=$(zet_unspace "${match[1]}")
            chek=$(zet_unspace "${match[2]}")
            want=$(zet_unspace "${match[3]}")
            if [[ -z $info || -z $chek || -z $want ]]; then
                zet_die "line $lineno of ${reffile:A}:" \
                        "   $line" \
                        "set one of these to an empty string:" \
                        "  info=\"$info\"" \
                        "  chek=\"$chek\"" \
                        "  want=\"$want\""
            fi
        else
            zet_die "malformed line $lineno of ${reffile:A}:" \
                    "   $line" \
                    "need: \"fname :: info ;; chek => want\""
            exit 1
        fi
        # Ensure $chek includes fname, or end with fname if not
        if [[ $chek != *" $fname"* ]]; then
            chek="${chek} ${fname}"
        fi
        # Assign to global arrays
        ZET_INFO["$fname"]="$info"
        ZET_CHEK["$fname"]="$chek"
        ZET_WANT["$fname"]="$want"
    done < "$reffile"
}

#__#__# _zet_init_reff reffile
# Either starts writing `reffile` if ZET_WRITE_REF,
# else reads it with zet_read_ICW
_zet_init_reff() {
    if (( ! ${#ZET_TSET[@]} )); then
        zet_die "Not currently running any test set"
    fi
    local reffile="$1"
    if [[ $ZET_TSET[mode] == "write" ]]; then
        local verb
        if [[ -f $reffile ]]; then
            verb="OVER-writing existing"
        else
            verb="Writing"
        fi
        # given how we're called, we don't need ZET_PFX
        print "$verb file $reffile ..."
        rm -f "$reffile"
        touch "$reffile"
    else
        # not writing reffile: we're *reading* it
        print -n "reading existing file=$reffile ... "
        _zet_read_reff "$reffile"
        print "done"
    fi
}

###### zet_begin [subname]
# Declares that we are starting a new test set,
# which will have full name `path/subname`
# where `path` is the path to caller script from here
# and `subname` is either given, else is the filename base of caller
zet_begin() {
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
    # Find `path` relative to ZET_ROOT
    local path=${caller_file#$ZET_ROOT/}  # strip ZET_ROOT prefix
    path=${path##/}                       # remove leading slash if any
    path=${path:h}                        # keep directory only
    # hard reset on all the global arrays
    _zet_arrays_reset
    # Initialize test set associative array
    ZET_TSET[name]=$path/$subname
    ZET_TSET[given]=$given
    ZET_TSET[reff]="${caller_file_raw%.*}-ref.txt"
    if (( ${+ZET_WRITE_REF} )); then
        # *Writing* reffile
        print "$ZET_PFX ZET_WRITE_REF set ==> will write reference file"
        ZET_TSET[mode]="write"
        ZET_PASS=("(moot; writing reference file)")
        ZET_FAIL=("(moot; writing reference file)")
    else
        print -n "$ZET_PFX ZET_WRITE_REF not set ==> "
        ZET_TSET[mode]="read"
        # we will be recording test results in these:
        ZET_PASS=() # regular array
        ZET_FAIL=() # regular array
        _zet_init_reff "$ZET_TSET[reff]"
    fi
    print "$ZET_PFX $ZET_DLM_LINE"
    print -n "$ZET_PFX Running test set $ZET_TSET[name]: "
    # -n so that progress indication can continue same line
}

###### zet_run_cmd cmd fname
# run command `cmd` that should create (write to) file `fname`
# but do so cautiously, and call junk on `fname`
_zet_run_cmd() {
    local cmd="$1" fname="$2"
    if [[ -f $fname ]]; then
        zet_die "problem with ${funcfiletrace[1]}:" \
                "File \"$fname\" already exists so will not run:" \
                "$cmd"
    fi
    # junk file first (more reliably erases it upon interrupt)
    zet_junk $fname
    # run the command and lose stdout (but not stderr)
    # Explains ChatGPT:
    #    When you call zsh -c "$cmd" inside a function, and that command fails
    #    while ERR_EXIT is set, the function aborts in a context where zsh does
    #    not run EXIT hooks.
    # hence the next line:
    setopt localoptions no_err_exit
    # now run the command
    zsh -c "$cmd" -- > /dev/null
    # re-enable manually for the remainder
    setopt err_exit
    if [[ ! -f $fname ]]; then
        zet_die "problem with ${funcfiletrace[1]}:" \
                "File \"$fname\" not created by:" \
                "$cmd"
    fi
}

zet_test() {
    if (( ! ${#ZET_TSET[@]} )); then
        zet_die "Not currently running any test set"
    fi
    local fout="$1" cmd="$2" fin="$3" reffile=$ZET_TSET[reff]
    _zet_run_cmd "$cmd" "$fout"
    if [[ $ZET_TSET[mode] == "write" ]]; then
        # *Writing* reffile (not reading it)
        # format=     fname :: info ;; chek => want
        rest=$(zet_enbrace "$cmd ;; cksum => $(cksum $fout)" "$fout")
        echo "$fout :: $rest" >> $reffile
        print -n "o"
    else
        # not writing reffile: we (already) *read* it in
        # remember: unquoted associative-array subscripts are parsed in arithmetic context
        local got=$(eval $ZET_CHEK["$fout"])
        local want=$ZET_WANT["$fout"]
        if [[ "$got" = "$want" ]]; then
            # test passes!
            print -n "."
            ZET_PASS+=($fout)
        else
            # test fails!
            print -n "X"
            ZET_FAIL+=($fout)
            ZET_FAIL_CMD[$fout]="$cmd"
            ZET_FAIL_GOT[$fout]="$got"
            ZET_FAIL_WANT[$fout]="$want"
            # preserve the input to the failed command
            zet_unjunk $fin # HEY should make this handle multiple inputs
        fi
    fi
}

###### zet_end name
# Declares that we are done with test set `name`
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
    print -n "$ZET_PFX $ZET_TSET[name] results: $#ZET_PASS passed, $#ZET_FAIL failed"
    if (( $#ZET_FAIL )); then
        print ", as follows:" # finishing previous line
        local fout
        for fout in $ZET_FAIL; do
            print "\n$ZET_PFX $fout ::"
            print "${ZET_FAIL_CMD[$fout]} ;;"
            print "$ZET_CHEK["$fout"] =>"
            print "   gave: ${ZET_FAIL_GOT[$fout]}"
            print "!= want: ${ZET_FAIL_WANT[$fout]}"
        done
    else
        print "." # still have to finish previous line
    fi
    print "$ZET_PFX $ZET_DLM_LINE"
    _zet_arrays_reset
}

# TODO:
# when reading reffile; make sure we don't duplicate keys
# enable running specific tests instead of all of them
# Figure out how to run with recompiling unu with UBSAN or running with valgrind
#   ---> how to report errors? from ubsan or valgrind, but not from outputs being wrong
#                              or from test command crashing out
