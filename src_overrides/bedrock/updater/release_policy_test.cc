// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/updater/release_policy.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace bedrock::update;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << "\n";
    ++failures;
  }
}

ReleaseCandidate FullPipeline() {
  ReleaseCandidate candidate;
  candidate.chromium_version = "151.0.7922.173";
  candidate.completed_stages = {Stage::kSecurityReview, Stage::kPrivacyPatches,
                                Stage::kBrowserPatches, Stage::kAutomatedTests,
                                Stage::kPrivacyRegression};
  return candidate;
}

SecurityFix Fix(Severity severity, int hours_public) {
  SecurityFix fix;
  fix.upstream_id = "upstream-1";
  fix.severity = severity;
  fix.hours_since_public = hours_public;
  return fix;
}

void TestDeadlinesAreOrdered() {
  Check(DeadlineHours(Severity::kCritical) < DeadlineHours(Severity::kHigh),
        "critical is tighter than high");
  Check(DeadlineHours(Severity::kHigh) < DeadlineHours(Severity::kMedium),
        "high is tighter than medium");
  Check(DeadlineHours(Severity::kMedium) < DeadlineHours(Severity::kLow),
        "medium is tighter than low");
  Check(DeadlineHours(Severity::kCritical) <= 72,
        "a public critical bug gets days, not weeks");
}

void TestSecurityBeatsFeature() {
  // The item 69 case, stated as plainly as the rule: a critical fix is pending
  // and a feature is not ready. The feature goes, not the release.
  ReleaseCandidate candidate = FullPipeline();
  candidate.security_fixes.push_back(Fix(Severity::kCritical, 24));
  candidate.features.push_back({"vertical tab groups", false});
  candidate.features.push_back({"filter list ui", true});

  Decision decision = Evaluate(candidate);
  Check(decision.action == Action::kDropFeatures,
        "an unready feature is dropped, the security release proceeds");
  Check(decision.dropped_features.size() == 1 &&
            decision.dropped_features[0] == "vertical tab groups",
        "only the unready feature is dropped");
  Check(decision.hours_remaining == DeadlineHours(Severity::kCritical) - 24,
        "hours remaining counts from when the fix went public");
  Check(!decision.overdue, "24h into a 72h deadline is not overdue");
}

void TestOverdue() {
  ReleaseCandidate candidate = FullPipeline();
  candidate.security_fixes.push_back(Fix(Severity::kHigh, 200));
  Decision decision = Evaluate(candidate);
  Check(decision.overdue, "200h into a one-week deadline is overdue");
  Check(decision.hours_remaining < 0, "hours remaining goes negative rather than clamping");
  Check(decision.action == Action::kShip, "overdue still ships — it does not block itself");
}

void TestTightestDeadlineWins() {
  ReleaseCandidate candidate = FullPipeline();
  candidate.security_fixes.push_back(Fix(Severity::kLow, 0));
  SecurityFix urgent = Fix(Severity::kCritical, 60);
  urgent.upstream_id = "upstream-critical";
  candidate.security_fixes.push_back(urgent);
  candidate.features.push_back({"half-done thing", false});

  Decision decision = Evaluate(candidate);
  Check(decision.hours_remaining == 12, "the tightest deadline drives the schedule");
  Check(decision.reason.find("upstream-critical") != std::string::npos,
        "the reason names the fix that sets the deadline");
}

void TestIncompletePipelineBlocks() {
  ReleaseCandidate candidate;
  candidate.completed_stages = {Stage::kSecurityReview, Stage::kPrivacyPatches};
  candidate.security_fixes.push_back(Fix(Severity::kCritical, 1));

  Decision decision = Evaluate(candidate);
  Check(decision.action == Action::kBlockRelease, "an untested build is not a release");
  Check(decision.missing_stages.size() == 3, "every missing stage is named");
  Check(decision.reason.find("privacy regression") != std::string::npos,
        "the reason lists what has not run");
}

