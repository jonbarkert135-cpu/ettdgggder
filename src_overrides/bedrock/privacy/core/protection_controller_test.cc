// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/privacy/core/protection_controller.h"

#include <cstddef>
#include <iostream>
#include <set>
#include <string>

namespace {

using namespace bedrock::privacy;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

const std::string kHost = "news.example.com";
const std::string kDomain = "example.com";

}  // namespace

int main() {
  ProtectionController pc;

  // Fresh profile: built-in defaults, nothing customised.
  Check(pc.Get(Control::kAds, kHost, kDomain) == Value::kBlock,
        "ads blocked by default");
  Check(pc.Get(Control::kScripts, kHost, kDomain) == Value::kAllow,
        "scripts allowed by default");
  Check(pc.Get(Control::kFingerprinting, kHost, kDomain) == Value::kReduce,
        "fingerprinting defaults to Level 1");
  Check(pc.EffectiveScope(Control::kAds, kHost, kDomain) == Scope::kGlobal,
        "unset control reports the global scope");
  Check(pc.CustomizedKeys(Scope::kSite).empty(), "no exceptions on a new profile");

  // Global change applies everywhere.
  pc.Set(Scope::kGlobal, "", Control::kScripts, Value::kBlock);
  Check(pc.Get(Control::kScripts, kHost, kDomain) == Value::kBlock,
        "global override applies");
  Check(pc.Get(Control::kScripts, "other.test", "other.test") == Value::kBlock,
        "global override applies to every site");

  // Domain override beats global, and covers subdomains.
  pc.Set(Scope::kDomain, kDomain, Control::kScripts, Value::kAllow);
  Check(pc.Get(Control::kScripts, kHost, kDomain) == Value::kAllow,
        "domain beats global");
  Check(pc.Get(Control::kScripts, "shop.example.com", kDomain) == Value::kAllow,
        "domain override covers every subdomain");
  Check(pc.Get(Control::kScripts, "other.test", "other.test") == Value::kBlock,
        "domain override does not leak to other domains");
  Check(pc.EffectiveScope(Control::kScripts, kHost, kDomain) == Scope::kDomain,
        "scope reported as domain");

  // Site override beats domain.
  pc.Set(Scope::kSite, kHost, Control::kScripts, Value::kBlock);
  Check(pc.Get(Control::kScripts, kHost, kDomain) == Value::kBlock,
        "site beats domain");
  Check(pc.Get(Control::kScripts, "shop.example.com", kDomain) == Value::kAllow,
        "site override applies to that host only");
  Check(pc.EffectiveScope(Control::kScripts, kHost, kDomain) == Scope::kSite,
        "scope reported as site");

  // Controls are independent: touching scripts must not move anything else.
  Check(pc.Get(Control::kAds, kHost, kDomain) == Value::kBlock,
        "unrelated control unchanged");

  // Reset restores inheritance rather than freezing the current value.
  pc.Clear(Scope::kSite, kHost);
  Check(pc.Get(Control::kScripts, kHost, kDomain) == Value::kAllow,
        "clearing a site falls back to the domain");
  pc.Clear(Scope::kDomain, kDomain);
  Check(pc.Get(Control::kScripts, kHost, kDomain) == Value::kBlock,
        "clearing a domain falls back to global");
  pc.Clear(Scope::kGlobal, "");
  Check(pc.Get(Control::kScripts, kHost, kDomain) == Value::kAllow,
        "clearing global restores the built-in default");

  // The shields panel gets every control in one call.
  pc.Set(Scope::kSite, kHost, Control::kFingerprinting, Value::kBlockStrict);
  const auto profile = pc.GetProfile(kHost, kDomain);
  Check(profile.size() == static_cast<size_t>(Control::kMaxValue) + 1,
        "profile covers every control");
  Check(profile.at(Control::kFingerprinting) == Value::kBlockStrict,
        "profile reflects the site override");
  Check(pc.CustomizedKeys(Scope::kSite).size() == 1,
        "customised site listed in exceptions");

  // Fingerprinting value -> renderer level.
  Check(ProtectionController::ToFpLevel(Value::kAllow) == FpLevel::kCompatibility,
        "allow -> level 0");
  Check(ProtectionController::ToFpLevel(Value::kReduce) == FpLevel::kBalanced,
        "reduce -> level 1");
  Check(ProtectionController::ToFpLevel(Value::kBlock) == FpLevel::kStrict,
        "block -> level 2");
  Check(ProtectionController::ToFpLevel(Value::kBlockStrict) == FpLevel::kMaximum,
        "block strict -> level 3");

  // Ids exist for prefs/logging and are unique.
  std::set<std::string> ids;
  for (int i = 0; i <= static_cast<int>(Control::kMaxValue); ++i) {
    const std::string id =
        ProtectionController::ControlId(static_cast<Control>(i));
    Check(!id.empty() && ids.insert(id).second, "unique control id: " + id);
  }

  if (failures == 0) {
    std::cout << "protection_controller_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
