#!/usr/bin/env bash
# Resume the local Chromium+Bedrock build without rebuilding anything from scratch.
# Read build/LOCAL_BUILD_HANDOFF.md first. Safe to re-run; it never runs `gn clean`.
set -u

SRC=/work/chromium/src
OUT="$SRC/out/Release"
export PATH=/work/tools/bin:/work/depot_tools:$PATH

step() { printf '\n== %s\n' "$1"; }

if [ ! -d "$OUT" ]; then
  echo "No $OUT — the 8.7 GB build directory is gone. See build/LOCAL_BUILD_HANDOFF.md" >&2
  echo "before starting a 12-hour full build." >&2
  exit 1
fi

step "state"
ls -l "$OUT/chrome" 2>/dev/null || echo "no chrome binary yet (a failed link deletes it)"
echo "bedrock:: symbols in binary: $(nm -C "$OUT/chrome" 2>/dev/null | grep -c 'bedrock::' || echo 0)"

step "sync overlay into the Chromium tree (patches + symlinks)"
python3 "$(dirname "$0")/../build/sync.py" --overlay-only

step "build"
# autoninja is the normal path. If siso stalls (>700% CPU, no compiler spawned,
# "schedule pending:1+ready:0"), kill it and use scripts/manual_compile.py +
# scripts/manual_link.py instead — see build/LOCAL_BUILD_HANDOFF.md §4.8.
cd "$SRC" && autoninja -C out/Release chrome
rc=$?

step "verify"
nm -C "$OUT/chrome" | grep 'bedrock::' | head
rm -rf /tmp/bedrock-verify
(cd "$OUT" && LD_LIBRARY_PATH="$OUT" timeout 90 ./chrome \
  --user-data-dir=/tmp/bedrock-verify --enable-logging=stderr --no-sandbox \
  --headless=new --disable-gpu --virtual-time-budget=4000 about:blank 2>&1) \
  | grep '\[bedrock\]'

exit $rc
