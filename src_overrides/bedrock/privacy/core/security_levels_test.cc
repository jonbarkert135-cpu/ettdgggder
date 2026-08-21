// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/privacy/core/security_levels.h"

#include <iostream>
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

int Rank(Value value) {
  switch (value) {
    case Value::kAllow: return 0;
    case Value::kReduce: return 1;
    case Value::kBlock: return 2;
    case Value::kBlockStrict: return 3;
    case Value::kInherit: return -1;
  }
  return -1;
}

}  // namespace

int main() {
  // A fresh browser must already *be* a preset. If the defaults matched no
  // preset, Bedrock would start in a state its own settings page calls Custom.
  ProtectionController fresh;
  Check(SecurityLevels::Detect(fresh) == SecurityLevel::kBalanced,
        "the shipped defaults are exactly BALANCED");

  // Round trip: applying a preset makes Detect() return it, for every preset.
  for (const LevelInfo& info : SecurityLevels::All()) {
    if (info.level == SecurityLevel::kCustom)
      continue;
    ProtectionController controls;
    SecurityLevels::Apply(&controls, info.level);
    Check(SecurityLevels::Detect(controls) == info.level,
          std::string("apply then detect round-trips for ") + info.name);
    Check(std::string(SecurityLevels::Name(info.level)) == info.name,
          "the name is the one shown in the UI");
    Check(!SecurityLevels::Values(info.level).empty(),
          "a preset sets something");
  }

  // The ladder is monotonic: no rung is weaker than the one below it for any
  // control. A "stricter" preset that quietly loosens one setting is the kind
  // of bug nobody finds by hand.
  const SecurityLevel ladder[] = {SecurityLevel::kStandard,
                                  SecurityLevel::kBalanced,
                                  SecurityLevel::kStrict,
                                  SecurityLevel::kMaximum};
  for (size_t i = 1; i < sizeof(ladder) / sizeof(ladder[0]); ++i) {
    const Overrides lower = SecurityLevels::Values(ladder[i - 1]);
    const Overrides higher = SecurityLevels::Values(ladder[i]);
    for (const auto& entry : lower) {
      const auto it = higher.find(entry.first);
      Check(it != higher.end(), "every preset covers every control");
      if (it == higher.end())
        continue;
      Check(Rank(it->second) >= Rank(entry.second),
            std::string("a higher preset never weakens ") +
                ProtectionController::ControlId(entry.first));
    }
  }

  // Maximum really is the strongest, and Standard really is the loosest.
  const Overrides maximum = SecurityLevels::Values(SecurityLevel::kMaximum);
  Check(maximum.at(Control::kScripts) == Value::kBlock,
        "Maximum blocks scripts by default");
  Check(SecurityLevels::Values(SecurityLevel::kStandard).at(Control::kAds) ==
            Value::kAllow,
        "Standard is compatibility first and does not block ads");

  // Applying a preset must not wipe per-site exceptions the user made.
  {
    ProtectionController controls;
    controls.Set(Scope::kSite, "bank.example", Control::kScripts, Value::kAllow);
    SecurityLevels::Apply(&controls, SecurityLevel::kMaximum);
    Check(controls.Get(Control::kScripts, "bank.example", "bank.example") ==
              Value::kAllow,
          "a per-site exception survives a preset change");
    Check(controls.Get(Control::kScripts, "other.example", "other.example") ==
              Value::kBlock,
          "while the global preset applies everywhere else");
  }

  // Compatibility honesty: anything above Standard states its cost.
  for (const LevelInfo& info : SecurityLevels::All()) {
    if (info.level == SecurityLevel::kStandard ||
        info.level == SecurityLevel::kCustom) {
      continue;
    }
    Check(!std::string(SecurityLevels::CompatibilityWarning(info.level)).empty(),
          std::string("the cost of ") + info.name + " is stated up front");
  }
  Check(std::string(SecurityLevels::CompatibilityWarning(SecurityLevel::kMaximum))
            .find("break") != std::string::npos,
        "Maximum admits sites will break instead of calling it 'may affect "
        "some sites'");

  // Tor Mode is not a rung on this ladder.
  for (const LevelInfo& info : SecurityLevels::All()) {
    Check(std::string(info.name).find("TOR") == std::string::npos,
          "Tor is not a protection level");
  }
  const std::string tor = SecurityLevels::TorModeDescription();
  Check(tor.find("transport mode") != std::string::npos,
        "Tor Mode is described as a transport");
  Check(tor.find("still identifies you") != std::string::npos,
        "and names the limit in the same breath");

  // Nothing in the preset vocabulary promises anonymity.
  const char* banned[] = {"anonymous", "anonymity", "untraceable", "100%",
                          "completely private", "invisible", "no one can"};
  for (const LevelInfo& info : SecurityLevels::All()) {
    const std::string text = std::string(info.summary) + " " + info.tradeoff;
    for (const char* word : banned) {
      Check(text.find(word) == std::string::npos,
            std::string("no anonymity promise in ") + info.name);
      Check(tor.find(word) == std::string::npos,
            "and none in the Tor description");
    }
  }

  if (failures == 0)
    std::cout << "security_levels_test: all assertions passed\n";
  return failures == 0 ? 0 : 1;
}
