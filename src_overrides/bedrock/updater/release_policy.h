// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_UPDATER_RELEASE_POLICY_H_
#define BEDROCK_UPDATER_RELEASE_POLICY_H_

#include <string>
#include <vector>

// Release policy: security update vs feature (roadmap items 66 and 69).
//
// Every Chromium derivative eventually faces the same choice on the same
// afternoon: an upstream security release is out, and the branch also carries a
// feature that is nearly ready. The projects that fall behind are not the ones
// that made the wrong call once; they are the ones that never wrote the rule
// down, so the call was re-argued every time and the answer drifted towards
// whatever was more interesting to work on.
//
// The rule, from item 69: **the security update always wins.** Not "usually",
// not "unless the feature is small". A browser is the program that executes
// hostile code all day; shipping a known-exploitable renderer to keep a feature
// on schedule is the one decision a privacy browser cannot survive.
//
// So the choice is expressed as code, with deadlines a person can be held to:
//
//   - A pending upstream security fix sets a deadline by severity. The deadline
//     starts when the fix is *public*, because that is when attackers get it —
//     not when we noticed, and not when the branch is convenient.
//   - Features never delay that deadline. If a feature is not ready, it is
//     dropped from the branch (`Action::kDropFeatures`); it is never a reason
//     to hold the release. Reverting a feature costs an afternoon, an
//     unpatched renderer costs users.
//   - A release candidate that skips the pipeline stages (privacy regression
//     tests, build verification) is not shippable even when it is urgent —
//     shipping a build nobody tested is a second incident on top of the first.
//     The escape hatch is `Action::kEmergencyRelease`, which is deliberately
//     narrow: security-only content, no feature commits, and a written
//     justification.
//
// This module makes decisions about a described release; it does not build or
// publish anything. Everything it needs is in the struct passed to it, so the
// same rules can be evaluated by CI, by a release script, or in a test.

namespace bedrock {
namespace update {

// Severity as published by upstream (Chromium uses these four words).
enum class Severity {
  kCritical,  // sandbox escape / remote code execution, exploited or trivial
  kHigh,      // memory safety in a renderer, UXSS, spoofing with real impact
  kMedium,
  kLow,
};

// How long Bedrock may take to ship a fix after it is public upstream.
// Deadlines are short by design: this is an overlay, not a fork with its own
// engine, so a Chromium security roll is mostly a rebuild, not a research task.
int DeadlineHours(Severity severity);

// A pipeline stage from docs/UPSTREAM_SYNC.md. A release candidate records
// which ones actually ran on the exact tree being shipped.
enum class Stage {
  kSecurityReview,     // what did upstream fix, does it touch our patches
  kPrivacyPatches,     // our privacy patches re-applied
  kBrowserPatches,     // the rest of the overlay re-applied
  kAutomatedTests,     // host tests + browser tests
  kPrivacyRegression,  // the privacy behaviours upstream keeps re-enabling
};

const char* StageName(Stage stage);

struct SecurityFix {
  std::string upstream_id;  // e.g. an upstream bug or CVE identifier
  Severity severity = Severity::kLow;
  int hours_since_public = 0;
  // True when the fix touches code Bedrock patches: the roll cannot be a blind
  // rebuild, someone has to re-read the patch against the new upstream code.
  bool touches_bedrock_patch = false;
};

struct Feature {
  std::string name;
  bool ready = false;  // merged, tested, documented — not "almost"
};

struct ReleaseCandidate {
  std::string chromium_version;
  std::vector<SecurityFix> security_fixes;
  std::vector<Feature> features;
  std::vector<Stage> completed_stages;
  // Set only by a human, with a reason, when the deadline has passed and the
  // normal pipeline cannot finish in time.
  bool emergency_declared = false;
  std::string emergency_justification;
};

enum class Action {
  kShip,              // everything ready, pipeline complete: normal release
  kDropFeatures,      // ship without the features that are not ready
  kBlockRelease,      // not shippable yet — the pipeline is incomplete
  kEmergencyRelease,  // security-only, reduced pipeline, justified in writing
};

const char* ActionName(Action action);

struct Decision {
  Action action = Action::kShip;
  std::string reason;
  // Features that must not ride along with this release.
  std::vector<std::string> dropped_features;
  // Stages that still have to run before anything is published.
  std::vector<Stage> missing_stages;
  // Hours left before the tightest deadline; negative means it has passed.
  int hours_remaining = 0;
  bool overdue = false;
};

// The whole policy, in one function so there is one place to read it.
Decision Evaluate(const ReleaseCandidate& candidate);

// Stages that must have run before *any* build reaches users, emergency or not.
// A build that skipped these is not a release, it is an untested binary.
std::vector<Stage> MandatoryStages(bool emergency);

}  // namespace update
}  // namespace bedrock

#endif  // BEDROCK_UPDATER_RELEASE_POLICY_H_
