// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/fingerprinting/fingerprint_policy.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace bedrock {
namespace privacy {
namespace {

constexpr int kLevels = 4;

struct Row {
  Surface surface;
  const char* id;
  // Strategy per level: [Compatibility, Balanced, Strict, Maximum].
  Strategy strategy[kLevels];
  // Lowest level at which sites commonly break.
  FpLevel breaks_from;
};

// The matrix. Reading down a column tells you exactly what a level does; that
// readability is the point, and it is why this is a table and not code.
constexpr Row kMatrix[] = {
    {Surface::kCanvas, "canvas",
     {Strategy::kAllow, Strategy::kFarble, Strategy::kFarble, Strategy::kBlock},
     FpLevel::kMaximum},
    {Surface::kWebgl, "webgl",
     {Strategy::kAllow, Strategy::kFarble, Strategy::kFarble, Strategy::kBlock},
     FpLevel::kStrict},
    {Surface::kAudio, "audio",
     {Strategy::kAllow, Strategy::kFarble, Strategy::kFarble, Strategy::kBlock},
     FpLevel::kMaximum},
    {Surface::kFonts, "fonts",
     {Strategy::kAllow, Strategy::kNormalize, Strategy::kNormalize,
      Strategy::kNormalize},
     FpLevel::kStrict},
    {Surface::kClientHints, "client-hints",
     {Strategy::kAllow, Strategy::kNormalize, Strategy::kNormalize,
      Strategy::kBlock},
     FpLevel::kMaximum},
    {Surface::kScreen, "screen",
     {Strategy::kAllow, Strategy::kNormalize, Strategy::kNormalize,
      Strategy::kNormalize},
     FpLevel::kStrict},
    {Surface::kLanguage, "language",
     {Strategy::kAllow, Strategy::kNormalize, Strategy::kNormalize,
      Strategy::kNormalize},
     FpLevel::kBalanced},
    {Surface::kTimezone, "timezone",
     {Strategy::kAllow, Strategy::kAllow, Strategy::kNormalize,
      Strategy::kNormalize},
     FpLevel::kStrict},
    {Surface::kHardwareConcurrency, "hardware-concurrency",
     {Strategy::kAllow, Strategy::kNormalize, Strategy::kNormalize,
      Strategy::kNormalize},
     FpLevel::kMaximum},
    {Surface::kDeviceMemory, "device-memory",
     {Strategy::kAllow, Strategy::kNormalize, Strategy::kNormalize,
      Strategy::kNormalize},
     FpLevel::kMaximum},
    {Surface::kMediaDevices, "media-devices",
     {Strategy::kAllow, Strategy::kNormalize, Strategy::kNormalize,
      Strategy::kBlock},
     FpLevel::kMaximum},
    {Surface::kSensors, "sensors",
     {Strategy::kAllow, Strategy::kBlock, Strategy::kBlock, Strategy::kBlock},
     FpLevel::kBalanced},
    {Surface::kWebrtc, "webrtc",
     {Strategy::kAllow, Strategy::kNormalize, Strategy::kNormalize,
      Strategy::kBlock},
     FpLevel::kMaximum},
    {Surface::kTimerResolution, "timer-resolution",
     {Strategy::kAllow, Strategy::kNormalize, Strategy::kNormalize,
      Strategy::kNormalize},
     FpLevel::kMaximum},
    {Surface::kUserAgent, "user-agent",
     {Strategy::kAllow, Strategy::kNormalize, Strategy::kNormalize,
      Strategy::kNormalize},
     FpLevel::kMaximum},
    {Surface::kPlugins, "plugins",
     {Strategy::kAllow, Strategy::kNormalize, Strategy::kNormalize,
      Strategy::kBlock},
     FpLevel::kMaximum},
    {Surface::kSpeechVoices, "speech-voices",
     {Strategy::kAllow, Strategy::kNormalize, Strategy::kNormalize,
      Strategy::kBlock},
     FpLevel::kStrict},
    {Surface::kBattery, "battery",
     {Strategy::kAllow, Strategy::kBlock, Strategy::kBlock, Strategy::kBlock},
     FpLevel::kMaximum},
    {Surface::kGamepad, "gamepad",
     {Strategy::kAllow, Strategy::kNormalize, Strategy::kBlock,
      Strategy::kBlock},
     FpLevel::kStrict},
    {Surface::kStorageIsolation, "storage-isolation",
     {Strategy::kNormalize, Strategy::kNormalize, Strategy::kNormalize,
      Strategy::kBlock},
     FpLevel::kMaximum},
    {Surface::kJsRestrictions, "js-restrictions",
     {Strategy::kAllow, Strategy::kAllow, Strategy::kNormalize,
      Strategy::kBlock},
     FpLevel::kStrict},
};

static_assert(sizeof(kMatrix) / sizeof(kMatrix[0]) ==
                  static_cast<size_t>(Surface::kMaxValue) + 1,
              "every Surface needs a row in kMatrix");

const Row& RowFor(Surface surface) {
  // Rows are declared in enum order; assert rather than search.
  const Row& row = kMatrix[static_cast<size_t>(surface)];
  return row;
}

// splitmix64: small, fast, well-distributed, and — crucially — reproducible.
uint64_t Mix(uint64_t x) {
  x += 0x9e3779b97f4a7c15ULL;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  return x ^ (x >> 31);
}

}  // namespace

Strategy GetStrategy(Surface surface, FpLevel level) {
  return RowFor(surface).strategy[static_cast<int>(level)];
}

const char* SurfaceId(Surface surface) {
  return RowFor(surface).id;
}

bool BreaksSites(Surface surface, FpLevel level) {
  return static_cast<int>(level) >= static_cast<int>(RowFor(surface).breaks_from);
}

uint64_t SurfaceSeed(uint64_t session_secret,
                     const std::string& etld_plus_one,
                     Surface surface) {
  // FNV-1a over the site, then mixed with the secret and the surface so that
  // learning one surface's noise tells an attacker nothing about another.
  uint64_t hash = 0xcbf29ce484222325ULL;
  for (unsigned char c : etld_plus_one) {
    hash ^= c;
    hash *= 0x100000001b3ULL;
  }
  return Mix(Mix(session_secret ^ hash) + static_cast<uint64_t>(surface) + 1);
}

double SeededUnit(uint64_t seed, uint64_t index) {
  // 53 bits -> [0,1), the same construction as a standard double generator.
  return static_cast<double>(Mix(seed + index) >> 11) / 9007199254740992.0;
}

int NormalizedHardwareConcurrency(int actual, FpLevel level) {
  switch (level) {
    case FpLevel::kCompatibility:
      return actual;
    case FpLevel::kBalanced:
      // Report a common value, but never more than the machine has: claiming 8
      // cores on a 2-core device makes sites schedule work the device cannot do.
      return std::min(actual, 8);
    case FpLevel::kStrict:
      return std::min(actual, 4);
    case FpLevel::kMaximum:
      return 2;
  }
  return actual;
}

int NormalizedDeviceMemoryGb(int actual, FpLevel level) {
  if (level == FpLevel::kCompatibility) {
    return actual;
  }
  // The spec already quantises to powers of two and caps at 8; we pin the mode.
  return level == FpLevel::kMaximum ? 4 : std::min(actual, 8);
}

const char* NormalizedLanguage(FpLevel level) {
  return level == FpLevel::kCompatibility ? nullptr : "en-US";
}

const char* NormalizedTimezone(FpLevel level) {
  return static_cast<int>(level) >= static_cast<int>(FpLevel::kStrict) ? "UTC"
                                                                      : nullptr;
}

Size QuantizeWindowSize(Size actual, FpLevel level) {
  int step = 0;
  switch (level) {
    case FpLevel::kCompatibility:
      return actual;
    case FpLevel::kBalanced:
      step = 50;
      break;
    case FpLevel::kStrict:
      step = 100;
      break;
    case FpLevel::kMaximum:
      step = 200;
      break;
  }
  Size quantized{actual.width - actual.width % step,
                 actual.height - actual.height % step};
  // Never report 0: a tiny window must still land in the smallest bucket.
  quantized.width = std::max(quantized.width, std::min(actual.width, step));
  quantized.height = std::max(quantized.height, std::min(actual.height, step));
  return quantized;
}

int TimerResolutionUs(FpLevel level) {
  switch (level) {
    case FpLevel::kCompatibility:
      return 0;
    case FpLevel::kBalanced:
      return 100;
    case FpLevel::kStrict:
      return 1000;
    case FpLevel::kMaximum:
      return 100000;
  }
  return 0;
}

}  // namespace privacy
}  // namespace bedrock
