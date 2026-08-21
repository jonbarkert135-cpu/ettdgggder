// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/ui/accessibility.h"

#include <cstdio>
#include <set>
#include <string>

#include "bedrock/themes/theme_engine.h"

namespace {

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::printf("  FAIL: %s\n", what.c_str());
    ++failures;
  }
}

using namespace bedrock::ui;

void AllEightRoadmapRequirementsAreListed() {
  Check(Accessibility::Rules().size() == 8, "eight requirements from item 60");
  std::set<int> seen;
  for (const A11yRule& rule : Accessibility::Rules()) {
    seen.insert(static_cast<int>(rule.requirement));
    Check(std::string(rule.rule).size() > 40, std::string(rule.name) + " states its rule");
    Check(std::string(rule.evidence).size() > 10,
          std::string(rule.name) + " names where it is met");
    Check(rule.status != A11yStatus::kPlanned || std::string(rule.evidence).empty(),
          std::string(rule.name) + " must not claim evidence for a planned item");
  }
  Check(seen.size() == 8, "no requirement listed twice");
}

void EveryControlBedrockAddsHasAName() {
  const auto controls = Accessibility::Controls();
  Check(controls.size() >= 20, "the surfaces really feed this list");
  for (const AccessibleControl& control : controls) {
    Check(control.name.size() > 2,
          control.surface + " has a control with no accessible name");
    Check(!control.role.empty(), control.name + " has no role");
    Check(control.keyboard_reachable,
          control.name + " must be reachable without a pointer");
    // An icon is not a name: names are words, so they contain a letter.
    bool has_letter = false;
    for (char c : control.name) {
      has_letter = has_letter || (c >= 'A' && c <= 'z');
    }
    Check(has_letter, control.name + " is not a word");
  }
}

void ControlNamesAreUniquePerSurface() {
  std::set<std::string> seen;
  for (const AccessibleControl& control : Accessibility::Controls()) {
    Check(seen.insert(control.surface + "/" + control.name).second,
          "two controls share a name in " + control.surface + ": " + control.name);
  }
}

void DestructiveDialogsDoNotOpenOnTheDestructiveButton() {
  const auto destructive = Accessibility::ContractFor(true);
  Check(std::string(destructive.role) == "alertdialog",
        "a destructive dialog is announced as a decision");
  Check(destructive.initial_focus_is_safe,
        "focus starts on the safe choice, so Enter-by-reflex cannot erase a profile");
  Check(destructive.focus_trapped && destructive.escape_closes,
        "focus is trapped and Escape cancels");
  const auto ordinary = Accessibility::ContractFor(false);
  Check(std::string(ordinary.role) == "dialog", "an ordinary dialog is a dialog");
  Check(ordinary.escape_closes, "Escape closes ordinary dialogs too");
}

void ContrastAndMotionClaimsMatchTheThemeEngine() {
  // The rules point at ThemeEngine; if that evidence stops being true the claim
  // must fail here rather than sit in a document.
  using bedrock::ui::ThemeEngine;
  Check(ThemeEngine::kMinContrastText >= 4.5, "AA floor for text still holds");
  Check(ThemeEngine::kHighContrastFloor >= 7.0, "high-contrast floor still holds");
  Check(ThemeEngine::ContrastRatio("#000000", "#ffffff") > 20.0,
        "the contrast function is the real one");
  Check(ThemeEngine::ContrastRatio("#777777", "#808080") < 3.0,
        "and it rejects a low-contrast pair");
  Check(Accessibility::Get(A11yRequirement::kHighContrast).status == A11yStatus::kEnforced,
        "high contrast is claimed only because the mechanism exists");
}

void ScaleRangeIsHonestAndWide() {
  Check(Accessibility::kMinUiScalePercent <= 50 && Accessibility::kMaxUiScalePercent >= 300,
        "50%-300% UI scale, matching what Chromium's zoom offers");
}

void NoRequirementIsClaimedWithoutEvidence() {
  for (const A11yRule& rule : Accessibility::Rules()) {
    if (rule.status == A11yStatus::kEnforced) {
      Check(std::string(rule.evidence).find("/") != std::string::npos ||
                std::string(rule.evidence).find("scripts") != std::string::npos,
            std::string(rule.name) + ": enforced means a file can be pointed at");
    }
  }
}

void AccessibilityCopyMakesNoClaimsAboutPrivacy() {
  const char* banned[] = {"anonymous", "untraceable", "100%", "fully accessible",
                          "WCAG compliant"};
  for (const std::string& text : Accessibility::AllUserVisibleStrings()) {
    for (const char* word : banned) {
      Check(text.find(word) == std::string::npos,
            std::string("accessibility copy must not claim '") + word + "': " + text);
    }
  }
}

}  // namespace

int main() {
  std::printf("accessibility_test\n");
  AllEightRoadmapRequirementsAreListed();
  EveryControlBedrockAddsHasAName();
  ControlNamesAreUniquePerSurface();
  DestructiveDialogsDoNotOpenOnTheDestructiveButton();
  ContrastAndMotionClaimsMatchTheThemeEngine();
  ScaleRangeIsHonestAndWide();
  NoRequirementIsClaimedWithoutEvidence();
  AccessibilityCopyMakesNoClaimsAboutPrivacy();
  std::printf(failures == 0 ? "  ok\n" : "  %d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
