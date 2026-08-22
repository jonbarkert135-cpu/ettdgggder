// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/settings/privacy_center.h"

#include <cstdint>
#include <string>
#include <vector>

#include "bedrock/privacy/core/security_levels.h"

namespace bedrock {
namespace stats {

PrivacyCenter::PrivacyCenter(const PrivacyEventLog* log,
                             const privacy::ProtectionController* controls)
    : log_(log), controls_(controls) {}

PrivacyCenter::~PrivacyCenter() = default;

std::string PrivacyCenter::FormatCount(int64_t value) {
  std::string digits = std::to_string(value < 0 ? -value : value);
  std::string out;
  int since_group = 0;
  for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
    out.insert(out.begin(), digits[i]);
    if (++since_group == 3 && i > 0) {
      out.insert(out.begin(), ',');
      since_group = 0;
    }
  }
  return value < 0 ? "-" + out : out;
}

std::vector<DashboardRow> PrivacyCenter::Rows() const {
  const struct {
    const char* label;
    EventType type;
  } kRows[] = {
      {"Trackers blocked", EventType::kTrackerBlocked},
      {"Ads blocked", EventType::kAdBlocked},
      {"Fingerprint attempts", EventType::kFingerprintAttemptBlocked},
      {"HTTPS upgrades", EventType::kHttpsUpgrade},
      {"Cookies partitioned", EventType::kCookiePartitioned},
  };
  std::vector<DashboardRow> rows;
  for (const auto& row : kRows) {
    DashboardRow out;
    out.label = row.label;
    out.value = log_->Total(row.type);
    out.formatted = FormatCount(out.value);
    rows.push_back(out);
  }
  return rows;
}

ProtectionLevel PrivacyCenter::Level() const {
  // Derived, never stored: the badge describes the current settings, so it
  // cannot fall out of step with them. The preset table itself lives in
  // privacy/security_levels (item 45) — a second copy here is how a browser
  // ends up shipping two different definitions of "Balanced".
  switch (privacy::SecurityLevels::Detect(*controls_)) {
    case privacy::SecurityLevel::kStandard:
      return ProtectionLevel::kStandard;
    case privacy::SecurityLevel::kBalanced:
      return ProtectionLevel::kBalanced;
    case privacy::SecurityLevel::kStrict:
      return ProtectionLevel::kStrict;
    case privacy::SecurityLevel::kMaximum:
      return ProtectionLevel::kMaximum;
    case privacy::SecurityLevel::kCustom:
      break;
  }
  return ProtectionLevel::kCustom;
}

const char* PrivacyCenter::LevelName(ProtectionLevel level) {
  switch (level) {
    case ProtectionLevel::kStandard:
      return "STANDARD";
    case ProtectionLevel::kBalanced:
      return "BALANCED";
    case ProtectionLevel::kStrict:
      return "STRICT";
    case ProtectionLevel::kMaximum:
      return "MAXIMUM";
    case ProtectionLevel::kCustom:
      return "CUSTOM";
  }
  return "CUSTOM";
}

const char* PrivacyCenter::DataSourceNote() {
  return "These counts come from this browser and stay on this device. "
         "Bedrock has no server to send them to.";
}

}  // namespace stats
}  // namespace bedrock
