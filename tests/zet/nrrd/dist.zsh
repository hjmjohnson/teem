#!/usr/bin/env zsh

source ${(q)0:A:h}/../zet-lib.zsh # set up the zet "library"
setopt ERR_EXIT           # exit on error
setopt PIPE_FAIL          # propagate pipeline errors

if (( $+commands[unu] )); then
    print "${0:A} starting with:\nunu = $(which unu)\n"
else
    print -u2 "unu not found, bye"
    exit 1
fi

# some sort of non-trivial terrain
cat > hght.txt <<'EOF'
0 0 0 0 1
0 2 2 2 1
0 2 4 3 1
0 2 3 3 1
0 0 0 0 1
EOF
zet_junk hght.txt
typeset -A Ckc
Setup() {
    # upsample it with a bit noise to make it interesting
    local In Out
    Out=hght-a11.nrrd; zet_junk $Out; unu resample -i hght.txt -s x3 x3 -k box |
        unu 2op nrand - 0.15 -s 53 |
        unu resample -s x15 x15 -k bspln3 |
        unu dnorm -o $Out
    # some different aspect ratios
    Out=hght-a13.nrrd; zet_junk $Out; unu resample -i hght-a11.nrrd -s  =   /3 -k tent -o $Out
    Out=hght-a21.nrrd; zet_junk $Out; unu resample -i hght-a11.nrrd -s /2.2  = -k tent -o $Out
    local nn
    for nn in 11 13 21; do
        In=hght-a$nn.nrrd
        Out=hght-b$nn.nrrd; zet_junk $Out; unu swap -i $In -a 0 1 -o $Out
        Out=hght-c$nn.nrrd; zet_junk $Out; unu swap -i $In -a 0 1 | unu flip -a 0 -o $Out
        Out=hght-d$nn.nrrd; zet_junk $Out; unu flip -i $In -a 0 | unu flip -a 1 -o $Out
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

RefFile=dist-ref.txt
zet_init_ref $RefFile

Test() {
    local FAIL_CMD; typeset -ga FAIL_CMD  # HEY this belongs at a higher-level
    local sgn; typeset -A sgn
    sgn[0]=""
    sgn[1]="-sgn"
    local inv; typeset -A inv
    inv[0]=""
    inv[1]="-inv"
    local th ss ii bi xnn opts obase fin fout cmd got want rest
    for th in 0.5 1.5 2.5 3.5; do
        for ss in 0 1; do
            for ii in 0 1; do
                for bi in 0 3; do
                    for xnn in ${(k)Ckc}; do  # for all keywords in Ckc
                        if [[ $bi == 3 && $ss == 1 ]]; then
                            # unu dist: sorry, signed and biased transform not yet implemented
                            continue
                        fi
                        # Usage: unu dist -th,--thresh <val> [-b,--bias <val>] [-t,--type <type>]
                        #        [-sgn] [-inv] [-i,--input <nin>] [-o,--output <nout>]
                        opts=(-th $th -b $bi $sgn[$ss] $inv[$ii])
                        obase=${(j::)opts} # join without spaces
                        fin="hght-${xnn}.nrrd"; fout=${fin/.nrrd/$obase.nrrd}
                        cmd="unu dist -i $fin $opts -o $fout"
                        zet_run_cmd "$cmd" "$fout"
                        if (( ${+ZET_WRITE_REF} )); then
                            # *Writing* RefFile (not reading it)
                            # format=     fname :: info ;; chek => want
                            rest=$(zet_enbrace "$cmd ;; cksum => $(cksum $fout)" "$fout")
                            echo "$fout :: $rest" >> $RefFile
                            print -n "o"
                        else
                            # not writing RefFile: we (already) *read* it
                            got=$(eval $ZET_CHEK[$fout])
                            want=$ZET_WANT[$fout]
                            if [[ "$got" = "$want" ]]; then
                                print -n "."
                            else
                                print -n "X"
                                FAIL_CMD+=($cmd)
                                # preserve the input to the failed command
                                zet_unjunk $fin
                            fi
                        fi
                    done
                done
            done
        done
    done
    print ""
    # zet_stop
    if (( ! ${+ZET_WRITE_REF} )); then
        # not writing ref = doing test
        if (( ${#FAIL_CMD} > 0 )); then
            print "Failed commands:"
            for cmd in "${FAIL_CMD[@]}"; do
                print -- "$cmd"
            done
        else
            print "All tests passed."
        fi
    fi
}
Test

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
fi

exit 0
