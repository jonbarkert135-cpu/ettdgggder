// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Fuzzes the filter list parser and matcher (roadmap item 43: content
// blocking). Filter lists are attacker-adjacent input: a user can subscribe to
// any list on the internet, and a list is just text that our parser trusts to
// be well formed. It is not.

#include "bedrock/fuzz/fuzz_main.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "bedrock/privacy/tracker_blocker/filter_engine.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string text(reinterpret_cast<const char*>(data), size);
  bedrock::blocking::FilterEngine engine;
  engine.AddList(text);

  // Matching against the parsed rules is where an inconsistent parse turns
  // into a crash, so the harness exercises both halves.
  bedrock::blocking::Request request;
  request.url = "https://example.com/" + text.substr(0, 64);
  request.host = "example.com";
  request.etld1 = "example.com";
  request.top_host = "site.test";
  request.top_etld1 = "site.test";
  request.type = bedrock::blocking::ResourceType::kScript;
  engine.Match(request);
  return 0;
}
