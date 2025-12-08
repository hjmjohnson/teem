#!/usr/bin/env zsh

source ${0:A:h}/../zet-lib.zsh # set up the zet "library"
setopt err_exit                # exit on error
setopt pipe_fail               # propagate pipeline errors
if (( $+commands[unu] )); then
    print "${0} using unu = $(which unu)\n"
else
    zet_die "unu not found, bye"
fi

# Working on this motivated the TeemV2 improvements in how nrrdHistoEq (unu heq) works:
# (1) keeping better track of "steady" so that incessantly hit bins can be ignored even
# if later values are more variable
# (2) only ignoring bins (with smart > 0) if they were incessantly hit

typeset -a Wuts
Setup() {
    setopt localoptions no_warn_nested_var # we're here to modify Wuts
    local szX=600 szY=100
    # an amount of padding with area ~1/3 of interior
    local szP=$(( (szX*szY/(2*szX + 2*szY))/3 ))
    # making various ramps, with various gammas, and paddings
    echo "1  $szY   0  $(( szY - 1 ))" | unu 2op / - $szY | unu reshape -s 2 2 |
        unu resample -s $szX $szY -c node -k tent |
        unu basinfo -c ramp -o ramp_10.nrrd; zet_junk ramp_10.nrrd
    Wuts=("_10")
    local tpows=(03 05 20 33) # various exponents, times 10
    local wut out tenpow
    for tenpow in $tpows; do
        local pow=$(( $tenpow / 10.0 ))
        # two ways of warping values
        wut="_${tenpow}"; Wuts+=($wut)
        out=ramp${wut}.nrrd; unu 2op pow ramp_10.nrrd $pow -o $out; zet_junk $out
        ## actually just one; this test set is getting out of hand
        wut="n${tenpow}"; Wuts+=($wut)
        out=ramp${wut}.nrrd; unu 2op - 1 ramp_10.nrrd | unu 2op pow - $pow | unu 2op - 1 - -o $out; zet_junk $out
    done
    local tppvals=(00 06 12) # various pad values (-0.1, 0.5, 1.1), times 10, plus 1
    local foo padval
    for wut in $Wuts; do
        local tppval
        for tppval in $tppvals; do
            local padval=$(( (tppval-1.0) / 10.0 ))
            local owut=${wut}p${tppval}
            out=ramp${owut}.nrrd
            unu pad -i ramp${wut}.nrrd -b pad -v $padval -min -$szP -$szP -max M+$szP M+$szP -o $out; zet_junk $out
            Wuts+=($owut)
            # *N*ested copy of above loop, for more padding
            local tppvalN
            for tppvalN in $tppvals; do
                padval=$(( (tppvalN-1.0) / 10.0 ))
                local owutN=${owut}p${tppvalN}
                local outN=ramp${owutN}.nrrd
                unu pad -i $out -b pad -v $padval -min -$szP -$szP -max M+$szP M+$szP -o $outN; zet_junk $outN
                Wuts+=($owutN)
            done
        done
    done
    # check on these files' contents (with Ckc) ...
    # (actually trying skipping this for this test set)
}
Setup

zet_begin ####### BEGIN #######

Test() {
    local wut
    for wut in $Wuts; do
        local bin
        for bin in 30 9000; do
            local smart
            for smart in 0 1 2 9; do
                local tamount
                for tamount in 00 05 10; do
                    # Usage: unu heq -b,--bin <bins> [-s,--smart [<bins>]] [-a,--amount <amount>]
                    #        [-m,--map <filename>] [-i,--input <nin>] [-o,--output <nout>]
                    local opts=(-b $bin -s $smart)
                    local obase=${(j::)opts} # join without spaces
                    if [[ $wut =~ '^_05' && $bin == 100 ]]; then
                        # for only this ramp and this $bin, we do try the different -a amounts
                        local amount=$(( tamount/10.0 ))
                        opts+=(-a $amount)
                        obase+="-a${tamount}"
                    else
                        # otherwise, we only do -a 1, which is the default anyway, so we
                        # don't record it in the filename. But we don't want to run the
                        # same command multiple times, so we bail if not at ~"-a 1"
                        # (thus mooting the `for tamount` loop)
                        if [[ $tamount != 10  ]]; then
                            continue
                        fi
                    fi
                    local fin="ramp${wut}.nrrd"
                    local fout=${fin/.nrrd/$obase.nrrd}
                    local comd="unu heq -i $fin "${opts[@]}" -o $fout"
                    zet_test "$fout" "$comd" "$fin" "cksum"
                done
            done
        done
    done
}
Test

zet_end ####### END #######

exit 0
