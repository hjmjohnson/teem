#!/usr/bin/env zsh

source ${0:A:h}/../zet-lib.zsh # set up the zet "library"
#       $0 path of me
#         :A made absolute
#           :h head = drop my filename to get path
#              /../ = parent directory
setopt err_exit                # exit on error
setopt pipe_fail               # propagate pipeline errors
if (( $+commands[unu] )); then
    print "${0} using unu = $(which unu)\n"
else
    zet_die "unu not found, bye"
fi

# This was the first zet test set written; things that weren't right:
#setopt XTRACE             # print commands before executing them
#setopt XTRACE; PS4=$'%N:%i> '    (with prompt showing where we are)
#setopt VERBOSE            # print shell input lines as they are read
#  ( _zet_debug seems better than these )

valgen() {
    local seed
    seed=$(( 42 + $1 ))
    shift
    unu 1op nrand -s $seed | # random values
        unu 2op spow - 3 | # mostly pushed to 0
        unu 3op min_sm  -  8 14 | # soft clamp from above
        unu 3op max_sm -14 8  - "$@" # soft clamp from below
}

typeset -A Ckc
Setup() {
    # make 1D, 2D, 3D, and 4D data arays, grayscale and color
    # Mx maps from dimension to max padding dimension (size - 1);
    # much more concise than $(( Size[1] - 1 ))
    local Mx; typeset -A Mx
    Mx[1]=1599
    Mx[2]=159
    Mx[3]=27
    Mx[4]=11
    local fin fout
    fout=data1gf.nrrd; zet_junk $fout; echo 1 | unu reshape -s         1 | unu pad -min         0 -max                        $Mx[1] | valgen 1 -o $fout
    fout=data1cf.nrrd; zet_junk $fout; echo 1 | unu reshape -s       1 1 | unu pad -min       0 0 -max                    2   $Mx[1] | valgen 2 -o $fout
    fout=data2gf.nrrd; zet_junk $fout; echo 1 | unu reshape -s       1 1 | unu pad -min       0 0 -max                 $Mx[2] $Mx[2] | valgen 3 -o $fout
    fout=data2cf.nrrd; zet_junk $fout; echo 1 | unu reshape -s     1 1 1 | unu pad -min     0 0 0 -max             2   $Mx[2] $Mx[2] | valgen 4 -o $fout
    fout=data3gf.nrrd; zet_junk $fout; echo 1 | unu reshape -s     1 1 1 | unu pad -min     0 0 0 -max          $Mx[3] $Mx[3] $Mx[3] | valgen 5 -o $fout
    fout=data3cf.nrrd; zet_junk $fout; echo 1 | unu reshape -s   1 1 1 1 | unu pad -min   0 0 0 0 -max     2    $Mx[3] $Mx[3] $Mx[3] | valgen 6 -o $fout
    fout=data4gf.nrrd; zet_junk $fout; echo 1 | unu reshape -s   1 1 1 1 | unu pad -min   0 0 0 0 -max   $Mx[4] $Mx[4] $Mx[4] $Mx[4] | valgen 7 -o $fout
    fout=data4cf.nrrd; zet_junk $fout; echo 1 | unu reshape -s 1 1 1 1 1 | unu pad -min 0 0 0 0 0 -max 2 $Mx[4] $Mx[4] $Mx[4] $Mx[4] | valgen 8 -o $fout
    # (checksums set manually)
    Ckc[1gf]="86347561 6658"
    Ckc[1cf]="2977287388 19468"
    Ckc[1g8]="1808551849 1904"
    Ckc[1c8]="969249504 5098"
    Ckc[2gf]="2227243899 102670"
    Ckc[2cf]="205356546 307480"
    Ckc[2g8]="1150354174 25900"
    Ckc[2c8]="274392946 77110"
    Ckc[3gf]="1644254305 88086"
    Ckc[3cf]="967722298 263712"
    Ckc[3g8]="1205032216 22260"
    Ckc[3c8]="53665354 66174"
    Ckc[4gf]="195377246 83234"
    Ckc[4cf]="1049383087 249132"
    Ckc[4g8]="1741878457 21056"
    Ckc[4c8]="1416163045 62538"
    Ckc[1gflip]="2136608429 6666"
    Ckc[2gflip]="595367765 102678"
    Ckc[3gflip]="220345779 88094"
    Ckc[4gflip]="1473397807 83242"
    # quantize float-type *f.nrrd to uchar *8.nrrd
    local dim w
    for dim in 1 2 3 4; do
        for w in g c; do
            fin=data${dim}${w}f.nrrd; fout=${fin/f.nrrd/8.nrrd}
            zet_assert_cksum $Ckc[${dim}${w}f] $fin
            unu quantize -b 8 -i $fin -o $fout; zet_junk $fout
            zet_assert_cksum $Ckc[${dim}${w}8] $fout
        done
    done
    # some array just for testing how the median filtering does incremental
    # updates of the histogram along the fastest axis, which deserves scrutiny
    fout=data1gflip.nrrd; zet_junk $fout; unu flip -i data1gf.nrrd -a 0 -o $fout; zet_assert_cksum $Ckc[1gflip] $fout
    fout=data2gflip.nrrd; zet_junk $fout; unu flip -i data2gf.nrrd -a 0 -o $fout; zet_assert_cksum $Ckc[2gflip] $fout
    fout=data3gflip.nrrd; zet_junk $fout; unu flip -i data3gf.nrrd -a 0 -o $fout; zet_assert_cksum $Ckc[3gflip] $fout
    fout=data4gflip.nrrd; zet_junk $fout; unu flip -i data4gf.nrrd -a 0 -o $fout; zet_assert_cksum $Ckc[4gflip] $fout
}
Setup

