#!/bin/sh
# Regenerates mwe_font.h exactly as shipped in v0.5 (needs: pip install fonttools).
# Every glyph comes from the Noto Sans family: Noto Sans CJK SC (Latin, Greek, Cyrillic, Chinese).
# Usage: tools/regen_font.sh NotoSansCJK-Regular.ttc NotoSansCJK-Bold.ttc [cjk-level 1|2]
# If you have the Latin-only "Noto Sans" (NotoSans-Regular.ttf / NotoSans-Bold.ttf), pass those as
# the Latin pair by calling genfont.py directly (see fonts/README.md).
set -e
L=${3:-2}
python3 "$(dirname "$0")/genfont.py" "$1" "$2" --latin-index 2 \
  --name "Noto Sans CJK SC 2.004 Regular+Bold (GB2312 level $L)" \
  --cjk "$1" --cjk-index 2 --cjk-level "$L" \
  --notice "Noto Sans CJK SC: Copyright 2014-2021 Adobe (http://www.adobe.com/). Noto is a trademark of Google Inc. Licensed under the SIL Open Font License 1.1." \
  -o "$(dirname "$0")/../mwe_font.h"
