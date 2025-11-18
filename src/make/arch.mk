#
# Teem: Tools to process and visualize scientific data and images
# Copyright (C) 2009--2025  University of Chicago
# Copyright (C) 2005--2008  Gordon Kindlmann
# Copyright (C) 1998--2004  University of Utah
#
# This library is free software; you can redistribute it and/or modify it under the terms
# of the GNU Lesser General Public License (LGPL) as published by the Free Software
# Foundation; either version 2.1 of the License, or (at your option) any later version.
# The terms of redistributing and/or modifying this software also include exceptions to
# the LGPL that facilitate static linking.
#
# This library is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, see <https://www.gnu.org/licenses/>.
#

#### See NOTE in ../GNUMakefile about the big simplification to the non-CMake build
#### process made for TeemV2.  This new file contains all the architecture-specific
#### details about how to compile (on a non-Windows machine). Here there is only the
#### most bare-bones platform-specific logic: some variables are set differently for
#### Mac vs Linux

### Maybe compile with some of the clang sanitizers
# Entirely separately, there is the static analyzer:
#    https://clang.llvm.org/docs/ClangStaticAnalyzer.html
# which is run differently, via:
#    CC='scan-build clang' make

SAN_LIST :=
ifneq ($(origin ubsan),undefined)
  # For UBSan: https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
  # compile with:
  #    ubsan= make
  SAN_LIST += undefined
endif
ifneq ($(origin asan),undefined)
  # For ASan: https://clang.llvm.org/docs/AddressSanitizer.html
  # compile with:
  #    asan= make
  SAN_LIST += address
endif
ifneq ($(origin lsan),undefined)
  # for LSan: https://clang.llvm.org/docs/LeakSanitizer.html
  # compile with:
  #    lsan= make
  SAN_LIST += leak
endif
ifneq ($(origin msan),undefined)
  # for MSan: https://clang.llvm.org/docs/MemorySanitizer.html
  # compile with:
  #    msan= make
  SAN_LIST += memory
endif
ifneq ($(SAN_LIST),)
  # if we have sanitizers to use, make the SAN_FLAG that lists them
  comma := ,
  # `$() $()` is a way to express space ` ` h/t https://stackoverflow.com/a/51874794
  SAN_FLAG := -fsanitize=$(subst $() $(),$(comma),$(strip $(SAN_LIST)))
  # The Clang sanitizer docs say to use clang as cc (and ld)
  CC := clang
else
  SAN_FLAG :=
endif

# yes, we should probably be using more ?= instead = for the assignments below,
# but the intent here is to be clear and explicit.  The assignment "AR ?= libtool"
# may not do anything because "AR" may default to "ar" for the sake of
# GNUmake's implicit rules, which is annoying because implicit rules are
# annoying. If you want different values for these variables, then change them.

CC ?= cc
ifeq ($(shell uname -s),Darwin)
  # on mac
  AR := libtool
  ARFLAGS := -static -o
  # if non-empty: also delete X$(LITTER) when deleting executable X with "make clean"
  LITTER := .dSYM
else
  # assume on linux
  AR := ar
  ARFLAGS := ru
  LITTER :=
endif
# sometimes needed after `ar` to store index in library
RANLIB :=
RM := rm -f
CP := cp
CHMOD := chmod

# other $(CC) flags for .c compilation (e.g. optimization and warnings)
# Any existing CFLAGS variable (e.g. from environment) is added at end
CFLAGS := -O3 -g -W -Wall -Wextra $(SAN_FLAG) $(CFLAGS)

# further $(CC) flags for creating executables when linking with (static) library
BIN_CFLAGS :=

# more flags to try:
# -std=c99 -pedantic -Wno-long-long -Wno-overlength-strings -Wstrict-aliasing=2 -Wstrict-overflow=5
# -Weverything -Wno-poison-system-directories -Wno-padded -Wno-format-nonliteral -Wno-float-equal -Wno-reserved-id-macro
## for trying undefined behavior flagging  -fsanitize=undefined
