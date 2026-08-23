// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_THEMES_THEME_ENGINE_H_
#define BEDROCK_THEMES_THEME_ENGINE_H_

#include <map>
#include <string>
#include <vector>

// Theme engine (roadmap items 27, 28 and 29).
//
// Customisation is a headline feature, which is exactly why it needs a real
// engine instead of a settings page that writes CSS somewhere. Three rules
// shape this file:
//
//   1. **Every change applies live.** `ApplyKind` says whether a property is a
//      repaint or a relayout; nothing returns "restart required". A test walks
//      every property to keep it that way (item 29).
//   2. **A theme cannot make the browser unusable.** Values are clamped to
//      ranges, and colour choices are checked for contrast — a custom theme
//      where the text disappears is a bug the theme system allowed, not the
//      user's mistake (item 27: accessible).
//   3. **The taste rules are values, not opinions.** Maximum corner radius,
//      maximum blur, maximum animation duration: the limits from item 27 live
//      here as numbers, so "no decorative junk" is enforceable rather than
//      argued about in review.

namespace bedrock {
namespace ui {

enum class ThemeMode {
  kLight,
  kDark,
  kSystem,        // follows the OS
  kHighContrast,  // maximum legibility, forced contrast floors
  kCustom,
};

// Everything the user can change (item 28).
enum class Property {
  kAccentColor,
  kBackgroundColor,
  kSurfaceColor,    // panels and cards
  kTextColor,       // primary text
  kGrain,           // 0..1, film grain over large surfaces
  kShadowStrength,  // 0..1.5 multiplier on the three elevations
  kGlow,            // 0..1, accent bloom around the focused surface
  kTransitionMs,    // 0..kMaxAnimationMs
  kTabShape,        // 0 rectangular, 1 rounded, 2 pill
  kToolbarDensity,  // 0 compact, 1 default, 2 comfortable
  kSidebarVisible,
  kIconSize,        // px
  kCornerRadius,    // px
  kTransparency,    // 0..1, window background opacity
  kBlur,            // px behind translucent surfaces
  kAnimations,      // 0 off, 1 reduced, 2 full
  kFontScale,       // 0.8..1.6
  kSpacing,         // 0.8..1.4 multiplier on the 4px grid
  kCompactMode,
  kImmersiveMode,   // chrome hides until pointer/keyboard asks for it
  kMaxValue = kImmersiveMode,
};

// How a change reaches the screen. There is no kRestartRequired: if a property
// cannot be applied live it does not belong in this enum.
enum class ApplyKind {
  kRepaint,   // colours, opacity
  kRelayout,  // sizes, density, spacing
};

struct Range {
  double min = 0;
  double max = 1;
  double step = 1;
  double fallback = 0;
};

struct Warning {
  Property property;
  std::string message;
};

class ThemeEngine {
 public:
  ThemeEngine();
  ~ThemeEngine();

  // Item 27's limits, as numbers.
  static constexpr double kMaxCornerRadius = 16;      // px; no giant cards
  static constexpr double kMaxBlur = 12;              // px; no glass soup
  static constexpr int kMaxAnimationMs = 200;         // no heavy motion
  static constexpr double kMinOpacity = 0.80;         // stays a window
  static constexpr double kMinContrastText = 4.5;     // WCAG AA
  static constexpr double kMinContrastControls = 3.0; // WCAG AA, non-text
  static constexpr double kHighContrastFloor = 7.0;   // WCAG AAA

  void SetMode(ThemeMode mode);
  ThemeMode mode() const { return mode_; }

  // Numeric properties. Out-of-range values are clamped, never rejected
  // silently, and the clamp is reported.
  void Set(Property property, double value);
  double Get(Property property) const;

  // Colours are "#rrggbb".
  void SetColor(Property property, const std::string& hex);
  std::string GetColor(Property property) const;

  static Range RangeFor(Property property);
  static ApplyKind ApplyKindFor(Property property);
  static const char* PropertyName(Property property);

  // Everything wrong with the current theme, in the user's language. Empty
  // means the theme is usable.
  std::vector<Warning> Validate() const;

  // Contrast ratio between two "#rrggbb" colours, WCAG 2.x definition.
  static double ContrastRatio(const std::string& a, const std::string& b);

  // Import/export so themes can be shared as a small text file. Unknown keys
  // are ignored rather than fatal, so a theme from a newer version still
  // mostly works.
  std::string Export() const;
  bool Import(const std::string& text);

  // Properties whose value differs from the mode's baseline: what the live
  // update has to touch, and the shortest path to "reset this one thing".
  std::vector<Property> Customized() const;
  void Reset(Property property);
  void ResetAll();

 private:
  void ApplyModeBaseline();

  // Dark is the shipped default (docs/design/026-visual-language.md); "System"
  // is a choice the user makes in setup, not one made for them.
  ThemeMode mode_ = ThemeMode::kDark;
  std::map<Property, double> values_;
  std::map<Property, std::string> colors_;
  std::map<Property, double> baseline_;
  std::map<Property, std::string> baseline_colors_;
};

}  // namespace ui
}  // namespace bedrock

#endif  // BEDROCK_THEMES_THEME_ENGINE_H_
