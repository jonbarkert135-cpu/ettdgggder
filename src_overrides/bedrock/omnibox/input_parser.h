// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_OMNIBOX_INPUT_PARSER_H_
#define BEDROCK_OMNIBOX_INPUT_PARSER_H_

#include <string>
#include <string_view>
#include <vector>

// Bedrock omnibox input classification.
//
// Scope note (deliberate): bookmark, history and open-tab matches are NOT done
// here. Chromium's AutocompleteController already ranks those through
// BookmarkProvider, HistoryQuickProvider and TabMatcher; duplicating them would
// be a second ranking system to maintain. Bedrock only adds what Chromium has
// no concept of: bang shortcuts and bedrock:// commands. Everything this
// parser calls kSearch is handed to Chromium's providers unchanged, so
// bookmark/history/tab results still appear.
//
// This file intentionally depends on nothing but the C++ standard library, so
// it can be unit tested on the host without a Chromium checkout.

namespace bedrock {

enum class InputType {
  kUrl,      // navigate directly: example.com, https://x, localhost:8080, file paths
  kSearch,   // send to a search engine
  kCommand,  // Bedrock browser command: "clear history", ">shields off"
};

struct ParsedInput {
  InputType type = InputType::kSearch;

  // For kSearch: the terms to send, bang removed ("!g linux" -> "linux").
  // For kUrl: the input, trimmed. For kCommand: the command text without '>'.
  std::string text;

  // Bang keyword found at either end of the input, e.g. "!g". Empty if none.
  // The caller resolves it to a provider; an unknown bang never reaches here.
  std::string bang;

  bool operator==(const ParsedInput& other) const {
    return type == other.type && text == other.text && bang == other.bang;
  }
};

// `known_bangs` are the keywords of the user's configured providers, including
// aliases, each with its leading '!' (e.g. {"!g", "!ddg"}). A bang-looking
// token that is not in this list is treated as ordinary search text — "!!!" and
// "!moved" must not silently disappear from the query.
ParsedInput ParseOmniboxInput(std::string_view input,
                              const std::vector<std::string>& known_bangs);

// Exposed for tests and for the address-bar drop target.
bool LooksLikeUrl(std::string_view text);

}  // namespace bedrock

#endif  // BEDROCK_OMNIBOX_INPUT_PARSER_H_
