// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/themes/theme_css.h"

#include <iostream>
#include <string>

namespace {

using namespace bedrock::ui;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

bool Has(const std::string& css, const std::string& needle) {
  return css.find(needle) != std::string::npos;
}

}  // namespace

int main() {
  ThemeEngine theme;

  // The shipped dark default, as CSS.
  const std::string normal = ThemeCss(theme, WindowMode::kNormal);
  Check(Has(normal, "--surface: #0B0C0D"), "the dark background is the default");
  Check(Has(normal, "--text: #F2F3F4"), "and its text colour");
  Check(Has(normal, "--window-mode: \"normal\""), "a normal window says so");

  // Customisation goes through the engine, so its limits are the CSS limits.
  theme.Set(Property::kCornerRadius, 400);
  theme.Set(Property::kBlur, 90);
  theme.Set(Property::kTransitionMs, 5000);
  const std::string clamped = ThemeCss(theme, WindowMode::kNormal);
  Check(Has(clamped, "--radius-md: 16px"), "a huge radius is clamped, not honoured");
  Check(Has(clamped, "--surface-blur: 12px"), "and so is blur");
  Check(Has(clamped, "--motion-standard: 200ms"), "and so is motion");
  Check(!Has(clamped, "5000"), "no unclamped value reaches the stylesheet");

  // Turning motion off beats any duration: nobody asks for a faster animation
  // by switching animations off.
  theme.Set(Property::kAnimations, 0);
  Check(Has(ThemeCss(theme, WindowMode::kNormal), "--motion-standard: 0ms"),
        "animations off means no motion");

  // The user's own colours survive customisation.
  ThemeEngine custom;
  custom.SetColor(Property::kAccentColor, "#5AA9E6");
  Check(Has(ThemeCss(custom, WindowMode::kNormal), "--accent: #5AA9E6"),
        "a chosen accent is used");

  // Private and Tor windows: a shift of the same palette, not a costume.
  const std::string priv = ThemeCss(custom, WindowMode::kPrivate);
  const std::string tor = ThemeCss(custom, WindowMode::kTor);
  Check(Has(priv, "--window-mode: \"private\""), "the mode is stated");
  Check(!Has(priv, "--surface: #0B0C0D"), "private surfaces are shifted");
  Check(Has(priv, "--accent: #5AA9E6") && Has(tor, "--accent: #5AA9E6"),
        "the accent is not repainted purple behind the user's back");
  Check(priv != tor, "Tor and private are not the same window");

  // The identity text is a claim we can defend, in both directions.
  const ModeIdentity identity = IdentityFor(WindowMode::kPrivate);
  Check(std::string(identity.label) == "Private", "labelled plainly");
  Check(std::string(identity.sentence).find("still see you") != std::string::npos,
        "and it says what private browsing does not do");
  Check(std::string(IdentityFor(WindowMode::kTor).sentence).find("slower") !=
            std::string::npos,
        "Tor mode states its cost as well as its benefit");
  Check(std::string(IdentityFor(WindowMode::kNormal).label).empty(),
        "a normal window needs no badge");

  if (failures == 0)
    std::cout << "theme_css: ok\n";
  return failures == 0 ? 0 : 1;
}
