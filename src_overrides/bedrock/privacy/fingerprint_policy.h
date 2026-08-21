// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_FINGERPRINT_POLICY_H_
#define BEDROCK_PRIVACY_FINGERPRINT_POLICY_H_

#include <cstdint>
#include <string>

// Anti-fingerprinting policy: which strategy applies to which Web API surface
// at which level, plus the deterministic value derivation used by every shim.
//
// Two rules the whole design obeys (docs/privacy/fingerprinting/README.md):
//
//   1. NORMALIZE FIRST. Making everyone look the same is worth more than making
//      everyone look different. Noise is used only where normalization is
//      impossible without breaking the feature outright (canvas, WebGL, audio).
//   2. NEVER RANDOM PER CALL. Per-call randomness is itself a fingerprint (a
//      surface that never repeats identifies the browser, and repeated sampling
//      averages the noise away). Every value comes from SurfaceSeed(), which is
//      a pure function of (session secret, site, surface).

namespace bedrock {
namespace privacy {

// Level 0..3, exactly as exposed in Settings. Higher = less unique, more broken
// sites. The UI must state the compatibility cost of 2 and 3.
enum class FpLevel {
  kCompatibility = 0,  // shims off; for sites that need it
  kBalanced = 1,       // default
  kStrict = 2,
  kMaximum = 3,        // Tor-like posture; openly flagged as site-breaking
};

// One protected Web API surface. Each has a doc under
// docs/privacy/fingerprinting/<id>.md, enforced by scripts/check_fp_docs.py.
enum class Surface {
  kCanvas,
  kWebgl,
  kAudio,
  kFonts,
  kClientHints,
  kScreen,
  kLanguage,
  kTimezone,
  kHardwareConcurrency,
  kDeviceMemory,
  kMediaDevices,
  kSensors,
  kWebrtc,
  kTimerResolution,
  kUserAgent,
  kPlugins,
  kSpeechVoices,
  kBattery,
  kGamepad,
  kStorageIsolation,
  kJsRestrictions,
  kMaxValue = kJsRestrictions,
};

enum class Strategy {
  kAllow,      // untouched
  kNormalize,  // replaced by a fixed, plausible, population-wide value
  kFarble,     // deterministic per-(session, site, surface) perturbation
  kBlock,      // API absent, empty, or rejected
};

// Policy matrix. Pure lookup, no state.
Strategy GetStrategy(Surface surface, FpLevel level);

// Stable id used in prefs, docs filenames and the privacy log.
const char* SurfaceId(Surface surface);

// Does this level break a meaningful number of sites for this surface? Drives
// the warning shown next to the control.
bool BreaksSites(Surface surface, FpLevel level);

// --- Deterministic derivation -----------------------------------------------
//
// session_secret: 64 random bits generated once per browsing session per
// profile (and once per tab-group in private windows), never persisted. Rotating
// it per session is the ONLY variation in the system; it is what defeats
// cross-session linking. Within a session the same site always sees the same
// values, which is what defeats "your noise is your fingerprint" detection.
//
// etld_plus_one: the top-level site, so an embedded iframe cannot re-sample the
// same surface under a different origin to average the perturbation away.
uint64_t SurfaceSeed(uint64_t session_secret,
                     const std::string& etld_plus_one,
                     Surface surface);

// Uniform double in [0,1) from a seed and a counter (pixel index, sample index).
// Pure: the same (seed, index) always yields the same value.
double SeededUnit(uint64_t seed, uint64_t index);

// --- Normalized values -------------------------------------------------------
// Fixed values reported at kBalanced and above. Chosen to be the most common
// value in the real population, not a distinctive one.

int NormalizedHardwareConcurrency(int actual, FpLevel level);
int NormalizedDeviceMemoryGb(int actual, FpLevel level);
const char* NormalizedLanguage(FpLevel level);   // "en-US" at >= kBalanced
const char* NormalizedTimezone(FpLevel level);   // "UTC" at >= kStrict

// Letterboxing: report (and, at kStrict+, render into) window dimensions rounded
// down to a step, so the reported size falls into a large bucket.
struct Size {
  int width = 0;
  int height = 0;
  bool operator==(const Size& other) const {
    return width == other.width && height == other.height;
  }
};
Size QuantizeWindowSize(Size actual, FpLevel level);

// Timer coarsening in microseconds; 0 = no coarsening.
int TimerResolutionUs(FpLevel level);

}  // namespace privacy
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_FINGERPRINT_POLICY_H_
