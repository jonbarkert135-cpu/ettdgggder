// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/core/privacy_policy.h"

#include <string>
#include <vector>

namespace bedrock {
namespace privacy {
namespace {

using session::BrowsingMode;

CookieMode CookiesFromValue(Value value) {
  switch (value) {
    case Value::kAllow:       return CookieMode::kAllow;
    case Value::kReduce:      return CookieMode::kBlockThirdParty;
    case Value::kBlock:       return CookieMode::kBlockAll;
    case Value::kBlockStrict: return CookieMode::kBlockAll;
    case Value::kInherit:     return CookieMode::kBlockThirdParty;
  }
  return CookieMode::kBlockThirdParty;
}

ScriptMode ScriptsFromValue(Value value) {
  switch (value) {
    case Value::kAllow:  return ScriptMode::kAllow;
    case Value::kReduce: return ScriptMode::kThirdPartyBlocked;
    case Value::kBlock:
    case Value::kBlockStrict: return ScriptMode::kBlockAll;
    case Value::kInherit: return ScriptMode::kAllow;
  }
  return ScriptMode::kAllow;
}

ReferrerMode ReferrerFromValue(Value value) {
  switch (value) {
    case Value::kAllow:  return ReferrerMode::kFull;
    case Value::kReduce: return ReferrerMode::kOriginOnly;
    case Value::kBlock:
    case Value::kBlockStrict: return ReferrerMode::kNone;
    case Value::kInherit: return ReferrerMode::kOriginOnly;
  }
  return ReferrerMode::kOriginOnly;
}

}  // namespace

PrivacyPolicy::PrivacyPolicy(ProtectionController* controls,
                             session::BrowsingModeController* modes,
                             net::HttpsPolicy* https,
                             net::DnsSettings* dns,
                             net::WebRtcPolicy* webrtc,
                             net::StorageIsolation* storage)
    : controls_(controls),
      modes_(modes),
      https_(https),
      dns_(dns),
      webrtc_(webrtc),
      storage_(storage) {}

PrivacyPolicy::~PrivacyPolicy() = default;

Resolution PrivacyPolicy::For(const std::string& host,
                              const std::string& etld1) const {
  Resolution resolution;
  resolution.top_level_site = etld1;
  resolution.mode = modes_->mode();

  // --- profile defaults, from the components that own them ---
  resolution.storage = storage_->level();
  resolution.dns = dns_->mode();
  resolution.webrtc = webrtc_->mode();
  resolution.https = https_->mode();

  // --- per-site / per-domain overrides ---
  resolution.block_ads =
      controls_->Get(Control::kAds, host, etld1) != Value::kAllow;
  resolution.block_trackers =
      controls_->Get(Control::kTrackers, host, etld1) != Value::kAllow;
  resolution.behavioral_detection = resolution.block_trackers;
  resolution.fingerprint = ProtectionController::ToFpLevel(
      controls_->Get(Control::kFingerprinting, host, etld1));
  resolution.cookies =
      CookiesFromValue(controls_->Get(Control::kCookies, host, etld1));
  resolution.scripts =
      ScriptsFromValue(controls_->Get(Control::kScripts, host, etld1));
  resolution.referrer =
      ReferrerFromValue(controls_->Get(Control::kReferrer, host, etld1));
  if (controls_->Get(Control::kHttps, host, etld1) == Value::kBlock) {
    resolution.https = net::HttpsMode::kHttpsOnly;
  }
  resolution.strip_tracking_params = resolution.block_trackers;
  resolution.send_privacy_signals = resolution.block_trackers;

  // --- mode floor: applied last, because it is a floor ---
  // A per-site exception is a decision about a normal window. Letting it lower
  // protection inside a Private or Tor window would silently carry the user's
  // convenience choice into the context where they expect the opposite.
  const session::ModeConfig& config = modes_->config();
  if (resolution.mode != BrowsingMode::kNormal) {
    resolution.storage = config.isolation;
    if (resolution.cookies == CookieMode::kAllow) {
      resolution.cookies = CookieMode::kBlockThirdParty;
    }
  }
  if (config.fingerprint_level_locked ||
      static_cast<int>(config.fingerprint_level) >
          static_cast<int>(resolution.fingerprint)) {
    resolution.fingerprint = config.fingerprint_level;
    resolution.fingerprint_locked = config.fingerprint_level_locked;
  }
  if (config.webrtc_disabled) {
    resolution.webrtc = net::WebRtcMode::kStrict;
  }
  if (config.dns_through_proxy) {
    // Names are resolved at the exit node; the profile's DoH provider must not
    // see them, and must not be asked either.
    resolution.dns = net::DnsMode::kSecureStrict;
  }
  // Fingerprinting at Maximum implies ephemeral third-party storage, per
  // docs/design/010: normalizing the APIs while letting a tracker keep a
  // persistent third-party cookie protects nothing. This is exactly the kind
  // of cross-layer rule that used to be nobody's job — hence this class.
  if (resolution.fingerprint == FpLevel::kMaximum &&
      resolution.storage == net::IsolationLevel::kStandard) {
    resolution.storage = net::IsolationLevel::kStrict;
  }

  if (resolution.mode == BrowsingMode::kTor) {
    resolution.permissions = PermissionDefault::kDenySilently;
    resolution.referrer = ReferrerMode::kOriginOnly;
    resolution.block_ads = true;
    resolution.block_trackers = true;
    resolution.behavioral_detection = true;
    resolution.strip_tracking_params = true;
    resolution.send_privacy_signals = true;
    resolution.https = net::HttpsMode::kHttpsOnly;
  }
  return resolution;
}

// static
std::vector<std::string> PrivacyPolicy::Conflicts(const Resolution& r) {
  std::vector<std::string> conflicts;
  auto require = [&conflicts](bool condition, const char* message) {
    if (!condition) {
      conflicts.push_back(message);
    }
  };

  require(r.fingerprint != FpLevel::kMaximum ||
              r.storage != net::IsolationLevel::kStandard,
          "fingerprinting at Maximum but third-party storage is persistent: "
          "sites can re-identify the user through storage regardless of the "
          "shims");
  require(r.mode == BrowsingMode::kNormal ||
              r.storage != net::IsolationLevel::kStandard,
          "private/Tor window with persistent storage");
  require(r.mode != BrowsingMode::kTor || r.webrtc == net::WebRtcMode::kStrict,
          "Tor window with WebRTC not locked down: it can bypass the proxy");
  require(r.mode != BrowsingMode::kTor || r.fingerprint_locked,
          "Tor window where a site could lower the fingerprinting level");
  require(r.mode != BrowsingMode::kTor || r.https == net::HttpsMode::kHttpsOnly,
          "Tor window allowing plaintext: the exit node can read and rewrite it");
  require(r.mode != BrowsingMode::kTor || r.cookies != CookieMode::kAllow,
          "Tor window with unrestricted cookies");
  require(r.block_trackers == r.behavioral_detection,
          "the behavioral layer disagrees with the tracker setting: one would "
          "block what the other allows");
  require(!r.strip_tracking_params || r.block_trackers,
          "link cleaning on while tracker protection is off");
  require(r.send_privacy_signals == r.block_trackers,
          "the GPC/DNT signal contradicts the user's own tracker setting");
  return conflicts;
}

// static
std::vector<std::string> PrivacyPolicy::Explain(const Resolution& r) {
  std::vector<std::string> lines;
  lines.push_back(std::string("Ads: ") + (r.block_ads ? "blocked" : "allowed"));
  lines.push_back(std::string("Trackers: ") +
                  (r.block_trackers ? "blocked, including ones detected on "
                                      "this device"
                                    : "allowed"));
  lines.push_back(std::string("Fingerprinting: level ") +
                  std::to_string(static_cast<int>(r.fingerprint)) +
                  (r.fingerprint_locked ? " (locked by this window's mode)"
                                        : ""));
  switch (r.cookies) {
    case CookieMode::kAllow:
      lines.push_back("Cookies: allowed, including across sites");
      break;
    case CookieMode::kPartition:
    case CookieMode::kBlockThirdParty:
      lines.push_back("Cookies: kept separately for each site");
      break;
    case CookieMode::kBlockAll:
      lines.push_back("Cookies: blocked");
      break;
  }
  lines.push_back(r.storage == net::IsolationLevel::kStandard
                      ? "Storage: separated per site, kept between sessions"
                      : "Storage: separated per site, cleared when you close "
                        "the site");
  lines.push_back(r.https == net::HttpsMode::kHttpsOnly
                      ? "Connection: HTTPS only"
                      : "Connection: upgraded to HTTPS when possible");
  switch (r.scripts) {
    case ScriptMode::kAllow:
      lines.push_back("Scripts: allowed");
      break;
    case ScriptMode::kThirdPartyBlocked:
      lines.push_back("Scripts: only from this site");
      break;
    case ScriptMode::kBlockAll:
      lines.push_back("Scripts: blocked (many sites will not work)");
      break;
  }
  lines.push_back(r.permissions == PermissionDefault::kAsk
                      ? "Permissions: you are asked each time"
                      : "Permissions: refused without asking in this window");
  return lines;
}

}  // namespace privacy
}  // namespace bedrock
