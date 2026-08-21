// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/session/new_identity.h"

#include <algorithm>

namespace bedrock {
namespace session {
namespace {

std::vector<ClearTarget> AllTargets() {
  std::vector<ClearTarget> targets;
  for (int i = 0; i <= static_cast<int>(ClearTarget::kMaxValue); ++i) {
    targets.push_back(static_cast<ClearTarget>(i));
  }
  return targets;
}

std::vector<Preserved> AllPreserved() {
  std::vector<Preserved> preserved;
  for (int i = 0; i <= static_cast<int>(Preserved::kMaxValue); ++i) {
    preserved.push_back(static_cast<Preserved>(i));
  }
  return preserved;
}

}  // namespace

NewIdentity::NewIdentity(BrowsingModeController* modes) : modes_(modes) {}
NewIdentity::~NewIdentity() = default;

// static
ResetPlan NewIdentity::PlanForNewIdentity() {
  ResetPlan plan;
  plan.targets = AllTargets();
  plan.preserved = AllPreserved();
  plan.closes_tabs = true;  // a tab that survives keeps its JS state and its
                            // open connections, which defeats the reset
  return plan;
}

// static
ResetPlan NewIdentity::PlanForPrivateWindowClose() {
  ResetPlan plan;
  plan.targets = AllTargets();
  // Closing a private window must not disturb the normal window's history or
  // its Tor circuits; those belong to another session.
  plan.targets.erase(
      std::remove_if(plan.targets.begin(), plan.targets.end(),
                     [](ClearTarget target) {
                       return target == ClearTarget::kLocalHistoryEntries ||
                              target == ClearTarget::kTorCircuits;
                     }),
      plan.targets.end());
  plan.preserved = AllPreserved();
  plan.closes_tabs = true;
  return plan;
}

ResetReport NewIdentity::Execute(const ResetPlan& plan,
                                 const std::vector<ClearTarget>& unsupported) {
  ResetReport report;
  for (ClearTarget target : plan.targets) {
    const bool can_clear =
        std::find(unsupported.begin(), unsupported.end(), target) ==
        unsupported.end();
    (can_clear ? report.cleared : report.failed).push_back(target);
  }
  if (modes_) {
    // Fresh circuits for everything that comes next, whatever the storage
    // layer managed to clear.
    modes_->RotateCircuits();
    report.new_epoch = modes_->epoch();
  }
  return report;
}

// static
const char* NewIdentity::Describe(ClearTarget target) {
  switch (target) {
    case ClearTarget::kCookies:
      return "Cookies — you will be signed out of sites in this session.";
    case ClearTarget::kSiteStorage:
      return "Site storage — data websites saved in this browser.";
    case ClearTarget::kHttpCache:
      return "Cache — pages and images stored to load faster.";
    case ClearTarget::kServiceWorkers:
      return "Background workers registered by websites.";
    case ClearTarget::kTemporaryPermissions:
      return "Temporary permissions — camera, microphone, location and "
             "notification grants made in this session.";
    case ClearTarget::kFormData:
      return "Text typed into forms and search boxes in this session.";
    case ClearTarget::kSessionHistory:
      return "Open tabs and their back/forward history. Tabs will close.";
    case ClearTarget::kLocalHistoryEntries:
      return "History entries recorded during this session.";
    case ClearTarget::kNetworkState:
      return "Open connections, cached addresses and reused secure sessions.";
    case ClearTarget::kTorCircuits:
      return "Tor circuits — new paths through the network for what comes "
             "next.";
    case ClearTarget::kMediaDeviceSalts:
      return "Camera and microphone identifiers websites could recognise.";
    case ClearTarget::kFingerprintSeed:
      return "The values used to vary canvas, audio and WebGL readings, so "
             "sites cannot match this session to the last one.";
  }
  return "";
}

// static
const char* NewIdentity::Describe(Preserved preserved) {
  switch (preserved) {
    case Preserved::kBookmarks:
      return "Bookmarks are kept.";
    case Preserved::kSettings:
      return "Your settings are kept.";
    case Preserved::kSavedPasswords:
      return "Saved passwords are kept.";
    case Preserved::kInstalledExtensions:
      return "Installed extensions are kept.";
    case Preserved::kDownloadedFiles:
      return "Files you already downloaded stay on your disk.";
  }
  return "";
}

// static
const char* NewIdentity::Caveat() {
  return "This clears what this browser stored on this device and starts new "
         "network connections. It cannot undo what already happened: sites you "
         "signed in to still know it was you, and your network provider still "
         "saw the connections you made.";
}

// static
std::vector<std::string> NewIdentity::AllUserVisibleStrings() {
  std::vector<std::string> strings;
  for (ClearTarget target : AllTargets()) {
    strings.push_back(Describe(target));
  }
  for (Preserved preserved : AllPreserved()) {
    strings.push_back(Describe(preserved));
  }
  strings.push_back(Caveat());
  return strings;
}

}  // namespace session
}  // namespace bedrock
