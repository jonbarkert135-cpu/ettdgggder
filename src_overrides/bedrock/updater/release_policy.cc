// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/updater/release_policy.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace bedrock {
namespace update {

namespace {

bool HasStage(const std::vector<Stage>& stages, Stage stage) {
  return std::find(stages.begin(), stages.end(), stage) != stages.end();
}

std::string Join(const std::vector<std::string>& items) {
  std::string out;
  for (size_t i = 0; i < items.size(); ++i) {
    if (i != 0) {
      out += ", ";
    }
    out += items[i];
  }
  return out;
}

}  // namespace

int DeadlineHours(Severity severity) {
  switch (severity) {
    // Public critical bugs are exploited within days; the overlay has no excuse
    // for a slower roll than a rebuild takes.
    case Severity::kCritical:
      return 72;
    case Severity::kHigh:
      return 168;  // one week
    case Severity::kMedium:
      return 336;  // two weeks, i.e. the next scheduled release
    case Severity::kLow:
      return 720;  // one month
  }
  return 720;
}

const char* StageName(Stage stage) {
  switch (stage) {
    case Stage::kSecurityReview:
      return "security review";
    case Stage::kPrivacyPatches:
      return "privacy patches";
    case Stage::kBrowserPatches:
      return "browser patches";
    case Stage::kAutomatedTests:
      return "automated tests";
    case Stage::kPrivacyRegression:
      return "privacy regression tests";
  }
  return "unknown stage";
}

const char* ActionName(Action action) {
  switch (action) {
    case Action::kShip:
      return "ship";
    case Action::kDropFeatures:
      return "drop features";
    case Action::kBlockRelease:
      return "block release";
    case Action::kEmergencyRelease:
      return "emergency release";
  }
  return "unknown action";
}

std::vector<Stage> MandatoryStages(bool emergency) {
  // An emergency drops the two stages that exist to protect *features*: the
  // full browser patch set and the full test matrix. It never drops the two
  // that protect users from the release itself — someone has to have read what
  // upstream changed, and the privacy behaviours must still hold, because a
  // Chromium roll is the single most common way a privacy patch silently stops
  // working.
  if (emergency) {
    return {Stage::kSecurityReview, Stage::kPrivacyRegression};
  }
  return {Stage::kSecurityReview, Stage::kPrivacyPatches, Stage::kBrowserPatches,
          Stage::kAutomatedTests, Stage::kPrivacyRegression};
}

Decision Evaluate(const ReleaseCandidate& candidate) {
  Decision decision;

  // 1. How much time is left, and does any fix need a human read of a patch?
  bool have_security = !candidate.security_fixes.empty();
  bool patch_conflict_risk = false;
  int tightest = 0;
  std::string tightest_id;
  for (const SecurityFix& fix : candidate.security_fixes) {
    int remaining = DeadlineHours(fix.severity) - fix.hours_since_public;
    if (tightest_id.empty() || remaining < tightest) {
      tightest = remaining;
      tightest_id = fix.upstream_id;
    }
    patch_conflict_risk = patch_conflict_risk || fix.touches_bedrock_patch;
  }
  decision.hours_remaining = have_security ? tightest : 0;
  decision.overdue = have_security && tightest < 0;

  // 2. Which mandatory stages have not run on this tree?
  const bool emergency = candidate.emergency_declared && have_security;
  for (Stage stage : MandatoryStages(emergency)) {
    if (!HasStage(candidate.completed_stages, stage)) {
      decision.missing_stages.push_back(stage);
    }
  }

  // A security fix that lands in code we patch is not a rebuild. Skipping the
  // read is how a fork silently re-opens a hole upstream just closed.
  if (patch_conflict_risk && !HasStage(candidate.completed_stages, Stage::kSecurityReview)) {
    decision.action = Action::kBlockRelease;
    decision.reason =
        "a security fix touches code Bedrock patches; the patch must be re-read against the "
        "new upstream code before this ships";
    return decision;
  }

  // 3. Unready features never hold a release; they leave the branch.
  for (const Feature& feature : candidate.features) {
    if (!feature.ready) {
      decision.dropped_features.push_back(feature.name);
    }
  }

  // 4. An emergency is only an emergency when it is written down and the branch
  // carries nothing but security work. "Emergency plus one small feature" is
  // the shape of every regrettable release.
  if (candidate.emergency_declared) {
    if (!have_security) {
      decision.action = Action::kBlockRelease;
      decision.reason = "emergency declared with no security fix on the branch";
      return decision;
    }
    if (candidate.emergency_justification.empty()) {
      decision.action = Action::kBlockRelease;
      decision.reason = "emergency declared without a written justification";
      return decision;
    }
    if (!candidate.features.empty()) {
      decision.action = Action::kBlockRelease;
      decision.reason =
          "an emergency release carries security fixes only; remove " +
          std::to_string(candidate.features.size()) + " feature(s) from the branch first";
      return decision;
    }
    if (!decision.missing_stages.empty()) {
      decision.action = Action::kBlockRelease;
      decision.reason =
          "even an emergency runs the security review and the privacy regression tests";
      return decision;
    }
    decision.action = Action::kEmergencyRelease;
    decision.reason = "security-only build, reduced pipeline, justified: " +
                      candidate.emergency_justification;
    return decision;
  }

  if (!decision.missing_stages.empty()) {
    std::vector<std::string> names;
    for (Stage stage : decision.missing_stages) {
      names.push_back(StageName(stage));
    }
    decision.action = Action::kBlockRelease;
    decision.reason = "pipeline incomplete: " + Join(names);
    return decision;
  }

  // 5. The ordinary case. With a deadline in play, features are cut rather than
  // waited for; with no security work pending, an unready feature simply does
  // not ride along.
  if (!decision.dropped_features.empty()) {
    decision.action = Action::kDropFeatures;
    decision.reason =
        have_security
            ? "security deadline in " + std::to_string(tightest) + "h (" + tightest_id +
                  "): unready features leave the branch, the release does not wait"
            : "unready features stay behind; the rest ships";
    return decision;
  }

  decision.action = Action::kShip;
  decision.reason = have_security
                        ? "security fixes and features ready, " +
                              std::to_string(tightest) + "h before the deadline"
                        : "pipeline complete, nothing pending";
  return decision;
}

}  // namespace update
}  // namespace bedrock
