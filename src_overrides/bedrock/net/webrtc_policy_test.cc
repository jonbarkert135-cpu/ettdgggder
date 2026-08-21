// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/net/webrtc_policy.h"

#include <iostream>
#include <string>

namespace {

using namespace bedrock::net;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

}  // namespace

int main() {
  // Privacy is the default: protection the user did not have to find.
  Check(WebRtcPolicy().mode() == WebRtcMode::kPrivacy,
        "Privacy mode is the default");

  // No mode gathers candidates for a page that has no media permission,
  // except the explicitly-unprotected Default.
  {
    WebRtcPolicy privacy(WebRtcMode::kPrivacy);
    Check(privacy.Handling(false) == IpHandlingPolicy::kDisableNonProxiedUdp,
          "no permission: nothing is gathered in Privacy mode");
    Check(privacy.Handling(true) ==
              IpHandlingPolicy::kDefaultPublicInterfaceOnly,
          "with permission: public interface only");
    Check(!privacy.ExposesLocalAddresses(true),
          "a granted call still does not expose local addresses");
    Check(privacy.UseMdnsCandidates(),
          "local candidates are obfuscated with mDNS names");
  }
  {
    WebRtcPolicy strict(WebRtcMode::kStrict);
    Check(strict.Handling(false) == IpHandlingPolicy::kDisableNonProxiedUdp &&
              strict.Handling(true) == IpHandlingPolicy::kDisableNonProxiedUdp,
          "Strict refuses non-proxied UDP regardless of permission");
    Check(!strict.ExposesLocalAddresses(true), "Strict exposes nothing local");
  }
  {
    WebRtcPolicy off(WebRtcMode::kDefault);
    Check(off.Handling(false) == IpHandlingPolicy::kDefault,
          "Default maps to Chromium's default policy");
    Check(off.ExposesLocalAddresses(false),
          "and honestly reports that local addresses are exposed");
    Check(!off.UseMdnsCandidates(), "no mDNS obfuscation in Default");
  }

  // Every mode explains the risk *and* the cost, and admits what is not
  // hidden: the public IP is visible in every mode.
  for (WebRtcMode mode :
       {WebRtcMode::kDefault, WebRtcMode::kPrivacy, WebRtcMode::kStrict}) {
    const std::string name = WebRtcPolicy::ModeName(mode);
    const std::string explanation = WebRtcPolicy::Explain(mode);
    const std::string tradeoff = WebRtcPolicy::Tradeoff(mode);
    Check(!name.empty(), "mode has a name");
    Check(explanation.size() > 60, name + " explains the risk");
    Check(tradeoff.size() > 20, name + " states the cost");
    Check(explanation.find("public IP") != std::string::npos ||
              tradeoff.find("proxy") != std::string::npos,
          name + " is honest about what is still visible");
  }

  if (failures == 0) {
    std::cout << "webrtc_policy_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
