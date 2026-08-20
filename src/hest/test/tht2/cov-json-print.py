#!/usr/bin/env python3
# This file was first written by an LLM so it is not copyrightable; subsequent human
# tweaks do not change that. Link with source, assuming GLK's account:
# https://claude.ai/chat/753238c4-c4d1-4241-824a-c1a310956286

# this was created to diagnose a mysteriously missing uncovered region
import json, sys

data = json.load(sys.stdin)
for func in data['data'][0]['functions']:
    for region in func.get('regions', []):
        # region: [lineStart, colStart, lineEnd, colEnd, execCount, fileID, expandedFileID, kind]
        line_start, col_start, line_end, col_end = region[0], region[1], region[2], region[3]
        exec_count = region[4]
        file_id = region[5]
        kind = region[7]   # 0=Code, 1=Expansion, 2=Skipped, 3=Gap, 4=Branch
        filename = func['filenames'][file_id]
        if 'argvHest' in filename and exec_count == 0 and kind == 0:
            print(
                f"{func['name']:40s}  {filename.split('/')[-1]}:{line_start}:{col_start}-{line_end}:{col_end}"
            )
