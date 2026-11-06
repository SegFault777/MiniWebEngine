#!/bin/sh
# Regenerates mwe_font.h exactly as shipped in v0.4 (needs: pip install fonttools).
# Usage: tools/regen_font.sh Inter-Regular.otf Inter-Bold.otf NotoSansCJK-Regular.ttc [cjk-level 1|2]
set -e
L=${4:-2}
python3 "$(dirname "$0")/genfont.py" "$1" "$2" \
  --name "Inter 4.000 Regular+Bold + Noto Sans CJK SC (GB2312 level $L)" \
  --cjk "$3" --cjk-index 2 --cjk-level "$L" \
  --notice "Inter: Copyright 2016 The Inter Project Authors (https://github.com/rsms/inter). Licensed under the SIL Open Font License 1.1." \
  --notice "Noto Sans CJK SC: Copyright 2014-2021 Adobe (http://www.adobe.com/). Noto is a trademark of Google Inc. Licensed under the SIL Open Font License 1.1." \
  -o "$(dirname "$0")/../mwe_font.h"
