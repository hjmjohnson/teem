_Teem_add_library(${CMAKE_CURRENT_SOURCE_DIR}
  SOURCES
    enumall.c meetGage.c meetNrrd.c meetPull.c
    # private header
    privateMeet.h
  PUBLIC_HEADERS
    meet.h
)
