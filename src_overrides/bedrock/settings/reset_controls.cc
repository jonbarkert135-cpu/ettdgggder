// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/settings/reset_controls.h"

#include <string>
#include <vector>

namespace bedrock {
namespace settings {

// static
const std::vector<ResetSpec>& ResetControls::All() {
  static const std::vector<ResetSpec> specs = {
      {ResetAction::kResetPrivacySettings,
       "Reset privacy settings",
       "Puts every privacy control back to its shipped default.",
       {"Privacy level, fingerprinting, cookie and HTTPS settings",
        "Per-site privacy exceptions you added",
        "Custom filter list subscriptions"},
       {"Bookmarks, history and passwords",
        "Open tabs",
        "Installed extensions",
        "Cookies and site data — use 'Clear all local data' for those"},
       Confirmation::kConfirm,
       true,
       "Sites you were signed into stay signed in: this changes settings, not stored data."},

      {ResetAction::kResetBrowserSettings,
       "Reset browser settings",
       "Puts every setting — privacy and everything else — back to its default.",
       {"All settings, including search engine, startup page and appearance",
        "Per-site permissions and site policies",
        "Advanced settings: custom DNS, proxy, user agent overrides"},
       {"Bookmarks, history, passwords and downloads",
        "Profiles other than this one",
        "Files you have downloaded"},
       Confirmation::kConfirm,
       true,
       "Extensions stay installed but their permissions are reset, so some will ask again."},

      {ResetAction::kCreateNewProfile,
       "Create new profile",
       "Starts a separate profile with its own storage, settings and history.",
       {"Nothing — a new profile is added alongside the current one"},
       {"Everything in your current profile"},
       Confirmation::kNone,
       false,
       "Profiles are separate on this computer. They do not hide you from a site you "
       "sign into from both."},

      {ResetAction::kClearAllLocalData,
       "Clear all local data",
       "Erases cookies, site storage, caches and history for this profile.",
       {"Cookies and site storage — you will be signed out everywhere",
        "Caches, service workers and network state",
        "Browsing and download history",
        "Per-session fingerprinting seed and media device ids"},
       {"Bookmarks",
        "Saved passwords",
        "Files already downloaded to disk",
        "Your settings"},
       Confirmation::kTypeToConfirm,
       true,
       "Bedrock can only erase what is on this computer. Sites keep their own logs, and "
       "your network provider keeps theirs."},

      {ResetAction::kRestoreDefaults,
       "Restore defaults",
       "Resets settings and clears local data: the state of a fresh install, with your "
       "bookmarks and passwords kept.",
       {"Everything under 'Reset browser settings'",
        "Everything under 'Clear all local data'"},
       {"Bookmarks and saved passwords",
        "Other profiles",
        "Files already downloaded to disk"},
       Confirmation::kTypeToConfirm,
       true,
       "This cannot be undone. Export first if you are unsure — the export is offered "
       "on the next screen."},
  };
  return specs;
}

// static
const ResetSpec& ResetControls::Get(ResetAction action) {
  for (const ResetSpec& spec : All()) {
    if (spec.action == action) {
      return spec;
    }
  }
  return All().front();
}

// static
bool ResetControls::RequiresTypedName(ResetAction action) {
  return Get(action).confirmation == Confirmation::kTypeToConfirm;
}

// static
std::string ResetControls::ConfirmationText(ResetAction action,
                                            const std::string& profile_name) {
  const ResetSpec& spec = Get(action);
  std::string text = std::string(spec.title) + "\n\n" + spec.effect + "\n\nThis changes:";
  for (const char* change : spec.changes) {
    text += std::string("\n  - ") + change;
  }
  text += "\n\nThis leaves alone:";
  for (const char* kept : spec.untouched) {
    text += std::string("\n  - ") + kept;
  }
  if (std::string(spec.caveat).size() > 0) {
    text += std::string("\n\n") + spec.caveat;
  }
  if (spec.offers_backup) {
    text += "\n\nYou can export your bookmarks, settings and rules first.";
  }
  if (spec.confirmation == Confirmation::kTypeToConfirm) {
    text += "\n\nType the profile name \"" + profile_name + "\" to continue.";
  }
  return text;
}

// static
std::vector<session::ClearTarget> ResetControls::ClearAllTargets() {
  // The private-window plan is exactly "everything this session touched", which
  // is what "clear all local data" means for a profile. One list, one place.
  return session::NewIdentity::PlanForPrivateWindowClose().targets;
}

// static
std::vector<std::string> ResetControls::AllUserVisibleStrings() {
  std::vector<std::string> out;
  for (const ResetSpec& spec : All()) {
    out.push_back(spec.title);
    out.push_back(spec.effect);
    out.push_back(spec.caveat);
    for (const char* change : spec.changes) {
      out.push_back(change);
    }
    for (const char* kept : spec.untouched) {
      out.push_back(kept);
    }
    out.push_back(ConfirmationText(spec.action, "Default"));
  }
  return out;
}

}  // namespace settings
}  // namespace bedrock
