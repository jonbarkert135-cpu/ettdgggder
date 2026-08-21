// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_FUZZ_FUZZ_MAIN_H_
#define BEDROCK_FUZZ_FUZZ_MAIN_H_

#include <cstddef>
#include <cstdint>

// Fuzz harnesses (roadmap item 43).
//
// Each harness is a normal libFuzzer entry point: build it with
// `-fsanitize=fuzzer,address,undefined` (see `build/args/bedrock-fuzz.gn`) and
// it fuzzes for as long as you let it.
//
// The same file also builds in CI without a fuzzing toolchain. Compiling with
// `-DBEDROCK_FUZZ_SMOKE` links a `main()` that replays the checked-in seed
// corpus plus a set of generated inputs, so every harness is *proven to still
// compile and not crash on known-bad input* on every commit. A fuzzer that
// silently stopped building six months ago is not a fuzzer, and that is the
// usual failure mode — not a missing fuzzer, but a rotten one.

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

#endif  // BEDROCK_FUZZ_FUZZ_MAIN_H_
