// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_CORE_PRIVACY_POLICY_H_
#define BEDROCK_PRIVACY_CORE_PRIVACY_POLICY_H_

#include <string>
#include <vector>

#include "bedrock/privacy/network/dns_settings.h"
#include "bedrock/privacy/network/https_policy.h"
#include "bedrock/privacy/storage/storage_isolation.h"
#include "bedrock/privacy/network/webrtc_policy.h"
#include "bedrock/privacy/fingerprinting/fingerprint_policy.h"
#include "bedrock/privacy/core/protection_controller.h"
#include "bedrock/session/browsing_mode.h"

// PrivacyPolicy — the single source of truth (roadmap item 25).
//
// Every privacy layer already exists as its own component. What was missing is
// the guarantee that they agree: without one resolver, the fingerprinting shim
// can be at Maximum while third-party storage is persistent, or a Tor window
// can inherit a per-site exception that turns protection off. Each subsystem
// would be individually correct and the browser would be wrong.
//
// So no subsystem reads settings directly any more. They ask for a Resolution
// for a given top-level site, and everything in it was decided in one place,
// in one pass, with the precedence rules written down once:
//
//   1. Mode floor      — Tor/Private impose minimums nothing may lower.
//   2. Site override   — the user's per-site choice (shields panel).
//   3. Domain override
//   4. Profile default
//
// `Conflicts()` re-checks the invariants on a finished Resolution. It should
// always return empty; the test drives every mode against every combination of
// site settings and asserts exactly that.

namespace bedrock {
namespace privacy {

enum class CookieMode { kAllow, kPartition, kBlockThirdParty, kBlockAll };
enum class ReferrerMode { kFull, kOriginOnly, kNone };
enum class ScriptMode { kAllow, kThirdPartyBlocked, kBlockAll };
enum class PermissionDefault { kAsk, kDenySilently };

// The resolved policy for one top-level site. Every layer named in roadmap
// item 25 appears exactly once.
struct Resolution {
  std::string top_level_site;
  session::BrowsingMode mode = session::BrowsingMode::kNormal;

  // TrackingProtection
  bool block_ads = true;
  bool block_trackers = true;
  bool behavioral_detection = true;
  // FingerprintProtection
  FpLevel fingerprint = FpLevel::kBalanced;
  bool fingerprint_locked = false;
  // CookiePolicy
  CookieMode cookies = CookieMode::kBlockThirdParty;
  // StoragePolicy
  net::IsolationLevel storage = net::IsolationLevel::kStandard;
  // NetworkPolicy
  net::DnsMode dns = net::DnsMode::kSystem;
  bool strip_tracking_params = true;
  bool send_privacy_signals = true;
  // WebRTCPolicy
  net::WebRtcMode webrtc = net::WebRtcMode::kPrivacy;
  // PermissionPolicy
  PermissionDefault permissions = PermissionDefault::kAsk;
  // ReferrerPolicy
  ReferrerMode referrer = ReferrerMode::kOriginOnly;
  // ScriptPolicy
  ScriptMode scripts = ScriptMode::kAllow;
  // SecureConnectionPolicy
  net::HttpsMode https = net::HttpsMode::kUpgrade;
};

class PrivacyPolicy {
 public:
  PrivacyPolicy(ProtectionController* controls,
                session::BrowsingModeController* modes,
                net::HttpsPolicy* https,
                net::DnsSettings* dns,
                net::WebRtcPolicy* webrtc,
                net::StorageIsolation* storage);
  ~PrivacyPolicy();

  // The only way any subsystem learns what to do.
  Resolution For(const std::string& host, const std::string& etld1) const;

  // Invariants that must hold for any resolution. Empty means consistent.
  static std::vector<std::string> Conflicts(const Resolution& resolution);

  // One-line summary per layer for the shields panel, so the UI is generated
  // from the resolution rather than from each subsystem's own idea of itself.
  static std::vector<std::string> Explain(const Resolution& resolution);

 private:
  ProtectionController* controls_;
  session::BrowsingModeController* modes_;
  net::HttpsPolicy* https_;
  net::DnsSettings* dns_;
  net::WebRtcPolicy* webrtc_;
  net::StorageIsolation* storage_;
};

}  // namespace privacy
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_CORE_PRIVACY_POLICY_H_
