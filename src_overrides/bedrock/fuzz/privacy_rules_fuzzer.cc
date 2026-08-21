// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Fuzzes privacy rule resolution and storage keying (roadmap item 76).
//
// Two things are fuzzed together because they fail together. `Conflicts()` is
// the assertion that a resolved policy is internally consistent — that a Tor
// window never ends up with third-party cookies allowed because a site
// override was applied in the wrong order. `KeyFor()` decides which partition
// a page's storage lands in, from host strings the page controls: an origin
// that produces the same key as another origin is a cross-site leak, and it
// would not crash anything, which is exactly why it needs a fuzzer that checks
// a property rather than one that waits for a segfault.

#include "bedrock/fuzz/fuzz_main.h"

#include <string>

#include "bedrock/privacy/core/privacy_policy.h"
#include "bedrock/privacy/storage/storage_isolation.h"

namespace {

std::string Field(const std::string& text, size_t index) {
  size_t start = 0;
  for (size_t i = 0; i < index; ++i) {
    const size_t next = text.find('\n', start);
    if (next == std::string::npos)
      return std::string();
    start = next + 1;
  }
  const size_t end = text.find('\n', start);
  return text.substr(start, end == std::string::npos ? end : end - start);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string text(reinterpret_cast<const char*>(data), size);
  const uint8_t selector = size ? data[0] : 0;

  using namespace bedrock::privacy;

  Resolution resolution;
  resolution.top_level_site = Field(text, 0);
  resolution.mode = static_cast<bedrock::session::BrowsingMode>(selector % 3);
  resolution.block_ads = selector & 0x02;
  resolution.block_trackers = selector & 0x04;
  resolution.behavioral_detection = selector & 0x08;
  resolution.fingerprint = static_cast<FpLevel>((selector >> 4) % 4);
  resolution.cookies = static_cast<CookieMode>((selector >> 1) % 4);
  resolution.storage = static_cast<bedrock::net::IsolationLevel>((selector >> 2) % 3);
  resolution.referrer = static_cast<ReferrerMode>((selector >> 3) % 3);
  resolution.scripts = static_cast<ScriptMode>((selector >> 5) % 3);

  // Both are pure functions of the resolution; neither may crash, and Explain
  // must produce a line for the panel whatever the combination is.
  PrivacyPolicy::Conflicts(resolution);
  if (PrivacyPolicy::Explain(resolution).empty())
    __builtin_trap();

  // Storage keying with hostile host strings.
  bedrock::net::StorageIsolation isolation(resolution.storage);
  const std::string origin = Field(text, 1);
  const std::string origin_site = Field(text, 2);
  const std::string top_site = Field(text, 3);
  const bedrock::net::StorageKey key =
      isolation.KeyFor(origin, origin_site, top_site);

  // The property that matters: a key must remember the top-level site it was
  // made for. If two different top-level sites ever produced the same
  // serialisation for the same origin, third-party storage would be shared
  // across sites — the tracking mechanism itself.
  const bedrock::net::StorageKey other =
      isolation.KeyFor(origin, origin_site, top_site + "x");
  if (key.Serialize() == other.Serialize())
    __builtin_trap();
  return 0;
}
