// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_ONBOARDING_FIRST_RUN_H_
#define BEDROCK_ONBOARDING_FIRST_RUN_H_

#include <string>
#include <vector>

#include "bedrock/settings/defaults.h"
#include "bedrock/themes/theme_engine.h"

// First run (item 98) and what it must tell the truth about (items 93 and 99).
//
// Six steps, in order: welcome, privacy level, search engine, theme, import,
// finish. Three rules hold it together:
//
//   1. **Every step already has an answer.** The flow starts from the shipped
//      Balanced Privacy profile, so a user who presses through without reading
//      lands exactly where a user who never saw this screen would (item 84).
//      Skipping is a valid answer, not an unfinished state.
//   2. **The search step discloses, it does not reassure.** Choosing a provider
//      shows who receives the queries, whether suggestions send keystrokes, and
//      that the request goes straight from this browser to that provider. There
//      is no Bedrock search proxy to hide behind (item 93, non-negotiable 1).
//   3. **Onboarding states the limits of the product.** Item 99's five points
//      ship as text on the privacy step: protection is not invisibility.
//
// Pure logic, no Chromium types: the WebUI drives it and the host tests run it.

namespace bedrock {
namespace onboarding {

enum class Step {
  kWelcome,
  kPrivacyLevel,
  kSearchEngine,
  kTheme,
  kImport,
  kFinish,
};

// Where imported data may come from. kSkip is a first-class answer.
enum class ImportSource {
  kChrome,
  kFirefox,
  kEdge,
  kChromium,
  kHtmlFile,
  kSkip,
};

// What the search layer knows about one provider, passed in rather than copied
// here: bedrock_search_engines.json stays the single source of truth.
struct EngineFacts {
  std::string id;
  std::string name;
  bool has_suggest_endpoint = false;
  bool sends_referrer = false;
};

// The four lines item 93 requires, ready to render.
struct SearchDisclosure {
  std::string provider;
  std::string suggestions;     // "On — each keystroke is sent to <provider>" / "Off"
  std::string safe_browsing;   // what protects the user from a bad result
  std::string privacy;         // "Searches are sent directly to <provider>."
};

struct Choices {
  settings::PrivacyChoice privacy = settings::PrivacyChoice::kBalanced;
  std::string engine_id;
  ui::ThemeMode theme = ui::ThemeMode::kSystem;
  bool search_suggestions = false;
  ImportSource import_source = ImportSource::kSkip;
};

class FirstRun {
 public:
  // `offered` are the providers shown on the search step, in order; the first
  // is preselected. Empty is a programming error and leaves the engine unset.
  explicit FirstRun(const std::vector<EngineFacts>& offered);
  ~FirstRun();

  Step current() const { return current_; }
  bool done() const { return done_; }

  // Navigation. Back() on the first step and Next() past the last are no-ops
  // that return false, so a UI cannot walk out of the flow by accident.
  bool Next();
  bool Back();

  // Choices. Each returns false and changes nothing if the value is not on
  // offer — an unknown engine id must not silently become "no search engine".
  bool ChoosePrivacy(settings::PrivacyChoice level);
  bool ChooseEngine(const std::string& engine_id);
  void SetSuggestions(bool enabled);
  bool ChooseTheme(ui::ThemeMode mode);
  void ChooseImport(ImportSource source);

  const Choices& choices() const { return choices_; }

  // The providers this flow offers, in order — the page renders these rather
  // than a list of its own.
  const std::vector<EngineFacts>& offered() const { return offered_; }

  // Disclosure for the currently selected provider (item 93).
  SearchDisclosure Disclosure() const;
  static SearchDisclosure Disclose(const EngineFacts& engine, bool suggestions);

  // Item 99: the five things the user must understand, plus the heading.
  static const char* PrivacyHeadline();
  static const std::vector<const char*>& PrivacyNotes();

 private:
  const EngineFacts* Find(const std::string& id) const;

  std::vector<EngineFacts> offered_;
  Choices choices_;
  Step current_ = Step::kWelcome;
  bool done_ = false;
};

}  // namespace onboarding
}  // namespace bedrock

#endif  // BEDROCK_ONBOARDING_FIRST_RUN_H_
