// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/fingerprinting/letterboxing.h"

#include <algorithm>

namespace bedrock {
namespace privacy {

namespace {

// A window with a non-positive dimension is not renderable (minimised, or a
// size arriving before the first layout). Report it unchanged: inventing a
// content box for it would make the browser lie about a window that shows
// nothing.
bool Renderable(const Size& window) {
  return window.width > 0 && window.height > 0;
}

}  // namespace

Letterbox ComputeLetterbox(Size window, FpLevel level) {
  Letterbox box;
  box.window = window;
  box.content = window;
  if (!Renderable(window) || level == FpLevel::kCompatibility) {
    return box;
  }

  const Size quantized = QuantizeWindowSize(window, level);

  // Quantisation rounds down, so this holds today; clamping keeps the
  // invariant local rather than depending on that. A content box larger than
  // its window would overflow the shell and clip the page.
  Size content{std::min(quantized.width, window.width),
               std::min(quantized.height, window.height)};

  // Too small to letterbox usefully: give the user the whole window and let
  // the panel report the protection as inactive. Both guards matter — the
  // absolute one for narrow windows, the proportional one for windows just
  // above a step boundary, where rounding down would cost half the pixels.
  if (content.width < kMinContentWidth || content.height < kMinContentHeight) {
    return box;
  }
  const long long kept = static_cast<long long>(content.width) * content.height;
  const long long whole = static_cast<long long>(window.width) * window.height;
  if (kept * 100 < whole * kMinContentAreaPercent) {
    return box;
  }

  box.content = content;
  const int extra_width = window.width - content.width;
  const int extra_height = window.height - content.height;
  // Even split; the odd pixel is always right/bottom, never variable.
  box.margin_left = extra_width / 2;
  box.margin_right = extra_width - box.margin_left;
  box.margin_top = extra_height / 2;
  box.margin_bottom = extra_height - box.margin_top;
  return box;
}

Size ReportedScreenSize(const Letterbox& box) {
  return box.content;
}

bool ViewportChanges(Size before, Size after, FpLevel level) {
  return !(ComputeLetterbox(before, level).content ==
           ComputeLetterbox(after, level).content);
}

}  // namespace privacy
}  // namespace bedrock
