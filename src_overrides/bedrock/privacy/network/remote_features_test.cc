// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.
//
// Items 94 and 95. The table is only worth having if the validator refuses the
// shapes the two items ban, so each case below is a table that must not be
// legal — a cloud service of ours, a background feature switched on, a feature
// with no off switch — plus the real table, which must be.

#include "bedrock/privacy/network/remote_features.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

using bedrock::network::AllRemoteFeatures;
using bedrock::network::FindRemoteFeature;
using bedrock::network::Operator;
using bedrock::network::RemoteFeature;
using bedrock::network::RemoteFeatureProblems;
using bedrock::network::Status;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

// The validator's rules, applied to a table the test builds. Mirrors
// RemoteFeatureProblems() for a single row so the bans can be demonstrated.
std::vector<std::string> ProblemsFor(const RemoteFeature& feature) {
  std::vector<std::string> problems;
  const std::string id = feature.id;
  if (feature.op == Operator::kBedrockOperated) problems.push_back(id + ": ours");
  if (std::string(feature.how_to_disable).empty()) problems.push_back(id + ": no off");
  if (feature.on_by_default && !feature.inherent) problems.push_back(id + ": on");
  if (feature.status == Status::kImplemented &&
      std::string(feature.replacement).empty()) {
    problems.push_back(id + ": fixed endpoint");
  }
  return problems;
}

RemoteFeature Legal() {
  return {"probe", "search", "someone else's server",
          Operator::kThirdPartyYouChose, Status::kPolicyOnly, false, false,
          "Settings > somewhere", "any endpoint", "docs/design/006-search-system.md"};
}

void TestTheShippedTableIsLegal() {
  Check(RemoteFeatureProblems().empty(), "the real table breaks no rule of items 94/95");
  Check(!AllRemoteFeatures().empty(), "the table is not empty");

  for (const RemoteFeature& feature : AllRemoteFeatures()) {
    Check(feature.op != Operator::kBedrockOperated,
          std::string(feature.id) + " is not a Bedrock-operated service");
    Check(!std::string(feature.doc).empty(),
          std::string(feature.id) + " points at a document");
  }
}

void TestNothingIsOnByDefaultExceptYourOwnNavigation() {
  for (const RemoteFeature& feature : AllRemoteFeatures()) {
    if (!feature.on_by_default) continue;
    Check(feature.inherent,
          std::string(feature.id) + " is on by default only because it is your "
                                    "own request going where you sent it");
  }
  // The browser is complete without any of them: turning every optional
  // feature off leaves only the inherent ones, which are the user's own actions.
  int background_on = 0;
  for (const RemoteFeature& feature : AllRemoteFeatures()) {
    if (feature.on_by_default && !feature.inherent) ++background_on;
  }
  Check(background_on == 0, "no background remote feature is on out of the box");
}

void TestTheBansAreEnforced() {
  RemoteFeature ours = Legal();
  ours.op = Operator::kBedrockOperated;
  Check(!ProblemsFor(ours).empty(), "a Bedrock-operated service is rejected (item 94)");

  RemoteFeature always_on = Legal();
  always_on.on_by_default = true;
  Check(!ProblemsFor(always_on).empty(),
        "a background feature on by default is rejected (item 95)");

  RemoteFeature stuck = Legal();
  stuck.status = Status::kImplemented;
  stuck.replacement = "";
  Check(!ProblemsFor(stuck).empty(), "an implemented feature tied to one endpoint is rejected");

  RemoteFeature no_off = Legal();
  no_off.how_to_disable = "";
  Check(!ProblemsFor(no_off).empty(), "a feature with no off switch is rejected");

  Check(ProblemsFor(Legal()).empty(), "a legal optional feature passes");
}

void TestLookup() {
  Check(FindRemoteFeature("doh_resolver") != nullptr, "lookup finds a known feature");
  Check(FindRemoteFeature("cloud_sync") == nullptr,
        "there is no cloud sync to find (item 94)");
  Check(FindRemoteFeature("") == nullptr, "an empty id matches nothing");
}

}  // namespace

int main() {
  TestTheShippedTableIsLegal();
  TestNothingIsOnByDefaultExceptYourOwnNavigation();
  TestTheBansAreEnforced();
  TestLookup();
  if (failures == 0) {
    std::cout << "remote_features_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
