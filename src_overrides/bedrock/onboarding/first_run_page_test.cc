// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

// What the first-run page is allowed to be: a renderer. These tests hold the
// bridge to that, because the moment the page needs a rule of its own, a
// privacy decision has moved into JavaScript.

#include "bedrock/onboarding/first_run_page.h"

#include "bedrock/privacy/core/security_levels.h"

#include <iostream>
#include <string>
#include <vector>

namespace bedrock {
namespace onboarding {
namespace {

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cout << "FAIL: " << what << "\n";
    ++failures;
  }
}

bool Contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

std::vector<EngineFacts> Engines() {
  return {{"google", "Google", true, true},
          {"duckduckgo", "DuckDuckGo", true, false}};
}

// A minimal well-formedness check: quotes balance and braces close. Enough to
// catch the failure this file exists to prevent — a page that cannot parse the
// state it is given.
bool WellFormedJson(const std::string& text) {
  int depth = 0;
  bool in_string = false;
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (in_string) {
      if (c == '\\') {
        ++i;
      } else if (c == '"') {
        in_string = false;
      } else if (static_cast<unsigned char>(c) < 0x20) {
        return false;  // a raw control character inside a string
      }
      continue;
    }
    if (c == '"') in_string = true;
    if (c == '{' || c == '[') ++depth;
    if (c == '}' || c == ']') --depth;
    if (depth < 0) return false;
  }
  return depth == 0 && !in_string;
}

void ThePageGetsEveryOptionItRenders() {
  FirstRun flow(Engines());
  std::string json = PageModelJson(flow);
  Check(WellFormedJson(json), "the model parses");
  for (const char* key : {"privacyOptions", "engineOptions", "themeOptions",
                          "importOptions", "disclosure", "privacyNotes"}) {
    Check(Contains(json, std::string("\"") + key + "\""),
          std::string("the model carries ") + key);
  }
  // Six import sources, three themes, three privacy levels, two engines: the
  // page never invents one, so they have to be here.
  for (const char* id : {"chrome", "firefox", "edge", "chromium", "html",
                         "skip", "light", "dark", "system", "standard",
                         "balanced", "strict", "google", "duckduckgo"}) {
    Check(Contains(json, std::string("\"") + id + "\""),
          std::string("option present: ") + id);
  }
  Check(Contains(json, "\"step\":\"welcome\""), "the flow starts at welcome");
  Check(Contains(json, "\"selected\":true"), "every step opens on an answer");
}

void ThePrivacyTextComesFromThePresetsNotFromThePage() {
  // Invariant 6: one definition of "Balanced". If the page model ever grew its
  // own summary, this string would stop matching the preset table.
  FirstRun flow(Engines());
  std::string json = PageModelJson(flow);
  Check(Contains(json, ::bedrock::privacy::SecurityLevels::Info(
                           ::bedrock::privacy::SecurityLevel::kBalanced)
                           .summary),
        "the Balanced line is the preset's own");
}

void TheSearchDisclosureTravelsWithTheEngine() {
  FirstRun flow(Engines());
  Check(Contains(PageModelJson(flow), "directly to Google"),
        "Google is preselected and disclosed");
  Check(ApplyPageChoice(flow, "engine", "duckduckgo"), "engine switch accepted");
  std::string json = PageModelJson(flow);
  Check(Contains(json, "directly to DuckDuckGo"), "disclosure follows the engine");
  Check(!Contains(json, "\"provider\":\"Google\""), "no stale provider left");
}

void AnUnknownMessageChangesNothing() {
  FirstRun flow(Engines());
  const std::string before = PageModelJson(flow);
  Check(!ApplyPageChoice(flow, "engine", "yandex"), "unoffered engine refused");
  Check(!ApplyPageChoice(flow, "theme", "neon"), "unoffered theme refused");
  Check(!ApplyPageChoice(flow, "privacy", "maximum"),
        "a level first run does not offer is refused");
  Check(!ApplyPageChoice(flow, "suggestions", "yes"),
        "a non-boolean is an error, not a silent false");
  Check(!ApplyPageChoice(flow, "telemetry", "on"), "unknown field refused");
  Check(PageModelJson(flow) == before, "nothing changed");
}

void NavigationRunsThroughTheSameBridge() {
  FirstRun flow(Engines());
  Check(!ApplyPageChoice(flow, "step", "back"), "back on the first step is a no-op");
  const char* order[] = {"privacy", "search", "theme", "import", "finish"};
  for (const char* step : order) {
    Check(ApplyPageChoice(flow, "step", "next"), "next moves");
    Check(Contains(PageModelJson(flow), std::string("\"step\":\"") + step + "\""),
          std::string("now at ") + step);
  }
  Check(ApplyPageChoice(flow, "step", "next"), "the last next finishes");
  Check(Contains(PageModelJson(flow), "\"done\":true"), "the flow ends");
}

void ATextWithQuotesCannotBreakThePage() {
  std::vector<EngineFacts> hostile = {{"odd", "A \"quoted\" \\ name\nwith lines",
                                       true, false}};
  FirstRun flow(hostile);
  std::string json = PageModelJson(flow);
  Check(WellFormedJson(json), "a hostile provider name is escaped, not injected");
  Check(Contains(json, "\\\"quoted\\\""), "quotes are escaped");
  Check(!Contains(json, "\\ name\nwith"), "the newline is not raw");
}

}  // namespace
}  // namespace onboarding
}  // namespace bedrock

int main() {
  using namespace bedrock::onboarding;
  ThePageGetsEveryOptionItRenders();
  ThePrivacyTextComesFromThePresetsNotFromThePage();
  TheSearchDisclosureTravelsWithTheEngine();
  AnUnknownMessageChangesNothing();
  NavigationRunsThroughTheSameBridge();
  ATextWithQuotesCannotBreakThePage();
  if (failures == 0) std::cout << "first_run_page: ok\n";
  return failures == 0 ? 0 : 1;
}
