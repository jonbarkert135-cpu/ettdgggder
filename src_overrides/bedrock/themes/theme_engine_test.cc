// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/themes/theme_engine.h"

#include <cmath>
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

}  // namespace

int main() {
  ThemeEngine theme;

  // Item 29: nothing needs a restart. The enum has no such value, and every
  // property is classified — so a future property must pick repaint or
  // relayout, not invent a third option.
  for (int i = 0; i <= static_cast<int>(Property::kMaxValue); ++i) {
    const Property property = static_cast<Property>(i);
    const ApplyKind kind = ThemeEngine::ApplyKindFor(property);
    Check(kind == ApplyKind::kRepaint || kind == ApplyKind::kRelayout,
          std::string(ThemeEngine::PropertyName(property)) +
              " applies live");
    Check(std::string(ThemeEngine::PropertyName(property)).size() > 3,
          "property " + std::to_string(i) + " has a name");
  }

  // Item 27's taste limits are numbers, and clamping enforces them.
  {
    theme.Set(Property::kCornerRadius, 64);
    Check(theme.Get(Property::kCornerRadius) == ThemeEngine::kMaxCornerRadius,
          "corner radius is clamped: no giant rounded cards");
    theme.Set(Property::kBlur, 40);
    Check(theme.Get(Property::kBlur) == ThemeEngine::kMaxBlur,
          "blur is clamped: no glass soup");
    theme.Set(Property::kTransparency, 0.1);
    Check(theme.Get(Property::kTransparency) == ThemeEngine::kMinOpacity,
          "the window cannot be made nearly invisible");
    theme.Set(Property::kFontScale, 9.0);
    Check(theme.Get(Property::kFontScale) == 1.6, "font scale is clamped");
    theme.Set(Property::kFontScale, 0.1);
    Check(theme.Get(Property::kFontScale) == 0.8, "in both directions");
    Check(ThemeEngine::kMaxAnimationMs <= 200,
          "animation budget stays short");
    theme.ResetAll();
  }

  // Contrast maths against known values.
  {
    Check(std::abs(ThemeEngine::ContrastRatio("#000000", "#ffffff") - 21.0) <
              0.01,
          "black on white is 21:1");
    Check(std::abs(ThemeEngine::ContrastRatio("#ffffff", "#ffffff") - 1.0) <
              0.01,
          "white on white is 1:1");
    Check(ThemeEngine::ContrastRatio("#777777", "#ffffff") > 4.4 &&
              ThemeEngine::ContrastRatio("#777777", "#ffffff") < 4.7,
          "mid grey on white is about 4.5:1");
    Check(ThemeEngine::ContrastRatio("not-a-colour", "#ffffff") == 0,
          "a malformed colour yields no ratio rather than a wrong one");
  }

  // A theme that hides the UI is refused as unusable.
  {
    theme.SetMode(ThemeMode::kCustom);
    Check(theme.Validate().empty(), "the default custom theme is fine");
    theme.SetColor(Property::kBackgroundColor, "#16181A");
    theme.SetColor(Property::kAccentColor, "#1A1C1E");  // nearly invisible
    const auto warnings = theme.Validate();
    Check(warnings.size() == 1 &&
              warnings[0].property == Property::kAccentColor,
          "an invisible accent is reported");
    Check(warnings[0].message.find("contrast") != std::string::npos,
          "and the message says why, with numbers");

    theme.SetColor(Property::kAccentColor, "#E08A4C");
    Check(theme.Validate().empty(), "a readable accent passes");

    theme.SetColor(Property::kAccentColor, "zzz");
    Check(theme.GetColor(Property::kAccentColor) == "#E08A4C",
          "a malformed colour is ignored, not applied as black");
  }

  // High contrast holds a stricter floor and switches decoration off.
  {
    ThemeEngine hc;
    hc.SetMode(ThemeMode::kHighContrast);
    Check(hc.Validate().empty(), "the high contrast baseline is valid");
    Check(hc.Get(Property::kAnimations) == 0 && hc.Get(Property::kBlur) == 0 &&
              hc.Get(Property::kTransparency) == 1.0,
          "no motion, no blur, no translucency in high contrast");
    hc.SetColor(Property::kAccentColor, "#8A6A3A");  // fine normally, not here
    const auto warnings = hc.Validate();
    Check(!warnings.empty(),
          "a colour that passes AA is still refused against the AAA floor");
    hc.Set(Property::kBlur, 8);
    Check(hc.Validate().size() >= 2, "turning blur back on is also reported");
  }

  // Combinations that clip text are caught even when each value is legal.
  {
    ThemeEngine combo;
    combo.Set(Property::kFontScale, 1.5);
    combo.Set(Property::kCompactMode, 1);
    Check(!combo.Validate().empty(),
          "large text plus compact mode is flagged");
  }

  // Switching mode keeps explicit choices, changes the ground under them.
  {
    ThemeEngine keep;
    keep.Set(Property::kIconSize, 22);
    keep.SetMode(ThemeMode::kLight);
    Check(keep.Get(Property::kIconSize) == 22,
          "an explicit choice survives a mode switch");
    Check(keep.GetColor(Property::kAccentColor) == "#B4622A",
          "but the mode's own baseline colour applies");
    keep.Reset(Property::kIconSize);
    Check(keep.Get(Property::kIconSize) == 18, "reset returns to the baseline");
  }

  // Export/import round-trip, and a broken line does not sink the theme.
  {
    ThemeEngine source;
    source.SetMode(ThemeMode::kLight);
    source.Set(Property::kCornerRadius, 4);
    source.Set(Property::kSpacing, 1.2);
    source.SetColor(Property::kAccentColor, "#2F6B4F");

    ThemeEngine restored;
    Check(restored.Import(source.Export()), "a theme file imports");
    Check(restored.mode() == ThemeMode::kLight, "mode round-trips");
    Check(restored.Get(Property::kCornerRadius) == 4, "numbers round-trip");
    Check(restored.GetColor(Property::kAccentColor) == "#2F6B4F",
          "colours round-trip");
    Check(std::abs(restored.Get(Property::kSpacing) - 1.2) < 0.01,
          "fractional values survive");

    ThemeEngine tolerant;
    Check(tolerant.Import("corner-radius=8\nnonsense\nfuture-key=1\n"),
          "unknown and malformed lines are skipped, the rest applies");
    Check(tolerant.Get(Property::kCornerRadius) == 8, "and the good line took");
    Check(!tolerant.Import("nothing here\n"),
          "an import with nothing usable reports false");
  }

  if (failures == 0) {
    std::cout << "theme_engine_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
