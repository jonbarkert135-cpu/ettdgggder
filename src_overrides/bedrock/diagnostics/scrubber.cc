// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/diagnostics/scrubber.h"

#include <regex>

namespace bedrock {
namespace diagnostics {

namespace {

struct Rule {
  const char* category;
  const char* pattern;
  const char* replacement;
};

// Order matters. Headers and named secrets go first, so that a cookie value
// that happens to look like a URL is redacted as a cookie rather than half
// rewritten; the catch-all blob rule goes last, after the specific rules have
// had their say.
//
// std::regex is slow, and that is fine here: this runs on the failure path —
// a log line, an error, a crash — never in the request pipeline. Performance
// budgets (item 46) cover the pipeline; correctness covers this.
const Rule kRules[] = {
    {"header",
     R"((?:Cookie|Set-Cookie|Authorization|Proxy-Authorization)\s*:\s*[^\r\n]+)",
     "<header>: <redacted>"},
    {"secret",
     R"((?:password|passwd|pwd|passphrase|token|secret|session|api[_-]?key|access[_-]?key)=[^&;\s"']+)",
     "<secret>=<redacted>"},
    // A URL with any scheme except the internal pages, which carry no user
    // data and are the most useful thing in a stack trace.
    {"url",
     R"(\b(?!chrome://|bedrock://|about:)[A-Za-z][A-Za-z0-9+.-]*://[^\s"'<>]+)",
     "<url>"},
    {"email", R"([A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,})", "<email>"},
    {"home", R"((?:/home/|/Users/|[A-Za-z]:\\Users\\)[^/\\\s"']+)", "<home>"},
    {"ip", R"(\b(?!127\.0\.0\.1)(?:\d{1,3}\.){3}\d{1,3}\b)", "<ip>"},
    // No "/" in the class: a base64 blob may contain one, but so does every
    // source path in a stack frame, and losing the frame costs more than
    // leaving a slash in a token that is redacted either side of it.
    {"blob", R"([A-Za-z0-9+=_-]{40,})", "<blob>"},
};

// The URL rule needs a negative lookahead, which std::regex (ECMAScript
// grammar) does support, but the internal-page schemes must survive the
// loopback exception too: http://127.0.0.1 is a local test server, not the
// user's browsing.
bool IsLoopbackUrl(const std::string& match) {
  return match.find("://127.0.0.1") != std::string::npos ||
         match.find("://localhost") != std::string::npos ||
         match.find("://[::1]") != std::string::npos;
}

std::string ApplyRule(const Rule& rule,
                      const std::string& input,
                      int* redactions,
                      bool* fired) {
  const std::regex re(rule.pattern, std::regex::ECMAScript | std::regex::icase);
  std::string out;
  auto begin = std::sregex_iterator(input.begin(), input.end(), re);
  auto end = std::sregex_iterator();
  std::size_t last = 0;
  for (auto it = begin; it != end; ++it) {
    const std::smatch& match = *it;
    const std::string text = match.str();
    if (std::string(rule.category) == "url" && IsLoopbackUrl(text)) {
      continue;
    }
    out.append(input, last, static_cast<std::size_t>(match.position()) - last);
    out.append(rule.replacement);
    last = static_cast<std::size_t>(match.position() + match.length());
    ++*redactions;
    *fired = true;
  }
  out.append(input, last, std::string::npos);
  return out;
}

}  // namespace

ScrubResult Scrubber::Scrub(const std::string& text) {
  ScrubResult result;
  result.text = text;
  for (const Rule& rule : kRules) {
    bool fired = false;
    result.text = ApplyRule(rule, result.text, &result.redactions, &fired);
    if (fired) {
      result.categories.push_back(rule.category);
    }
  }
  return result;
}

bool Scrubber::LooksSensitive(const std::string& text) {
  return Scrub(text).redactions > 0;
}

}  // namespace diagnostics
}  // namespace bedrock
