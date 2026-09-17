#!/usr/bin/env zsh

source ${0:A:h}/../zet-lib.zsh # set up the zet "library"
setopt err_exit                # exit on error
setopt pipe_fail               # propagate pipeline errors
if (($+commands[unu])); then
    print "${0} using unu = $(which unu)\n"
else
    zet_die "unu not found, bye"
fi

Setup() {
    # some sort of non-trivial terrain
    cat >hght.txt <<'EOF'
0 0 0 0 1
0 2 2 2 1
0 2 4 3 1
0 2 3 3 1
0 0 0 0 1
EOF
    zet_junk hght.txt
    # upsample it with a bit noise to make it interesting
    local inn out
    zet_inf_begin 'hght-%s.nrrd' ######## BEGIN input file creation
    zet_inf a11 out
    unu resample -i hght.txt -s x3 x3 -k box |
        unu 2op nrand - 0.15 -s 53 |
        unu resample -s x15 x15 -k bspln3 |
        unu dnorm -o $out
    # some different aspect ratios
    zet_inf a13 out
    unu resample -i hght-a11.nrrd -s = /3 -k tent -o $out
    zet_inf a21 out
    unu resample -i hght-a11.nrrd -s /2.2 = -k tent -o $out
    local nn
    for nn in 11 13 21; do
        inn=hght-a$nn.nrrd
        zet_inf b$nn out
        unu swap -i $inn -a 0 1 -o $out
        zet_inf c$nn out
        unu swap -i $inn -a 0 1 | unu flip -a 0 -o $out
        zet_inf d$nn out
        unu flip -i $inn -a 0 | unu flip -a 1 -o $out
    done
    zet_inf_end hght-%s.nrrd ######## END input file creation
}
Setup

zet_begin ####### BEGIN testing

Test() {
    local th sgn inv bi ike opts obase fin fout comd got want
    for th in 0.5 1.5 2.5 3.5; do
        for sgn in "" "-sgn"; do
            for inv in "" "-inv"; do
                for bi in 0 3; do
                    for ike in $ZET_IKE; do
                        if [[ $bi == 3 && $sgn == "-sgn" ]]; then
                            # "unu dist: sorry, signed and biased transform not yet implemented"
                            continue
                        fi
                        # Usage: unu dist -th,--thresh <val> [-b,--bias <val>] [-t,--type <type>]
                        #        [-sgn] [-inv] [-i,--input <nin>] [-o,--output <nout>]
                        opts=(-th $th -b $bi $sgn $inv)
                        obase=${(j::)opts} # join without spaces
                        fin=$(zet_inf $ike)
                        fout=${fin/.nrrd/$obase.nrrd}
                        comd="unu dist -i $fin ${opts[@]} -o $fout"
                        zet_test "$fout" "$comd" "$fin" "cksum"
                    done
                done
            done
        done
    done
}
Test

zet_end ####### END testing

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
