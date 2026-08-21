#!/usr/bin/env python3
# This file was first written by an LLM so it is not copyrightable; subsequent human
# tweaks do not change that. Link with source, assuming GLK's account:
# https://claude.ai/chat/16fae4e4-4bba-4965-8507-26292ccff3d6

# this was created to diagnose a mysteriously missing uncovered region
import json, sys

data = json.load(sys.stdin)
for func in data['data'][0]['functions']:
    for region in func.get('regions', []):
        # (GLK) as per line ~96 of
        # https://github.com/llvm/llvm-project/blob/main/llvm/tools/llvm-cov/CoverageExporterJson.cpp
        # a "region" has these fields:
        #      0          1        2        3        4        5          6           7
        # [lineStart, colStart, lineEnd, colEnd, execCount, fileID, expandedFileID, kind]
        line_start, col_start, line_end, col_end = region[0], region[1], region[2], region[3]
        exec_count = region[4]
        file_id = region[5]
        # (GLK) as per line ~233 of
        # https://github.com/llvm/llvm-project/blob/main/llvm/include/llvm/ProfileData/Coverage/CoverageMapping.h
        # the RegionKind enum defines the meanings of the integer "kind":
        # 0=Code, 1=Expansion, 2=Skipped, 3=Gap, 4=Branch
        kind = region[7]
        filename = func['filenames'][file_id]
        # (GLK) was: if 'argvHest' in filename and exec_count == 0 and kind == 0:
        if line_start == line_end and col_start == col_end and exec_count == 0 and kind == 0:
            fname = f"{func['name']}()"
            print(
                f"{fname:40s}  {filename.split('/')[-1]}:{line_start}:{col_start}-{line_end}:{col_end}"
            )
