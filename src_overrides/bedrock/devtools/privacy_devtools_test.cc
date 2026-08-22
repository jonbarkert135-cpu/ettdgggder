// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/devtools/privacy_devtools.h"

#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {

using bedrock::devtools::BlockedRequestRow;
using bedrock::devtools::PanelInfo;
using bedrock::devtools::PrivacyDevTools;
using bedrock::devtools::PrivacyPanel;
using bedrock::stats::EventType;
using bedrock::stats::PrivacyEvent;
using bedrock::stats::PrivacyEventLog;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

PrivacyEvent Event(EventType type, const std::string& party) {
  PrivacyEvent event;
  event.type = type;
  event.site = "example.com";
  event.third_party = party;
  event.detail = "||" + party + "^$third-party";
  return event;
}

}  // namespace

int main() {
  // Rule zero: DevTools keeps working.
  Check(!PrivacyDevTools::ModifiesUpstreamProtocol(),
        "no upstream DevTools protocol domain is modified");
  Check(PrivacyDevTools::DegradesGracefully(),
        "DevTools still opens if the Bedrock front-end fails to load");
  Check(PrivacyDevTools::UpstreamPanelsPreserved().size() >= 8,
        "the upstream panels that must keep working are listed");
  for (const std::string& panel : PrivacyDevTools::UpstreamPanelsPreserved()) {
    Check(panel.rfind("bedrock", 0) != 0,
          "the preserved list is upstream panels, not ours: " + panel);
  }
  Check(std::string(PrivacyDevTools::InspectShortcut()) == "F12",
        "the standard shortcut is untouched");

  // The added panels are the seven from item 36, on their own domain.
  const std::vector<PanelInfo>& panels = PrivacyDevTools::Panels();
  Check(panels.size() == 7, "seven privacy panels");
  std::set<std::string> ids;
  for (const PanelInfo& info : panels) {
    Check(std::string(info.id).rfind("bedrock-", 0) == 0,
          std::string("panel ids are namespaced: ") + info.id);
    Check(std::string(info.protocol_domain) == "Bedrock.privacy",
          "panels speak Bedrock's own protocol domain");
    ids.insert(info.id);
  }
  Check(ids.size() == panels.size(), "panel ids are unique");

  PrivacyEventLog log;
  PrivacyDevTools tools(&log);

  // Before anything is measured, the panels say so rather than showing zeros.
  {
    const auto lines = tools.Summary(PrivacyPanel::kBlockedRequests, "example.com");
    Check(lines[0].find("not measured") != std::string::npos,
          "an unmeasured page is reported as unmeasured, not as zero");
  }

  log.Record(Event(EventType::kTrackerBlocked, "tracker.test"));
  log.Record(Event(EventType::kAdBlocked, "ads.test"));
  log.Record(Event(EventType::kFingerprintAttemptBlocked, "fp.test"));
  log.Record(Event(EventType::kHttpsUpgrade, ""));
  log.Record(Event(EventType::kRequestAllowed, "cdn.test"));

  const std::vector<BlockedRequestRow> rows =
      tools.BlockedRequests("example.com");
  Check(rows.size() == 3,
        "allowed requests and https upgrades are not listed as blocked");
  for (const BlockedRequestRow& row : rows) {
    Check(!row.stage.empty(),
          "every row names the pipeline stage that decided it");
    Check(!row.rule.empty(), "and the exact rule that matched");
    Check(row.action == "Blocked" || row.action == "Partitioned",
          "with a real action");
  }

  Check(tools.Trackers("example.com").size() == 3,
        "the tracker panel lists the third parties actually involved");

  const auto blocked = tools.Summary(PrivacyPanel::kBlockedRequests,
                                     "example.com");
  Check(blocked[0] == "Trackers blocked: 1", "counts come from the same log");
  Check(tools.Summary(PrivacyPanel::kConnectionSecurity, "example.com")[0] ==
            "HTTPS upgrades: 1",
        "so does the connection panel");
  Check(tools.Summary(PrivacyPanel::kStoragePartition, "example.com")[0].find(
            "origin, top-level site, is-cross-site") != std::string::npos,
        "the storage panel shows the real storage key shape");

  // Every panel produces something; an empty panel is a bug, not a design.
  for (const PanelInfo& info : panels) {
    Check(!tools.Summary(info.panel, "example.com").empty(),
          std::string("panel has content: ") + info.title);
  }

  if (failures == 0)
    std::cout << "privacy_devtools_test: all assertions passed\n";
  return failures == 0 ? 0 : 1;
}
