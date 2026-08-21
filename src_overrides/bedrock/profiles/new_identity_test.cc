// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/profiles/new_identity.h"

#include <algorithm>
#include <iostream>
#include <string>

namespace {

using namespace bedrock::session;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

bool Has(const std::vector<ClearTarget>& targets, ClearTarget target) {
  return std::find(targets.begin(), targets.end(), target) != targets.end();
}

}  // namespace

int main() {
  BrowsingModeController modes;
  NewIdentity identity(&modes);

  // The plan is shown before anything happens, and it covers everything.
  {
    const ResetPlan plan = NewIdentity::PlanForNewIdentity();
    Check(plan.targets.size() ==
              static_cast<size_t>(ClearTarget::kMaxValue) + 1,
          "New Identity clears every target it knows about");
    Check(plan.preserved.size() ==
              static_cast<size_t>(Preserved::kMaxValue) + 1,
          "and lists everything it deliberately keeps");
    Check(plan.closes_tabs,
          "tabs close: a surviving tab keeps its state and its connections");
    Check(Has(plan.targets, ClearTarget::kFingerprintSeed),
          "a new session secret, so canvas/audio values do not match the old "
          "session");
    Check(Has(plan.targets, ClearTarget::kNetworkState) &&
              Has(plan.targets, ClearTarget::kMediaDeviceSalts),
          "network state and device salts are part of the reset");
  }

  // A private window closing uses the same machinery, minus what belongs to
  // other sessions, and never touches bookmarks or settings.
  {
    const ResetPlan plan = NewIdentity::PlanForPrivateWindowClose();
    Check(Has(plan.targets, ClearTarget::kCookies) &&
              Has(plan.targets, ClearTarget::kSiteStorage) &&
              Has(plan.targets, ClearTarget::kHttpCache) &&
              Has(plan.targets, ClearTarget::kServiceWorkers) &&
              Has(plan.targets, ClearTarget::kTemporaryPermissions) &&
              Has(plan.targets, ClearTarget::kFormData) &&
              Has(plan.targets, ClearTarget::kSessionHistory),
          "everything roadmap item 20 lists is cleared");
    Check(!Has(plan.targets, ClearTarget::kLocalHistoryEntries),
          "the normal window's history is not touched");
    Check(!Has(plan.targets, ClearTarget::kTorCircuits),
          "and neither are another session's circuits");
    for (Preserved preserved :
         {Preserved::kBookmarks, Preserved::kSettings, Preserved::kSavedPasswords,
          Preserved::kInstalledExtensions, Preserved::kDownloadedFiles}) {
      Check(std::find(plan.preserved.begin(), plan.preserved.end(),
                      preserved) != plan.preserved.end(),
            "bookmarks/settings/passwords/extensions/downloads are preserved");
    }
  }

  // Execution reports honestly, including what it could not do.
  {
    const ResetPlan plan = NewIdentity::PlanForNewIdentity();
    const uint64_t before = modes.epoch();
    ResetReport report = identity.Execute(plan);
    Check(report.complete(), "a supported reset reports complete");
    Check(report.cleared.size() == plan.targets.size(), "everything cleared");
    Check(report.new_epoch != before, "circuits rotated as part of the reset");

    report = identity.Execute(plan, {ClearTarget::kNetworkState});
    Check(!report.complete(), "an unsupported target makes the reset partial");
    Check(report.failed.size() == 1 &&
              report.failed[0] == ClearTarget::kNetworkState,
          "and it is named, not silently counted as cleared");
    Check(!Has(report.cleared, ClearTarget::kNetworkState),
          "a target that failed is not in the cleared list");
    Check(report.new_epoch != 0,
          "circuits still rotate even when storage clearing is partial");
  }

  // Every item is described in words, and the caveat refuses to overpromise.
  {
    for (int i = 0; i <= static_cast<int>(ClearTarget::kMaxValue); ++i) {
      Check(std::string(NewIdentity::Describe(static_cast<ClearTarget>(i)))
                .size() > 20,
            "clear target " + std::to_string(i) + " is described");
    }
    for (int i = 0; i <= static_cast<int>(Preserved::kMaxValue); ++i) {
      Check(std::string(NewIdentity::Describe(static_cast<Preserved>(i)))
                .size() > 10,
            "preserved item " + std::to_string(i) + " is described");
    }
    const std::string caveat = NewIdentity::Caveat();
    Check(caveat.find("cannot undo") != std::string::npos,
          "the caveat says what a reset cannot do");
    Check(caveat.find("signed in") != std::string::npos,
          "and names the obvious case");
  }

  if (failures == 0) {
    std::cout << "new_identity_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
