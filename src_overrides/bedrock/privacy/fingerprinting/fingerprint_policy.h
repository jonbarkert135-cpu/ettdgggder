// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_FINGERPRINTING_FINGERPRINT_POLICY_H_
#define BEDROCK_PRIVACY_FINGERPRINTING_FINGERPRINT_POLICY_H_

#include <cstddef>
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
//      averages the noise away). Every value comes from a surface key, which is
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
// session_secret: kSessionSecretSize random BYTES from the platform CSPRNG,
// generated once per browsing session per profile (and once per tab-group in
// private windows), never persisted. Rotating it per session is the ONLY
// variation in the system; it is what defeats cross-session linking. Within a
// session the same site always sees the same values, which is what defeats
// "your noise is your fingerprint" detection.
//
// etld_plus_one: the top-level site, so an embedded iframe cannot re-sample the
// same surface under a different origin to average the perturbation away.
//
// Derivation is KEYED AND ONE-WAY: HKDF-SHA256 per site, then HMAC-SHA256 per
// surface (bedrock/crypto). It used to be an unkeyed invertible mixer over a
// 64-bit secret, which meant a site that recovered its own seed from the noise
// could invert it, obtain the session secret, and link the user across every
// other site in the session — F4 in docs/security/AUDIT-2026-08-25.md. The
// properties this construction has and that one did not:
//
//   * one-way: a recovered surface key reveals nothing about the secret;
//   * per-site: two colluding sites cannot reach a shared value;
//   * separated: streams for different surfaces or sites never overlap;
//   * 256-bit: the secret is no longer brute-forceable at all.
constexpr size_t kSessionSecretSize = 32;

// Opaque 32-byte key for one (session, site, surface). Never leaves the browser
// and is never exposed to the page.
std::string SurfaceKey(const std::string& session_secret,
                       const std::string& etld_plus_one,
                       Surface surface);

// Uniform double in [0,1) from a surface key and a counter (pixel index, sample
// index). Pure: the same (key, index) always yields the same value.
double SeededUnit(const std::string& surface_key, uint64_t index);

// A 64-bit view of a surface key, for shims whose upstream API needs an integer
// (a hash bucket, a shuffle seed). Never use it as a key.
uint64_t SeedValue(const std::string& surface_key);

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

#endif  // BEDROCK_PRIVACY_FINGERPRINTING_FINGERPRINT_POLICY_H_
