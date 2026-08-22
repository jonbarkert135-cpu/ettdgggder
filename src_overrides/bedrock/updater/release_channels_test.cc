// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/updater/release_channels.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace bedrock::update;  // NOLINT — test-local convenience

constexpr char kPin[] = "151.0.7922.173";

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL: " << what << "\n";
    ++failures;
  }
}

ReleaseDescription GoodStable() {
  ReleaseDescription release;
  release.channel = Channel::kStable;
  release.version = "1.0.0.1";
  release.chromium_version = kPin;
  release.notes_present = RequiredNoteFields();
  release.days_in_previous_channel = 14;
  release.blocking_issues = 0;
  release.pipeline_complete = true;
  release.signed_and_attested = true;
  return release;
}

void TestSixFieldsAlways() {
  Check(RequiredNoteFields().size() == 6, "item 71 lists six fields");
  for (Channel channel : {Channel::kNightly, Channel::kBeta, Channel::kStable}) {
    ReleaseDescription release = GoodStable();
    release.channel = channel;
    release.notes_present.pop_back();  // drop "known issues"
    Check(Evaluate(release, kPin) == ReleaseVerdict::kMissingNotes,
          std::string("incomplete notes block ") + ChannelName(channel));
    const std::vector<NoteField> missing = MissingNoteFields(release);
    Check(missing.size() == 1 && missing.front() == NoteField::kKnownIssues,
          "the missing field is named, not just counted");
  }
}

void TestKnownIssuesIsNotOptionalForNightly() {
  // The tempting exception: "it's a nightly, everyone knows it's rough."
  ReleaseDescription release = GoodStable();
  release.channel = Channel::kNightly;
  release.blocking_issues = 3;      // allowed on nightly
  release.pipeline_complete = false;  // also allowed on nightly
  Check(Evaluate(release, kPin) == ReleaseVerdict::kPublishable,
        "a nightly may carry open issues — that is what it is for");
  release.notes_present.pop_back();
  Check(Evaluate(release, kPin) == ReleaseVerdict::kMissingNotes,
        "but it still says what those issues are");
}

void TestUnsignedIsNeverARelease() {
  for (Channel channel : {Channel::kNightly, Channel::kBeta, Channel::kStable}) {
    ReleaseDescription release = GoodStable();
    release.channel = channel;
    release.signed_and_attested = false;
    Check(Evaluate(release, kPin) == ReleaseVerdict::kUnsigned,
          std::string("unsigned build is not publishable on ") + ChannelName(channel));
  }
}

void TestChromiumMustMatchThePin() {
  ReleaseDescription release = GoodStable();
  release.chromium_version = "150.0.7000.1";
  Check(Evaluate(release, kPin) == ReleaseVerdict::kChromiumMismatch,
        "a release states the Chromium it was built from, and it is the pinned one");
}

void TestSoakIsEnforced() {
  ReleaseDescription release = GoodStable();
  release.days_in_previous_channel = 13;
  Check(Evaluate(release, kPin) == ReleaseVerdict::kInsufficientSoak,
        "stable requires the full beta soak");
  release.days_in_previous_channel = 14;
  Check(Evaluate(release, kPin) == ReleaseVerdict::kPublishable, "14 days is enough");

  ReleaseDescription beta = GoodStable();
  beta.channel = Channel::kBeta;
  beta.days_in_previous_channel = 6;
  Check(Evaluate(beta, kPin) == ReleaseVerdict::kInsufficientSoak,
        "beta requires a week of nightly first");
}

void TestBlockingIssuesBeatTheCalendar() {
  ReleaseDescription release = GoodStable();
  release.blocking_issues = 1;
  Check(Evaluate(release, kPin) == ReleaseVerdict::kBlockingIssues,
        "an open blocker stops a stable release even when the soak is done");
}

void TestPipelineIsRequiredOffNightly() {
  ReleaseDescription release = GoodStable();
  release.pipeline_complete = false;
  Check(Evaluate(release, kPin) == ReleaseVerdict::kPipelineIncomplete,
        "beta and stable go through the pipeline");
}

void TestPromotionIsOneStepAtATime() {
  Check(IsPromotionAllowed(Channel::kNightly, Channel::kBeta), "nightly -> beta");
  Check(IsPromotionAllowed(Channel::kBeta, Channel::kStable), "beta -> stable");
  Check(!IsPromotionAllowed(Channel::kNightly, Channel::kStable),
        "nightly -> stable skips the only soak that exists");
  Check(!IsPromotionAllowed(Channel::kStable, Channel::kBeta), "no promotion backwards");
  Check(!IsPromotionAllowed(Channel::kBeta, Channel::kBeta), "not to itself");
}

void TestChannelProperties() {
  Check(PropertiesFor(Channel::kNightly).accepts_direct_landings,
        "changes land in nightly first");
  Check(!PropertiesFor(Channel::kBeta).accepts_direct_landings &&
            !PropertiesFor(Channel::kStable).accepts_direct_landings,
        "nothing lands directly in beta or stable");
  Check(PropertiesFor(Channel::kNightly).cadence_days == 1, "nightly is nightly");
  Check(PropertiesFor(Channel::kStable).version_suffix_prefix.empty(),
        "stable versions carry no channel suffix");
  Check(!PropertiesFor(Channel::kBeta).version_suffix_prefix.empty(),
        "pre-release versions are marked in the version string, not only on the page");
  Check(PropertiesFor(Channel::kNightly).soak_days <
            PropertiesFor(Channel::kBeta).soak_days,
        "the soak lengthens as the audience widens");
}

}  // namespace

int main() {
  TestSixFieldsAlways();
  TestKnownIssuesIsNotOptionalForNightly();
  TestUnsignedIsNeverARelease();
  TestChromiumMustMatchThePin();
  TestSoakIsEnforced();
  TestBlockingIssuesBeatTheCalendar();
  TestPipelineIsRequiredOffNightly();
  TestPromotionIsOneStepAtATime();
  TestChannelProperties();

  if (failures == 0) {
    std::cout << "release_channels: all checks passed\n";
  }
  return failures == 0 ? 0 : 1;
}
