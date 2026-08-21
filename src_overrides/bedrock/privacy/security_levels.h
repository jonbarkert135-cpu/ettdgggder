// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_SECURITY_LEVELS_H_
#define BEDROCK_PRIVACY_SECURITY_LEVELS_H_

#include <map>
#include <string>
#include <vector>

#include "bedrock/privacy/protection_controller.h"

// Global protection presets (roadmap item 45).
//
//   Standard  — maximum compatibility
//   Balanced  — the recommended default, and what Bedrock ships with
//   Strict    — strong privacy
//   Maximum   — strongest web privacy, with compatibility costs stated
//
// **Tor Mode is not on this ladder.** It is a transport mode (item 19),
// orthogonal to the preset: you can run Tor transport with Balanced settings.
// Folding it in as "level 5" would suggest it is simply more of the same
// thing, and would quietly imply Tor makes the user anonymous, which is
// exactly the claim this project refuses to make.
//
// This is the single source of truth for what a preset means. The Privacy
// Center badge (item 37) asks `Detect()` rather than keeping its own copy of
// the table — two definitions of "Balanced" is one definition too many, and
// the one that drifts is always the one in the UI.

namespace bedrock {
namespace privacy {

enum class SecurityLevel {
  kStandard,
  kBalanced,
  kStrict,
  kMaximum,
  kCustom,  // the settings match no preset; an honest answer, not a failure
};

struct LevelInfo {
  SecurityLevel level;
  const char* name;         // "BALANCED"
  const char* summary;      // one line for the settings page
  const char* tradeoff;     // what it costs; never empty except for Standard
};

class SecurityLevels {
 public:
  static const std::vector<LevelInfo>& All();
  static const LevelInfo& Info(SecurityLevel level);
  static const char* Name(SecurityLevel level);

  // The control values a preset sets. Nothing else is touched, so a preset
  // never silently reverts a per-site exception the user made.
  static Overrides Values(SecurityLevel level);

  // Applies the preset globally.
  static void Apply(ProtectionController* controls, SecurityLevel level);

  // Which preset the current global settings correspond to.
  static SecurityLevel Detect(const ProtectionController& controls);

  // Compatibility warning shown before switching, empty when there is none.
  static const char* CompatibilityWarning(SecurityLevel level);

  // Tor Mode is described separately, on purpose.
  static const char* TorModeDescription();
  static bool IsLevel(SecurityLevel level) { return level != SecurityLevel::kCustom; }
};

}  // namespace privacy
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_SECURITY_LEVELS_H_
