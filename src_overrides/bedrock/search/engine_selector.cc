// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/search/engine_selector.h"

#include <algorithm>
#include <string>
#include <vector>

namespace bedrock {
namespace {

bool IsAvailable(const std::string& id,
                 const std::vector<std::string>& available) {
  return !id.empty() &&
         std::find(available.begin(), available.end(), id) != available.end();
}

}  // namespace

std::string SelectEngine(SearchContext context,
                         const SearchPrefs& prefs,
                         const std::string& bang_engine,
                         const std::vector<std::string>& available,
                         const std::string& fallback) {
  // Ordered candidates: most specific first, each a possible empty/stale value.
  std::vector<std::string> candidates;
  if (context == SearchContext::kBang || IsAvailable(bang_engine, available)) {
    candidates.push_back(bang_engine);
  }
  switch (context) {
    case SearchContext::kPrivate:
      candidates.push_back(prefs.private_engine);
      break;
    case SearchContext::kOmnibox:
      candidates.push_back(prefs.omnibox_engine);
      candidates.push_back(prefs.normal_engine);
      break;
    case SearchContext::kNormal:
    case SearchContext::kBang:
      candidates.push_back(prefs.normal_engine);
      break;
  }
  candidates.push_back(prefs.default_engine);
  candidates.push_back(fallback);

  for (const std::string& candidate : candidates) {
    if (IsAvailable(candidate, available)) {
      return candidate;
    }
  }
  return fallback;
}

bool SuggestionsAllowed(SearchContext context, const SearchPrefs& prefs) {
  return prefs.suggestions_enabled && context != SearchContext::kPrivate;
}

}  // namespace bedrock
