_Teem_add_library(${CMAKE_CURRENT_SOURCE_DIR}
  SOURCES
    convertDye.c methodsDye.c
    # private header
    privateDye.h
  PUBLIC_HEADERS
    dye.h
)
