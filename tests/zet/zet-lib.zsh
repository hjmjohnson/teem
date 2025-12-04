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
# New/minimal/experimental zsh-based machinery to test command-line tools

setopt NO_UNSET           # error on unbound variables
setopt WARN_CREATE_GLOBAL # warn if a function leaks variables

# junk and cleanup functions for any transient files
typeset -ga ZET_JUNK
zet_junk() {
    ZET_JUNK+=("$@");
}
zet_unjunk() {
    # For each argument, remove matching entries from the array
    local f
    for f in "$@"; do
        ZET_JUNK=(${ZET_JUNK:#${~f}})
    done
}
zet_cleanup() {
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

zet_die() {
    print -u2 "ERROR in ${funcstack[2]}() called from ${funcfiletrace[2]}:"
    local line
    for line in "$@"; do
        print -u2 "$line"
    done
    exit 1
}

#### zet_assert_out want cmd
# runs command `cmd`, asserts that its output is same as `want`
zet_assert_out() {
    #emulate -L zsh   # create a local, clean shell environment
    #unsetopt XTRACE  # disable verbose in this local scope
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

#### zet_assert_cksum want fname
# runs (effectively) "cat fname | cksum", asserts that its output is same as `want`
zet_assert_cksum() {
    #emulate -L zsh   # create a local, clean shell environment
    #unsetopt XTRACE  # disable verbose in this local scope
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

#### zet_run_cmd cmd fname
# run command `cmd` that should create (write to) file `fname`
# but do so cautiously, and call junk on `fname`
zet_run_cmd() {
    local cmd="$1" fname="$2"
    if [[ -f $fname ]]; then
        zet_die "problem with ${funcfiletrace[1]}:" \
                "File \"$fname\" already exists so will not run:" \
                "$cmd"
    fi
    # junk file first (more reliably erases it upon interrupt)
    zet_junk $fname
    # run the command but lose stdout and stderr
    # echo "$funcstack[1]: $cmd"
    eval $cmd >& /dev/null
    if [[ ! -f $fname ]]; then
        zet_die "problem with ${funcfiletrace[1]}:" \
                "File \"$fname\" not created by:" \
                "$cmd"
    fi
}

#### zet_unspace str
# Collapse internal multiple spaces in `str` to single space,
# and entirely remove leading and trailing spaces
zet_unspace() {
    emulate -L zsh
    unsetopt WARN_CREATE_GLOBAL
    unsetopt ERR_EXIT
    setopt extendedglob  # ensure extended globbing for pattern matching
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

#### zet_enbrace rest fname
# replace occurances of `fname` in `rest` with `{}`
zet_enbrace() {
    emulate -L zsh
    setopt NO_BRACE_EXPAND
    local Rest="$1" Fname="$2"
    # that } but not { needs \ escaping is mysterious
    Rest="${Rest//$Fname/{\}}"
    print -n "$Rest"
}

#### zet_init_ICW reffile
# Sets up (global) maps (associative arrays) `ZET_INFO`, `ZET_CHEK`, `ZET_WANT`
# from reference data in file `reffile`, each line of which is formatted as:
#       fname :: info ;; chek => want
# The `::`, `;;`, and `=>` delimit the `fname`, `info`, `check`, and `want`
# fields (with or without any white-space between them).
# * `fname` is a file generated by a test; effectively this is the test name.
# * `info` is info about how `fname` was generated, often a command ending
#   with "-o $fname". NOTE: this is for documentation and context rather
#   than for actually re-generating `fname`. The calling test script has to
#   already know from context exactly how to generate `fname`.
#   `info` is saved in `ZET_INFO[$fname]`
# * `chek` is the actual command(s) to run to asses something about `fname`,
#   to see if it looks like the expected output (does it match `want`).
#   `chek` is saved to `ZET_CHEK[$fname]`, but if $chek does not include $fname
#   then $fname is added as a final word before saving to `ZET_CHEK[$fname]`.
# * `want` is the expected output of `chek`
# NOTE: Within `info`, `chek`, and `want`, just like with unix `find`,
# the token "{}" works as an abbreviation for the filename `fname`.
# Thus, if $chek is "cksum {}" that is expanded to "cksum $fname" prior to
# saving in to ZET_CHEK[$fname]. Also, if $chek is just "cksum" that is also
# expanded to "cksum $fname" because of the rule above.
zet_init_ICW() {
    emulate -L zsh
    local reffile="$1"
    unsetopt WARN_CREATE_GLOBAL
    unsetopt ERR_EXIT
    setopt extendedglob
    if [[ ! -f $reffile ]]; then
        zet_die "reffile \"$reffile\" not found"
    fi
    # print -u2 "$funcstack[1]: reading from $reffile"
    typeset -g -A ZET_INFO ZET_CHEK ZET_WANT
    local line lineno=0
    local info fname check want
    while IFS= read -r line; do
        (( lineno++ ))
        # print -u2 "$funcstack[1]: read line $lineno: |$line|"
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
        ZET_INFO[$fname]="$info"
        ZET_CHEK[$fname]="$chek"
        ZET_WANT[$fname]="$want"
    done < "$reffile"
}
