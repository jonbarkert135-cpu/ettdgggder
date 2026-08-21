// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/stats/privacy_center.h"

namespace bedrock {
namespace stats {

using privacy::Control;
using privacy::Value;

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
  // cannot fall out of step with them. The presets below are the same three
  // the settings page offers; anything else is honestly CUSTOM.
  const Value ads = controls_->Get(Control::kAds, "", "");
  const Value trackers = controls_->Get(Control::kTrackers, "", "");
  const Value fp = controls_->Get(Control::kFingerprinting, "", "");
  const Value cookies = controls_->Get(Control::kCookies, "", "");
  const Value scripts = controls_->Get(Control::kScripts, "", "");

  const bool strict = (ads == Value::kBlock || ads == Value::kBlockStrict) &&
                      trackers == Value::kBlock &&
                      (fp == Value::kBlock || fp == Value::kBlockStrict) &&
                      (cookies == Value::kBlock ||
                       cookies == Value::kBlockStrict) &&
                      scripts == Value::kBlock;
  if (strict)
    return ProtectionLevel::kStrict;

  // Balanced is what Bedrock ships with (item 11 defaults).
  if (ads == Value::kBlock && trackers == Value::kBlock &&
      fp == Value::kReduce && cookies == Value::kReduce &&
      scripts == Value::kAllow) {
    return ProtectionLevel::kBalanced;
  }

  // Standard: compatibility first — trackers still go, ads and fingerprinting
  // protection do not.
  if (ads == Value::kAllow && trackers == Value::kBlock &&
      fp == Value::kAllow && cookies == Value::kReduce &&
      scripts == Value::kAllow) {
    return ProtectionLevel::kStandard;
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
