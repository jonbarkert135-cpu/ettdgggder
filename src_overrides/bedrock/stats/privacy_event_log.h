// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_STATS_PRIVACY_EVENT_LOG_H_
#define BEDROCK_STATS_PRIVACY_EVENT_LOG_H_

#include <cstdint>
#include <string>
#include <vector>

// The privacy event log — the single source for items 36, 37 and 38.
//
// Three surfaces show privacy numbers: the DevTools privacy panels, the
// Privacy Center dashboard and the per-site Privacy Shield popup. If each one
// counted for itself they would disagree within a week, and a dashboard that
// contradicts the popup is worse than no dashboard. So there is one log:
// subsystems *report facts they actually performed*, and every screen is a
// query over it.
//
// Two rules make the numbers honest:
//
//  1. Nothing may be recorded that did not happen. `Record()` takes the event
//     from the subsystem that carried out the action — the blocking pipeline
//     for a blocked request, the fingerprint policy for a blocked probe, the
//     HTTPS policy for an upgrade. There is no estimator, no extrapolation
//     and no "typical page has ~30 trackers" filler.
//  2. Nothing leaves the machine. The log lives in memory and in the profile;
//     there is no upload path in this class and no consumer that takes one.
//     Item 37 asks for a dashboard, not telemetry.
//
// Absence of data is also data: a site that has not been measured reports
// `kNotMeasured`, never 0. "0 trackers blocked" and "we did not look" mean
// very different things to someone deciding whether to trust a page.

namespace bedrock {
namespace stats {

enum class EventType {
  kTrackerBlocked,
  kAdBlocked,
  kFingerprintAttemptBlocked,
  kHttpsUpgrade,
  kCookiePartitioned,
  kThirdPartyCookieBlocked,
  kScriptBlocked,
  kRequestAllowed,  // recorded so "0 blocked" can be distinguished from silence
  kMaxValue = kRequestAllowed,
};

struct PrivacyEvent {
  EventType type;
  std::string site;            // eTLD+1 of the top-level page
  std::string third_party;     // the party the event is about ("" if none)
  std::string detail;          // rule text, surface name, reason
  int64_t at = 0;              // unix seconds
  bool private_window = false;
};

// Per-site counters. -1 means "not measured", not zero.
struct SiteCounters {
  int trackers_blocked = -1;
  int ads_blocked = -1;
  int fingerprint_attempts = -1;
  int https_upgrades = -1;
  int cookies_partitioned = -1;
  int third_party_cookies_blocked = -1;
  int scripts_blocked = -1;
  bool measured = false;
};

class PrivacyEventLog {
 public:
  PrivacyEventLog();
  ~PrivacyEventLog();

  // Events from private windows are counted for the live session view (the
  // popup has to work there) but never persisted and never added to the
  // lifetime totals.
  void Record(const PrivacyEvent& event);

  // Lifetime totals for the Privacy Center (item 37).
  int64_t Total(EventType type) const;
  int64_t TotalAll() const;

  // Per-site view for the Privacy Shield popup (item 38).
  SiteCounters ForSite(const std::string& site) const;
  // Distinct third parties seen blocked on a site, for the DevTools panel.
  std::vector<std::string> BlockedPartiesFor(const std::string& site) const;
  std::vector<PrivacyEvent> EventsFor(const std::string& site) const;

  // The user's own data, in a plain file they can read. Not a report to us.
  std::string ExportJson() const;

  // Clearing is real: totals, per-site counters and the event list all go.
  void ClearSite(const std::string& site);
  void ClearAll();

  int event_count() const { return static_cast<int>(events_.size()); }
  // Ring buffer for the detail list; totals are kept as integers and are not
  // affected when old events fall off the end.
  static constexpr int kMaxDetailedEvents = 5000;

 private:
  std::vector<PrivacyEvent> events_;
  int64_t totals_[static_cast<int>(EventType::kMaxValue) + 1] = {0};
};

}  // namespace stats
}  // namespace bedrock

#endif  // BEDROCK_STATS_PRIVACY_EVENT_LOG_H_
