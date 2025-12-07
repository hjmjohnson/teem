#!/usr/bin/env zsh

source ${0:A:h}/../zet-lib.zsh # set up the zet "library"
setopt err_exit                # exit on error
setopt pipe_fail               # propagate pipeline errors

if (( $+commands[unu] )); then
    print "${0} using unu = $(which unu)\n"
else
    zet_die "unu not found, bye"
fi

typeset -A Ckc
Setup() {
    # some sort of non-trivial terrain
    cat > hght.txt <<'EOF'
0 0 0 0 1
0 2 2 2 1
0 2 4 3 1
0 2 3 3 1
0 0 0 0 1
EOF
    zet_junk hght.txt
    # upsample it with a bit noise to make it interesting
    local inn out
    out=hght-a11.nrrd; zet_junk $out; unu resample -i hght.txt -s x3 x3 -k box |
        unu 2op nrand - 0.15 -s 53 |
        unu resample -s x15 x15 -k bspln3 |
        unu dnorm -o $out
    # some different aspect ratios
    out=hght-a13.nrrd; zet_junk $out; unu resample -i hght-a11.nrrd -s  =   /3 -k tent -o $out
    out=hght-a21.nrrd; zet_junk $out; unu resample -i hght-a11.nrrd -s /2.2  = -k tent -o $out
    local nn
    for nn in 11 13 21; do
        inn=hght-a$nn.nrrd
        out=hght-b$nn.nrrd; zet_junk $out; unu swap -i $inn -a 0 1 -o $out
        out=hght-c$nn.nrrd; zet_junk $out; unu swap -i $inn -a 0 1 | unu flip -a 0 -o $out
        out=hght-d$nn.nrrd; zet_junk $out; unu flip -i $inn -a 0 | unu flip -a 1 -o $out
    done
    # check on these files' contents
    Ckc[a11]="1967624952 202667"
    Ckc[a13]="2947370679 67803"
    Ckc[a21]="3602888103 92139"
    Ckc[b11]="1572555009 202783"
    Ckc[b13]="139883114 67813"
    Ckc[b21]="2930763932 92149"
    Ckc[c11]="2311120603 202795"
    Ckc[c13]="4291150667 67825"
    Ckc[c21]="3538037484 92161"
    Ckc[d11]="2091956497 202797"
    Ckc[d13]="2122642803 67827"
    Ckc[d21]="234418738 92160"
    local xnn
    for xnn in ${(k)Ckc}; do  # for all keywords in Ckc
        zet_assert_cksum $Ckc[$xnn] hght-$xnn.nrrd
    done
}
Setup

zet_begin ####### BEGIN #######
zet_stop

Test() {
    local th sgn inv bi xnn opts obase fin fout comd got want
    for th in 0.5 1.5 2.5 3.5; do
        for sgn in "" "-sgn"; do
            for inv in "" "-inv"; do
                for bi in 0 3; do
                    for xnn in ${(ok)Ckc}; do  # "o" sorting all "k" keywords in Ckc
                        if [[ $bi == 3 && $sgn == "-sgn" ]]; then
                            # "unu dist: sorry, signed and biased transform not yet implemented"
                            continue
                        fi
                        # Usage: unu dist -th,--thresh <val> [-b,--bias <val>] [-t,--type <type>]
                        #        [-sgn] [-inv] [-i,--input <nin>] [-o,--output <nout>]
                        opts=(-th $th -b $bi $sgn $inv)
                        obase=${(j::)opts} # join without spaces
                        fin="hght-${xnn}.nrrd"; fout=${fin/.nrrd/$obase.nrrd}
                        comd="unu dist -i $fin "${opts[@]}" -o $fout"
                        zet_test "$fout" "$comd" "$fin" "cksum"
                    done
                done
            done
        done
    done
}
Test

zet_end ####### END #######

exit 0

if false; then
    # one-time checks, perhaps should also be scripted:
    unu join -i hght-a11-th???.nrrd -a 2 -incr | unu cksum -
    unu join -i hght-b11-th???.nrrd -a 2 -incr | unu swap -a 0 1 | unu cksum -
    unu join -i hght-c11-th???.nrrd -a 2 -incr | unu flip -a 0 | unu swap -a 0 1 | unu cksum -
    unu join -i hght-d11-th???.nrrd -a 2 -incr | unu flip -a 0 | unu flip -a 1 | unu cksum -
    # which show that `dist` commutes with axis manipulation

    unu join -i hght-a11-th???-sgn.nrrd -a 2 -incr | unu tile -a 2 0 1 -s 2 2 -o tmp.nrrd
    unu join -i hght-a13-th???-sgn.nrrd -a 2 -incr |
        unu resample -s = x3 = -k tent | unu tile -a 2 0 1 -s 2 2 |
        unu jhisto -i tmp.nrrd - -b 300 300 -t float | unu 1op log1p | XV
    unu join -i hght-c13-th???-sgn.nrrd -a 2 -incr | unu flip -a 0 | unu swap -a 0 1 |
        unu resample -s = x3 = -k tent | unu tile -a 2 0 1 -s 2 2 |
        unu jhisto -i tmp.nrrd - -b 300 300 -t float | unu 1op log1p | XV
    # which show that resampling roughly commutes with distance transform
    # (distance transform is usefully using the inter-sample distance)
fi

