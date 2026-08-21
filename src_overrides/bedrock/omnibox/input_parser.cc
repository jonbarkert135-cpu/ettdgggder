// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/omnibox/input_parser.h"

#include <algorithm>
#include <cctype>

namespace bedrock {
namespace {

constexpr std::string_view kSpace = " \t\n\r";

std::string_view Trim(std::string_view text) {
  const size_t begin = text.find_first_not_of(kSpace);
  if (begin == std::string_view::npos) {
    return {};
  }
  return text.substr(begin, text.find_last_not_of(kSpace) - begin + 1);
}

bool IsKnown(std::string_view token, const std::vector<std::string>& bangs) {
  return std::find(bangs.begin(), bangs.end(), token) != bangs.end();
}

// Splits off the first or last whitespace-delimited token if it is a known
// bang. Both positions are supported because "!g cats" and "cats !g" are both
// natural to type. Returns the bang, or empty, and shrinks `text`.
std::string ExtractBang(std::string_view& text,
                        const std::vector<std::string>& bangs) {
  const size_t first_end = text.find_first_of(kSpace);
  const std::string_view first = text.substr(0, first_end);
  if (IsKnown(first, bangs)) {
    text = first_end == std::string_view::npos ? std::string_view()
                                               : Trim(text.substr(first_end));
    return std::string(first);
  }

  const size_t last_begin = text.find_last_of(kSpace);
  if (last_begin != std::string_view::npos) {
    const std::string_view last = text.substr(last_begin + 1);
    if (IsKnown(last, bangs)) {
      text = Trim(text.substr(0, last_begin));
      return std::string(last);
    }
  }
  return {};
}

bool HasScheme(std::string_view text) {
  const size_t colon = text.find(':');
  if (colon == 0 || colon == std::string_view::npos) {
    return false;
  }
  for (const char c : text.substr(0, colon)) {
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '-' &&
        c != '.') {
      return false;
    }
  }
  // "localhost:8080" and "example.com:443" are host:port, not scheme:path.
  const std::string_view rest = text.substr(colon + 1);
  const bool all_digits =
      !rest.empty() && std::all_of(rest.begin(), rest.end(), [](char c) {
        return std::isdigit(static_cast<unsigned char>(c));
      });
  return !all_digits;
}

bool IsHostPort(std::string_view text) {
  const size_t colon = text.find(':');
  if (colon == std::string_view::npos || colon + 1 >= text.size()) {
    return false;
  }
  const std::string_view port = text.substr(colon + 1);
  return std::all_of(port.begin(), port.end(), [](char c) {
    return std::isdigit(static_cast<unsigned char>(c));
  });
}

}  // namespace

bool LooksLikeUrl(std::string_view text) {
  text = Trim(text);
  if (text.empty() || text.find_first_of(" \t") != std::string_view::npos) {
    return false;  // whitespace inside => a query, not a URL
  }
  if (text.front() == '/' || text.rfind("~/", 0) == 0) {
    return true;  // absolute or home-relative file path
  }
  if (HasScheme(text)) {
    return true;  // https:, file:, bedrock:, mailto:, magnet: ...
  }
  if (text == "localhost" || text.rfind("localhost:", 0) == 0) {
    return true;
  }
  if (IsHostPort(text)) {
    return true;  // 127.0.0.1:8000, example.com:8443
  }

  // Bare host: needs a dot with a plausible TLD, and no dot at either end.
  const std::string_view host = text.substr(0, text.find_first_of("/?#"));
  const size_t dot = host.find_last_of('.');
  if (dot == std::string_view::npos || dot == 0 || dot + 1 >= host.size()) {
    return false;
  }
  const std::string_view tld = host.substr(dot + 1);
  return tld.size() >= 2 &&
         std::all_of(tld.begin(), tld.end(), [](char c) {
           return std::isalpha(static_cast<unsigned char>(c));
         });
}

ParsedInput ParseOmniboxInput(std::string_view input,
                              const std::vector<std::string>& known_bangs) {
  ParsedInput result;
  std::string_view text = Trim(input);
  if (text.empty()) {
    return result;
  }

  // A leading '>' is the explicit command escape, so a command can never be
  // shadowed by a site named like it.
  if (text.front() == '>') {
    result.type = InputType::kCommand;
    result.text = std::string(Trim(text.substr(1)));
    return result;
  }

  result.bang = ExtractBang(text, known_bangs);
  result.text = std::string(text);

  if (!result.bang.empty()) {
    result.type = InputType::kSearch;  // an explicit bang always searches
    return result;
  }
  result.type = LooksLikeUrl(text) ? InputType::kUrl : InputType::kSearch;
  return result;
}

}  // namespace bedrock
