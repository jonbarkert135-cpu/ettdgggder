// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/search/engine_selector.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

using bedrock::SearchContext;
using bedrock::SearchPrefs;

const std::vector<std::string> kAvailable = {"duckduckgo", "google", "brave"};
constexpr char kFallback[] = "duckduckgo";
int failures = 0;

void Expect(const char* what, const std::string& got, const std::string& want) {
  if (got != want) {
    std::cerr << "FAIL " << what << ": got [" << got << "] want [" << want << "]\n";
    ++failures;
  }
}

std::string Select(SearchContext context, const SearchPrefs& prefs,
                   const std::string& bang = "") {
  return bedrock::SelectEngine(context, prefs, bang, kAvailable, kFallback);
}

}  // namespace

int main() {
  SearchPrefs prefs;
  prefs.default_engine = "duckduckgo";

  // Only the default is set: every context inherits it.
  Expect("normal inherits", Select(SearchContext::kNormal, prefs), "duckduckgo");
  Expect("private inherits", Select(SearchContext::kPrivate, prefs), "duckduckgo");
  Expect("omnibox inherits", Select(SearchContext::kOmnibox, prefs), "duckduckgo");

  // Per-context overrides.
  prefs.normal_engine = "google";
  prefs.private_engine = "brave";
  Expect("normal override", Select(SearchContext::kNormal, prefs), "google");
  Expect("private override", Select(SearchContext::kPrivate, prefs), "brave");
  Expect("omnibox falls back to normal", Select(SearchContext::kOmnibox, prefs), "google");

  prefs.omnibox_engine = "duckduckgo";
  Expect("omnibox override", Select(SearchContext::kOmnibox, prefs), "duckduckgo");

  // A bang beats every pref, in any context.
  Expect("bang wins", Select(SearchContext::kNormal, prefs, "brave"), "brave");
  Expect("bang wins in private", Select(SearchContext::kPrivate, prefs, "google"), "google");

  // Stale prefs (engine uninstalled) fall through instead of breaking search.
  prefs.normal_engine = "removed-engine";
  Expect("stale normal falls back to default",
         Select(SearchContext::kNormal, prefs), "duckduckgo");
  prefs.default_engine = "also-removed";
  Expect("stale default falls back to builtin",
         Select(SearchContext::kNormal, prefs), "duckduckgo");
  Expect("unknown bang ignored",
         Select(SearchContext::kNormal, prefs, "nope"), "duckduckgo");

  // Suggestions never run in private windows.
  prefs.suggestions_enabled = true;
  if (!bedrock::SuggestionsAllowed(SearchContext::kNormal, prefs)) {
    std::cerr << "FAIL suggestions should be on in normal windows\n";
    ++failures;
  }
  if (bedrock::SuggestionsAllowed(SearchContext::kPrivate, prefs)) {
    std::cerr << "FAIL suggestions must be off in private windows\n";
    ++failures;
  }

  if (failures == 0) {
    std::cout << "engine_selector_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
