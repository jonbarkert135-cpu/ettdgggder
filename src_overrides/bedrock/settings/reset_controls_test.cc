// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/settings/reset_controls.h"

#include <cstdio>
#include <set>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::printf("  FAIL: %s\n", what.c_str());
    ++failures;
  }
}

using namespace bedrock::settings;

void AllFiveActionsFromTheRoadmapExist() {
  const std::vector<ResetSpec>& specs = ResetControls::All();
  Check(specs.size() == 5, "five reset actions");
  std::set<std::string> titles;
  for (const ResetSpec& spec : specs) {
    titles.insert(spec.title);
  }
  for (const char* required : {"Reset privacy settings", "Reset browser settings",
                               "Create new profile", "Clear all local data",
                               "Restore defaults"}) {
    Check(titles.count(required) == 1, std::string("action present: ") + required);
  }
}

void EveryActionSaysWhatItLeavesAlone() {
  for (const ResetSpec& spec : ResetControls::All()) {
    Check(!spec.untouched.empty(),
          std::string(spec.title) + " must state what it does not touch");
    Check(!spec.changes.empty(), std::string(spec.title) + " must state what it changes");
    Check(std::string(spec.effect).size() > 20,
          std::string(spec.title) + " explains itself in a sentence");
  }
}

void DestructiveActionsAreHardToTriggerByAccident() {
  Check(ResetControls::RequiresTypedName(ResetAction::kClearAllLocalData),
        "clearing all data needs the profile name typed");
  Check(ResetControls::RequiresTypedName(ResetAction::kRestoreDefaults),
        "restoring defaults needs the profile name typed");
  Check(!ResetControls::RequiresTypedName(ResetAction::kCreateNewProfile),
        "creating a profile destroys nothing, so no ceremony");
  Check(ResetControls::Get(ResetAction::kCreateNewProfile).confirmation ==
            Confirmation::kNone,
        "a non-destructive action does not nag");
  for (const ResetSpec& spec : ResetControls::All()) {
    if (spec.confirmation == Confirmation::kTypeToConfirm) {
      Check(spec.offers_backup, std::string(spec.title) + " offers an export first");
    }
  }
}

void ConfirmationTextCarriesBothListsAndTheProfileName() {
  const std::string text =
      ResetControls::ConfirmationText(ResetAction::kClearAllLocalData, "Research");
  Check(text.find("This changes:") != std::string::npos, "dialog lists what changes");
  Check(text.find("This leaves alone:") != std::string::npos, "dialog lists what survives");
  Check(text.find("Bookmarks") != std::string::npos, "the reassuring part is in the dialog");
  Check(text.find("\"Research\"") != std::string::npos,
        "the typed confirmation names the profile, so it cannot be given for the wrong one");
  Check(text.find("export") != std::string::npos, "recovery is offered before destruction");

  const std::string easy =
      ResetControls::ConfirmationText(ResetAction::kCreateNewProfile, "Research");
  Check(easy.find("Type the profile name") == std::string::npos,
        "a harmless action asks for no typing");
}

void ClearAllUsesTheExistingSessionMachinery() {
  const auto targets = ResetControls::ClearAllTargets();
  Check(targets.size() >= 8, "clearing all data covers the session targets");
  bool has_cookies = false;
  bool has_network_state = false;
  for (bedrock::session::ClearTarget target : targets) {
    has_cookies = has_cookies || target == bedrock::session::ClearTarget::kCookies;
    has_network_state =
        has_network_state || target == bedrock::session::ClearTarget::kNetworkState;
  }
  Check(has_cookies && has_network_state,
        "the list comes from NewIdentity rather than being retyped here");
}

void NoResetPretendsToEraseTheInternet() {
  const char* banned[] = {"anonymous", "untraceable", "no trace", "completely",
                          "everything about you", "100%"};
  for (const std::string& text : ResetControls::AllUserVisibleStrings()) {
    for (const char* word : banned) {
      Check(text.find(word) == std::string::npos,
            std::string("reset copy must not overpromise ('") + word + "'): " + text);
    }
  }
  const ResetSpec& clear = ResetControls::Get(ResetAction::kClearAllLocalData);
  Check(std::string(clear.caveat).find("Sites keep their own logs") != std::string::npos,
        "the limit of a local wipe is stated where the user reads it");
}

}  // namespace

int main() {
  std::printf("reset_controls_test\n");
  AllFiveActionsFromTheRoadmapExist();
  EveryActionSaysWhatItLeavesAlone();
  DestructiveActionsAreHardToTriggerByAccident();
  ConfirmationTextCarriesBothListsAndTheProfileName();
  ClearAllUsesTheExistingSessionMachinery();
  NoResetPretendsToEraseTheInternet();
  std::printf(failures == 0 ? "  ok\n" : "  %d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