zet_begin ####### BEGIN #######

# The FLIPCHECK logic was for seeing if filtering commutes with flipping an axis.
# And, it was discovered that no, *weighted* filtering does not always commute with
# flipping (and probably not with axis re-ordering), and some detailed debugging
# revealed that it's because of the non-commutativity of FP addition: when using
# weighted filtering, the histogram bins are incremented by some non-integral weight
# (a separable product of weight vector lookups). Even if the dynamic range of the
# weights is small, just the consequences of round-to-nearest-ties-to-even will
# create non-commutativity.  The weights are added to a (FP-valued) histogram bins
# in the order that values are traversed in the array, and that is determined by
# values along the axis (hence the sensitivity to flipping).
FLIPCHECK=0
if (( $FLIPCHECK )); then
    print "Doing one-time flip check ..."
    rm -f flipcheck.zsh
    touch flipcheck.zsh
fi

Test() {
    local wut rad bins wght mode pad
    local opts obase fin fout comd got want
    for wut in ${(ok)Ckc}; do  # "o" sorting all "k" keywords in Ckc
        for rad in 1 2 4; do
            for bins in 100 5000; do
                for wght in 1 2 5; do
                    for mode in "" "-mode"; do
                        for pad in "" "-p"; do
                            #Usage: unu cmedian [-r,--radius <radius>] [-mode] [-b,--bins <num>]
                            #      [-w,--weight <weight>] [-p,--pad] [-c,--channel]
                            #      [-i,--input <nin>] [-o,--output <nout>]
                            opts=(-r $rad -b $bins -w $wght $mode $pad)
                            if [[ c == $wut[2] ]]; then
                                # if middle character is c for color/multichannel, then say so
                                opts+=("-c")
                            fi
                            obase=${(j::)opts} # join without spaces
                            fin="data${wut}.nrrd"; fout=${fin/.nrrd/$obase.nrrd}
                            comd="unu cmedian -i $fin "${opts[@]}" -o $fout"
                            zet_test "$fout" "$comd" "$fin" "cksum"
                            if (( $FLIPCHECK )); then
                                # generate commands to (outsize of zet testing) see if cmedian commutes with unu flip -a 0
                                # Oooo! actually found some discrepencies, so added logic to unjunk files for later checks
                                # (the eventually discovered explanation is above FLIPCHECK=0)
                                if [[ $wut =~ '.*flip.*' ]]; then
                                    local flipout=${fout/flip/f}
                                    # we are using zsh to write zsh commands that will be run later via `source`
                                    local fcmd="unu flip -i $fout -a 0 | unu 2op - - $flipout | unu minmax -sl -"
                                    print "fin=\"$fin\"; fcmd=\"$fcmd\"" >> flipcheck.zsh
                                    print 'rslt=$(eval $fcmd)' >> flipcheck.zsh
                                    print 'if [[ "$rslt" != "0 0" ]]; then' >> flipcheck.zsh
                                    print '    print "$fcmd => $rslt"' >> flipcheck.zsh
                                    # preserve for testing: the unflipped,flipped inputs, and their corresponding outputs
                                    print "    zet_unjunk ${fin/flip.nrrd/f.nrrd} $fin $fout $flipout" >> flipcheck.zsh
                                    print 'fi' >> flipcheck.zsh
                                fi
                            fi
                        done
                    done
                done
            done
        done
    done
}
Test

if (( $FLIPCHECK )); then
    print ""
    # this may unjunk files
    source flipcheck.zsh
fi

zet_end ####### END #######

exit 0
