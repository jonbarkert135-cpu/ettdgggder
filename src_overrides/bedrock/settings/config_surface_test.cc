// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/settings/config_surface.h"

#include <cstdio>
#include <set>
#include <string>

// Host test, no Chromium. What matters here is not that parsing works but that
// it never *pretends* to work: an unknown switch, a bad value or a setting with
// no surface has to be reported, because a configuration surface that shrugs is
// the fake-feature problem of item 55 wearing a command line.

namespace {

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::printf("  FAIL: %s\n", what.c_str());
    ++failures;
  }
}

using namespace bedrock::settings;

void KeysSwitchesAndPoliciesAreUnique() {
  std::set<std::string> keys, switches, policies;
  for (const SettingSpec& spec : ConfigSurface::All()) {
    Check(keys.insert(spec.key).second, std::string("duplicate key ") + spec.key);
    if (std::string(spec.switch_name).size() > 0) {
      Check(switches.insert(spec.switch_name).second,
            std::string("duplicate switch --") + spec.switch_name);
    }
    if (std::string(spec.policy_name).size() > 0) {
      Check(policies.insert(spec.policy_name).second,
            std::string("duplicate policy ") + spec.policy_name);
    }
  }
}

void EverySettingIsReachableOrExplains() {
  for (const SettingSpec& spec : ConfigSurface::All()) {
    if (spec.surfaces != kAllSurfaces) {
      Check(std::string(spec.restriction_reason).size() > 20,
            std::string(spec.key) + " is not on every surface and does not say why");
    }
    Check((spec.surfaces & static_cast<int>(Surface::kGui)) != 0,
          std::string(spec.key) + " is not reachable from the GUI");
    const bool has_switch = std::string(spec.switch_name).size() > 0;
    const bool cli = (spec.surfaces & static_cast<int>(Surface::kCommandLine)) != 0;
    Check(has_switch == cli,
          std::string(spec.key) + ": command-line surface and switch name disagree");
  }
}

void DescriptionsExist() {
  for (const SettingSpec& spec : ConfigSurface::All()) {
    Check(std::string(spec.description).size() > 10,
          std::string(spec.key) + " has no usable description for --help");
  }
}

void GoodSwitchesParse() {
  const ParseResult result = ConfigSurface::ParseCommandLine(
      {"--privacy-level=balanced", "--search-engine=duckduckgo", "--profile=research",
       "--disable-telemetry"});
  Check(result.ok(), "a valid command line reported errors");
  Check(result.values.at("privacy.level") == "balanced", "privacy level not parsed");
  Check(result.values.at("search.default_engine") == "duckduckgo", "search engine not parsed");
  Check(result.values.at("profile.name") == "research", "profile not parsed");
  Check(result.values.at("telemetry.enabled") == "false",
        "--disable-telemetry must resolve to telemetry off");
}

void UnknownSwitchIsAnError() {
  const ParseResult result = ConfigSurface::ParseCommandLine({"--make-me-anonymous"});
  Check(!result.ok(), "an unknown switch was accepted silently");
  Check(result.values.empty(), "an unknown switch produced a value");
}

void BadValueIsAnError() {
  const ParseResult result = ConfigSurface::ParseCommandLine({"--privacy-level=paranoid"});
  Check(!result.ok(), "an invalid value was accepted");
  Check(result.values.find("privacy.level") == result.values.end(),
        "an invalid value was stored anyway");
}

void MissingValueIsAnError() {
  const ParseResult result = ConfigSurface::ParseCommandLine({"--privacy-level"});
  Check(!result.ok(), "a switch missing its value was accepted");
}

void TelemetryCannotBeSwitchedOn() {
  // The only honest answer: there is nothing to enable (item 39).
  const ParseResult result = ConfigSurface::ParseCommandLine({"--disable-telemetry=true"});
  Check(!result.ok(), "telemetry.enabled=true was accepted");
}

void PolicyWinsAndLocks() {
  const auto resolved = ConfigSurface::Resolve(
      {{"privacy.level", "standard"}},   // config file
      {{"privacy.level", "strict"}},     // GUI prefs
      {{"privacy.level", "maximum"}},    // command line
      {{"privacy.level", "balanced"}});  // policy
  const Resolved& level = resolved.at("privacy.level");
  Check(level.value == "balanced", "policy did not win");
  Check(level.origin == Origin::kPolicy, "origin is not reported as policy");
  Check(level.locked, "a policy value must be reported as locked");
}

void PrecedenceBelowPolicy() {
  const auto resolved = ConfigSurface::Resolve(
      {{"privacy.level", "standard"}}, {{"privacy.level", "strict"}},
      {{"privacy.level", "maximum"}}, {});
  Check(resolved.at("privacy.level").value == "maximum", "command line should beat config file");
  Check(resolved.at("privacy.level").origin == Origin::kCommandLine, "wrong origin reported");

  const auto without_cli = ConfigSurface::Resolve(
      {{"privacy.level", "standard"}}, {{"privacy.level", "strict"}}, {}, {});
  Check(without_cli.at("privacy.level").value == "standard",
        "config file should beat GUI prefs");
}

void DefaultsAreReportedAsDefaults() {
  const auto resolved = ConfigSurface::Resolve({}, {}, {}, {});
  Check(resolved.at("privacy.level").value == "balanced", "default level is not balanced");
  Check(resolved.at("privacy.level").origin == Origin::kDefault, "default origin misreported");
  Check(resolved.at("telemetry.enabled").value == "false", "telemetry default is not off");
}

void HelpMentionsEverySwitch() {
  const std::string help = ConfigSurface::HelpText();
  for (const SettingSpec& spec : ConfigSurface::All()) {
    if ((spec.surfaces & static_cast<int>(Surface::kCommandLine)) == 0) {
      continue;
    }
    Check(help.find(std::string("--") + spec.switch_name) != std::string::npos,
          std::string("--help does not mention --") + spec.switch_name);
  }
}

}  // namespace

int main() {
  KeysSwitchesAndPoliciesAreUnique();
  EverySettingIsReachableOrExplains();
  DescriptionsExist();
  GoodSwitchesParse();
  UnknownSwitchIsAnError();
  BadValueIsAnError();
  MissingValueIsAnError();
  TelemetryCannotBeSwitchedOn();
  PolicyWinsAndLocks();
  PrecedenceBelowPolicy();
  DefaultsAreReportedAsDefaults();
  HelpMentionsEverySwitch();

  if (failures == 0) {
    std::printf("config_surface: %zu settings, four surfaces, strict parsing\n",
                ConfigSurface::All().size());
    return 0;
  }
  std::printf("config_surface: %d failure(s)\n", failures);
  return 1;
}
