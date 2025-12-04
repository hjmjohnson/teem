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

####### TeemZT = TZT = Teem Zsh-based Testing
#
# A way to do shell-script-based testing of Teem tools
# that have command-line interfaces

setopt NO_UNSET           # error on unbound variables
setopt WARN_CREATE_GLOBAL # warn if a function leaks variables

typeset -ga TZT_JUNK  # junk and cleanup functions for tmp files
tzt_junk() { TZT_JUNK+=("$@"); }
tzt_unjunk() {
    # For each argument, remove matching entries from the array
    local f
    for f in "$@"; do
        TZT_JUNK=(${TZT_JUNK:#${~f}})
    done
}
tzt_cleanup() {
    [[ ${#TZT_JUNK[@]} -gt 0 ]] && rm -rf -- "${TZT_JUNK[@]}"
}
trap tzt_cleanup EXIT INT TERM
#autoload -Uz add-zsh-hook
#add-zsh-hook zshexit tzt_cleanup

tzt_die() { print -u2 "$@"; exit 1 }

# tzt_assert_out want cmd: runs command `cmd`, asserts that its output is same as `want`
tzt_assert_out() {
    #emulate -L zsh   # create a local, clean shell environment
    #unsetopt XTRACE  # disable verbose in this local scope
    local cmd want got
    want="$1"
    shift
    cmd=("$@")       # store the command and all its arguments in an array
    got="$("${cmd[@]}")"  # run the command safely and capture stdout
    if [[ "$got" != "$want" ]]; then
        print -u2 "$funcstack[1]: problem at ${funcfiletrace[1]}"
        print -u2 "\"$cmd\" supposed to say: $want"
        print -u2 "but actually got: $got\n"
        exit 1
    fi
}

# tzt_assert_cksum want fname: runs (effectively) "cat fname | cksum", asserts that its output is same as `want`
tzt_assert_cksum() {
    #emulate -L zsh   # create a local, clean shell environment
    #unsetopt XTRACE  # disable verbose in this local scope
    local want="$1" fname="$2"
    local got="$(cksum $fname)" # actually 3 words, ending with filename
    got=${${=got}[1,2]} # get first two words: cksum and size
    if [[ "$want" != "$got" ]]; then
        print -u2 "$funcstack[1]: problem at ${funcfiletrace[1]}"
        print -u2 "\"cat $fname | cksum\" supposed to say: $want"
        print -u2 "but actually got: $got\n"
        exit 1
    fi
}

# tzt_run_cmd $Cmd $Fname: run command $Cmd that should write file $Fname
# but do so cautiously, and call junk on $Fname
tzt_run_cmd() {
    local cmd="$1" fname="$2"
    if [[ -f $fname ]]; then
        print -u2 "$funcstack[1]: problem with ${funcfiletrace[1]}:"
        print -u2 "File \"$fname\" already exists so will not run: $cmd"
        exit 1
    fi
    # run the command but lose stdout and stderr
    # echo "$funcstack[1]: $cmd"
    eval $cmd >& /dev/null
    if [[ ! -f $fname ]]; then
        print -u2 "$funcstack[1]: problem with ${funcfiletrace[1]}:"
        print -u2 "File \"$fname\" not created by: $cmd"
        exit 1
    fi
    tzt_junk $fname
}

# tzt_unspace str: Collapse internal multiple spaces in `str` to one,
#   as well as entirely removing leading and trailing spaces
tzt_unspace() {
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

# tzt_enbrace Rest Fname: replace occurances of $Fname in $Rest with `{}`
tzt_enbrace() {
    emulate -L zsh
    setopt NO_BRACE_EXPAND
    local Rest="$1" Fname="$2"
    # that } but not { needs \ escaping is mysterious
    Rest="${Rest//$Fname/{\}}"
    print -n "$Rest"
}

# tzt_init_ICW: sets up (global) maps ("associative array") INFO, CHEK, and WANT:
# INFO[`fname`] = info about how `fname` was generated
#    e.g. a command ending with "-o `fname`". NOTE: this is for documentation
#    and context rather than for actually re-generating fname. The test
#    script has to otherwise know exactly how to generate `fname`.
# CHEK[`fname`] = command(s) to run to asses something
#    about `fname` in order to see if it looks like the expected output.
#    Like in `find`, the token "{}" stands in for the filename. If "{}" is not
#    used, then it is assumed to be at the end of the command.
#    E.g. If CHEK[`fname`] is "cksum" that is the same as "cksum {}",
#    which should be expanded to "cksum `fname`" when saved to CHEK.
# WANT[`fname`] = expected output of CHEK[`fname`]
# init_ICW assumes variable $RefFile is filename of reference data.
#    Each line should be formatted as:
#       fname :: info ;; chek => want
#    The `::`, `;;`, and `=>` delimit the `fname`, `info`, `check`, and `want`
#    fields described above; with or without any white-space between them
tzt_init_ICW() {
    emulate -L zsh
    unsetopt WARN_CREATE_GLOBAL
    unsetopt ERR_EXIT
    setopt extendedglob
    if [[ -z ${RefFile:-} ]]; then
        print -u2 "$funcstack[1]: RefFile variable is empty or not set"
        exit 1
    fi
    if [[ ! -f $RefFile ]]; then
        print -u2 "$funcstack[1]: RefFile file \"$RefFile\" not found"
        exit 1
    fi
    # print -u2 "$funcstack[1]: reading from $RefFile"
    typeset -g -A INFO CHEK WANT
    local line lineno=0
    local info fname check want
    while IFS= read -r line; do
        (( lineno++ ))
        # print -u2 "$funcstack[1]: read line $lineno: |$line|"
        # Skip blank lines and comment lines
        [[ -z $line || $line == '#'* ]] && continue
        # Expected format: `fname :: info ;; chek => want`
        # Split off fname :: rest
        if [[ $line =~ '^(.*[^[:space:]])[[:space:]]*::[[:space:]]*(.*)$' ]]; then
            #                fname                   ::            rest
            fname=${match[1]}
            rest=${match[2]}
        else
            print -u2 "$funcstack[1]: malformed line $lineno of $RefFile (missing `::`)"
            print -u2 "    $line"
            exit 1
        fi
        # Replace {} with filename in the rest of the line
        rest=${${(S)rest}//\{\}/$fname}
        # Parse `rest` as `info ;; chek => want`
        if [[ $rest =~ '^(.*)[[:space:]]*;;[[:space:]]*(.*)[[:space:]]*=>[[:space:]]*(.*)$' ]]; then
            #            info            ;;            chek            =>            want
            info=$(tzt_unspace "${match[1]}")
            chek=$(tzt_unspace "${match[2]}")
            want=$(tzt_unspace "${match[3]}")
        else
            print -u2 "$funcstack[1]: malformed line $lineno of $RefFile (need: `info ;; chek => want`)"
            print -u2 "    $line"
            exit 1
        fi
        # Ensure $chek includes fname, or end with fname if not
        if [[ $chek != *" $fname"* ]]; then
            chek="${chek} ${fname}"
        fi
        # Assign to global arrays
        INFO[$fname]="$info"
        CHEK[$fname]="$chek"
        WANT[$fname]="$want"
    done < "$RefFile"
}
