// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/onboarding/first_run.h"

namespace bedrock {
namespace onboarding {
namespace {

const Step kOrder[] = {
    Step::kWelcome,   Step::kPrivacyLevel, Step::kSearchEngine,
    Step::kTheme,     Step::kImport,       Step::kFinish,
};
constexpr int kSteps = sizeof(kOrder) / sizeof(kOrder[0]);

int IndexOf(Step step) {
  for (int i = 0; i < kSteps; ++i) {
    if (kOrder[i] == step) return i;
  }
  return 0;
}

}  // namespace

FirstRun::FirstRun(const std::vector<EngineFacts>& offered) : offered_(offered) {
  if (!offered_.empty()) choices_.engine_id = offered_.front().id;
}

FirstRun::~FirstRun() = default;

bool FirstRun::Next() {
  int i = IndexOf(current_);
  if (i + 1 >= kSteps) {
    // Next() on the finish step ends the flow; a second call changes nothing.
    if (done_) return false;
    done_ = true;
    return true;
  }
  current_ = kOrder[i + 1];
  return true;
}

bool FirstRun::Back() {
  int i = IndexOf(current_);
  if (i == 0 || done_) return false;
  current_ = kOrder[i - 1];
  return true;
}

bool FirstRun::ChoosePrivacy(settings::PrivacyChoice level) {
  choices_.privacy = level;
  return true;
}

const EngineFacts* FirstRun::Find(const std::string& id) const {
  for (const EngineFacts& engine : offered_) {
    if (engine.id == id) return &engine;
  }
  return nullptr;
}

bool FirstRun::ChooseEngine(const std::string& engine_id) {
  if (!Find(engine_id)) return false;
  choices_.engine_id = engine_id;
  return true;
}

void FirstRun::SetSuggestions(bool enabled) {
  choices_.search_suggestions = enabled;
}

bool FirstRun::ChooseTheme(ui::ThemeMode mode) {
  // The first-run step offers light, dark and system only. High contrast and
  // custom exist in Settings; putting five options here would trade the one
  // decision the user can make quickly for a menu.
  if (mode != ui::ThemeMode::kLight && mode != ui::ThemeMode::kDark &&
      mode != ui::ThemeMode::kSystem) {
    return false;
  }
  choices_.theme = mode;
  return true;
}

void FirstRun::ChooseImport(ImportSource source) {
  choices_.import_source = source;
}

SearchDisclosure FirstRun::Disclose(const EngineFacts& engine, bool suggestions) {
  SearchDisclosure out;
  out.provider = engine.name;
  if (suggestions && engine.has_suggest_endpoint) {
    out.suggestions = "On - what you type is sent to " + engine.name +
                      " as you type, before you press Enter";
  } else if (suggestions) {
    out.suggestions = "Off - " + engine.name + " offers no suggestion endpoint";
  } else {
    out.suggestions = "Off - nothing is sent until you press Enter";
  }
  // Google Safe Browsing is compiled out (safe_browsing_mode=0, ADR 0001): it
  // would send URL data to Google, which non-negotiable 1 forbids. Saying so is
  // the honest version of a "Safe browsing: on" row.
  out.safe_browsing =
      "Off - Bedrock ships without Google Safe Browsing, which would send "
      "browsing data to Google. HTTPS enforcement and content blocking still "
      "apply.";
  out.privacy = "Searches are sent directly to " + engine.name +
                ". Bedrock runs no search proxy and no server of any kind" +
                (engine.sends_referrer
                     ? ", and this provider receives a referrer."
                     : ", and no referrer is sent.");
  return out;
}

SearchDisclosure FirstRun::Disclosure() const {
  const EngineFacts* engine = Find(choices_.engine_id);
  if (!engine) return SearchDisclosure();
  return Disclose(*engine, choices_.search_suggestions);
}

const char* FirstRun::PrivacyHeadline() {
  return "Privacy protection is not invisibility.";
}

const std::vector<const char*>& FirstRun::PrivacyNotes() {
  static const std::vector<const char*> kNotes = {
      "Websites still know what you give them: what you type, upload or agree "
      "to send.",
      "Signing in to an account identifies you to that service, whatever the "
      "browser does.",
      "Fingerprinting protection raises the cost of identifying you. No "
      "browser can guarantee it is impossible.",
      "Tor Mode and normal browsing answer different threats; Tor Mode is "
      "slower and some sites refuse it.",
      "Extensions can read the pages you open, so each one you add is a risk "
      "you take on.",
  };
  return kNotes;
}

}  // namespace onboarding
}  // namespace bedrock
