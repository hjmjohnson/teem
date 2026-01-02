#!/usr/bin/env bash
#
# Teem: Tools to process and visualize scientific data and images
# Copyright (C) 2009--2026  University of Chicago
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

set -o errexit
set -o nounset
shopt -s expand_aliases

shopt -s nullglob
TODO=$(echo *.h *.c)
if [ "$TODO" = "" ]; then
    echo "Error: this expects to be be run in a directory with .c and .h files"
    exit 1
fi

rm -rf /tmp/tmp

echo "Running over files: $TODO"
for F in $TODO; do
    echo ================= $F
    clang-format -style=file < $F > /tmp/tmp
    set +o errexit
    diff $F /tmp/tmp
    RET=$?
    set -o errexit
    if [ $RET -ne 0 ]; then
        mv /tmp/tmp $F
    fi
done
