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

# This was the first zet test set written; things that didn't stick
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

Setup() {
    # make 1D, 2D, 3D, and 4D data arays, grayscale and color
    # Mx maps from dimension to max padding dimension (size - 1);
    # much more concise than $(( Size[1] - 1 ))
    local Mx; typeset -A Mx
    Mx[1]=1599
    Mx[2]=159
    Mx[3]=27
    Mx[4]=11
    zet_inf_begin 'data%s.nrrd' ######## BEGIN input file creation
    local fout
    zet_inf 1gf fout; echo 1 | unu reshape -s         1 | unu pad -min         0 -max                        $Mx[1] | valgen 1 -o $fout
    zet_inf 1cf fout; echo 1 | unu reshape -s       1 1 | unu pad -min       0 0 -max                    2   $Mx[1] | valgen 2 -o $fout
    zet_inf 2gf fout; echo 1 | unu reshape -s       1 1 | unu pad -min       0 0 -max                 $Mx[2] $Mx[2] | valgen 3 -o $fout
    zet_inf 2cf fout; echo 1 | unu reshape -s     1 1 1 | unu pad -min     0 0 0 -max             2   $Mx[2] $Mx[2] | valgen 4 -o $fout
    zet_inf 3gf fout; echo 1 | unu reshape -s     1 1 1 | unu pad -min     0 0 0 -max          $Mx[3] $Mx[3] $Mx[3] | valgen 5 -o $fout
    zet_inf 3cf fout; echo 1 | unu reshape -s   1 1 1 1 | unu pad -min   0 0 0 0 -max     2    $Mx[3] $Mx[3] $Mx[3] | valgen 6 -o $fout
    zet_inf 4gf fout; echo 1 | unu reshape -s   1 1 1 1 | unu pad -min   0 0 0 0 -max   $Mx[4] $Mx[4] $Mx[4] $Mx[4] | valgen 7 -o $fout
    zet_inf 4cf fout; echo 1 | unu reshape -s 1 1 1 1 1 | unu pad -min 0 0 0 0 0 -max 2 $Mx[4] $Mx[4] $Mx[4] $Mx[4] | valgen 8 -o $fout
    # quantize float-type *f.nrrd to uchar *8.nrrd
    local dim
    for dim in 1 2 3 4; do
        local w
        for w in g c; do
            local ike=${dim}${w}f
            zet_inf ${ike/f/8} fout; unu quantize -b 8 -i $(zet_inf $ike) -o $fout
            if [[ $w == g ]]; then
                # some arrays just for testing how the median filtering does incremental
                # updates of the histogram along the fastest axis, which deserves scrutiny
                zet_inf ${ike/f/flip} fout; unu flip -i $(zet_inf $ike) -a 0 -o $fout
            fi
        done
    done
    zet_inf_end 'data%s.nrrd' ######## END input file creation
}
Setup

zet_begin ####### BEGIN testing

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
    local ike
    for ike in $ZET_IKE; do
        local rad
        for rad in 1 2 4; do
            local bins
            for bins in 100 5000; do
                local wght
                for wght in 1 2 5; do
                    local mode
                    for mode in "" "-mode"; do
                        local pad
                        for pad in "" "-p"; do
                            #Usage: unu cmedian [-r,--radius <radius>] [-mode] [-b,--bins <num>]
                            #      [-w,--weight <weight>] [-p,--pad] [-c,--channel]
                            #      [-i,--input <nin>] [-o,--output <nout>]
                            local opts=(-r $rad -b $bins -w $wght $mode $pad)
                            if [[ c == $ike[2] ]]; then
                                # if middle character is c for color/multichannel, then say so
                                opts+=("-c")
                            fi
                            local fout
                            local obase=${(j::)opts} # join without spaces
                            local fin="$(zet_inf $ike)"
                            local fout=${fin/.nrrd/$obase.nrrd}
                            local comd="unu cmedian -i $fin ${opts[@]} -o $fout"
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

zet_end ####### END testing

exit 0
