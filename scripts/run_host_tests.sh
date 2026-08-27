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
  # Fuzz harnesses each define LLVMFuzzerTestOneInput and are linked separately.
  case "$src" in *_test.cc|src_overrides/bedrock/fuzz/*) continue;; esac
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

# Fuzz harnesses: prove each still builds and survives the seed corpus.
for fuzzer in src_overrides/bedrock/fuzz/*_fuzzer.cc; do
  name=$(basename "${fuzzer%.cc}")
  echo "== $name"
  "${cxx[@]}" "$fuzzer" src_overrides/bedrock/fuzz/fuzz_smoke_main.cc "${objs[@]}" \
      -o "$out/$name"
  "$out/$name" || status=1
done

python3 scripts/check_fp_docs.py || status=1
python3 scripts/check_ui_style.py || status=1
python3 scripts/check_catalog.py || status=1
python3 scripts/check_security_testing.py || status=1
python3 scripts/check_perf_claims.py || status=1
python3 scripts/check_no_telemetry.py || status=1
python3 scripts/check_languages.py || status=1
python3 scripts/check_no_fake_features.py || status=1
python3 scripts/check_test_matrix.py --selftest || status=1
python3 scripts/check_test_matrix.py || status=1
python3 scripts/check_privacy_suite.py --selftest || status=1
python3 scripts/check_privacy_suite.py || status=1
python3 tests/browser/run.py --selftest || status=1
python3 tests/privacy/run.py --selftest || status=1
python3 scripts/check_frameworks.py --selftest || status=1
python3 scripts/check_frameworks.py || status=1
python3 scripts/check_diagnostics.py --selftest || status=1
python3 scripts/check_diagnostics.py || status=1
python3 scripts/check_transparency.py --selftest || status=1
python3 scripts/check_transparency.py || status=1
python3 scripts/check_defaults.py --selftest || status=1
python3 scripts/check_defaults.py || status=1
python3 scripts/check_adr.py --selftest || status=1
python3 scripts/check_adr.py || status=1
python3 scripts/check_phases.py --selftest || status=1
python3 scripts/check_phases.py || status=1
python3 scripts/check_docs.py --selftest || status=1
python3 scripts/check_docs.py || status=1
python3 scripts/check_releases.py --selftest || status=1
python3 scripts/check_releases.py || status=1
python3 scripts/verify_release.py --selftest || status=1
python3 scripts/check_branding.py --selftest || status=1
python3 scripts/check_branding.py || status=1
python3 scripts/check_trademarks.py --selftest || status=1
python3 scripts/check_trademarks.py || status=1
python3 scripts/check_remote_features.py --selftest || status=1
python3 scripts/check_remote_features.py || status=1
python3 scripts/check_upstream.py --selftest || status=1
python3 scripts/check_upstream.py || status=1
python3 scripts/upstream_sync.py --selftest || status=1
python3 scripts/check_strings.py --selftest || status=1
python3 scripts/check_strings.py || status=1
python3 scripts/check_platform.py --selftest || status=1
python3 scripts/check_platform.py || status=1
python3 scripts/check_config_surface.py --selftest || status=1
python3 scripts/check_config_surface.py || status=1
python3 scripts/check_open_source.py || status=1
python3 scripts/generate_sbom.py --check || status=1
python3 scripts/check_dns_presets.py --selftest || status=1
python3 scripts/check_dns_presets.py || status=1
python3 scripts/check_host_matching.py --selftest || status=1
python3 scripts/check_host_matching.py || status=1
python3 scripts/check_toolchain_limits.py --selftest || status=1
python3 scripts/check_toolchain_limits.py || status=1
python3 scripts/check_tokens.py || status=1
python3 scripts/gen_theme_css.py --selftest || status=1
python3 scripts/gen_theme_css.py || status=1
python3 scripts/gen_build_gn.py --selftest || status=1
python3 scripts/gen_build_gn.py || status=1

python3 scripts/check_memory.py --selftest || status=1

# Project memory. On a pull request GitHub sets GITHUB_BASE_REF, and the gate
# additionally requires that this change updated the memory (see
# .ai/memory/PROTOCOL.md). Locally it just checks the memory is consistent.
if [ -n "${GITHUB_BASE_REF:-}" ]; then
  git fetch --no-tags --quiet origin "$GITHUB_BASE_REF" || true
  python3 scripts/check_memory.py --base "origin/$GITHUB_BASE_REF" || status=1
else
  python3 scripts/check_memory.py || status=1
fi

exit $status
