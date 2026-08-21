// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh. Items 83 and 84.

#include "bedrock/settings/defaults.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

using bedrock::settings::Axis;
using bedrock::settings::Change;
using bedrock::settings::ChangesFor;
using bedrock::settings::CompatibilityChoice;
using bedrock::settings::DefaultSetting;
using bedrock::settings::FactoryDefaults;
using bedrock::settings::FactoryProfile;
using bedrock::settings::FactoryProfileName;
using bedrock::settings::FindDefault;
using bedrock::settings::PerformanceChoice;
using bedrock::settings::PrivacyChoice;
using bedrock::settings::Profile;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

// Item 84, written out again here rather than read from the code, so this test
// is a check and not a mirror.
const std::pair<const char*, const char*> kRequired[] = {
    {"secure_browsing", "enabled"},
    {"https_upgrades", "enabled"},
    {"third_party_tracking_protection", "enabled"},
    {"third_party_cookies", "restricted"},
    {"fingerprint_protection", "balanced"},
    {"ad_and_tracker_blocking", "enabled"},
    {"telemetry", "disabled"},
    {"crash_reporting", "disabled"},
    {"webrtc_privacy", "enabled"},
    {"secure_dns", "configurable"},
    {"extension_permissions", "explicit"},
    {"site_permissions", "ask_when_needed"},
};

void ShippedDefaultsAreExactlyWhatWasSpecified() {
  Check(FactoryDefaults().size() == sizeof(kRequired) / sizeof(kRequired[0]),
        "no default was added or dropped");
  for (const auto& required : kRequired) {
    const DefaultSetting* setting = FindDefault(required.first);
    Check(setting != nullptr, std::string(required.first) + ": exists");
    if (!setting) {
      continue;
    }
    Check(std::string(setting->value) == required.second,
          std::string(required.first) + ": ships as " + required.second);
    Check(std::string(setting->rationale).size() > 40,
          std::string(required.first) + ": the default has a stated reason");
  }
}

void TheDefaultProfileIsBalancedPrivacy() {
  const Profile profile = FactoryProfile();
  Check(profile.privacy == PrivacyChoice::kBalanced, "privacy axis is balanced");
  Check(profile.compatibility == CompatibilityChoice::kBalanced,
        "compatibility axis is balanced");
  Check(profile.performance == PerformanceChoice::kBalanced,
        "performance axis is balanced");
  Check(std::string(FactoryProfileName()) == "Balanced Privacy",
        "the shipped profile is named Balanced Privacy");
}

void TelemetryAndCrashUploadAreNotOnAnyAxis() {
  for (const char* id : {"telemetry", "crash_reporting"}) {
    const DefaultSetting* setting = FindDefault(id);
    Check(setting && !setting->negotiable,
          std::string(id) + ": marked non-negotiable");
    Check(setting && std::string(setting->value) == "disabled",
          std::string(id) + ": disabled");
  }
  for (Axis axis : {Axis::kPrivacy, Axis::kCompatibility, Axis::kPerformance,
                    Axis::kAppearance}) {
    for (int choice = 0; choice < 3; ++choice) {
      for (const Change& change : ChangesFor(axis, choice)) {
        Check(change.setting_id != "telemetry" &&
                  change.setting_id != "crash_reporting",
              "no axis touches " + change.setting_id);
      }
    }
  }
}

void EveryWeakeningIsLabelledAndExplained() {
  for (Axis axis : {Axis::kPrivacy, Axis::kCompatibility, Axis::kPerformance,
                    Axis::kAppearance}) {
    for (int choice = 0; choice < 3; ++choice) {
      for (const Change& change : ChangesFor(axis, choice)) {
        Check(!change.explanation.empty(),
              change.setting_id + ": the change is explained");
        Check(change.from != change.to,
              change.setting_id + ": a listed change really changes something");
      }
    }
  }
  // The two directions that reduce protection must say so.
  bool found_weakening = false;
  for (const Change& change :
       ChangesFor(Axis::kCompatibility,
                  static_cast<int>(CompatibilityChoice::kMaximum))) {
    if (change.setting_id == "third_party_cookies") {
      Check(change.weakens_protection,
            "allowing third-party cookies is labelled as weaker");
      found_weakening = true;
    }
  }
  Check(found_weakening, "maximum compatibility lists its cost");

  for (const Change& change :
       ChangesFor(Axis::kPrivacy, static_cast<int>(PrivacyChoice::kStrict))) {
    Check(!change.weakens_protection,
          "the strict privacy choice weakens nothing: " + change.setting_id);
  }
}

void AppearanceAndPerformanceDoNotTouchPrivacy() {
  Check(ChangesFor(Axis::kAppearance, 0).empty() &&
            ChangesFor(Axis::kAppearance, 1).empty() &&
            ChangesFor(Axis::kAppearance, 2).empty(),
        "a theme is a theme");
  for (int choice = 0; choice < 3; ++choice) {
    for (const Change& change : ChangesFor(Axis::kPerformance, choice)) {
      Check(!change.weakens_protection,
            "the performance axis does not sell privacy: " + change.setting_id);
    }
  }
}

void DefaultsAreNotTheMaximumOfAnyAxis() {
  // The point of item 83: the shipped configuration is a balance, so both a
  // stricter and a looser choice must exist on the privacy axis.
  Check(!ChangesFor(Axis::kPrivacy, static_cast<int>(PrivacyChoice::kStrict)).empty(),
        "there is something stricter than the default");
  Check(!ChangesFor(Axis::kPrivacy, static_cast<int>(PrivacyChoice::kStandard)).empty(),
        "there is something looser than the default");
}

}  // namespace

int main() {
  ShippedDefaultsAreExactlyWhatWasSpecified();
  TheDefaultProfileIsBalancedPrivacy();
  TelemetryAndCrashUploadAreNotOnAnyAxis();
  EveryWeakeningIsLabelledAndExplained();
  AppearanceAndPerformanceDoNotTouchPrivacy();
  DefaultsAreNotTheMaximumOfAnyAxis();
  if (failures == 0) {
    std::cout << "defaults: ok (" << FactoryDefaults().size()
              << " shipped defaults, profile \"" << FactoryProfileName()
              << "\")\n";
  }
  return failures == 0 ? 0 : 1;
}
