#!/usr/bin/env zsh
# This file was first written by an LLM so it is not copyrightable; subsequent human
# tweaks do not change that. Link with source, assuming GLK's account:
# https://claude.ai/chat/feff60c4-53f6-461e-bf2b-f0840dace180

# zet-run.zsh: run all zet test scripts found under subdirectories

setopt pipe_fail
# nb: deliberately no err_exit; individual script failures must not stop the run

typeset ZRR=${0:A:h} # ZRR: Zet Runner Root
typeset -a zr_pass=()
typeset -a zr_fail=()

for script in ${ZRR}/*/*.zsh(N); do
    print "\n(zet-run): ${script#$ZRR/}"
    if zsh $script; then
        zr_pass+=($script)
    else
        zr_fail+=($script)
    fi
done

print "\n(zet-run): ${#zr_pass} script(s) passed, ${#zr_fail} failed"
if (($#zr_fail)); then
    for s in $zr_fail; do
        print "(zet-run): FAILED: ${s#$ZRR/}"
    done
    exit 1
fi
exit 0
