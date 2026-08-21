// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/core/telemetry_policy.h"

namespace bedrock {
namespace privacy {

TelemetryPolicy::TelemetryPolicy() = default;
TelemetryPolicy::~TelemetryPolicy() = default;

const std::vector<TelemetryCategory>& TelemetryPolicy::Categories() {
  static const std::vector<TelemetryCategory> kAll = {
      TelemetryCategory::kAnalytics,
      TelemetryCategory::kUsageStatistics,
      TelemetryCategory::kTracking,
      TelemetryCategory::kFingerprintCollection,
      TelemetryCategory::kBrowsingHistoryUpload,
      TelemetryCategory::kCrashReports};
  return kAll;
}

const char* TelemetryPolicy::CategoryName(TelemetryCategory category) {
  switch (category) {
    case TelemetryCategory::kAnalytics:
      return "Analytics";
    case TelemetryCategory::kUsageStatistics:
      return "Usage statistics";
    case TelemetryCategory::kTracking:
      return "Tracking";
    case TelemetryCategory::kFingerprintCollection:
      return "Fingerprint collection";
    case TelemetryCategory::kBrowsingHistoryUpload:
      return "Browsing history upload";
    case TelemetryCategory::kCrashReports:
      return "Crash reports";
  }
  return "Unknown";
}

bool TelemetryPolicy::IsPermanentlyProhibited(TelemetryCategory category) {
  // Everything except crash reporting. These are not settings with a
  // sensible "on" — a privacy browser that ships an analytics toggle has
  // already decided it might use it one day.
  return category != TelemetryCategory::kCrashReports;
}

bool TelemetryPolicy::Enabled(TelemetryCategory category) const {
  if (IsPermanentlyProhibited(category))
    return false;
  return enabled_[static_cast<int>(category)];
}

bool TelemetryPolicy::AnyEnabled() const {
  for (TelemetryCategory category : Categories()) {
    if (Enabled(category))
      return true;
  }
  return false;
}

bool TelemetryPolicy::OptIn(TelemetryCategory category,
                            const DisclosureText& disclosure) {
  if (IsPermanentlyProhibited(category))
    return false;
  // Consent is to a specific disclosure. Accepting an empty or mismatched one
  // is not consent.
  const DisclosureText current = CrashReportDisclosure();
  if (disclosure.version.empty() || disclosure.version != current.version)
    return false;
  if (disclosure.what_is_sent.empty() || disclosure.where_it_goes.empty() ||
      disclosure.how_to_turn_it_off.empty()) {
    return false;
  }
  enabled_[static_cast<int>(category)] = true;
  accepted_disclosure_version_ = disclosure.version;
  return true;
}

void TelemetryPolicy::OptOut(TelemetryCategory category) {
  enabled_[static_cast<int>(category)] = false;
}

std::string TelemetryPolicy::Endpoint(TelemetryCategory category) const {
  if (!Enabled(category))
    return std::string();
  if (category == TelemetryCategory::kCrashReports)
    return crash_endpoint_;
  return std::string();
}

void TelemetryPolicy::SetCrashEndpointForOptIn(std::string endpoint) {
  crash_endpoint_ = std::move(endpoint);
}

void TelemetryPolicy::OnDisclosureChanged(const DisclosureText& current) {
  if (current.version == accepted_disclosure_version_)
    return;
  // The terms changed, so the agreement did too. Ask again.
  for (TelemetryCategory category : Categories())
    enabled_[static_cast<int>(category)] = false;
  accepted_disclosure_version_.clear();
}

DisclosureText TelemetryPolicy::CrashReportDisclosure() {
  DisclosureText disclosure;
  disclosure.what_is_sent =
      "A crash report contains the state of the program when it stopped: the "
      "call stack, the Bedrock version, your operating system version, and "
      "the memory the crashed component was using. That memory can include "
      "page content and, in rare cases, an address you had open.";
  disclosure.when_it_is_sent = "Only after a crash, and only if you say yes.";
  disclosure.where_it_goes =
      "To the crash service configured in settings. Bedrock runs no crash "
      "server of its own; if this field is empty, no report can be sent.";
  disclosure.how_to_turn_it_off =
      "Settings > Privacy > Crash reports. Off by default and after every "
      "update.";
  disclosure.version = "crash-disclosure-1";
  return disclosure;
}

const char* TelemetryPolicy::Summary() {
  return "Bedrock collects nothing. There is no analytics, no usage "
         "statistics, no history upload and no crash reporting unless you "
         "turn crash reporting on yourself.";
}

}  // namespace privacy
}  // namespace bedrock
