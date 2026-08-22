// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Fuzzes omnibox input classification (roadmap item 43: URL parser). Every
// keystroke in the address bar reaches this code, and pasted input can be
// anything at all — including data crafted by the page the user just left.

#include "bedrock/fuzz/fuzz_main.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "bedrock/omnibox/input_parser.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);
  const std::vector<std::string> bangs = {"!g", "!ddg", "!w", "!yt"};
  bedrock::ParseOmniboxInput(input, bangs);
  bedrock::LooksLikeUrl(input);
  return 0;
}
