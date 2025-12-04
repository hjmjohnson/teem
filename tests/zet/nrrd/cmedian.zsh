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
    print "${0:A} starting with:\nunu = $(which unu)\n"
else
    print -u2 "unu not found, bye"
    exit 1
fi

# make 1D, 2D, 3D, and 4D data arays, grayscale and color
valgen() {
    local seed
    seed=$(( 42 + $1 ))
    shift
    unu 1op nrand -s $seed | # random values
        unu 2op spow - 3 | # mostly pushed to 0
        unu 3op min_sm  -  8 14 | # soft clamp from above
        unu 3op max_sm -14 8  - "$@" # soft clamp from below
}
# Mx maps from dimension to max padding dimension (size - 1);
# much more concise than $(( Size[1] - 1 ))
typeset -A Mx
Mx[1]=1599
Mx[2]=159
Mx[3]=27
Mx[4]=11
Out=data1gf.nrrd; zet_junk $Out; echo 1 | unu reshape -s         1 | unu pad -min         0 -max                        $Mx[1] | valgen 1 -o $Out
Out=data1cf.nrrd; zet_junk $Out; echo 1 | unu reshape -s       1 1 | unu pad -min       0 0 -max                    2   $Mx[1] | valgen 2 -o $Out
Out=data2gf.nrrd; zet_junk $Out; echo 1 | unu reshape -s       1 1 | unu pad -min       0 0 -max                 $Mx[2] $Mx[2] | valgen 3 -o $Out
Out=data2cf.nrrd; zet_junk $Out; echo 1 | unu reshape -s     1 1 1 | unu pad -min     0 0 0 -max             2   $Mx[2] $Mx[2] | valgen 4 -o $Out
Out=data3gf.nrrd; zet_junk $Out; echo 1 | unu reshape -s     1 1 1 | unu pad -min     0 0 0 -max          $Mx[3] $Mx[3] $Mx[3] | valgen 5 -o $Out
Out=data3cf.nrrd; zet_junk $Out; echo 1 | unu reshape -s   1 1 1 1 | unu pad -min   0 0 0 0 -max     2    $Mx[3] $Mx[3] $Mx[3] | valgen 6 -o $Out
Out=data4gf.nrrd; zet_junk $Out; echo 1 | unu reshape -s   1 1 1 1 | unu pad -min   0 0 0 0 -max   $Mx[4] $Mx[4] $Mx[4] $Mx[4] | valgen 7 -o $Out
Out=data4cf.nrrd; zet_junk $Out; echo 1 | unu reshape -s 1 1 1 1 1 | unu pad -min 0 0 0 0 0 -max 2 $Mx[4] $Mx[4] $Mx[4] $Mx[4] | valgen 8 -o $Out
# (checksums set manually)
typeset -A Ckc
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
for dim in 1 2 3 4; do
    for w in g c; do
        In=data${dim}${w}f.nrrd; Out=${In/f.nrrd/8.nrrd}
        zet_assert_cksum $Ckc[${dim}${w}f] $In
        unu quantize -b 8 -i $In -o $Out; zet_junk $Out
        zet_assert_cksum $Ckc[${dim}${w}8] $Out
    done
done

RefFile=data.txt
if (( ${+ZET_WRITE_REF} )); then
    # *Writing* RefFile (not reading it)
    if [[ -f $RefFile ]]; then
        Verb="OVER-writing"
    else
        Verb="Writing"
    fi
    print "$0: === $Verb === RefFile=$RefFile"
    rm -f $RefFile
    touch $RefFile
else
    # not writing RefFile: we're *reading* it
    print "$0: === Reading === RefFile=$RefFile"
    zet_init_ICW $RefFile
    #echo "====== ZET_INFO"; for k v in ${(kv)ZET_INFO}; do print "[$k] => [$v]"; done
    #echo "====== ZET_CHEK"; for k v in ${(kv)ZET_CHEK}; do print "[$k] => [$v]"; done
    #echo "====== ZET_WANT"; for k v in ${(kv)ZET_WANT}; do print "[$k] => [$v]"; done
fi

typeset -A mode
mode[mode0]=""
mode[mode1]="-mode"
typeset -A pad
pad[pad0]=""
pad[pad1]="-p"
BINS_LIST=(100 5000)
WGHT_LIST=(1 2 5)
typeset -ga FAIL_CMD
for WUT in ${(k)Ckc}; do  # for all keywords in Ckc
    for RAD in 1 2 4; do
        for BINS in $BINS_LIST; do
            for WGHT in $WGHT_LIST; do
                for MM in mode0 mode1; do
                    for PP in pad0 pad1; do
                        #Usage: unu cmedian [-r,--radius <radius>] [-mode] [-b,--bins <num>]
                        #      [-w,--weight <weight>] [-p,--pad] [-c,--channel]
                        #      [-i,--input <nin>] [-o,--output <nout>]
                        Opts=(-r $RAD -b $BINS -w $WGHT $mode[$MM] $pad[$PP])
                        if [[ c == $WUT[2] ]]; then
                            # if middle character is c for color/multichannel, then say so
                            Opts+=("-c")
                        fi
                        Obase=${(j::)Opts} # join without spaces
                        Fin="data${WUT}.nrrd"; Fout=${Fin/.nrrd/$Obase.nrrd}
                        Cmd="unu cmedian -i $Fin $Opts -o $Fout"
                        zet_run_cmd "$Cmd" "$Fout"
                        if (( ${+ZET_WRITE_REF} )); then
                            # *Writing* RefFile (not reading it)
                            # format=     fname :: info ;; chek => want
                            Rest=$(zet_enbrace "$Cmd ;; cksum => $(cksum $Fout)" "$Fout")
                            echo "$Fout :: $Rest" >> $RefFile
                            print -n "o"
                        else
                            # not writing RefFile: we (already) *read* it
                            Got=$(eval $ZET_CHEK[$Fout])
                            Want=$ZET_WANT[$Fout]
                            if [[ "$Got" = "$Want" ]]; then
                                print -n "."
                            else
                                print -n "X"
                                FAIL_CMD+=($Cmd)
                                # preserve the input to the failed command
                                zet_unjunk $Fin
                            fi
                        fi
                    done
                done
            done
        done
    done
done
print ""

if (( ! ${+ZET_WRITE_REF} )); then
    # not writing ref = doing test
    if (( ${#FAIL_CMD} > 0 )); then
        print "Failed commands:"
        for Cmd in "${FAIL_CMD[@]}"; do
            print -- "$Cmd"
        done
    else
        print "All tests passed."
    fi
fi

# TODO:
# Create one more test set
# Create test sets with names like nrrd/cmedian, and rename data.txt accordingly
# Figure out how to register test sets, so as to "run all tests", and report stats
# Figure out how to run with recompiling unu with UBSAN or running with valgrind
#   ---> how to report errors? from ubsan or valgrind, but not from outputs being wrong
