// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/settings/settings_page.h"

#include <iostream>
#include <set>
#include <string>

namespace {

using namespace bedrock::settings;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

bool Has(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

}  // namespace

int main() {
  // The rail is the one from the design, in order.
  Check(Sections().size() == 9, "nine sections");
  Check(std::string(Sections().front().id) == "general" &&
            std::string(Sections().back().id) == "advanced",
        "General first, Advanced last");
  Check(std::string(Info(Section::kPrivacy).title) == "Privacy & Security",
        "the privacy section is named as designed");

  // Every setting the browser has is somewhere in this page. A key that nobody
  // placed must not vanish from the GUI.
  std::set<std::string> placed;
  for (const SettingSpec& spec : ConfigSurface::All()) {
    const Section section = SectionForKey(spec.key);
    Check(std::string(Info(section).id) != "",
          std::string("a section owns ") + spec.key);
    placed.insert(spec.key);
  }
  Check(placed.size() == ConfigSurface::All().size(),
        "every key is accounted for exactly once");

  Check(SectionForKey("privacy.level") == Section::kPrivacy, "privacy keys");
  Check(SectionForKey("blocking.lists") == Section::kPrivacy,
        "blocking lists belong with privacy, not in a corner");
  Check(SectionForKey("telemetry.enabled") == Section::kPrivacy,
        "telemetry is a privacy setting, wherever it is implemented");
  Check(SectionForKey("search.default_engine") == Section::kSearch, "search");
  Check(SectionForKey("network.dns.mode") == Section::kAdvanced, "advanced");
  Check(SectionForKey("something.invented.later") == Section::kAdvanced,
        "an unplaced key is visible in Advanced, never hidden");

  // The page explains a locked control instead of just disabling it.
  std::map<std::string, Resolved> resolved;
  resolved["privacy.level"] = {"strict", Origin::kPolicy, true};
  const std::string json = SettingsPageJson(Section::kPrivacy, resolved);
  Check(Has(json, "\"section\":\"privacy\""), "the active section is named");
  Check(Has(json, "\"key\":\"privacy.level\""), "its rows are present");
  Check(Has(json, "\"label\":\"Global protection preset\""),
        "the row title is short");
  Check(Has(json, "\"label\":\"Global protection preset\",\"detail\":\"\""),
        "a title that is the whole line is not repeated underneath");
  Check(Has(json, "\"detail\":\"Cookie policy: allow all,"),
        "a longer line is kept, under a short title");
  Check(Has(json, "\"value\":\"strict\""), "with the resolved value");
  Check(Has(json, "\"origin\":\"policy\",\"locked\":true"),
        "and why it cannot be changed");
  Check(Has(json, "\"active\":true"), "the rail marks where we are");
  Check(!Has(json, "\"key\":\"search.default_engine\""),
        "another section's rows are not smuggled in");

  // A row with no stored value falls back to the documented default, not to an
  // empty control.
  const std::string plain = SettingsPageJson(Section::kSearch, {});
  Check(Has(plain, "\"origin\":\"default\""), "unset values say so");
  Check(!Has(plain, "\"value\":\"\""), "and are never blank");

  // Extension cards ride with their own section and nowhere else.
  const std::vector<ExtensionCard> cards = {
      {"abc", "Reader view", "1.2.0", "Low", "This site only", true, false}};
  const std::string ext = SettingsPageJson(Section::kExtensions, {}, cards);
  Check(Has(ext, "\"name\":\"Reader view\""), "the card is drawn");
  Check(Has(ext, "\"risk\":\"Low\",\"hostAccess\":\"This site only\""),
        "with what it can read, which is the only interesting fact about it");
  Check(!Has(SettingsPageJson(Section::kPrivacy, {}, cards), "Reader view"),
        "and not on an unrelated page");

  if (failures == 0)
    std::cout << "settings_page: ok\n";
  return failures == 0 ? 0 : 1;
}
