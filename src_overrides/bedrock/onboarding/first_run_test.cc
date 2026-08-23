// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh. Items 93, 98, 99.

#include "bedrock/onboarding/first_run.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

using bedrock::onboarding::EngineFacts;
using bedrock::onboarding::FirstRun;
using bedrock::onboarding::ImportSource;
using bedrock::onboarding::SearchDisclosure;
using bedrock::onboarding::Step;
using bedrock::settings::PrivacyChoice;
using bedrock::ui::ThemeMode;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cout << "FAIL: " << what << "\n";
    ++failures;
  }
}

std::vector<EngineFacts> Offered() {
  return {{"duckduckgo", "DuckDuckGo", true, false},
          {"google", "Google", true, false}};
}

bool Contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

void TheSixStepsRunInOrderAndEnd() {
  FirstRun run(Offered());
  const Step expected[] = {Step::kWelcome, Step::kPrivacyLevel,
                           Step::kSearchEngine, Step::kTheme, Step::kImport,
                           Step::kFinish};
  for (Step step : expected) {
    Check(run.current() == step, "step order");
    if (step != Step::kFinish) Check(run.Next(), "advancing works");
  }
  Check(!run.done(), "the flow is not done before the finish step is confirmed");
  Check(run.Next(), "confirming finish ends the flow");
  Check(run.done(), "done after the finish step");
  Check(!run.Next(), "there is nothing after done");
  Check(!run.Back(), "a finished flow does not reopen");
}

void BackWorksExceptOnTheFirstStep() {
  FirstRun run(Offered());
  Check(!run.Back(), "no step before welcome");
  run.Next();
  Check(run.Back(), "back from the privacy step");
  Check(run.current() == Step::kWelcome, "back lands on welcome");
}

void PressingThroughLandsOnTheShippedDefaults() {
  // Item 84: a user who reads nothing must end up where a user without an
  // onboarding screen would — Balanced Privacy, system theme, nothing imported.
  FirstRun run(Offered());
  while (!run.done()) run.Next();
  Check(run.choices().privacy == PrivacyChoice::kBalanced, "balanced privacy");
  Check(run.choices().theme == ThemeMode::kSystem, "system theme");
  Check(run.choices().import_source == ImportSource::kSkip, "nothing imported");
  Check(!run.choices().search_suggestions, "suggestions off by default");
  Check(run.choices().engine_id == "duckduckgo", "the first offered engine");
}

void AnUnofferedChoiceIsRefusedAndChangesNothing() {
  FirstRun run(Offered());
  Check(!run.ChooseEngine("yandex"), "an engine that is not offered is refused");
  Check(run.choices().engine_id == "duckduckgo", "the selection is unchanged");
  Check(!run.ChooseTheme(ThemeMode::kCustom), "custom theme is not a first-run option");
  Check(run.choices().theme == ThemeMode::kSystem, "the theme is unchanged");
  Check(run.ChooseEngine("google"), "an offered engine is accepted");
  Check(run.choices().engine_id == "google", "and applied");
}

void WithNoEnginesTheFlowStillTerminates() {
  FirstRun run({});
  Check(run.choices().engine_id.empty(), "no engine is preselected");
  Check(run.Disclosure().provider.empty(), "no disclosure without a provider");
  while (!run.done()) run.Next();
  Check(run.done(), "the flow ends rather than hanging on the search step");
}

void TheSearchStepDisclosesWhoReceivesTheQuery() {
  FirstRun run(Offered());
  run.ChooseEngine("google");
  SearchDisclosure off = run.Disclosure();
  Check(off.provider == "Google", "the provider is named");
  Check(Contains(off.privacy, "Searches are sent directly to Google"),
        "item 93's required sentence");
  Check(Contains(off.privacy, "no server of any kind"),
        "no intermediate search proxy is claimed or implied");
  Check(Contains(off.suggestions, "Off"), "suggestions are off by default");
  Check(!Contains(off.suggestions, "keystroke") ||
            Contains(off.suggestions, "Off"),
        "an off row does not describe sending keystrokes");
  run.SetSuggestions(true);
  SearchDisclosure on = run.Disclosure();
  Check(Contains(on.suggestions, "On") && Contains(on.suggestions, "Google"),
        "an on row says who receives the keystrokes");
  Check(Contains(off.safe_browsing, "Off"),
        "Safe Browsing is off and the row says so instead of implying cover");
}

void DisclosureFollowsTheEngineNotATableOfItsOwn() {
  EngineFacts referrer_engine{"example", "Example Search", false, true};
  SearchDisclosure d = FirstRun::Disclose(referrer_engine, true);
  Check(Contains(d.privacy, "receives a referrer"),
        "a provider that gets a referrer is not described as one that does not");
  Check(Contains(d.suggestions, "no suggestion endpoint"),
        "suggestions cannot be on for a provider that has no endpoint");
}

void OnboardingStatesTheLimitsOfTheProduct() {
  // Item 99. Five points, and none of them may be a promise.
  Check(std::string(FirstRun::PrivacyHeadline()).find("not invisibility") !=
            std::string::npos,
        "the headline is the honest one");
  const std::vector<const char*>& notes = FirstRun::PrivacyNotes();
  Check(notes.size() == 5, "five points");
  const char* required[] = {"know what you give them", "Signing in",
                            "Fingerprinting", "Tor Mode", "Extensions"};
  for (const char* needle : required) {
    bool found = false;
    for (const char* note : notes) found = found || Contains(note, needle);
    Check(found, std::string("item 99 covers: ") + needle);
  }
  for (const char* note : notes) {
    std::string text(note);
    Check(!Contains(text, "anonymous") && !Contains(text, "untraceable") &&
              !Contains(text, "guarantee that"),
          "no unprovable claim in: " + text);
  }
}

}  // namespace

int main() {
  TheSixStepsRunInOrderAndEnd();
  BackWorksExceptOnTheFirstStep();
  PressingThroughLandsOnTheShippedDefaults();
  AnUnofferedChoiceIsRefusedAndChangesNothing();
  WithNoEnginesTheFlowStillTerminates();
  TheSearchStepDisclosesWhoReceivesTheQuery();
  DisclosureFollowsTheEngineNotATableOfItsOwn();
  OnboardingStatesTheLimitsOfTheProduct();
  if (failures == 0) std::cout << "first_run: ok (6 steps, 5 privacy notes)\n";
  return failures == 0 ? 0 : 1;
}
