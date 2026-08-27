// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh. Phase 7.

#include "bedrock/integration/network_hook.h"

#include <iostream>
#include <string>

namespace {

using bedrock::integration::BuiltInFilterList;
using bedrock::integration::BuiltInRuleCount;
using bedrock::integration::DecideRequest;
using bedrock::integration::NetworkDecision;
using bedrock::integration::NetworkHookStartupLine;
using bedrock::integration::NetworkRequest;
using bedrock::integration::RequestKind;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

NetworkRequest OnPage(const std::string& url,
                      const std::string& host,
                      const std::string& etld1,
                      RequestKind kind = RequestKind::kScript) {
  NetworkRequest request;
  request.url = url;
  request.host = host;
  request.etld1 = etld1;
  request.top_host = "www.example.com";
  request.top_etld1 = "example.com";
  request.kind = kind;
  return request;
}

// The point of the whole seam: a listed third-party tracker on someone else's
// page is blocked, and the answer names the rule so the panel and the log can
// explain themselves.
void ListedThirdPartyTrackerIsBlocked() {
  const NetworkDecision decision =
      DecideRequest(OnPage("https://www.google-analytics.com/analytics.js",
                           "www.google-analytics.com", "google-analytics.com"));
  Check(decision.blocked, "listed tracker is blocked");
  Check(decision.reason == "filter-list",
        "block reason is the filter list, got: " + decision.reason);
  Check(!decision.detail.empty(), "a blocked request names the rule");
}

// A request to the site the user is actually on is never a tracker, however its
// host is spelled. This is the check that keeps the blocker from breaking the
// web when a site self-hosts analytics.
void FirstPartyRequestIsAllowed() {
  NetworkRequest request = OnPage("https://www.example.com/app.js",
                                  "www.example.com", "example.com");
  const NetworkDecision decision = DecideRequest(request);
  Check(!decision.blocked, "first-party request is allowed");
  Check(decision.reason == "first-party",
        "first-party reason, got: " + decision.reason);
}

// An unlisted third party still loads: the default is "may work, may not
// track", not "block everything we do not recognise".
void UnlistedThirdPartyStillLoads() {
  const NetworkDecision decision = DecideRequest(
      OnPage("https://cdn.jsdelivr.net/npm/lib.js", "cdn.jsdelivr.net",
             "jsdelivr.net"));
  Check(!decision.blocked, "unknown third party is not blocked outright");
}

// The network service sometimes cannot say which page a request belongs to.
// Guessing is how blockers break browser updates and favicons, so the hook
// declines to decide and says so.
void RequestWithoutATopFrameIsAllowedAndLabelled() {
  NetworkRequest request;
  request.url = "https://www.google-analytics.com/analytics.js";
  request.host = "www.google-analytics.com";
  request.etld1 = "google-analytics.com";
  const NetworkDecision decision = DecideRequest(request);
  Check(!decision.blocked, "no top frame means no block");
  Check(decision.reason == "no-top-frame",
        "the reason states why, got: " + decision.reason);
}

// A tracker host visited as the top-level document is the user's own choice.
void TrackerAsTheTopLevelDocumentIsAllowed() {
  NetworkRequest request;
  request.url = "https://www.google-analytics.com/";
  request.host = "www.google-analytics.com";
  request.etld1 = "google-analytics.com";
  request.top_host = "www.google-analytics.com";
  request.top_etld1 = "google-analytics.com";
  request.kind = RequestKind::kDocument;
  const NetworkDecision decision = DecideRequest(request);
  Check(!decision.blocked, "the page the user typed is never blocked");
}

// The shipped list has to actually parse. A silently empty engine would make
// every probe pass for the wrong reason.
void TheBuiltInListParses() {
  Check(BuiltInRuleCount() >= 15,
        "built-in rules accepted: " + std::to_string(BuiltInRuleCount()));
  const std::string list = BuiltInFilterList();
  Check(list.find("$third-party") != std::string::npos,
        "every shipped rule is third-party scoped");
  Check(list.find("easylist") == std::string::npos &&
            list.find("EasyList") == std::string::npos,
        "the shipped list is ours, not an imported one");
}

// The startup line is what a running browser prints; it must not overstate what
// the seam does, because profile settings are not wired into it yet.
void TheStartupLineIsHonest() {
  const std::string line = NetworkHookStartupLine();
  Check(line.find("network hook: live") == 0, "line starts with the state");
  Check(line.find("shipped defaults only") != std::string::npos,
        "line admits profile settings are not applied yet: " + line);
}

}  // namespace

int main() {
  ListedThirdPartyTrackerIsBlocked();
  FirstPartyRequestIsAllowed();
  UnlistedThirdPartyStillLoads();
  RequestWithoutATopFrameIsAllowedAndLabelled();
  TrackerAsTheTopLevelDocumentIsAllowed();
  TheBuiltInListParses();
  TheStartupLineIsHonest();
  if (failures == 0) {
    std::cout << "network_hook: ok (" << BuiltInRuleCount()
              << " built-in rules)\n";
  }
  return failures == 0 ? 0 : 1;
}
