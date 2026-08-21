// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/ui/theme_engine.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace bedrock {
namespace ui {
namespace {

bool IsColor(Property property) {
  return property == Property::kAccentColor ||
         property == Property::kBackgroundColor;
}

double Channel(const std::string& hex, size_t offset) {
  const std::string byte = hex.substr(offset, 2);
  return static_cast<double>(std::stoi(byte, nullptr, 16)) / 255.0;
}

double Linearize(double channel) {
  return channel <= 0.03928 ? channel / 12.92
                            : std::pow((channel + 0.055) / 1.055, 2.4);
}

double Luminance(const std::string& hex) {
  if (hex.size() != 7 || hex[0] != '#') {
    return -1;
  }
  return 0.2126 * Linearize(Channel(hex, 1)) +
         0.7152 * Linearize(Channel(hex, 3)) +
         0.0722 * Linearize(Channel(hex, 5));
}

}  // namespace

ThemeEngine::ThemeEngine() {
  ApplyModeBaseline();
}

ThemeEngine::~ThemeEngine() = default;

// static
Range ThemeEngine::RangeFor(Property property) {
  switch (property) {
    case Property::kTabShape:       return {0, 2, 1, 1};
    case Property::kToolbarDensity: return {0, 2, 1, 1};
    case Property::kSidebarVisible: return {0, 1, 1, 0};
    case Property::kIconSize:       return {14, 24, 1, 18};
    case Property::kCornerRadius:   return {0, kMaxCornerRadius, 1, 10};
    case Property::kTransparency:   return {kMinOpacity, 1.0, 0.01, 1.0};
    case Property::kBlur:           return {0, kMaxBlur, 1, 0};
    case Property::kAnimations:     return {0, 2, 1, 2};
    case Property::kFontScale:      return {0.8, 1.6, 0.05, 1.0};
    case Property::kSpacing:        return {0.8, 1.4, 0.05, 1.0};
    case Property::kCompactMode:    return {0, 1, 1, 0};
    case Property::kImmersiveMode:  return {0, 1, 1, 0};
    case Property::kAccentColor:
    case Property::kBackgroundColor: return {0, 0, 0, 0};
  }
  return {};
}

// static
ApplyKind ThemeEngine::ApplyKindFor(Property property) {
  switch (property) {
    case Property::kAccentColor:
    case Property::kBackgroundColor:
    case Property::kTransparency:
    case Property::kBlur:
    case Property::kAnimations:
      return ApplyKind::kRepaint;
    case Property::kTabShape:
    case Property::kToolbarDensity:
    case Property::kSidebarVisible:
    case Property::kIconSize:
    case Property::kCornerRadius:
    case Property::kFontScale:
    case Property::kSpacing:
    case Property::kCompactMode:
    case Property::kImmersiveMode:
      return ApplyKind::kRelayout;
  }
  return ApplyKind::kRepaint;
}

// static
const char* ThemeEngine::PropertyName(Property property) {
  switch (property) {
    case Property::kAccentColor:     return "accent-color";
    case Property::kBackgroundColor: return "background-color";
    case Property::kTabShape:        return "tab-shape";
    case Property::kToolbarDensity:  return "toolbar-density";
    case Property::kSidebarVisible:  return "sidebar-visible";
    case Property::kIconSize:        return "icon-size";
    case Property::kCornerRadius:    return "corner-radius";
    case Property::kTransparency:    return "transparency";
    case Property::kBlur:            return "blur";
    case Property::kAnimations:      return "animations";
    case Property::kFontScale:       return "font-scale";
    case Property::kSpacing:         return "spacing";
    case Property::kCompactMode:     return "compact-mode";
    case Property::kImmersiveMode:   return "immersive-mode";
  }
  return "";
}

void ThemeEngine::ApplyModeBaseline() {
  baseline_.clear();
  baseline_colors_.clear();
  for (int i = 0; i <= static_cast<int>(Property::kMaxValue); ++i) {
    const Property property = static_cast<Property>(i);
    if (!IsColor(property)) {
      baseline_[property] = RangeFor(property).fallback;
    }
  }
  switch (mode_) {
    case ThemeMode::kLight:
      baseline_colors_[Property::kAccentColor] = "#B4622A";
      baseline_colors_[Property::kBackgroundColor] = "#F7F6F4";
      break;
    case ThemeMode::kHighContrast:
      baseline_colors_[Property::kAccentColor] = "#FFD166";
      baseline_colors_[Property::kBackgroundColor] = "#000000";
      // Legibility beats decoration: no translucency, no blur, no motion.
      baseline_[Property::kTransparency] = 1.0;
      baseline_[Property::kBlur] = 0;
      baseline_[Property::kAnimations] = 0;
      break;
    case ThemeMode::kDark:
    case ThemeMode::kSystem:
    case ThemeMode::kCustom:
      baseline_colors_[Property::kAccentColor] = "#E08A4C";
      baseline_colors_[Property::kBackgroundColor] = "#16181A";
      break;
  }
}

void ThemeEngine::SetMode(ThemeMode mode) {
  mode_ = mode;
  ApplyModeBaseline();
  // Switching mode keeps the user's explicit choices; it changes the ground
  // they sit on. Wiping them would punish anyone who tries Light for a minute.
}

void ThemeEngine::Set(Property property, double value) {
  if (IsColor(property)) {
    return;
  }
  const Range range = RangeFor(property);
  values_[property] = std::min(range.max, std::max(range.min, value));
}

double ThemeEngine::Get(Property property) const {
  auto it = values_.find(property);
  if (it != values_.end()) {
    return it->second;
  }
  auto base = baseline_.find(property);
  return base == baseline_.end() ? RangeFor(property).fallback : base->second;
}

void ThemeEngine::SetColor(Property property, const std::string& hex) {
  if (!IsColor(property) || Luminance(hex) < 0) {
    return;  // malformed colour: keep the previous one rather than go blank
  }
  colors_[property] = hex;
}

std::string ThemeEngine::GetColor(Property property) const {
  auto it = colors_.find(property);
  if (it != colors_.end()) {
    return it->second;
  }
  auto base = baseline_colors_.find(property);
  return base == baseline_colors_.end() ? "#000000" : base->second;
}

// static
double ThemeEngine::ContrastRatio(const std::string& a, const std::string& b) {
  const double la = Luminance(a);
  const double lb = Luminance(b);
  if (la < 0 || lb < 0) {
    return 0;
  }
  const double lighter = std::max(la, lb);
  const double darker = std::min(la, lb);
  return (lighter + 0.05) / (darker + 0.05);
}

std::vector<Warning> ThemeEngine::Validate() const {
  std::vector<Warning> warnings;
  const std::string accent = GetColor(Property::kAccentColor);
  const std::string background = GetColor(Property::kBackgroundColor);
  const double floor_ratio = mode_ == ThemeMode::kHighContrast
                                 ? kHighContrastFloor
                                 : kMinContrastControls;
  const double ratio = ContrastRatio(accent, background);
  if (ratio < floor_ratio) {
    warnings.push_back(
        {Property::kAccentColor,
         "This accent colour is too close to the background to be visible "
         "(contrast " + std::to_string(ratio).substr(0, 4) + ":1, minimum " +
             std::to_string(floor_ratio).substr(0, 3) + ":1)."});
  }
  if (mode_ == ThemeMode::kHighContrast) {
    if (Get(Property::kTransparency) < 1.0) {
      warnings.push_back({Property::kTransparency,
                          "High contrast mode keeps windows fully opaque."});
    }
    if (Get(Property::kBlur) > 0) {
      warnings.push_back(
          {Property::kBlur, "High contrast mode turns blur off."});
    }
  }
  if (Get(Property::kFontScale) > 1.3 && Get(Property::kCompactMode) > 0) {
    warnings.push_back(
        {Property::kCompactMode,
         "Large text and compact mode together will clip labels."});
  }
  return warnings;
}

std::string ThemeEngine::Export() const {
  std::string text = "mode=" + std::to_string(static_cast<int>(mode_)) + "\n";
  for (int i = 0; i <= static_cast<int>(Property::kMaxValue); ++i) {
    const Property property = static_cast<Property>(i);
    text += PropertyName(property);
    text += "=";
    text += IsColor(property) ? GetColor(property)
                              : std::to_string(Get(property)).substr(0, 6);
    text += "\n";
  }
  return text;
}

bool ThemeEngine::Import(const std::string& text) {
  std::istringstream stream(text);
  std::string line;
  bool any = false;
  while (std::getline(stream, line)) {
    const size_t equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    const std::string key = line.substr(0, equals);
    const std::string value = line.substr(equals + 1);
    if (key == "mode") {
      try {
        const int mode = std::stoi(value);
        if (mode >= 0 && mode <= static_cast<int>(ThemeMode::kCustom)) {
          SetMode(static_cast<ThemeMode>(mode));
          any = true;
        }
      } catch (...) {
        // Ignore: a malformed line must not take the whole theme down.
      }
      continue;
    }
    for (int i = 0; i <= static_cast<int>(Property::kMaxValue); ++i) {
      const Property property = static_cast<Property>(i);
      if (key != PropertyName(property)) {
        continue;
      }
      if (IsColor(property)) {
        SetColor(property, value);
      } else {
        try {
          Set(property, std::stod(value));
        } catch (...) {
          break;
        }
      }
      any = true;
      break;
    }
  }
  return any;
}

std::vector<Property> ThemeEngine::Customized() const {
  std::vector<Property> customized;
  for (int i = 0; i <= static_cast<int>(Property::kMaxValue); ++i) {
    const Property property = static_cast<Property>(i);
    if (IsColor(property)) {
      auto it = colors_.find(property);
      if (it != colors_.end() && it->second != GetColor(property)) {
        customized.push_back(property);
      } else if (it != colors_.end() &&
                 baseline_colors_.at(property) != it->second) {
        customized.push_back(property);
      }
      continue;
    }
    auto it = values_.find(property);
    if (it != values_.end() && it->second != baseline_.at(property)) {
      customized.push_back(property);
    }
  }
  return customized;
}

void ThemeEngine::Reset(Property property) {
  values_.erase(property);
  colors_.erase(property);
}

void ThemeEngine::ResetAll() {
  values_.clear();
  colors_.clear();
}

}  // namespace ui
}  // namespace bedrock
