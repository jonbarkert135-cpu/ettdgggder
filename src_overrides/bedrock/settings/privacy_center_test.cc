// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/settings/privacy_center.h"

#include <iostream>
#include <string>

namespace {

using bedrock::privacy::ProtectionController;
using bedrock::stats::DashboardJson;
using bedrock::stats::EventType;
using bedrock::stats::PrivacyCenter;
using bedrock::stats::PrivacyEvent;
using bedrock::stats::PrivacyEventLog;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

bool Has(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

PrivacyEvent Event(EventType type) {
  PrivacyEvent event;
  event.type = type;
  event.site = "example.com";
  event.third_party = "tracker.example";
  return event;
}

}  // namespace

int main() {
  // Formatting is presentation; the raw number stays in the model.
  Check(PrivacyCenter::FormatCount(12481) == "12,481", "counts are grouped");
  Check(PrivacyCenter::FormatCount(0) == "0", "zero is zero");

  PrivacyEventLog log;
  ProtectionController controls;
  PrivacyCenter center(&log, &controls);

  // An empty log means an empty dashboard — not an invented one.
  const std::string empty = DashboardJson(center);
  Check(Has(empty, "\"label\":\"Trackers blocked\",\"value\":0"),
        "a browser that blocked nothing yet reports nothing");
  Check(Has(empty, "\"note\":\"These counts come from this browser"),
        "the dashboard says where its numbers come from");

  for (int i = 0; i < 3; ++i)
    log.Record(Event(EventType::kTrackerBlocked));
  log.Record(Event(EventType::kHttpsUpgrade));

  const std::string json = DashboardJson(center);
  Check(Has(json, "\"label\":\"Trackers blocked\",\"value\":3,\"formatted\":\"3\""),
        "every figure is a count of something the engine actually did");
  Check(Has(json, "\"label\":\"HTTPS upgrades\",\"value\":1"), "and so is this one");
  Check(Has(json, "\"label\":\"Ads blocked\",\"value\":0"),
        "an unused counter stays at zero rather than being hidden");
  Check(Has(json, "\"level\":\""), "the protection level is derived and shown");

  if (failures == 0)
    std::cout << "privacy_center: ok\n";
  return failures == 0 ? 0 : 1;
}
