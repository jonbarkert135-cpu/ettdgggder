// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/privacy/fingerprinting/letterboxing.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

using bedrock::privacy::ComputeLetterbox;
using bedrock::privacy::FpLevel;
using bedrock::privacy::kMinContentAreaPercent;
using bedrock::privacy::kMinContentHeight;
using bedrock::privacy::kMinContentWidth;
using bedrock::privacy::Letterbox;
using bedrock::privacy::ReportedScreenSize;
using bedrock::privacy::Size;
using bedrock::privacy::ViewportChanges;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

const std::vector<FpLevel> kAllLevels = {
    FpLevel::kCompatibility, FpLevel::kBalanced, FpLevel::kStrict,
    FpLevel::kMaximum};

// The property the mechanism exists for: whatever the page is told, that is
// also the area it renders into. A mismatch is measurable from JavaScript in
// one line, so it is checked over a wide sweep rather than a few samples.
void RenderedEqualsReported() {
  for (FpLevel level : kAllLevels) {
    for (int width = 1; width <= 3840; width += 7) {
      for (int height = 1; height <= 2160; height += 13) {
        const Letterbox box = ComputeLetterbox({width, height}, level);
        if (box.content.width + box.margin_left + box.margin_right != width ||
            box.content.height + box.margin_top + box.margin_bottom != height) {
          Check(false, "content plus margins fills the window at " +
                           std::to_string(width) + "x" +
                           std::to_string(height));
          return;
        }
        if (box.content.width > width || box.content.height > height) {
          Check(false, "content never exceeds the window at " +
                           std::to_string(width) + "x" +
                           std::to_string(height));
          return;
        }
        if (!(ReportedScreenSize(box) == box.content)) {
          Check(false, "screen.* reports the content box, not the display");
          return;
        }
      }
    }
  }
  Check(true, "sweep");
}

void NoLetterboxAtCompatibilityLevel() {
  const Letterbox box = ComputeLetterbox({1366, 768}, FpLevel::kCompatibility);
  Check(!box.active() && box.content == Size{1366, 768},
        "level 0 hands the page the real window");
  Check(box.margin_area() == 0, "level 0 costs no pixels");
}

// The point of quantisation: different machines land in the same bucket.
void DifferentDisplaysShareABucket() {
  const Letterbox a = ComputeLetterbox({1366, 768}, FpLevel::kStrict);
  const Letterbox b = ComputeLetterbox({1440, 810}, FpLevel::kStrict);
  Check(a.content == Size{1300, 700} && b.content == Size{1400, 800},
        "level 2 rounds down to 100px steps");
  const Letterbox c = ComputeLetterbox({1366, 768}, FpLevel::kMaximum);
  const Letterbox d = ComputeLetterbox({1300, 760}, FpLevel::kMaximum);
  Check(c.content == d.content && c.content == Size{1200, 600},
        "level 3's 200px steps put 1366x768 and 1300x760 in one bucket");
}

// A varying or randomised margin split would itself be a signal.
void MarginsAreCentredAndStable() {
  const Letterbox box = ComputeLetterbox({1365, 769}, FpLevel::kStrict);
  Check(box.margin_left == 32 && box.margin_right == 33,
        "the odd horizontal pixel goes right, never left");
  Check(box.margin_top == 34 && box.margin_bottom == 35,
        "the odd vertical pixel goes bottom, never top");
  for (int i = 0; i < 50; ++i) {
    const Letterbox again = ComputeLetterbox({1365, 769}, FpLevel::kStrict);
    if (!(again.content == box.content) ||
        again.margin_left != box.margin_left) {
      Check(false, "repeated calls return the same box (never per call)");
      return;
    }
  }
  Check(true, "stable");
}

// requestFullscreen() must not become a way to read the display size: the
// geometry function has no fullscreen input at all, so a fullscreen window is
// letterboxed exactly like any other window of that size.
void FullscreenIsNotAnException() {
  const Letterbox box = ComputeLetterbox({1920, 1080}, FpLevel::kMaximum);
  Check(box.active() && box.content == Size{1800, 1000},
        "a 1920x1080 fullscreen window still reports a quantised box");
}

// A protection that makes the window unusable would be turned off by the user,
// which protects nobody.
void TinyWindowsAreLeftAlone() {
  const Letterbox box = ComputeLetterbox({320, 240}, FpLevel::kMaximum);
  Check(!box.active() && box.content == Size{320, 240},
        "a window that would lose half its pixels keeps its real size");
  const Letterbox narrow = ComputeLetterbox({190, 600}, FpLevel::kBalanced);
  Check(!narrow.active() && narrow.content == Size{190, 600},
        "a window narrower than the floor is left alone, not squeezed");
  // No window, at any level, may be letterboxed below either guard.
  for (FpLevel level : kAllLevels) {
    for (int width = 1; width <= 1200; width += 3) {
      for (int height = 1; height <= 900; height += 3) {
        const Letterbox any = ComputeLetterbox({width, height}, level);
        if (!any.active()) {
          continue;
        }
        const bool above_floor = any.content.width >= kMinContentWidth &&
                                 any.content.height >= kMinContentHeight;
        const bool above_share =
            any.content.width * any.content.height * 100 >=
            width * height * kMinContentAreaPercent;
        if (!above_floor || !above_share) {
          Check(false, "letterboxing respects both guards at " +
                           std::to_string(width) + "x" +
                           std::to_string(height));
          return;
        }
      }
    }
  }
  for (FpLevel level : kAllLevels) {
    const Letterbox degenerate = ComputeLetterbox({0, 0}, level);
    Check(degenerate.content == Size{0, 0} && !degenerate.active(),
          "a minimised window is reported unchanged, not invented");
  }
}

// Without this, a slow window drag streams hundreds of distinct sizes to the
// page and the quantisation buys nothing.
void ResizeOnlyReachesThePageAtBucketBoundaries() {
  Check(!ViewportChanges({1366, 768}, {1399, 799}, FpLevel::kStrict),
        "a drag inside one bucket is invisible to the page");
  Check(ViewportChanges({1399, 768}, {1400, 768}, FpLevel::kStrict),
        "crossing a bucket boundary does reach the page");
  Check(ViewportChanges({1366, 768}, {1367, 768}, FpLevel::kCompatibility),
        "at level 0 every pixel of a resize reaches the page");
}

// Higher level, never less protection: the box must not grow as the level does.
void LevelLadderIsMonotone() {
  const Size window{1600, 900};
  const Letterbox l1 = ComputeLetterbox(window, FpLevel::kBalanced);
  const Letterbox l2 = ComputeLetterbox(window, FpLevel::kStrict);
  const Letterbox l3 = ComputeLetterbox(window, FpLevel::kMaximum);
  Check(!l1.active() && l3.active(),
        "an exact multiple of the step costs nothing; a coarser step does");
  Check(l1.content.width >= l2.content.width &&
            l2.content.width >= l3.content.width &&
            l1.content.height >= l2.content.height &&
            l2.content.height >= l3.content.height,
        "a higher level never reports a larger viewport");
}

}  // namespace

int main() {
  RenderedEqualsReported();
  NoLetterboxAtCompatibilityLevel();
  DifferentDisplaysShareABucket();
  MarginsAreCentredAndStable();
  FullscreenIsNotAnException();
  TinyWindowsAreLeftAlone();
  ResizeOnlyReachesThePageAtBucketBoundaries();
  LevelLadderIsMonotone();
  if (failures == 0) {
    std::cout << "letterboxing: all checks passed\n";
  }
  return failures == 0 ? 0 : 1;
}
