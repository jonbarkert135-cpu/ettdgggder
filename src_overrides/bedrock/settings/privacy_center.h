// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_SETTINGS_PRIVACY_CENTER_H_
#define BEDROCK_SETTINGS_PRIVACY_CENTER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "bedrock/privacy/core/protection_controller.h"
#include "bedrock/privacy/stats/privacy_event_log.h"

// Privacy Center (roadmap item 37).
//
// The dashboard from the roadmap (sample values — every real figure comes from
// the privacy event log, and "not measured" is shown when nothing was logged):
//
//   Trackers blocked       12,481
//   Ads blocked             7,294
//   Fingerprint attempts    1,182
//   HTTPS upgrades            213
//   Cookies partitioned     5,912
//   Protection Level:      BALANCED
//
// Every number is a query over the privacy event log, which only contains
// actions a subsystem actually performed. Nothing is uploaded: the dashboard
// is a view of local state, and the only export is a file the user asks for.
//
// The protection level is *derived from the current settings*, not stored
// separately. A stored label drifts: the user changes one control, the badge
// still says BALANCED, and the badge is now a lie. If the settings do not
// match any preset, the level is CUSTOM — which is a real answer, not a
// failure.

namespace bedrock {
namespace stats {

// Mirrors privacy::SecurityLevel (item 45); the preset definitions live
// there, this enum only exists so the dashboard does not depend on the
// settings vocabulary changing underneath it.
enum class ProtectionLevel {
  kStandard,
  kBalanced,
  kStrict,
  kMaximum,
  kCustom,
};

struct DashboardRow {
  std::string label;
  int64_t value = 0;
  std::string formatted;  // formatting example: "12,481"
};

class PrivacyCenter {
 public:
  PrivacyCenter(const PrivacyEventLog* log,
                const privacy::ProtectionController* controls);
  ~PrivacyCenter();

  std::vector<DashboardRow> Rows() const;
  ProtectionLevel Level() const;
  static const char* LevelName(ProtectionLevel level);

  // Shown under the numbers so the dashboard says what it is.
  static const char* DataSourceNote();

  // Formatting example: 12481 -> "12,481".
  static std::string FormatCount(int64_t value);

 private:
  const PrivacyEventLog* log_;
  const privacy::ProtectionController* controls_;
};

}  // namespace stats
}  // namespace bedrock

#endif  // BEDROCK_SETTINGS_PRIVACY_CENTER_H_
