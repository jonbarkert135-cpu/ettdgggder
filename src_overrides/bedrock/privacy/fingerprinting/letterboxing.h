// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_FINGERPRINTING_LETTERBOXING_H_
#define BEDROCK_PRIVACY_FINGERPRINTING_LETTERBOXING_H_

#include "bedrock/privacy/fingerprinting/fingerprint_policy.h"

// Letterboxing: the geometry half of the `screen` surface.
//
// `QuantizeWindowSize()` in fingerprint_policy decides *which* size a page may
// see. This component decides *what the window then looks like*: the page is
// rendered into a quantised content box, centred, with the leftover pixels
// painted as a neutral margin. Reported size and rendered size are therefore
// the same number — the point of the mechanism. Reporting a rounded size while
// rendering the real one is not letterboxing, it is a bug that a two-line
// `getBoundingClientRect()` measurement uncovers.
//
// Mechanism studied from the public Tor Browser design document and Firefox's
// RFP letterboxing notes (docs/research/TOR_BROWSER.md,
// docs/research/FIREFOX.md); no code from either was used
// (docs/THIRD_PARTY.md).
//
// Deliberate decisions, none of them oversights:
//
// * There is no fullscreen exception. `requestFullscreen()` is the cheapest
//   way to ask a window how big the display really is, so an exception would
//   hand back the entropy the whole surface exists to remove.
// * Margins are split evenly and the odd pixel always goes right/bottom. A
//   varying or randomised split would be a per-window signal — exactly the
//   "your noise is your fingerprint" failure rule 2 of the policy forbids.
// * The box never depends on the site: the function has no site parameter, so
//   two tabs of the same window cannot be told apart by their viewport, and a
//   frame cannot re-sample it under another origin.
// * Two guards stop the mechanism from eating the window: an absolute floor
//   (`kMinContent`) and a proportional one (`kMinContentAreaPercent`). A
//   320x240 window quantised at level 3 would keep 200x200 — 52% of the
//   pixels — and the entropy saved at that size is negligible, because few
//   windows are that small in the first place. Both guards leave the window
//   alone, `Letterbox::active()` is false, and the privacy panel says the
//   protection is not applied instead of claiming one that is not.

namespace bedrock {
namespace privacy {

// Smallest content box worth letterboxing into (CSS px). Below this the real
// window size is used.
constexpr int kMinContentWidth = 200;
constexpr int kMinContentHeight = 100;

// Smallest share of the window's pixels the page may be left with. Chosen so
// that no ordinary desktop window loses more than the top level's step in each
// direction, while pathologically small windows fall through to their real
// size.
constexpr int kMinContentAreaPercent = 60;

struct Letterbox {
  Size window;   // the real content area of the window, unchanged
  Size content;  // where the page renders AND what it is allowed to measure
  int margin_left = 0;
  int margin_top = 0;
  int margin_right = 0;
  int margin_bottom = 0;

  // True when pixels are actually being given up. False means the page sees
  // the real window — at level 0, on an exact multiple of the step, or below
  // kMinContent.
  bool active() const { return !(content == window); }

  // Pixels the user paid for the protection, for the privacy panel.
  int margin_area() const {
    return window.width * window.height - content.width * content.height;
  }
};

// The single geometry source for the `screen` surface: quantise, then centre.
// Pure function of (window, level).
Letterbox ComputeLetterbox(Size window, FpLevel level);

// What `screen.width/height/availWidth/availHeight` report. The content box,
// never the physical display: a page that can read the display size does not
// care that the window was letterboxed.
Size ReportedScreenSize(const Letterbox& box);

// True if dragging the window from `before` to `after` changes what the page
// can observe. Used to skip the relayout — and, more importantly, to keep a
// resize from becoming a high-resolution stream of window sizes.
bool ViewportChanges(Size before, Size after, FpLevel level);

}  // namespace privacy
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_FINGERPRINTING_LETTERBOXING_H_