void TestPatchTouchingFixNeedsReview() {
  ReleaseCandidate candidate = FullPipeline();
  candidate.completed_stages = {Stage::kPrivacyPatches, Stage::kBrowserPatches,
                                Stage::kAutomatedTests, Stage::kPrivacyRegression};
  SecurityFix fix = Fix(Severity::kHigh, 2);
  fix.touches_bedrock_patch = true;
  candidate.security_fixes.push_back(fix);

  Decision decision = Evaluate(candidate);
  Check(decision.action == Action::kBlockRelease,
        "a fix in patched code cannot be a blind rebuild");
  Check(decision.reason.find("re-read") != std::string::npos,
        "the reason says a human has to read the patch");
}

void TestEmergencyIsNarrow() {
  ReleaseCandidate base;
  base.completed_stages = {Stage::kSecurityReview, Stage::kPrivacyRegression};
  base.security_fixes.push_back(Fix(Severity::kCritical, 80));
  base.emergency_declared = true;

  ReleaseCandidate no_reason = base;
  Check(Evaluate(no_reason).action == Action::kBlockRelease,
        "an emergency without a written reason is not an emergency");

  ReleaseCandidate with_feature = base;
  with_feature.emergency_justification = "exploited in the wild";
  with_feature.features.push_back({"tiny tweak", true});
  Check(Evaluate(with_feature).action == Action::kBlockRelease,
        "'emergency plus one small feature' is refused");

  ReleaseCandidate untested = base;
  untested.emergency_justification = "exploited in the wild";
  untested.completed_stages = {Stage::kSecurityReview};
  Decision untested_decision = Evaluate(untested);
  Check(untested_decision.action == Action::kBlockRelease,
        "an emergency still runs the privacy regression tests");

  ReleaseCandidate ok = base;
  ok.emergency_justification = "exploited in the wild";
  Decision decision = Evaluate(ok);
  Check(decision.action == Action::kEmergencyRelease, "a justified security-only build ships");
  Check(decision.reason.find("exploited in the wild") != std::string::npos,
        "the justification travels with the decision");

  ReleaseCandidate not_security = FullPipeline();
  not_security.emergency_declared = true;
  not_security.emergency_justification = "we would like to ship this";
  Check(Evaluate(not_security).action == Action::kBlockRelease,
        "there is no emergency channel for features");
}

void TestEmergencyNeverSkipsPrivacyRegression() {
  const std::vector<Stage> emergency = MandatoryStages(true);
  bool has_regression = false;
  bool has_review = false;
  for (Stage stage : emergency) {
    has_regression = has_regression || stage == Stage::kPrivacyRegression;
    has_review = has_review || stage == Stage::kSecurityReview;
  }
  Check(has_regression && has_review,
        "the two stages that protect users survive an emergency");
  Check(emergency.size() < MandatoryStages(false).size(),
        "an emergency does drop something, or the escape hatch is theatre");
}

void TestQuietRelease() {
  ReleaseCandidate candidate = FullPipeline();
  candidate.features.push_back({"reader mode", true});
  Decision decision = Evaluate(candidate);
  Check(decision.action == Action::kShip, "no security work, everything ready: ship");
  Check(decision.dropped_features.empty(), "nothing is dropped for no reason");
  Check(decision.hours_remaining == 0, "no pending fix means no deadline");
}

}  // namespace

int main() {
  TestDeadlinesAreOrdered();
  TestSecurityBeatsFeature();
  TestOverdue();
  TestTightestDeadlineWins();
  TestIncompletePipelineBlocks();
  TestPatchTouchingFixNeedsReview();
  TestEmergencyIsNarrow();
  TestEmergencyNeverSkipsPrivacyRegression();
  TestQuietRelease();

  if (failures == 0) {
    std::cout << "release_policy: all checks passed\n";
  }
  return failures == 0 ? 0 : 1;
}
