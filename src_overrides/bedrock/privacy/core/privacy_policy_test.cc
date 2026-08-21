// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/privacy/core/privacy_policy.h"

#include <iostream>
#include <string>

namespace {

using namespace bedrock::privacy;  // NOLINT — test-local convenience
using bedrock::net::DnsSettings;
using bedrock::net::HttpsMode;
using bedrock::net::HttpsPolicy;
using bedrock::net::IsolationLevel;
using bedrock::net::StorageIsolation;
using bedrock::net::WebRtcMode;
using bedrock::net::WebRtcPolicy;
using bedrock::session::BrowsingMode;
using bedrock::session::BrowsingModeController;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

}  // namespace

int main() {
  ProtectionController controls;
  BrowsingModeController modes;
  HttpsPolicy https(&controls);
  DnsSettings dns;
  WebRtcPolicy webrtc;
  StorageIsolation storage;
  PrivacyPolicy policy(&controls, &modes, &https, &dns, &webrtc, &storage);

  // Defaults are consistent and protective.
  {
    const Resolution r = policy.For("news.test", "news.test");
    Check(PrivacyPolicy::Conflicts(r).empty(), "default resolution is consistent");
    Check(r.block_ads && r.block_trackers && r.behavioral_detection,
          "protection is on by default");
    Check(r.fingerprint == FpLevel::kBalanced, "fingerprinting at level 1");
    Check(r.cookies == CookieMode::kBlockThirdParty,
          "third-party cookies blocked");
    Check(r.scripts == ScriptMode::kAllow, "scripts allowed by default");
  }

  // A per-site exception is honoured in a normal window...
  {
    controls.Set(Scope::kSite, "news.test", Control::kTrackers, Value::kAllow);
    const Resolution r = policy.For("news.test", "news.test");
    Check(!r.block_trackers, "the user's per-site exception applies");
    Check(!r.behavioral_detection,
          "and the behavioral layer follows it instead of overruling it");
    Check(!r.send_privacy_signals,
          "the GPC signal is not sent against the user's own setting");
    Check(!r.strip_tracking_params, "link cleaning follows the same switch");
    Check(PrivacyPolicy::Conflicts(r).empty(),
          "an exception does not create a contradiction");
  }

  // ...but it never lowers protection inside a Private or Tor window.
  {
    modes.SetMode(BrowsingMode::kPrivate);
    const Resolution priv = policy.For("news.test", "news.test");
    Check(priv.storage == IsolationLevel::kEphemeralAll,
          "private windows keep nothing, whatever the site setting says");
    Check(PrivacyPolicy::Conflicts(priv).empty(), "private resolution is consistent");

    modes.SetMode(BrowsingMode::kTor);
    const Resolution tor = policy.For("news.test", "news.test");
    Check(tor.block_trackers && tor.behavioral_detection,
          "the normal-window exception does not follow the user into Tor");
    Check(tor.fingerprint == FpLevel::kMaximum && tor.fingerprint_locked,
          "fingerprinting is locked at Maximum");
    Check(tor.webrtc == WebRtcMode::kStrict, "WebRTC is locked down");
    Check(tor.https == HttpsMode::kHttpsOnly, "plaintext is refused");
    Check(tor.permissions == PermissionDefault::kDenySilently,
          "permission prompts are not shown in a Tor window");
    Check(tor.cookies != CookieMode::kAllow, "cookies stay restricted");
    Check(PrivacyPolicy::Conflicts(tor).empty(), "Tor resolution is consistent");
    modes.SetMode(BrowsingMode::kNormal);
    controls.Clear(Scope::kSite, "news.test");
  }

  // The property that matters: no combination of settings produces a
  // self-contradicting policy. Every mode x every value of every control.
  {
    const Control controls_to_vary[] = {Control::kAds, Control::kTrackers,
                                        Control::kFingerprinting,
                                        Control::kCookies, Control::kScripts,
                                        Control::kHttps, Control::kReferrer};
    const Value values[] = {Value::kAllow, Value::kReduce, Value::kBlock,
                            Value::kBlockStrict};
    int checked = 0;
    for (BrowsingMode mode :
         {BrowsingMode::kNormal, BrowsingMode::kPrivate, BrowsingMode::kTor}) {
      modes.SetMode(mode);
      for (Control control : controls_to_vary) {
        for (Value value : values) {
          controls.Set(Scope::kSite, "site.test", control, value);
          const Resolution r = policy.For("site.test", "site.test");
          const auto conflicts = PrivacyPolicy::Conflicts(r);
          for (const std::string& conflict : conflicts) {
            std::cerr << "  conflict (mode " << static_cast<int>(mode)
                      << ", control " << static_cast<int>(control) << ", value "
                      << static_cast<int>(value) << "): " << conflict << "\n";
          }
          Check(conflicts.empty(), "no contradiction for this combination");
          ++checked;
        }
        controls.Clear(Scope::kSite, "site.test");
      }
    }
    Check(checked == 3 * 7 * 4, "84 combinations were checked");
    modes.SetMode(BrowsingMode::kNormal);
  }

  // Profile-level changes flow through the one resolver, not around it.
  {
    storage.set_level(IsolationLevel::kStrict);
    dns.UsePreset("Quad9");
    https.set_mode(HttpsMode::kHttpsOnly);
    const Resolution r = policy.For("news.test", "news.test");
    Check(r.storage == IsolationLevel::kStrict, "storage level comes from the profile");
    Check(r.dns == bedrock::net::DnsMode::kSecurePreset, "DNS mode too");
    Check(r.https == HttpsMode::kHttpsOnly, "and the HTTPS mode");
    Check(PrivacyPolicy::Conflicts(r).empty(), "still consistent");
    storage.set_level(IsolationLevel::kStandard);
    https.set_mode(HttpsMode::kUpgrade);
  }

  // The conflict checker is not vacuous: hand it a bad resolution.
  {
    Resolution bad;
    bad.fingerprint = FpLevel::kMaximum;
    bad.storage = IsolationLevel::kStandard;
    bad.block_trackers = false;
    bad.behavioral_detection = true;
    Check(PrivacyPolicy::Conflicts(bad).size() >= 2,
          "an inconsistent resolution is actually reported");

    Resolution fake_tor;
    fake_tor.mode = BrowsingMode::kTor;
    Check(!PrivacyPolicy::Conflicts(fake_tor).empty(),
          "a Tor resolution without Tor's floors is rejected");
  }

  // The panel text is generated from the resolution, so it cannot drift.
  {
    const Resolution r = policy.For("news.test", "news.test");
    const auto lines = PrivacyPolicy::Explain(r);
    Check(lines.size() >= 8, "every layer is explained");
    for (const std::string& line : lines) {
      Check(line.size() > 8, "no stub line: " + line);
    }
  }

  if (failures == 0) {
    std::cout << "privacy_policy_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
