// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_CORE_TELEMETRY_POLICY_H_
#define BEDROCK_PRIVACY_CORE_TELEMETRY_POLICY_H_

#include <string>
#include <vector>

// Telemetry policy (roadmap item 39).
//
// The default, and the shipped state, is **zero telemetry**: no analytics, no
// usage statistics, no tracking, no fingerprint collection, no history upload,
// no crash reports. Not "anonymised telemetry", not "aggregate counts" — none.
//
// This class is not a switchboard for turning things on. It exists so the
// promise is *checkable*: every category is enumerated, `Enabled()` answers
// for each one, and `Endpoint()` returns the empty string unless a category
// has been explicitly opted into by the user. A subsystem that wants to
// report anything must ask for an endpoint, and there is none to give.
//
// Crash reporting is the one category the roadmap allows to exist at all, and
// only as explicit opt-in with a disclosure the user reads first. Even then:
// off after install, off after update, and off again if the disclosure text
// ever changes — an opt-in to a different deal is not consent.
//
// `scripts/check_no_telemetry.py` scans the tree for the usual reporting
// machinery (histogram macros, metrics services, crash upload endpoints,
// analytics hosts) and fails the build. A policy object alone would only
// document the intent; the scanner is what keeps it true.

namespace bedrock {
namespace privacy {

enum class TelemetryCategory {
  kAnalytics,
  kUsageStatistics,
  kTracking,
  kFingerprintCollection,
  kBrowsingHistoryUpload,
  kCrashReports,
  kMaxValue = kCrashReports,
};

struct DisclosureText {
  std::string what_is_sent;
  std::string when_it_is_sent;
  std::string where_it_goes;
  std::string how_to_turn_it_off;
  std::string version;  // changing the deal invalidates the consent
};

class TelemetryPolicy {
 public:
  TelemetryPolicy();
  ~TelemetryPolicy();

  static const std::vector<TelemetryCategory>& Categories();
  static const char* CategoryName(TelemetryCategory category);

  // Categories that may never be enabled, by anyone, including the user:
  // there is no configuration in which Bedrock collects these.
  static bool IsPermanentlyProhibited(TelemetryCategory category);

  bool Enabled(TelemetryCategory category) const;
  bool AnyEnabled() const;

  // Returns false for a prohibited category, or when the disclosure the user
  // agreed to is not the current one.
  bool OptIn(TelemetryCategory category, const DisclosureText& disclosure);
  void OptOut(TelemetryCategory category);

  // The endpoint a subsystem would have to use. Empty unless that exact
  // category is opted in — so "just send it anyway" has nowhere to send.
  std::string Endpoint(TelemetryCategory category) const;
  void SetCrashEndpointForOptIn(std::string endpoint);

  // Called after an update: a new disclosure version revokes consent.
  void OnDisclosureChanged(const DisclosureText& current);

  static DisclosureText CrashReportDisclosure();
  // One-line summary for the settings page.
  static const char* Summary();

 private:
  bool enabled_[static_cast<int>(TelemetryCategory::kMaxValue) + 1] = {false};
  std::string accepted_disclosure_version_;
  std::string crash_endpoint_;
};

}  // namespace privacy
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_CORE_TELEMETRY_POLICY_H_
