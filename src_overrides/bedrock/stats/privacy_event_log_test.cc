// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/stats/privacy_event_log.h"

#include <iostream>
#include <string>

#include "bedrock/privacy/protection_controller.h"
#include "bedrock/privacy/security_levels.h"
#include "bedrock/stats/privacy_center.h"

namespace {

using bedrock::privacy::Control;
using bedrock::privacy::ProtectionController;
using bedrock::privacy::Scope;
using bedrock::privacy::Value;
using bedrock::stats::EventType;
using bedrock::stats::PrivacyCenter;
using bedrock::stats::PrivacyEvent;
using bedrock::stats::PrivacyEventLog;
using bedrock::stats::ProtectionLevel;
using bedrock::stats::SiteCounters;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

PrivacyEvent Event(EventType type,
                   const std::string& site,
                   const std::string& party = "",
                   bool private_window = false) {
  PrivacyEvent event;
  event.type = type;
  event.site = site;
  event.third_party = party;
  event.detail = "||" + party + "^";
  event.at = 1'787'000'000;
  event.private_window = private_window;
  return event;
}

}  // namespace

int main() {
  PrivacyEventLog log;

  // Nothing measured yet: -1, not 0. The distinction is the point.
  const SiteCounters unknown = log.ForSite("example.com");
  Check(!unknown.measured, "an unvisited site is not measured");
  Check(unknown.trackers_blocked == -1,
        "an unmeasured counter is -1, never a confident zero");

  // A page that was measured and had nothing blocked reports a real 0.
  log.Record(Event(EventType::kRequestAllowed, "clean.example"));
  const SiteCounters clean = log.ForSite("clean.example");
  Check(clean.measured, "a visited site is measured");
  Check(clean.trackers_blocked == 0,
        "and an honest zero is different from 'we did not look'");

  for (int i = 0; i < 12; ++i)
    log.Record(Event(EventType::kTrackerBlocked, "example.com", "tracker.test"));
  for (int i = 0; i < 27; ++i)
    log.Record(Event(EventType::kAdBlocked, "example.com", "ads.test"));
  log.Record(Event(EventType::kHttpsUpgrade, "example.com"));
  log.Record(Event(EventType::kCookiePartitioned, "example.com", "cdn.test"));
  log.Record(
      Event(EventType::kFingerprintAttemptBlocked, "example.com", "fp.test"));

  const SiteCounters site = log.ForSite("example.com");
  Check(site.trackers_blocked == 12 && site.ads_blocked == 27,
        "per-site counts match the events that were recorded");
  Check(log.Total(EventType::kTrackerBlocked) == 12, "totals match too");
  Check(log.BlockedPartiesFor("example.com").size() == 4,
        "the distinct third parties are listed once each");

  // Private windows: visible in the live view, absent from lifetime totals.
  log.Record(Event(EventType::kTrackerBlocked, "secret.example", "t.test", true));
  Check(log.ForSite("secret.example").trackers_blocked == 1,
        "the popup still works in a private window");
  Check(log.Total(EventType::kTrackerBlocked) == 12,
        "but private-window events never reach the lifetime totals");

  // Export is a local file, and it is the same numbers.
  const std::string json = log.ExportJson();
  Check(json.find("\"trackers_blocked\":12") != std::string::npos,
        "the export carries the same totals the dashboard shows");

  // Clearing is real.
  log.ClearSite("example.com");
  Check(!log.ForSite("example.com").measured, "clearing a site removes it");
  Check(log.Total(EventType::kTrackerBlocked) == 0,
        "and takes its contribution out of the totals");

  // ---- Privacy Center (item 37) ----
  PrivacyEventLog dashboard_log;
  for (int i = 0; i < 12'481; ++i)
    dashboard_log.Record(Event(EventType::kTrackerBlocked, "a.example", "t"));
  for (int i = 0; i < 7'294; ++i)
    dashboard_log.Record(Event(EventType::kAdBlocked, "a.example", "ads"));

  ProtectionController controls;
  PrivacyCenter center(&dashboard_log, &controls);

  Check(PrivacyCenter::FormatCount(12481) == "12,481", "counts are grouped");
  Check(PrivacyCenter::FormatCount(213) == "213", "small counts are plain");
  Check(PrivacyCenter::FormatCount(1'000'000) == "1,000,000", "and big ones");

  const auto rows = center.Rows();
  Check(rows.size() == 5, "the dashboard has the five rows from the roadmap");
  Check(rows[0].label == "Trackers blocked" && rows[0].formatted == "12,481",
        "trackers row is the real total");
  Check(rows[1].formatted == "7,294", "ads row is the real total");
  Check(rows[3].value == 0,
        "a counter with no events is 0 rather than an invented number");

  // The level is derived from the settings, so it cannot go stale.
  Check(center.Level() == ProtectionLevel::kBalanced,
        "shipped defaults are BALANCED");
  controls.Set(Scope::kGlobal, "", Control::kScripts, Value::kBlock);
  Check(center.Level() == ProtectionLevel::kCustom,
        "changing one control immediately changes the badge");
  // The presets themselves live in privacy/security_levels (item 45); the
  // dashboard reads them rather than keeping a second table.
  bedrock::privacy::SecurityLevels::Apply(&controls,
                                          bedrock::privacy::SecurityLevel::kStrict);
  Check(center.Level() == ProtectionLevel::kStrict, "the strict preset is STRICT");
  Check(std::string(PrivacyCenter::LevelName(center.Level())) == "STRICT",
        "and renders in the roadmap's capitals");
  bedrock::privacy::SecurityLevels::Apply(&controls,
                                          bedrock::privacy::SecurityLevel::kMaximum);
  Check(std::string(PrivacyCenter::LevelName(center.Level())) == "MAXIMUM",
        "and the maximum preset is MAXIMUM");

  // The dashboard says where the numbers live.
  const std::string note = PrivacyCenter::DataSourceNote();
  Check(note.find("stay on this device") != std::string::npos,
        "the note says the data is local");
  const char* banned[] = {"anonymous", "anonymity", "untraceable", "100%",
                          "completely private", "invisible"};
  for (const char* word : banned) {
    Check(note.find(word) == std::string::npos,
          std::string("the dashboard note avoids: ") + word);
  }

  if (failures == 0)
    std::cout << "privacy_event_log_test: all assertions passed\n";
  return failures == 0 ? 0 : 1;
}
