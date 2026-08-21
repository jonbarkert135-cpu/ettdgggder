// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_SETTINGS_RESET_CONTROLS_H_
#define BEDROCK_SETTINGS_RESET_CONTROLS_H_

#include <string>
#include <vector>

#include "bedrock/profiles/new_identity.h"

// Reset and recovery (roadmap item 58).
//
// Five actions the user must be able to reach: reset privacy settings, reset
// browser settings, create a new profile, clear all local data, restore
// defaults. The machinery that erases things already exists — `NewIdentity`
// (items 20/22) — so this is the surface around it, and the surface is where
// the mistakes happen.
//
// Two rules:
//
//   1. **Every action states what it does NOT touch.** A user clicking "Reset
//      privacy settings" wants to know their bookmarks survive. Silence there
//      is why people avoid reset buttons and end up reinstalling.
//   2. **Irreversible actions cannot be triggered by one click.** Anything that
//      destroys data the user cannot get back requires an explicit
//      confirmation, and the confirmation names the profile so it cannot be
//      given for the wrong one.
//
// Recovery is the other half: before an irreversible action Bedrock offers an
// export (item 59) of everything that is about to disappear. That is a real
// offer — the exporter exists and produces documented formats.

namespace bedrock {
namespace settings {

enum class ResetAction {
  kResetPrivacySettings,
  kResetBrowserSettings,
  kCreateNewProfile,
  kClearAllLocalData,
  kRestoreDefaults,
  kMaxValue = kRestoreDefaults,
};

enum class Confirmation {
  kNone,          // nothing is lost; no dialog
  kConfirm,       // one dialog, listing what changes
  kTypeToConfirm, // the user types the profile name: data is gone for good
};

struct ResetSpec {
  ResetAction action;
  const char* title;        // exact button label
  const char* effect;       // what it does, in one sentence
  std::vector<const char*> changes;    // what this action changes or erases
  std::vector<const char*> untouched;  // what it explicitly leaves alone
  Confirmation confirmation;
  bool offers_backup;       // an export is offered before the action runs
  const char* caveat;       // what the action cannot undo, "" when nothing
};

class ResetControls {
 public:
  static const std::vector<ResetSpec>& All();
  static const ResetSpec& Get(ResetAction action);

  // The text of the confirmation dialog: effect, the two lists, the caveat and
  // the backup offer. Built here so no caller can ship a shorter version.
  static std::string ConfirmationText(ResetAction action,
                                      const std::string& profile_name);

  // True when the dialog must refuse to proceed until the user has typed the
  // profile name exactly.
  static bool RequiresTypedName(ResetAction action);

  // Which storage a "clear all local data" run actually touches. Reuses the
  // session machinery rather than a second list that could drift out of step.
  static std::vector<session::ClearTarget> ClearAllTargets();

  static std::vector<std::string> AllUserVisibleStrings();
};

}  // namespace settings
}  // namespace bedrock

#endif  // BEDROCK_SETTINGS_RESET_CONTROLS_H_
