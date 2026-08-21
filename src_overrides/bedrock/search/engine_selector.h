// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_SEARCH_ENGINE_SELECTOR_H_
#define BEDROCK_SEARCH_ENGINE_SELECTOR_H_

#include <string>
#include <vector>

// Which engine handles a given search, per Settings -> Search. Pure logic, no
// Chromium types, so it is host-testable. BedrockSearchService reads the prefs,
// calls SelectEngine(), and hands the result to TemplateURLService.

namespace bedrock {

enum class SearchContext {
  kNormal,    // search from a normal window / search box
  kPrivate,   // search from a private window
  kOmnibox,   // typed in the address bar (normal window)
  kBang,      // an explicit !bang was used — always wins
};

// Pref keys, registered in the profile pref registry. Empty value = "inherit
// the default engine", which is what keeps a single-engine setup working
// without the user touching three settings.
inline constexpr char kPrefDefaultEngine[] = "bedrock.search.default";
inline constexpr char kPrefNormalEngine[] = "bedrock.search.normal";
inline constexpr char kPrefPrivateEngine[] = "bedrock.search.private";
inline constexpr char kPrefOmniboxEngine[] = "bedrock.search.omnibox";
inline constexpr char kPrefSuggestionsEnabled[] = "bedrock.search.suggestions";

struct SearchPrefs {
  std::string default_engine;   // must be a known engine id
  std::string normal_engine;    // empty => default_engine
  std::string private_engine;   // empty => default_engine
  std::string omnibox_engine;   // empty => normal_engine, then default_engine
  bool suggestions_enabled = false;
};

// Returns the engine id to use. `bang_engine` is the id a bang resolved to, or
// empty. `available` is the ids currently installed; a pref pointing at a
// removed engine falls back instead of leaving the user with no search.
// `fallback` is the built-in required engine (DuckDuckGo) and is returned only
// if nothing else resolves.
std::string SelectEngine(SearchContext context,
                         const SearchPrefs& prefs,
                         const std::string& bang_engine,
                         const std::vector<std::string>& available,
                         const std::string& fallback);

// Suggestions are off in private windows regardless of the pref: a suggest
// request leaks every keystroke to the provider, which contradicts the mode.
bool SuggestionsAllowed(SearchContext context, const SearchPrefs& prefs);

}  // namespace bedrock

#endif  // BEDROCK_SEARCH_ENGINE_SELECTOR_H_
