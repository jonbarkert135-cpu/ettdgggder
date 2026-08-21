// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_SETTINGS_DEFAULTS_H_
#define BEDROCK_SETTINGS_DEFAULTS_H_

#include <string>
#include <vector>

// User control (item 83) and the shipped defaults (item 84).
//
// Two ideas that are easy to state and easy to get wrong together:
//
//   * the user chooses along four axes — privacy, compatibility, performance
//     and appearance — and the choices are separate, because someone who wants
//     a dark theme has not asked for weaker protection;
//   * the **default** is a defensible balance rather than the maximum of any
//     axis. Bedrock ships **Balanced Privacy**: strong enough that a normal
//     person is materially better off without configuring anything, mild
//     enough that they do not spend their first week adding exceptions and
//     then turn the whole thing off.
//
// Two settings are deliberately *not* on any axis: telemetry and crash-report
// upload. There is no compatibility or performance argument for either, so
// offering them as a trade-off would be theatre. They are off, and the only
// direction they can be moved is by the user asking for crash upload per report
// (item 81).
//
// `Change` exists so that no axis can quietly weaken protection: every effect
// of a choice is enumerated, and one that reduces a protection is marked. The
// settings UI renders those in the confirmation, so "I want fewer broken sites"
// never turns into "I silently accepted third-party cookies".

namespace bedrock {
namespace settings {

enum class Axis {
  kPrivacy,
  kCompatibility,
  kPerformance,
  kAppearance,
};

// Per-axis choices. The middle value of each is the shipped default.
enum class PrivacyChoice { kStandard, kBalanced, kStrict };
enum class CompatibilityChoice { kStrict, kBalanced, kMaximum };
enum class PerformanceChoice { kEfficiency, kBalanced, kSpeed };
enum class AppearanceChoice { kSystem, kLight, kDark };

struct Profile {
  PrivacyChoice privacy = PrivacyChoice::kBalanced;
  CompatibilityChoice compatibility = CompatibilityChoice::kBalanced;
  PerformanceChoice performance = PerformanceChoice::kBalanced;
  AppearanceChoice appearance = AppearanceChoice::kSystem;
};

// One shipped default. `value` is the state a fresh profile starts in, and
// `rationale` is why — a default without a reason is a default nobody can argue
// with later.
struct DefaultSetting {
  const char* id;
  const char* value;
  const char* rationale;
  bool negotiable;  // false: no axis may change it (telemetry, crash upload)
};

// The list item 84 specifies, in that order.
const std::vector<DefaultSetting>& FactoryDefaults();
const DefaultSetting* FindDefault(const std::string& id);

// What a fresh profile is: Balanced Privacy on every axis, system appearance.
Profile FactoryProfile();

// The human-readable name of the shipped configuration, used in the UI and in
// documentation so both say the same thing.
const char* FactoryProfileName();

struct Change {
  std::string setting_id;
  std::string from;
  std::string to;
  bool weakens_protection;  // shown in the confirmation, never hidden
  std::string explanation;
};

// Everything that changes when an axis is moved away from its default. An axis
// that would touch a non-negotiable setting returns no change for it — the
// enumeration is the enforcement.
std::vector<Change> ChangesFor(Axis axis, int choice);

}  // namespace settings
}  // namespace bedrock

#endif  // BEDROCK_SETTINGS_DEFAULTS_H_
