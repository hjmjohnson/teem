#!/usr/bin/env zsh

cd ${0:A:h} # get to directory of this script
#   $0 path of me
#     :A made absolute
#       :h head = drop my filename to get path
source ../zet-lib.zsh # set up the zet "library"
setopt err_exit       # exit on error
setopt pipe_fail      # propagate pipeline errors
if (($+commands[unu])); then
    print "${0} using unu = $(which unu)\n"
else
    zet_die "unu not found, bye"
fi

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

Test() {
    # chek is how we check a single output; here we are doing a round-trip comparison with ourself
    local chek="unu save -i {} -f nrrd | unu diff - {}"
    local bargs="-e ascii -t uint -s 4 3" # basic args always needed
    # data file: (single)      LIST                               printf
    #               1           2                                  3
    local inI inA=("vals.ptxt" "line0.ptxt line1.ptxt line2.ptxt" "line%d.ptxt 0 2 1")
    for inI in 1 2 3; do
        local fin=${inA[$inI]}
        #               1  2
        local kvI kvA=("" "-kv 'a key:=a value' bkey:=bvalue")
        for kvI in 1 2; do
            #               1  2
            local cmI cmA=("" "-cm 'this is the first comment' 'and this is a 2nd comment'")
            for cmI in 1 2; do
                # output file: detached (1) or self-contained (2)
                #                  1    2         1      2
                local ooI dashhA=("-h" "") sfxA=("nhdr" "nrrd")
                local fnbase=in$((inI - 1))-kv$((kvI - 1))-cm$((cmI - 1))
                for ooI in 1 2; do
                    local fout=$fnbase.$sfxA[$ooI]
                    zet_test $fout "unu make -i $fin $bargs $kvA[$kvI] $cmA[$cmI] $dashhA[$ooI] -o $fout" "$fin" "$chek"
                done
                # HERE, want to run a command to compare $fnbase.nhdr and $fnbase.nrrd
            done
        done
    done
}
Test

zet_end ####### END testing

exit 0
