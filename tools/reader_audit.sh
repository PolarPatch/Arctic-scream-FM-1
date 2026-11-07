#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
# Arctic Scream p0larpatch: the screen reader's words checked (fm1-reader's tools/audit.mjs, the rules in its
# skill/fm1-screen-reader/RULES.md). Needs fm1-reader next to this repository (../fm1-reader) and node.
#   sh tools/reader_audit.sh [--spoken | --all]     (--spoken: every message as it will be heard)
# It builds the host, then collects: every page (each source's), its knobs through their range, the lists' rows
# (GRIMM_SPEAKDUMP); the header's messages (hd_say in the sources); what a walk through the pages says
# (tests/reader_walk.txt, GRIMM_SAID_LOG); and audits them with the words file firmware/words.tsv makes.
set -e
cd "$(dirname "$0")/.."
FR="${FM1_READER:-../fm1-reader}"
[ -f "$FR/tools/audit.mjs" ] || { echo "reader_audit: no fm1-reader at $FR (FM1_READER)"; exit 2; }
D=build/sr
mkdir -p $D
python3 tools/gen_words.py firmware/words.tsv web/words firmware/gen/ann_words.h >/dev/null
cc -O1 -w -DDINGHY_HOST -DAPP_VER="\"${APP_VER:-1.5}\"" "-Dsection(x)=used" -Ifirmware/src -Ifirmware/gen \
   -o $D/grimm firmware/src/grimm.c
printf '8000 end\n' > $D/none.txt
GRIMM_SPEAKDUMP=1 $D/grimm $D/dump $D/none.txt | grep -E "^[A-Z]+	" | sort -u > $D/dump.tsv
python3 - > $D/msg.tsv <<'PY'
import re, pathlib
W = ("NOT ", "NOTHING", "EMPTY", "STOP AND", "CPU FULL", "SAVE ERROR", "NO ")
for f in pathlib.Path("firmware/src").glob("*.c"):
    for m in re.finditer(r'hd_say\(([^;]*)\);', f.read_text()):
        for s in re.findall(r'"([^"]*)"', m.group(1)):
            if s and s.strip():
                print(("WARN" if s.startswith(W) else "MSG") + "\t" + s)
PY
rm -f $D/said.tsv
GRIMM_LISTEN=1 GRIMM_SAID_LOG=$PWD/$D/said.tsv $D/grimm $D/walk tests/reader_walk.txt >/dev/null
sort -u $D/msg.tsv $D/said.tsv | cat $D/dump.tsv - > $D/all.tsv
ID=$(sed -n 's/.*words\/\([0-9a-f]*\)\.json.*/\1/p' firmware/gen/ann_words.h)
node "$FR/tools/audit.mjs" $D/all.tsv --words web/words/$ID.json --allow tools/reader_allow.txt "$@"
