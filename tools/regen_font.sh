#!/bin/sh
# Regenerates mwe_font.h exactly as shipped in v0.6 (needs: pip install fonttools).
# Every glyph comes from the Noto Sans family:
#   Latin/Greek/Cyrillic (Regular+Bold) and Simplified Chinese (GB2312) from Noto Sans CJK SC,
#   Hangul (KS X 1001, 2350 syllables + jamo) from Noto Sans CJK KR.
# Usage: tools/regen_font.sh NotoSansCJK-Regular.ttc NotoSansCJK-Bold.ttc [cjk-level 1|2]
set -e
L=${3:-2}
python3 "$(dirname "$0")/genfont.py" "$1" "$2" --latin-index 2 \
  --name "Noto Sans CJK SC+KR 2.004 Regular+Bold (GB2312 level $L, KS X 1001)" \
  --cjk "$1" --cjk-index 2 --cjk-level "$L" --hangul "$1" --hangul-index 1 \
  --notice "Noto Sans CJK SC/KR: Copyright 2014-2021 Adobe (http://www.adobe.com/). Noto is a trademark of Google Inc. Licensed under the SIL Open Font License 1.1." \
  -o "$(dirname "$0")/../mwe_font.h"
