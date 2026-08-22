// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/stats/privacy_event_log.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace bedrock {
namespace stats {

PrivacyEventLog::PrivacyEventLog() = default;
PrivacyEventLog::~PrivacyEventLog() = default;

void PrivacyEventLog::Record(const PrivacyEvent& event) {
  events_.push_back(event);
  if (static_cast<int>(events_.size()) > kMaxDetailedEvents)
    events_.erase(events_.begin());
  if (!event.private_window)
    ++totals_[static_cast<int>(event.type)];
}

int64_t PrivacyEventLog::Total(EventType type) const {
  return totals_[static_cast<int>(type)];
}

int64_t PrivacyEventLog::TotalAll() const {
  int64_t sum = 0;
  for (int i = 0; i <= static_cast<int>(EventType::kMaxValue); ++i) {
    if (i == static_cast<int>(EventType::kRequestAllowed))
      continue;  // allowed requests are context, not an achievement
    sum += totals_[i];
  }
  return sum;
}

SiteCounters PrivacyEventLog::ForSite(const std::string& site) const {
  SiteCounters counters;
  for (const PrivacyEvent& event : events_) {
    if (event.site != site)
      continue;
    if (!counters.measured) {
      // The first event for this site turns every -1 into a real 0: from here
      // on the numbers mean something.
      counters = SiteCounters{0, 0, 0, 0, 0, 0, 0, true};
    }
    switch (event.type) {
      case EventType::kTrackerBlocked:
        ++counters.trackers_blocked;
        break;
      case EventType::kAdBlocked:
        ++counters.ads_blocked;
        break;
      case EventType::kFingerprintAttemptBlocked:
        ++counters.fingerprint_attempts;
        break;
      case EventType::kHttpsUpgrade:
        ++counters.https_upgrades;
        break;
      case EventType::kCookiePartitioned:
        ++counters.cookies_partitioned;
        break;
      case EventType::kThirdPartyCookieBlocked:
        ++counters.third_party_cookies_blocked;
        break;
      case EventType::kScriptBlocked:
        ++counters.scripts_blocked;
        break;
      case EventType::kRequestAllowed:
        break;
    }
  }
  return counters;
}

std::vector<std::string> PrivacyEventLog::BlockedPartiesFor(
    const std::string& site) const {
  std::vector<std::string> parties;
  for (const PrivacyEvent& event : events_) {
    if (event.site != site || event.third_party.empty())
      continue;
    if (event.type == EventType::kRequestAllowed)
      continue;
    if (std::find(parties.begin(), parties.end(), event.third_party) ==
        parties.end()) {
      parties.push_back(event.third_party);
    }
  }
  std::sort(parties.begin(), parties.end());
  return parties;
}

std::vector<PrivacyEvent> PrivacyEventLog::EventsFor(
    const std::string& site) const {
  std::vector<PrivacyEvent> found;
  for (const PrivacyEvent& event : events_) {
    if (event.site == site)
      found.push_back(event);
  }
  return found;
}

std::string PrivacyEventLog::ExportJson() const {
  // The user's own numbers in a file they can open. Producing it is a local
  // action; nothing in this class sends it anywhere.
  std::string json = "{\"totals\":{";
  const char* names[] = {"trackers_blocked",
                         "ads_blocked",
                         "fingerprint_attempts_blocked",
                         "https_upgrades",
                         "cookies_partitioned",
                         "third_party_cookies_blocked",
                         "scripts_blocked",
                         "requests_allowed"};
  for (int i = 0; i <= static_cast<int>(EventType::kMaxValue); ++i) {
    if (i > 0)
      json += ",";
    json += "\"";
    json += names[i];
    json += "\":" + std::to_string(totals_[i]);
  }
  json += "},\"detailed_events\":" + std::to_string(events_.size()) + "}";
  return json;
}

void PrivacyEventLog::ClearSite(const std::string& site) {
  for (const PrivacyEvent& event : events_) {
    if (event.site == site && !event.private_window) {
      int64_t& total = totals_[static_cast<int>(event.type)];
      total = std::max<int64_t>(0, total - 1);
    }
  }
  events_.erase(std::remove_if(events_.begin(), events_.end(),
                               [&site](const PrivacyEvent& event) {
                                 return event.site == site;
                               }),
                events_.end());
}

void PrivacyEventLog::ClearAll() {
  events_.clear();
  for (int64_t& total : totals_)
    total = 0;
}

}  // namespace stats
}  // namespace bedrock
