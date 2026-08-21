// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_DEVTOOLS_PRIVACY_DEVTOOLS_H_
#define BEDROCK_DEVTOOLS_PRIVACY_DEVTOOLS_H_

#include <string>
#include <vector>

#include "bedrock/stats/privacy_event_log.h"

// Privacy panels for DevTools (roadmap item 36).
//
// Rule zero: **do not break Chromium DevTools.** Every existing panel,
// protocol domain and keyboard shortcut keeps working exactly as upstream.
// Bedrock *adds* panels; it removes and rewrites nothing. Web developers
// debug in this browser or they leave, and a privacy browser nobody can
// develop against has a very small future.
//
// Concretely that means:
//   - the panels below are added through the standard DevTools extension
//     surface, the same one Lighthouse and Recorder use;
//   - no upstream protocol domain is modified — Bedrock data arrives over its
//     own `Bedrock.privacy` domain, so a stock protocol client still works;
//   - if the Bedrock front-end fails to load, DevTools opens without it
//     rather than not at all (`degrades_gracefully`).
//
// The panels are views over the same privacy event log as the Privacy Center
// and the Shield popup — the numbers cannot disagree because there is only
// one set of them.

namespace bedrock {
namespace devtools {

enum class PrivacyPanel {
  kBlockedRequests,
  kTrackerInformation,
  kFingerprintProtection,
  kStoragePartition,
  kCookieState,
  kPermissions,
  kConnectionSecurity,
  kMaxValue = kConnectionSecurity,
};

struct PanelInfo {
  PrivacyPanel panel;
  const char* title;
  const char* id;             // devtools panel id, bedrock-prefixed
  const char* protocol_domain; // always "Bedrock.privacy"
};

// One row of the blocked-requests table.
struct BlockedRequestRow {
  std::string url;
  std::string third_party;
  std::string stage;   // which pipeline stage decided (item 13)
  std::string rule;    // the exact rule or heuristic that matched
  std::string action;  // Blocked / Partitioned / Redirected
};

class PrivacyDevTools {
 public:
  explicit PrivacyDevTools(const stats::PrivacyEventLog* log);
  ~PrivacyDevTools();

  static const std::vector<PanelInfo>& Panels();
  // Upstream panels Bedrock must not touch. Asserted by the test.
  static const std::vector<std::string>& UpstreamPanelsPreserved();
  static bool ModifiesUpstreamProtocol() { return false; }
  static bool DegradesGracefully() { return true; }

  // Easy access (item 36): the standard Chromium shortcuts still work, and
  // Bedrock adds one entry point straight to the privacy panels.
  static const char* InspectShortcut() { return "F12"; }
  static const char* PrivacyPanelShortcut() { return "Ctrl+Shift+Y"; }
  static const char* MenuPath() { return "Menu > More tools > Developer tools"; }

  std::vector<BlockedRequestRow> BlockedRequests(const std::string& site) const;
  std::vector<std::string> Trackers(const std::string& site) const;
  // Every row is "<label>: <value>" built from real state.
  std::vector<std::string> Summary(PrivacyPanel panel,
                                   const std::string& site) const;

 private:
  const stats::PrivacyEventLog* log_;
};

}  // namespace devtools
}  // namespace bedrock

#endif  // BEDROCK_DEVTOOLS_PRIVACY_DEVTOOLS_H_
