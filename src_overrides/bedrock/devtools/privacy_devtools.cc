// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/devtools/privacy_devtools.h"

namespace bedrock {
namespace devtools {
namespace {

using stats::EventType;
using stats::PrivacyEvent;

const char kDomain[] = "Bedrock.privacy";

std::string ActionFor(EventType type) {
  switch (type) {
    case EventType::kCookiePartitioned:
      return "Partitioned";
    case EventType::kRequestAllowed:
      return "Allowed";
    default:
      return "Blocked";
  }
}

}  // namespace

PrivacyDevTools::PrivacyDevTools(const stats::PrivacyEventLog* log)
    : log_(log) {}

PrivacyDevTools::~PrivacyDevTools() = default;

const std::vector<PanelInfo>& PrivacyDevTools::Panels() {
  static const std::vector<PanelInfo> kPanels = {
      {PrivacyPanel::kBlockedRequests, "Blocked requests",
       "bedrock-blocked-requests", kDomain},
      {PrivacyPanel::kTrackerInformation, "Trackers", "bedrock-trackers",
       kDomain},
      {PrivacyPanel::kFingerprintProtection, "Fingerprint protection",
       "bedrock-fingerprinting", kDomain},
      {PrivacyPanel::kStoragePartition, "Storage partition",
       "bedrock-storage-partition", kDomain},
      {PrivacyPanel::kCookieState, "Cookie state", "bedrock-cookies", kDomain},
      {PrivacyPanel::kPermissions, "Permissions", "bedrock-permissions",
       kDomain},
      {PrivacyPanel::kConnectionSecurity, "Connection security",
       "bedrock-connection", kDomain},
  };
  return kPanels;
}

const std::vector<std::string>& PrivacyDevTools::UpstreamPanelsPreserved() {
  // The list a regression would show up in: if any of these stops working,
  // item 36's first sentence has been violated.
  static const std::vector<std::string> kUpstream = {
      "elements", "console",     "sources",   "network",
      "performance", "memory",   "application", "security",
      "lighthouse", "recorder"};
  return kUpstream;
}

std::vector<BlockedRequestRow> PrivacyDevTools::BlockedRequests(
    const std::string& site) const {
  std::vector<BlockedRequestRow> rows;
  for (const PrivacyEvent& event : log_->EventsFor(site)) {
    if (event.type == EventType::kRequestAllowed)
      continue;
    if (event.type == EventType::kHttpsUpgrade)
      continue;  // an upgrade is not a blocked request
    BlockedRequestRow row;
    row.third_party = event.third_party;
    row.url = event.third_party;
    row.rule = event.detail;
    row.action = ActionFor(event.type);
    switch (event.type) {
      case EventType::kTrackerBlocked:
        row.stage = "filter lists / heuristic";
        break;
      case EventType::kAdBlocked:
        row.stage = "filter lists";
        break;
      case EventType::kFingerprintAttemptBlocked:
        row.stage = "fingerprint policy";
        break;
      case EventType::kCookiePartitioned:
      case EventType::kThirdPartyCookieBlocked:
        row.stage = "cookie policy";
        break;
      case EventType::kScriptBlocked:
        row.stage = "script policy";
        break;
      default:
        row.stage = "pipeline";
        break;
    }
    rows.push_back(row);
  }
  return rows;
}

std::vector<std::string> PrivacyDevTools::Trackers(
    const std::string& site) const {
  return log_->BlockedPartiesFor(site);
}

std::vector<std::string> PrivacyDevTools::Summary(
    PrivacyPanel panel,
    const std::string& site) const {
  const stats::SiteCounters counters = log_->ForSite(site);
  std::vector<std::string> lines;
  const auto number = [&counters](int value) {
    return counters.measured ? std::to_string(value)
                             : std::string("not measured");
  };
  switch (panel) {
    case PrivacyPanel::kBlockedRequests:
      lines.push_back("Trackers blocked: " +
                      number(counters.trackers_blocked));
      lines.push_back("Ads blocked: " + number(counters.ads_blocked));
      lines.push_back("Scripts blocked: " + number(counters.scripts_blocked));
      break;
    case PrivacyPanel::kTrackerInformation:
      for (const std::string& party : Trackers(site))
        lines.push_back("Third party: " + party);
      if (lines.empty()) {
        lines.push_back(counters.measured
                            ? "No third-party requests were blocked here"
                            : "Third parties: not measured");
      }
      break;
    case PrivacyPanel::kFingerprintProtection:
      lines.push_back("Attempts blocked: " +
                      number(counters.fingerprint_attempts));
      lines.push_back(
          "Surfaces are listed in docs/privacy/fingerprinting/");
      break;
    case PrivacyPanel::kStoragePartition:
      lines.push_back("Storage key: (origin, top-level site, is-cross-site)");
      lines.push_back("Cookies partitioned: " +
                      number(counters.cookies_partitioned));
      break;
    case PrivacyPanel::kCookieState:
      lines.push_back("Third-party cookies blocked: " +
                      number(counters.third_party_cookies_blocked));
      break;
    case PrivacyPanel::kPermissions:
      lines.push_back("Permissions are shown from the page's own state");
      break;
    case PrivacyPanel::kConnectionSecurity:
      lines.push_back("HTTPS upgrades: " + number(counters.https_upgrades));
      break;
  }
  return lines;
}

}  // namespace devtools
}  // namespace bedrock
