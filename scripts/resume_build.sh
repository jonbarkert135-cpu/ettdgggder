#!/usr/bin/env bash
# Resume the local Chromium+Bedrock build without rebuilding anything from scratch.
# Read build/LOCAL_BUILD_HANDOFF.md first. Safe to re-run; it never runs `gn clean`.
#
# Workspace layout expected (override with BEDROCK_WORKSPACE):
#   $BEDROCK_WORKSPACE/depot_tools   $BEDROCK_WORKSPACE/src   $BEDROCK_WORKSPACE/src/out/Release
set -u

WORKSPACE="${BEDROCK_WORKSPACE:-/work/chromium}"
SRC="$WORKSPACE/src"
OUT="$SRC/out/Release"
export PATH=/work/tools/bin:"$WORKSPACE/depot_tools":$PATH

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
# --workspace is required by build/sync.py; omitting it exits 5.
python3 "$(dirname "$0")/../build/sync.py" --workspace "$WORKSPACE" --overlay-only

step "build"
# autoninja is the normal path. If siso stalls (>700% CPU, no compiler spawned,
# "schedule pending:1+ready:0"), kill it and use scripts/manual_compile.py +
# scripts/manual_link.py instead — see build/LOCAL_BUILD_HANDOFF.md §4.8.
cd "$SRC" && autoninja -C out/Release chrome
rc=$?
echo "BUILD_RC=$rc"
[ "$rc" -eq 0 ] || exit $rc

step "verify: overlay symbols"
nm -C "$OUT/chrome" | grep 'bedrock::' | head

step "verify: the binary actually starts"
# Headless (--dump-dom/--screenshot) hangs in sandboxes without D-Bus, GPU or
# network. Asking the running browser over the DevTools port is reliable:
# it proves process startup, and the [bedrock] lines prove the overlay ran.
PORT="${BEDROCK_DEVTOOLS_PORT:-9333}"
rm -rf /tmp/bedrock-verify
LAUNCH_LOG=$(mktemp)
( cd "$OUT" && LD_LIBRARY_PATH="$OUT" ./chrome \
    --user-data-dir=/tmp/bedrock-verify --enable-logging=stderr --no-sandbox \
    --headless=new --disable-gpu --disable-dev-shm-usage --no-first-run \
    --disable-component-update --disable-sync --disable-background-networking \
    --remote-debugging-port="$PORT" --remote-allow-origins='*' \
    about:blank > "$LAUNCH_LOG" 2>&1 ) &
launch_pid=$!
version=""
for _ in $(seq 1 60); do
  version=$(curl -s --max-time 2 "http://127.0.0.1:$PORT/json/version" || true)
  [ -n "$version" ] && break
  sleep 1
done
if [ -n "$version" ]; then
  echo "devtools says: $version"
else
  echo "browser did not answer on 127.0.0.1:$PORT within 60s" >&2
fi
grep '\[bedrock\]' "$LAUNCH_LOG" | head
kill "$launch_pid" 2>/dev/null
wait "$launch_pid" 2>/dev/null
rm -f "$LAUNCH_LOG"

[ -n "$version" ] || exit 1
exit 0
