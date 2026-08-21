#!/usr/bin/env bash
# Build and run the dependency-free host tests (no Chromium checkout needed).
set -euo pipefail
shopt -s globstar
cd "$(dirname "$0")/.."
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
status=0
# Link every non-test source: some tests span several components on purpose
# (the blocking pipeline test drives the real filter engine and heuristic).
srcs=$(ls src_overrides/bedrock/**/*.cc | grep -v '_test\.cc$')
for test in src_overrides/bedrock/**/*_test.cc; do
  name=$(basename "${test%_test.cc}")
  echo "== $name"
  g++ -std=c++17 -O2 -Wall -Wextra -Werror -I src_overrides "$test" $srcs -o "$out/$name"
  "$out/$name" || status=1
done
python3 scripts/check_fp_docs.py || status=1

exit $status
