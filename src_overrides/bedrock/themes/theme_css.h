// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_THEMES_THEME_CSS_H_
#define BEDROCK_THEMES_THEME_CSS_H_

#include <string>

#include "bedrock/themes/theme_engine.h"

// The bridge from the theme engine to the pages (design items 26-30).
//
// `tokens.css` is the shipped default, generated from
// branding/design-tokens.json. This file produces the *override* block for a
// window: the user's customisation on top of it, plus the window mode.
//
// Two things it exists to guarantee:
//
//   1. **Customisation cannot break the browser.** Values come through
//      `ThemeEngine`, which clamps them, so a corner radius of 400 px or a
//      500 ms transition cannot arrive here. The block is generated, never
//      typed, so there is no path from a settings page to arbitrary CSS.
//   2. **A private or Tor window looks different without a costume.** The mode
//      shifts the surfaces a step and changes one label. No purple neon, no
//      hooded figure, no borrowed Tor branding — a window that looks like a
//      toy is a window people trust for the wrong reasons.

namespace bedrock {
namespace ui {

enum class WindowMode {
  kNormal,
  kPrivate,
  kTor,
};

// The identity of a window mode, as the chrome states it. One short label and
// one sentence, so the difference is readable rather than decorative.
struct ModeIdentity {
  const char* id;
  const char* label;     // "Private", "Tor"
  const char* sentence;  // what it does and does not do
};

ModeIdentity IdentityFor(WindowMode mode);

// A `:root{...}` block with only the variables that differ from tokens.css.
// An untouched theme in a normal window produces an empty block, so the
// default ships without a second copy of itself.
std::string ThemeCss(const ThemeEngine& theme, WindowMode mode);

}  // namespace ui
}  // namespace bedrock

#endif  // BEDROCK_THEMES_THEME_CSS_H_
