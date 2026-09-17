#!/usr/bin/env zsh

source ${0:A:h}/../zet-lib.zsh # set up the zet "library"
setopt err_exit                # exit on error
setopt pipe_fail               # propagate pipeline errors
if (($+commands[unu])); then
    print "${0} using unu = $(which unu)\n"
else
    zet_die "unu not found, bye"
fi

# Working on this motivated the TeemV2 improvements in how nrrdHistoEq (unu heq) works:
# (1) keeping better track of "steady" so that incessantly hit bins can be ignored even
# if later values are more variable
# (2) only ignoring bins (with smart > 0) if they were incessantly hit

Setup() {
    local szX=600 szY=100 # size of ramp array
    # an amount of padding with area ~1/3 of interior
    local szP=$(((szX * szY / (2 * szX + 2 * szY)) / 3))
    zet_inf_begin 'ramp%s.nrrd' ######## BEGIN input file creation
    # starting with one linear ramp
    local outf
    zet_inf _10 outf
    echo "1  $szY   0  $((szY - 1))" | unu 2op / - $szY | unu reshape -s 2 2 |
        unu resample -s $szX $szY -c node -k tent |
        unu basinfo -c ramp -o $outf

    # raising linear ramp to various exponents
    local tenpow
    for tenpow in 03 05 20 33; do # the exponents, times 10
        local pow=$(($tenpow / 10.0))
        zet_inf "_${tenpow}" outf
        unu 2op pow ramp_10.nrrd $pow -o $outf
        ## actually maybe just one warping; this test set size is getting out of hand
        #zet_inf "n${tenpow}" outf
        #unu 2op - 1 ramp_10.nrrd | unu 2op pow - $pow | unu 2op - 1 - -o $outf
    done
    # for all ramps make so far, add various paddings
    local tppvals=(00 06 12) # various pad values (-0.1, 0.5, 1.1), times 10, plus 1
    local ike padval
    for ike in $ZET_IKE; do
        local tppval
        for tppval in $tppvals; do
            local padval=$(((tppval - 1.0) / 10.0))
            local ikeP="${ike}p${tppval}"
            zet_inf $ikeP outf
            unu pad -i $(zet_inf $ike) -b pad -v $padval -min -$szP -$szP -max M+$szP M+$szP -o $outf
            # *N*ested copy of above loop, for more padding
            local tppvalN
            for tppvalN in $tppvals; do
                padval=$(((tppvalN - 1.0) / 10.0))
                local outN
                zet_inf "${ikeP}p${tppvalN}" outN
                unu pad -i $outf -b pad -v $padval -min -$szP -$szP -max M+$szP M+$szP -o $outN
            done
        done
    done
    zet_inf_end 'ramp%s.nrrd' ######## END input file creation
}
Setup

zet_begin ####### BEGIN testing

Test() {
    local ike
    for ike in $ZET_IKE; do
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
                    if [[ $ike =~ '^_05' && $bin == 100 ]]; then
                        # for only this ramp and this $bin, we do try the different -a amounts
                        local amount=$((tamount / 10.0))
                        opts+=(-a $amount)
                        obase+="-a${tamount}"
                    else
                        # otherwise, we only do -a 1, which is the default anyway, so we
                        # don't record it in the filename. But we don't want to run the
                        # same command multiple times, so we bail if not at ~"-a 1"
                        # (thus mooting the `for tamount` loop)
                        if [[ $tamount != 10 ]]; then
                            continue
                        fi
                    fi
                    local inf=$(zet_inf $ike)
                    local outf=${inf/.nrrd/$obase.nrrd}
                    local comd="unu heq -i $inf ${opts[@]} -o $outf"
                    zet_test "$outf" "$comd" "$inf" "cksum"
                done
            done
        done
    done
}
Test

zet_end ####### END testing

exit 0
