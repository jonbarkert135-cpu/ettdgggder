#!/usr/bin/env bash
# Build and run the dependency-free host tests (no Chromium checkout needed).
set -euo pipefail
shopt -s globstar
cd "$(dirname "$0")/.."
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
status=0
cxx=(g++ -std=c++17 -O1 -Wall -Wextra -Werror -I src_overrides)

# Compile every non-test source once, then link each test against all of them.
# Tests span several components on purpose (the blocking pipeline test drives
# the real filter engine and heuristic), and recompiling the tree per test made
# the suite scale quadratically.
objs=()
for src in src_overrides/bedrock/**/*.cc; do
  case "$src" in *_test.cc) continue;; esac
  obj="$out/$(echo "${src#src_overrides/}" | tr / _).o"
  "${cxx[@]}" -c "$src" -o "$obj"
  objs+=("$obj")
done

for test in src_overrides/bedrock/**/*_test.cc; do
  name=$(basename "${test%_test.cc}")
  echo "== $name"
  "${cxx[@]}" "$test" "${objs[@]}" -o "$out/$name"
  "$out/$name" || status=1
done

python3 scripts/check_fp_docs.py || status=1
python3 scripts/check_ui_style.py || status=1
python3 scripts/check_catalog.py || status=1

exit $status
