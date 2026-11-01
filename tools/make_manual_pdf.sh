#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
# Arctic Scream p0larpatch: the visual manual as an A4 PDF (docs/manual/arctic-scream-manual.pdf), printed by a headless
# Chrome from web/dist/manual.html (web/build_web.sh first).  Run from the repository:  tools/make_manual_pdf.sh
set -e
CHROME=${CHROME:-"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"}
[ -f web/dist/manual.html ] || { echo "web/dist/manual.html missing: run web/build_web.sh"; exit 1; }
"$CHROME" --headless=new --disable-gpu --no-pdf-header-footer --virtual-time-budget=4000 \
    --print-to-pdf="$PWD/docs/manual/arctic-scream-manual.pdf" "file://$PWD/web/dist/manual.html" 2>/dev/null
echo "manual: docs/manual/arctic-scream-manual.pdf ($(wc -c < docs/manual/arctic-scream-manual.pdf | tr -d ' ') B)"
