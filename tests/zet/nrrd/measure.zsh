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
    local szX=1000 szY=5 # size of ramp array
    # zet_inf_begin triggers kvp history tracing, which we want in yramp.nrrd
    zet_inf_begin 'rows%s.nrrd' ######## BEGIN input file creation
    zet_inf_sec starting with one linear ramp yramp.nrrd
    echo 1 1 -1 -1 |
        (TEEM_KVP_HISTORY_STDIN="echo 1  1 -1 -1" unu reshape -s 2 2) |
        unu convert -t double |
        unu resample -s $szX $szY -c node -k tent -o yramp.nrrd
    zet_junk yramp.nrrd
    zet_inf_sec make xramp, apply different powers, and then add in yramp
    local tenpow outf
    for tenpow in 08 10 13; do # the powers times 10
        zet_inf $tenpow outf
        local pow=$(($tenpow / 10.0))
        echo 0 1 0 1 |
            (TEEM_KVP_HISTORY_STDIN="echo 0 1 0 1" unu reshape -s 2 2) |
            unu convert -t double |
            unu resample -s $szX $szY -c node -k tent |
            unu 2op pow - $pow |
            unu 2op + - yramp.nrrd -o $outf
    done
    zet_inf_sec shifting values Up and Down
    local ike
    for ike in $ZET_IKE; do
        local inf=$(zet_inf $ike)
        zet_inf ${ike}U outf
        unu 2op + $inf 10000 -o $outf
        zet_inf ${ike}D outf
        unu 2op - $inf 10000 -o $outf
    done
    zet_inf_sec inserting span of constant values in the middle
    for ike in $ZET_IKE; do
        local inf=$(zet_inf $ike)
        zet_inf ${ike}C outf
        unu slice -i $inf -a 0 -p $((szX / 2)) |
            unu axinsert -a 0 -s $((szX / 25)) |
            unu inset -i $inf -s - -min $((szX / 2 - szX / 50)) 0 -o $outf
    done
    zet_inf_sec histograms of all of these
    for ike in $ZET_IKE; do
        local inf=$(zet_inf $ike)
        zet_inf ${ike}H outf
        unu histax -i $inf -a 0 -b 200 -o $outf
    done
    zet_inf_end 'rows%s.nrrd' ######## END input file creation
}
Setup

zet_begin ####### BEGIN testing

#      min nrrdMeasureMin,            /* 1: smallest value */
#      max nrrdMeasureMax,            /* 2: biggest value */
#     mean nrrdMeasureMean,           /* 3: average of values */
#   median nrrdMeasureMedian,         /* 4: value at 50th percentile */
#     mode nrrdMeasureMode,           /* 5: most common value */
#     prod nrrdMeasureProduct,        /* 6: product of all values */
#      sum nrrdMeasureSum,            /* 7: sum of all values */
#       L1 nrrdMeasureL1,             /* 8 */
#       L2 nrrdMeasureL2,             /* 9 */
#       L4 nrrdMeasureL4,             /* 10 */
#      nL2 nrrdMeasureNormalizedL2,   /* 11 */
#      rms nrrdMeasureRootMeanSquare, /* 12 */
#     Linf nrrdMeasureLinf,           /* 13 */
#      var nrrdMeasureVariance,       /* 14 */
#     stdv nrrdMeasureSD,             /* 15: standard deviation */
#      cov nrrdMeasureCoV,            /* 16: coefficient of variation */
#     skew nrrdMeasureSkew,           /* 17: skew */
#    slope nrrdMeasureLineSlope,      /* 18: slope of line of best fit */
#     intc nrrdMeasureLineIntercept,  /* 19: y-intercept of line of best fit */
#    error nrrdMeasureLineError,      /* 20: error of line fitting */
# ("h" prefix needs to be turned to "histo-")
#     hmin nrrdMeasureHistoMin,       /* 21 */
#     hmax nrrdMeasureHistoMax,       /* 22 */
#    hmean nrrdMeasureHistoMean,      /* 23 */
#  hmedian nrrdMeasureHistoMedian,    /* 24 */
#    hmode nrrdMeasureHistoMode,      /* 25 */
# hproduct nrrdMeasureHistoProduct,   /* 26 */
#     hsum nrrdMeasureHistoSum,       /* 27 */
#      hL2 nrrdMeasureHistoL2,        /* 28 */
#     hvar nrrdMeasureHistoVariance,  /* 29 */
#    hstdv nrrdMeasureHistoSD,        /* 30 */

Test() {
    local MM=(min max mean median mode prod sum L1 L2 L4 nL2
        rms Linf var stdv cov skew slope intc error
        hmin hmax hmean hmedian hmode hproduct hsum hL2 hvar hsd)
    # NOTE: "histo-stdv" is a newer alias for histo-sd but for compatability
    # with testing old code, use old histo-sd name, hence "hsd" above not "hstdv"
    local ike
    for ike in $ZET_IKE; do
        local mm
        for mm in $MM; do
            # luckily "h" appears nowhere in any of the non-histo- measures
            # so this substitution needn't be specific to leading h
            local opts=(-m ${mm/h/histo-})
            local obase="-m${mm}"
            local inf=$(zet_inf $ike)
            local outf=${inf/.nrrd/$obase.nrrd}
            local comd="unu project -i $inf -a 0 ${opts[@]} -o $outf"
            zet_test "$outf" "$comd" "$inf" "cksum"
        done
    done
}
Test

zet_end ####### END testing

exit 0
