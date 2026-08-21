#!/usr/bin/env bash
# Build and run the dependency-free host tests (no Chromium checkout needed).
set -euo pipefail
shopt -s globstar
cd "$(dirname "$0")/.."
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
status=0
for test in src_overrides/bedrock/**/*_test.cc; do
  src=${test%_test.cc}.cc
  name=$(basename "${test%_test.cc}")
  echo "== $name"
  g++ -std=c++17 -Wall -Wextra -Werror -I src_overrides "$test" "$src" -o "$out/$name"
  "$out/$name" || status=1
done
exit $status
