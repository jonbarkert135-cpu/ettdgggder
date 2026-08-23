// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/ui/profile_menu.h"

#include <iostream>
#include <string>

namespace {

using bedrock::session::ProfileManager;
using bedrock::ui::ProfileMenuJson;
using bedrock::ui::WindowMode;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

bool Has(const std::string& json, const std::string& needle) {
  return json.find(needle) != std::string::npos;
}

}  // namespace

int main() {
  ProfileManager profiles = ProfileManager::WithDefaults();
  const std::string json = ProfileMenuJson(profiles, WindowMode::kNormal);

  Check(Has(json, "\"active\":{"), "the active profile leads");
  Check(Has(json, "\"initial\":"), "each entry has a letter to draw");
  Check(Has(json, "\"others\":[{"), "the others are listed for switching");
  Check(!Has(json, "\"others\":[{\"id\":\"" +
                       std::string(profiles.active().id) + "\""),
        "the active profile is not repeated in the switch list");

  // The claim this menu must not make.
  Check(Has(json, "Not synced"), "sync state is stated, not implied");
  Check(!Has(json, "Sign in"), "and there is no account prompt");

  const std::string priv = ProfileMenuJson(profiles, WindowMode::kPrivate);
  Check(Has(priv, "\"modeLabel\":\"Private\""), "a private window is labelled");
  Check(Has(priv, "still see you"),
        "with what private browsing does not protect");
  Check(Has(ProfileMenuJson(profiles, WindowMode::kTor), "\"modeLabel\":\"Tor\""),
        "and Tor windows say Tor");
  Check(Has(json, "\"modeLabel\":\"\""), "a normal window carries no badge");

  if (failures == 0)
    std::cout << "profile_menu: ok\n";
  return failures == 0 ? 0 : 1;
}
