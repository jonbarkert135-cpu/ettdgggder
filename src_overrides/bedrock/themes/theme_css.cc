// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/themes/theme_css.h"

#include <cmath>
#include <vector>

namespace bedrock {
namespace ui {
namespace {

// No std::to_string for doubles: it prints six decimals and Chromium builds
// without exceptions, so a hand-rolled fixed formatter is both smaller and
// predictable.
std::string Number(double value, int decimals) {
  const bool negative = value < 0;
  double scaled = std::fabs(value);
  for (int i = 0; i < decimals; ++i)
    scaled *= 10;
  long long units = static_cast<long long>(scaled + 0.5);
  std::string digits = std::to_string(units);
  if (decimals > 0) {
    while (static_cast<int>(digits.size()) <= decimals)
      digits.insert(digits.begin(), '0');
    digits.insert(digits.end() - decimals, '.');
    while (digits.back() == '0')
      digits.pop_back();
    if (digits.back() == '.')
      digits.pop_back();
  }
  return negative ? "-" + digits : digits;
}

void Emit(std::string* out, const std::string& name, const std::string& value) {
  *out += "  --" + name + ": " + value + ";\n";
}

// A colour nudged towards black, for the private and Tor surfaces. Kept as a
// shift of the user's own palette rather than a fixed purple: their theme is
// still their theme in a private window.
std::string Darken(const std::string& hex, double amount) {
  if (hex.size() != 7 || hex[0] != '#')
    return hex;
  std::string out = "#";
  static const char* kHexDigits = "0123456789ABCDEF";
  for (int i = 0; i < 3; ++i) {
    int value = 0;
    for (int j = 0; j < 2; ++j) {
      const char c = hex[1 + i * 2 + j];
      const int digit = (c >= '0' && c <= '9')   ? c - '0'
                        : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                        : (c >= 'A' && c <= 'F') ? c - 'A' + 10
                                                 : 0;
      value = value * 16 + digit;
    }
    value = static_cast<int>(value * (1.0 - amount) + 0.5);
    if (value < 0)
      value = 0;
    out += kHexDigits[(value >> 4) & 0xF];
    out += kHexDigits[value & 0xF];
  }
  return out;
}

}  // namespace

ModeIdentity IdentityFor(WindowMode mode) {
  switch (mode) {
    case WindowMode::kPrivate:
      return {"private", "Private",
              "This window keeps no history, cookies or cache after you close "
              "it. Your network and the sites you sign in to still see you."};
    case WindowMode::kTor:
      return {"tor", "Tor",
              "This window routes through the Tor network, which hides your "
              "address from the sites you visit. It is slower, and some sites "
              "refuse it."};
    case WindowMode::kNormal:
      break;
  }
  return {"normal", "", ""};
}

std::string ThemeCss(const ThemeEngine& theme, WindowMode mode) {
  std::string body;

  const struct {
    Property property;
    const char* name;
  } kColors[] = {
      {Property::kBackgroundColor, "surface"},
      {Property::kSurfaceColor, "surface-raised"},
      {Property::kTextColor, "text"},
      {Property::kAccentColor, "accent"},
  };

  for (const auto& entry : kColors) {
    std::string value = theme.GetColor(entry.property);
    if (value.empty())
      continue;
    // The mode shift: surfaces a step darker, text and accent untouched, so
    // the window reads as a different state and not as a different product.
    if (mode != WindowMode::kNormal &&
        (entry.property == Property::kBackgroundColor ||
         entry.property == Property::kSurfaceColor)) {
      value = Darken(value, mode == WindowMode::kTor ? 0.45 : 0.30);
    }
    Emit(&body, entry.name, value);
  }

  const double radius = theme.Get(Property::kCornerRadius);
  Emit(&body, "radius-sm", Number(radius * 0.6, 0) + "px");
  Emit(&body, "radius-md", Number(radius, 0) + "px");
  Emit(&body, "radius-lg", Number(radius * 1.4, 0) + "px");

  const double blur = theme.Get(Property::kBlur);
  Emit(&body, "surface-blur", Number(blur, 0) + "px");
  Emit(&body, "grain", Number(theme.Get(Property::kGrain), 2));
  Emit(&body, "glow", Number(theme.Get(Property::kGlow), 2));
  Emit(&body, "shadow-strength", Number(theme.Get(Property::kShadowStrength), 2));
  Emit(&body, "window-opacity", Number(theme.Get(Property::kTransparency), 2));

  // Motion: "animations off" wins over any duration, because a user who turned
  // motion off did not ask for a faster animation.
  const double animations = theme.Get(Property::kAnimations);
  const double transition =
      animations == 0 ? 0
                      : (animations == 1 ? theme.Get(Property::kTransitionMs) / 2
                                         : theme.Get(Property::kTransitionMs));
  Emit(&body, "motion-standard", Number(transition, 0) + "ms ease");

  Emit(&body, "font-scale", Number(theme.Get(Property::kFontScale), 2));
  Emit(&body, "density-scale", Number(theme.Get(Property::kSpacing), 2));
  Emit(&body, "icon-size", Number(theme.Get(Property::kIconSize), 0) + "px");

  std::string out = ":root {\n";
  out += body;
  const ModeIdentity identity = IdentityFor(mode);
  out += "  --window-mode: \"";
  out += identity.id;
  out += "\";\n}\n";
  return out;
}

}  // namespace ui
}  // namespace bedrock
