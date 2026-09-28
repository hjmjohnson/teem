#!/usr/bin/env zsh

# (begin zet test set boilerplate)
cd ${0:A:h} # get to directory of this script
#   $0 path of me
#     :A made absolute
#       :h head = drop my filename to get path
source ../zet-lib.zsh # set up the zet "library"
setopt err_exit       # exit on error
setopt pipe_fail      # propagate pipeline errors
# (end zet test set boilerplate)

Setup() {
    # different scanlines of a 4x3 little array
    local line0="0  1  2  3"
    local line1="10 11 12 13"
    local line2="44 33 22 11"
    local outf
    # using .ptxt (for plain text) so that "rm -f *.nhdr *.ptxt" works to
    # do cleanup without clobbering the -ref.txt and -ich.txt files
    zet_inf_begin "%s.ptxt" ######## BEGIN input file creation
    zet_inf vals outf && echo -e "$line0\n$line1\n$line2" >$outf
    zet_inf line0 outf && echo $line0 >$outf
    zet_inf line1 outf && echo $line1 >$outf
    zet_inf line2 outf && echo $line2 >$outf
    zet_inf_end "%s.ptxt" ######## END input file creation
}
Setup

zet_begin ####### BEGIN testing

# to silence the "formatNRRD_read: reading <encoding> data ... done" diagnostics
export NRRD_DEFAULT_IO_VERBOSE=0

Test() {
    # chek is how we check a single output; here we are doing a round-trip comparison with ourself
    local chek="unu save -i {} -f nrrd | unu diff - {}"
    local bargs="-e ascii -t uint -s 4 3" # basic args always needed
    # data file: (single)      LIST                               printf
    #               1           2                                  3
    local inI inA=("vals.ptxt" "line0.ptxt line1.ptxt line2.ptxt" "line%d.ptxt 0 2 1")
    for inI in 1 2 3; do
        local fin=${inA[$inI]}
        # (keys and values with spaces are supported in new unu (with its re-written hest),
        # but we abstain for the sake of comparison to old unu)
        #               1  2
        local kvI kvA=("" "-kv 'akey:=avalue' bkey:=bvalue")
        for kvI in 1 2; do
            # (same as above: we abstain from comments with spaces for comparison to old unu)
            #               1  2
            local cmI cmA=("" "-cm this-is-the-first-comment and-this-is-a-2nd-comment")
            for cmI in 1 2; do
                # output file: detached (1) or self-contained (2)
                #                  1    2         1      2
                local ooI dashhA=("-h" "") sfxA=("nhdr" "nrrd")
                local fnbase=in$((inI - 1))-kv$((kvI - 1))-cm$((cmI - 1))
                for ooI in 1 2; do
                    if ((${+ZET_OMIT_BUGGY})); then
                        # with detached header (ooI == 1), with datafile LIST, with either KVPs or comments:
                        # prior to commit 237b202d, KVPs or comments could follow and thereby corrupt
                        # the LIST of data files
                        if ((ooI == 1 && inI == 2 && (kvI == 2 || cmI == 2))); then
                            # (have to manually match the ommitted tests/reviws)
                            zet_test_omit
                            zet_review_omit
                            zet_review_omit
                            continue # to next iter of "for ooI" to actually omit test + reviews below
                        fi
                    fi
                    local fout=$fnbase.$sfxA[$ooI]
                    zet_test $fout "unu make -i $fin $bargs $kvA[$kvI] $cmA[$cmI] $dashhA[$ooI] -o $fout" "$fin" "$chek"
                    # also do a cksum on file as a whole (this is not really purpose of a "review",
                    # but we don't otherwise have a way of running multiple checks on one output file)
                    zet_review cksum-$fout "cksum $fout" $fout
                    # there should be no difference in values between original vals.ptxt and $fout
                    # (The final "-" arg for unu minmax is there for the older unu that need it)
                    zet_review vals-$fout "unu 2op - vals.ptxt $fout | unu minmax -sl -" "vals.ptxt $fout"
                done
                # (this is the context that motivated creating zet_review)
                # there should be no difference between Nrrd read from .nhdr vs .nrrd
                if [[ -f $fnbase.nhdr && -f $fnbase.nrrd ]]; then
                    zet_review diff-$fnbase "unu diff $fnbase.nhdr $fnbase.nrrd" "$fnbase.nhdr $fnbase.nrrd"
                else
                    # ZET_OMIT_BUGGY is only reason for these two files do not exist
                    zet_review_omit
                fi
            done
        done
    done
}
Test

zet_end ####### END testing

exit 0
