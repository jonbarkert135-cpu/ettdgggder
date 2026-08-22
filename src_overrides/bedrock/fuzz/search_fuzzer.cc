// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Fuzzes search engine selection (roadmap item 76).
//
// The inputs here are attacker-adjacent in an indirect but real way: engine ids
// come from a profile that may have been edited by hand, imported, or written
// by an older version, and the bang comes from whatever the user typed. The
// property that matters is not "does not crash" but "never returns nothing":
// a search that resolves to no engine is a broken address bar, and the usual
// fix a user finds is to install a search extension.

#include "bedrock/fuzz/fuzz_main.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "bedrock/search/engine_selector.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string text(reinterpret_cast<const char*>(data), size);

  std::vector<std::string> fields;
  std::string current;
  for (char c : text) {
    if (c == '\n') {
      fields.push_back(current);
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  fields.push_back(current);

  auto field = [&fields](size_t index) -> std::string {
    return index < fields.size() ? fields[index] : std::string();
  };

  bedrock::SearchPrefs prefs;
  prefs.default_engine = field(0);
  prefs.normal_engine = field(1);
  prefs.private_engine = field(2);
  prefs.omnibox_engine = field(3);
  prefs.suggestions_enabled = !text.empty() && (text[0] & 1);

  std::vector<std::string> available;
  for (size_t i = 4; i < fields.size() && i < 32; ++i)
    available.push_back(fields[i]);

  const std::string fallback = "duckduckgo";
  for (bedrock::SearchContext context :
       {bedrock::SearchContext::kNormal, bedrock::SearchContext::kPrivate,
        bedrock::SearchContext::kOmnibox, bedrock::SearchContext::kBang}) {
    const std::string engine =
        bedrock::SelectEngine(context, prefs, field(0), available, fallback);
    if (engine.empty())
      __builtin_trap();  // there is always an engine, or the address bar dies

    // Suggestions leak every keystroke; private windows must never allow them,
    // whatever the prefs say.
    if (context == bedrock::SearchContext::kPrivate &&
        bedrock::SuggestionsAllowed(context, prefs))
      __builtin_trap();
  }
  return 0;
}
