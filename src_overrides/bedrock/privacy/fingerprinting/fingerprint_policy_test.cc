// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.
// These tests encode the anti-linking properties, not just the code paths.

#include "bedrock/privacy/fingerprinting/fingerprint_policy.h"

#include <cstdint>
#include <iostream>
#include <set>
#include <string>

namespace {

using namespace bedrock::privacy;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

constexpr FpLevel kAllLevels[] = {FpLevel::kCompatibility, FpLevel::kBalanced,
                                  FpLevel::kStrict, FpLevel::kMaximum};

}  // namespace

int main() {
  // --- Policy matrix ---------------------------------------------------------

  // Level 0 must not touch anything a site can notice, except storage
  // isolation, which is a privacy guarantee rather than a fingerprint shim.
  for (int i = 0; i <= static_cast<int>(Surface::kMaxValue); ++i) {
    const auto surface = static_cast<Surface>(i);
    if (surface == Surface::kStorageIsolation) {
      continue;
    }
    Check(GetStrategy(surface, FpLevel::kCompatibility) == Strategy::kAllow,
          std::string("level 0 must allow ") + SurfaceId(surface));
  }

  // Protection is monotonic: no surface may get weaker as the level rises.
  for (int i = 0; i <= static_cast<int>(Surface::kMaxValue); ++i) {
    const auto surface = static_cast<Surface>(i);
    for (int l = 1; l < 4; ++l) {
      Check(static_cast<int>(GetStrategy(surface, kAllLevels[l])) >=
                static_cast<int>(GetStrategy(surface, kAllLevels[l - 1])),
            std::string("monotonic protection for ") + SurfaceId(surface));
    }
  }

  // Every surface has a unique, non-empty id (docs and prefs depend on it).
  std::set<std::string> ids;
  for (int i = 0; i <= static_cast<int>(Surface::kMaxValue); ++i) {
    const std::string id = SurfaceId(static_cast<Surface>(i));
    Check(!id.empty() && ids.insert(id).second, "unique surface id: " + id);
  }

  // The site-breaking flag must be honest: Maximum breaks things.
  Check(BreaksSites(Surface::kJsRestrictions, FpLevel::kMaximum),
        "maximum js restrictions must be flagged as site-breaking");
  Check(!BreaksSites(Surface::kCanvas, FpLevel::kBalanced),
        "balanced canvas farbling should not be flagged as site-breaking");

  // --- Determinism (roadmap item 10) ----------------------------------------

  // A session secret is 32 random bytes, not an integer (audit F4).
  const std::string secret(kSessionSecretSize, '\x5a');
  std::string other_secret(kSessionSecretSize, '\x5a');
  other_secret[31] = '\x5b';  // one bit of difference is enough

  // Same session + same site + same surface => identical, every single call.
  const std::string a = SurfaceKey(secret, "example.com", Surface::kCanvas);
  const std::string b = SurfaceKey(secret, "example.com", Surface::kCanvas);
  Check(a == b, "seed must be stable within a session");
  Check(a.size() == 32, "a surface key is 256 bits");
  for (uint64_t i = 0; i < 32; ++i) {
    Check(SeededUnit(a, i) == SeededUnit(b, i), "noise must be reproducible");
    const double v = SeededUnit(a, i);
    Check(v >= 0.0 && v < 1.0, "noise must be in [0,1)");
  }

  // Different site => different key (no cross-site linking of the noise).
  Check(SurfaceKey(secret, "other.example", Surface::kCanvas) != a,
        "different site must get a different key");
  // Different surface => independent key (leaking one must not leak another).
  Check(SurfaceKey(secret, "example.com", Surface::kWebgl) != a,
        "different surface must get a different key");
  // Different session => different key (no cross-session linking).
  Check(SurfaceKey(other_secret, "example.com", Surface::kCanvas) != a,
        "new session secret must change the key");

  // The session secret must not be reachable from a surface key. It cannot be
  // proven by a test, but the structural property that made F4 exploitable can:
  // the key does not contain the secret, and no site's key is a function of
  // another's that the page could walk.
  Check(a.find(secret) == std::string::npos,
        "a surface key does not carry the session secret");
  Check(SurfaceKey(secret, "example.com", Surface::kCanvas) !=
            SurfaceKey(secret, "example.com.", Surface::kCanvas),
        "site strings are not merged by the derivation");

  // Streams of different surfaces must not overlap: with the old seed+index
  // construction, one surface's stream continued into another's.
  {
    const std::string canvas = SurfaceKey(secret, "example.com", Surface::kCanvas);
    const std::string webgl = SurfaceKey(secret, "example.com", Surface::kWebgl);
    std::set<double> values;
    for (uint64_t i = 0; i < 200; ++i) {
      values.insert(SeededUnit(canvas, i));
      values.insert(SeededUnit(webgl, i));
    }
    Check(values.size() == 400, "400 samples from two surfaces are all distinct");
  }

  // Sanity: keys spread out rather than collapsing onto a few values.
  std::set<std::string> seeds;
  for (int i = 0; i < 500; ++i) {
    seeds.insert(SurfaceKey(secret, "site" + std::to_string(i) + ".example",
                            Surface::kCanvas));
  }
  Check(seeds.size() == 500, "keys must not collide across 500 sites");

  // The integer view is derived, not the key itself.
  Check(SeedValue(a) != SeedValue(SurfaceKey(secret, "other.example",
                                             Surface::kCanvas)),
        "the 64-bit view differs per site too");

  // --- Normalized values -----------------------------------------------------

  Check(NormalizedHardwareConcurrency(16, FpLevel::kBalanced) == 8,
        "balanced caps cores at 8");
  Check(NormalizedHardwareConcurrency(2, FpLevel::kBalanced) == 2,
        "never report more cores than the machine has");
  Check(NormalizedHardwareConcurrency(16, FpLevel::kMaximum) == 2,
        "maximum pins cores at 2");
  Check(NormalizedHardwareConcurrency(3, FpLevel::kCompatibility) == 3,
        "level 0 reports the truth");

  Check(NormalizedDeviceMemoryGb(32, FpLevel::kBalanced) == 8, "memory capped");
  Check(std::string(NormalizedLanguage(FpLevel::kBalanced)) == "en-US",
        "language normalized from level 1");
  Check(NormalizedLanguage(FpLevel::kCompatibility) == nullptr,
        "language untouched at level 0");
  Check(NormalizedTimezone(FpLevel::kBalanced) == nullptr,
        "timezone stays real at level 1 (too many sites break)");
  Check(std::string(NormalizedTimezone(FpLevel::kStrict)) == "UTC",
        "timezone normalized from level 2");

  Check((QuantizeWindowSize({1366, 768}, FpLevel::kStrict) == Size{1300, 700}),
        "letterboxing rounds down to 100px buckets");
  Check((QuantizeWindowSize({1366, 768}, FpLevel::kCompatibility) ==
         Size{1366, 768}),
        "no letterboxing at level 0");
  // A window smaller than one bucket must still report something usable.
  const Size tiny = QuantizeWindowSize({80, 40}, FpLevel::kStrict);
  Check(tiny.width > 0 && tiny.height > 0, "quantized size is never zero");

  Check(TimerResolutionUs(FpLevel::kCompatibility) == 0, "no coarsening at L0");
  Check(TimerResolutionUs(FpLevel::kMaximum) > TimerResolutionUs(FpLevel::kStrict),
        "coarsening increases with level");

  if (failures == 0) {
    std::cout << "fingerprint_policy_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
