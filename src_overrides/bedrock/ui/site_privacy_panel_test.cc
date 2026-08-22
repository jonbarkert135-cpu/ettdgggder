// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/ui/site_privacy_panel.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

using bedrock::privacy::Control;
using bedrock::privacy::ProtectionController;
using bedrock::privacy::Scope;
using bedrock::privacy::Value;
using bedrock::stats::EventType;
using bedrock::stats::PrivacyEvent;
using bedrock::stats::PrivacyEventLog;
using bedrock::ui::ConnectionState;
using bedrock::ui::PanelRow;
using bedrock::ui::RowKind;
using bedrock::ui::SitePrivacyPanel;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

std::string ValueOf(const std::vector<PanelRow>& rows,
                    const std::string& label) {
  for (const PanelRow& row : rows) {
    if (row.label == label)
      return row.value;
  }
  return "<missing>";
}

PrivacyEvent Event(EventType type, const std::string& party) {
  PrivacyEvent event;
  event.type = type;
  event.site = "example.com";
  event.third_party = party;
  return event;
}

ConnectionState Https() {
  ConnectionState state;
  state.https = true;
  state.certificate_valid = true;
  return state;
}

}  // namespace

int main() {
  PrivacyEventLog log;
  ProtectionController controls;
  SitePrivacyPanel panel(&log, &controls);

  // A page nobody has measured yet must not show zeros.
  {
    const auto rows = panel.Build("example.com", "example.com", Https());
    Check(ValueOf(rows, "Trackers") == "Not measured",
          "an unmeasured page says so instead of claiming zero trackers");
    Check(ValueOf(rows, "Ads") == "Not measured", "same for ads");
    for (const PanelRow& row : rows) {
      if (row.kind == RowKind::kCount)
        Check(!row.measured, "count rows are marked unmeasured");
    }
  }

  for (int i = 0; i < 12; ++i)
    log.Record(Event(EventType::kTrackerBlocked, "tracker.test"));
  for (int i = 0; i < 27; ++i)
    log.Record(Event(EventType::kAdBlocked, "ads.test"));

  {
    const auto rows = panel.Build("example.com", "example.com", Https());
    Check(rows.size() == 7, "the panel has the seven rows from the roadmap");
    Check(ValueOf(rows, "Connection") == "HTTPS", "connection row");
    Check(ValueOf(rows, "Trackers") == "12 blocked",
          "the tracker count is the number of real block events");
    Check(ValueOf(rows, "Ads") == "27 blocked", "so is the ad count");
    Check(ValueOf(rows, "Fingerprinting").find("Protected") == 0,
          "fingerprinting reports the policy in force");
    Check(ValueOf(rows, "Third-party cookies") == "Blocked", "cookie row");
    Check(ValueOf(rows, "Scripts") == "Allowed", "script row");
    Check(ValueOf(rows, "Site storage") == "Partitioned", "storage row");
  }

  // Counts follow the events, not a guess: recording one more moves the row.
  log.Record(Event(EventType::kTrackerBlocked, "tracker2.test"));
  Check(ValueOf(panel.Build("example.com", "example.com", Https()),
                "Trackers") == "13 blocked",
        "the count tracks the log exactly");
  Check(panel.BlockedParties("example.com").size() == 3,
        "the expanded list names the third parties actually blocked");

  // Fingerprint attempts appear on the row only when they were observed.
  log.Record(Event(EventType::kFingerprintAttemptBlocked, "fp.test"));
  Check(ValueOf(panel.Build("example.com", "example.com", Https()),
                "Fingerprinting")
                .find("1 attempts blocked") != std::string::npos,
        "an observed fingerprinting attempt is shown");

  // Per-site settings show through, because the row is the effective policy.
  controls.Set(Scope::kSite, "example.com", Control::kScripts, Value::kBlock);
  Check(ValueOf(panel.Build("example.com", "example.com", Https()),
                "Scripts") == "Blocked",
        "a per-site override is reflected immediately");
  controls.Set(Scope::kSite, "example.com", Control::kCookies,
               Value::kBlockStrict);
  Check(ValueOf(panel.Build("example.com", "example.com", Https()),
                "Site storage") == "Partitioned, cleared on close",
        "strict cookies change what the storage row promises");

  // Connection states are facts about the load.
  {
    ConnectionState http;
    Check(ValueOf(panel.Build("example.com", "example.com", http),
                  "Connection") == "Not secure (HTTP)",
          "plain http is called what it is");

    ConnectionState upgraded = Https();
    upgraded.upgraded = true;
    Check(ValueOf(panel.Build("example.com", "example.com", upgraded),
                  "Connection") == "HTTPS (upgraded)",
          "an upgrade is disclosed rather than hidden");

    ConnectionState bad_cert = Https();
    bad_cert.certificate_valid = false;
    Check(ValueOf(panel.Build("example.com", "example.com", bad_cert),
                  "Connection") == "Certificate problem",
          "a certificate problem is never softened into a green tick");

    ConnectionState mixed = Https();
    mixed.mixed_content = true;
    Check(ValueOf(panel.Build("example.com", "example.com", mixed), "Connection")
              .find("mixed content") != std::string::npos,
          "blocked mixed content is stated");
  }

  // No row may promise anonymity.
  {
    const auto rows = panel.Build("example.com", "example.com", Https());
    const char* banned[] = {"anonymous", "anonymity",  "untraceable",
                            "100%",      "invisible",  "no one can"};
    for (const PanelRow& row : rows) {
      for (const char* word : banned) {
        Check(row.value.find(word) == std::string::npos,
              std::string("no anonymity promise in: ") + row.value);
      }
    }
  }

  if (failures == 0)
    std::cout << "site_privacy_panel_test: all assertions passed\n";
  return failures == 0 ? 0 : 1;
}
