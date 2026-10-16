_Teem_add_library(${CMAKE_CURRENT_SOURCE_DIR}
  # NOTE: teem/src/_util/gen_biffdata.py expects both "SOURCES" and "PUBLIC_HEADERS"
  # to be on lines by themselves (modulo whitespace) as THE way to list all .c files
  SOURCES
    # The file list here is pasted in (on Mac) after:
    #    git status *.c   # to make sure we don't have untracked .c files
    #    echo *.c | fold -s -w 80 | sed -e 's/^/    /' | sed -e 's/ *$//' | tee list.txt | pbcopy
    # And then, to make the file list to paste into GNUmakefile $(L).Obj:
    #    sed -e 's/$/ \\/' list.txt | pbcopy; rm -f list.txt
    754.c array.c endianAir.c enum.c fpio.c heap.c math.c miscAir.c mop.c
    parseAir.c randJSF.c randMT.c sane.c string.c threadAir.c
    # private header
    privateAir.h
  PUBLIC_HEADERS
    air.h
)
