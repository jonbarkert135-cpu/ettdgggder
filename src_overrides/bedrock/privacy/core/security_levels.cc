// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/core/security_levels.h"

#include <vector>

namespace bedrock {
namespace privacy {

const std::vector<LevelInfo>& SecurityLevels::All() {
  static const std::vector<LevelInfo> kLevels = {
      {SecurityLevel::kStandard, "STANDARD",
       "Blocks known trackers. Everything else behaves like an ordinary "
       "browser.",
       ""},
      {SecurityLevel::kBalanced, "BALANCED",
       "Blocks ads and trackers, partitions storage, blocks third-party "
       "cookies and reduces fingerprinting. The recommended setting.",
       "A small number of sites need an exception for embedded logins."},
      {SecurityLevel::kStrict, "STRICT",
       "Adds stronger fingerprint protection and HTTPS-only browsing. "
       "Third-party cookies are blocked and the rest, including the sites you "
       "sign in to, are erased when you close the site.",
       "You are signed out when you close a site, and embedded content, single "
       "sign-on and some payment flows will need exceptions."},
      {SecurityLevel::kMaximum, "MAXIMUM",
       "The strongest settings Bedrock has: maximum fingerprint protection, "
       "all third-party storage blocked, scripts blocked by default.",
       "Many sites will break until you allow scripts for them. This is a "
       "deliberate trade, not a bug."},
      {SecurityLevel::kCustom, "CUSTOM",
       "Your own combination of settings.", ""},
  };
  return kLevels;
}

const LevelInfo& SecurityLevels::Info(SecurityLevel level) {
  for (const LevelInfo& info : All()) {
    if (info.level == level)
      return info;
  }
  return All().back();
}

const char* SecurityLevels::Name(SecurityLevel level) {
  return Info(level).name;
}

Overrides SecurityLevels::Values(SecurityLevel level) {
  Overrides values;
  switch (level) {
    case SecurityLevel::kStandard:
      values[Control::kAds] = Value::kAllow;
      values[Control::kTrackers] = Value::kBlock;
      values[Control::kFingerprinting] = Value::kAllow;
      values[Control::kCookies] = Value::kReduce;
      values[Control::kScripts] = Value::kAllow;
      values[Control::kHttps] = Value::kReduce;
      values[Control::kReferrer] = Value::kReduce;
      break;
    case SecurityLevel::kBalanced:
      // Identical to the shipped defaults (item 11). If these two ever
      // disagree, the browser starts in a state its own settings page calls
      // Custom.
      values[Control::kAds] = Value::kBlock;
      values[Control::kTrackers] = Value::kBlock;
      values[Control::kFingerprinting] = Value::kReduce;
      values[Control::kCookies] = Value::kReduce;
      values[Control::kScripts] = Value::kAllow;
      values[Control::kHttps] = Value::kReduce;
      values[Control::kReferrer] = Value::kReduce;
      break;
    case SecurityLevel::kStrict:
      values[Control::kAds] = Value::kBlock;
      values[Control::kTrackers] = Value::kBlock;
      values[Control::kFingerprinting] = Value::kBlock;
      // Third-party cookies only. Blocking first-party cookies too would mean
      // no site can be signed in to, and a privacy level nobody can stay on
      // protects nobody; Isolation() below makes the cookies that remain
      // ephemeral instead.
      values[Control::kCookies] = Value::kReduce;
      values[Control::kScripts] = Value::kAllow;
      values[Control::kHttps] = Value::kBlock;
      values[Control::kReferrer] = Value::kBlock;
      break;
    case SecurityLevel::kMaximum:
      values[Control::kAds] = Value::kBlockStrict;
      values[Control::kTrackers] = Value::kBlock;
      values[Control::kFingerprinting] = Value::kBlockStrict;
      values[Control::kCookies] = Value::kBlockStrict;
      values[Control::kScripts] = Value::kBlock;
      values[Control::kHttps] = Value::kBlockStrict;
      values[Control::kReferrer] = Value::kBlockStrict;
      break;
    case SecurityLevel::kCustom:
      break;  // Custom is a description, not a thing you can apply
  }
  return values;
}

net::IsolationLevel SecurityLevels::Isolation(SecurityLevel level) {
  switch (level) {
    case SecurityLevel::kStandard:
    case SecurityLevel::kBalanced:
      // The shipped default, so it must equal StorageIsolation's own default.
      return net::IsolationLevel::kStandard;
    case SecurityLevel::kStrict:
    case SecurityLevel::kMaximum:
      return net::IsolationLevel::kEphemeralAll;
    case SecurityLevel::kCustom:
      return net::IsolationLevel::kStandard;
  }
  return net::IsolationLevel::kStandard;
}

void SecurityLevels::Apply(ProtectionController* controls,
                           net::StorageIsolation* storage,
                           SecurityLevel level) {
  if (level == SecurityLevel::kCustom)
    return;
  for (const auto& entry : Values(level))
    controls->Set(Scope::kGlobal, "", entry.first, entry.second);
  if (storage)
    storage->set_level(Isolation(level));
}

SecurityLevel SecurityLevels::Detect(const ProtectionController& controls) {
  for (SecurityLevel level :
       {SecurityLevel::kMaximum, SecurityLevel::kStrict,
        SecurityLevel::kBalanced, SecurityLevel::kStandard}) {
    bool matches = true;
    for (const auto& entry : Values(level)) {
      if (controls.Get(entry.first, "", "") != entry.second) {
        matches = false;
        break;
      }
    }
    if (matches)
      return level;
  }
  return SecurityLevel::kCustom;
}

const char* SecurityLevels::CompatibilityWarning(SecurityLevel level) {
  return Info(level).tradeoff;
}

const char* SecurityLevels::TorModeDescription() {
  // Deliberately describes a transport, and deliberately names a limit in the
  // same breath.
  return "Tor Mode routes this window's traffic through the Tor network and "
         "isolates circuits per site. It is a transport mode, not a "
         "protection level: your Bedrock settings still apply, and what you "
         "sign in to still identifies you.";
}

}  // namespace privacy
}  // namespace bedrock
