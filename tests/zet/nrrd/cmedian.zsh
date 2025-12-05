#!/usr/bin/env zsh

source ${(q)0:A:h}/../zet-lib.zsh # set up the zet "library"
#          $0 path of me
#            :A made absolute
#              :h head = drop my filename to get path
#                 /../ = parent directory
#        (q) with quoting of result

setopt ERR_EXIT           # exit on error
setopt PIPE_FAIL          # propagate pipeline errors
#setopt XTRACE             # print commands before executing them
#setopt VERBOSE            # print shell input lines as they are read

if (( $+commands[unu] )); then
    print "${0} using unu = $(which unu)\n"
else
    zet_die "unu not found, bye"
fi

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
    local inn out
    out=data1gf.nrrd; zet_junk $out; echo 1 | unu reshape -s         1 | unu pad -min         0 -max                        $Mx[1] | valgen 1 -o $out
    out=data1cf.nrrd; zet_junk $out; echo 1 | unu reshape -s       1 1 | unu pad -min       0 0 -max                    2   $Mx[1] | valgen 2 -o $out
    out=data2gf.nrrd; zet_junk $out; echo 1 | unu reshape -s       1 1 | unu pad -min       0 0 -max                 $Mx[2] $Mx[2] | valgen 3 -o $out
    out=data2cf.nrrd; zet_junk $out; echo 1 | unu reshape -s     1 1 1 | unu pad -min     0 0 0 -max             2   $Mx[2] $Mx[2] | valgen 4 -o $out
    out=data3gf.nrrd; zet_junk $out; echo 1 | unu reshape -s     1 1 1 | unu pad -min     0 0 0 -max          $Mx[3] $Mx[3] $Mx[3] | valgen 5 -o $out
    out=data3cf.nrrd; zet_junk $out; echo 1 | unu reshape -s   1 1 1 1 | unu pad -min   0 0 0 0 -max     2    $Mx[3] $Mx[3] $Mx[3] | valgen 6 -o $out
    out=data4gf.nrrd; zet_junk $out; echo 1 | unu reshape -s   1 1 1 1 | unu pad -min   0 0 0 0 -max   $Mx[4] $Mx[4] $Mx[4] $Mx[4] | valgen 7 -o $out
    out=data4cf.nrrd; zet_junk $out; echo 1 | unu reshape -s 1 1 1 1 1 | unu pad -min 0 0 0 0 0 -max 2 $Mx[4] $Mx[4] $Mx[4] $Mx[4] | valgen 8 -o $out
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
    # quantize float-type *f.nrrd to uchar *8.nrrd
    local dim w
    for dim in 1 2 3 4; do
        for w in g c; do
            inn=data${dim}${w}f.nrrd; out=${inn/f.nrrd/8.nrrd}
            zet_assert_cksum $Ckc[${dim}${w}f] $inn
            unu quantize -b 8 -i $inn -o $out; zet_junk $out
            zet_assert_cksum $Ckc[${dim}${w}8] $out
        done
    done
}
Setup

zet_begin ####### BEGIN ####### 

Test() {
    local wut rad bins wght mode pad
    local opts obase fin fout comd got want
    for wut in ${(k)Ckc}; do  # for all keywords in Ckc
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
                            comd="unu cmedian -i $fin $opts -o $fout"
                            zet_test "$fout" "$comd" "$fin" "cksum"
                        done
                    done
                done
            done
        done
    done
}
Test

zet_end ####### END #######

exit 0
