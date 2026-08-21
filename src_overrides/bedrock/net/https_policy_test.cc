// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/net/https_policy.h"

#include <iostream>
#include <string>

namespace {

using namespace bedrock::net;  // NOLINT — test-local convenience
using bedrock::privacy::Control;
using bedrock::privacy::ProtectionController;
using bedrock::privacy::Scope;
using bedrock::privacy::Value;

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
  HttpsPolicy https(&controls);

  // Upgrade mode: silent upgrade, no interruption.
  Check(https.ForNavigation("http://example.test/page", "example.test") ==
            UpgradeAction::kUpgrade,
        "plaintext navigation is upgraded");
  Check(https.ForNavigation("https://example.test/page", "example.test") ==
            UpgradeAction::kAlreadySecure,
        "https navigation is left alone");

  // Local devices and .onion have no public certificate; warning there would
  // train the user to ignore warnings.
  for (const char* host : {"localhost", "192.168.1.10", "printer.local",
                           "abcd.onion", "127.0.0.1"}) {
    Check(https.ForNavigation(std::string("http://") + host + "/", host) ==
              UpgradeAction::kAllowPlaintext,
          std::string("local/onion host allowed: ") + host);
  }

  // HTTPS-Only mode asks before leaving encryption.
  https.set_mode(HttpsMode::kHttpsOnly);
  Check(https.ForNavigation("http://example.test/page", "example.test") ==
            UpgradeAction::kInterstitial,
        "HTTPS-Only shows an interstitial instead of loading plaintext");

  // Exceptions are per host and explicit.
  https.AllowPlaintextForHost("legacy.test");
  Check(https.ForNavigation("http://legacy.test/x", "legacy.test") ==
            UpgradeAction::kAllowPlaintext,
        "per-host exception allows plaintext");
  Check(https.ForNavigation("http://other.test/x", "other.test") ==
            UpgradeAction::kInterstitial,
        "the exception does not leak to other hosts");
  https.ClearPlaintextException("legacy.test");
  Check(https.ForNavigation("http://legacy.test/x", "legacy.test") ==
            UpgradeAction::kInterstitial,
        "clearing the exception restores protection");

  // A per-site shields setting may strengthen HTTPS, never weaken HTTPS-Only.
  controls.Set(Scope::kSite, "strict.test", Control::kHttps, Value::kBlock);
  https.set_mode(HttpsMode::kUpgrade);
  Check(https.ForNavigation("http://strict.test/x", "strict.test") ==
            UpgradeAction::kInterstitial,
        "per-site HTTPS-Only works while the profile is in upgrade mode");
  controls.Set(Scope::kSite, "loose.test", Control::kHttps, Value::kAllow);
  Check(https.ForNavigation("http://loose.test/x", "loose.test") ==
            UpgradeAction::kAllowPlaintext,
        "turning upgrading off for one site is honoured in upgrade mode");

  // Mixed content: active content is never allowed, exception or not.
  Check(https.ForSubresource("cdn.test", /*active_content=*/true) ==
            MixedContentAction::kBlock,
        "active mixed content is blocked");
  https.AllowPlaintextForHost("cdn.test");
  Check(https.ForSubresource("cdn.test", /*active_content=*/true) ==
            MixedContentAction::kBlock,
        "and an exception cannot enable it");
  Check(https.ForSubresource("cdn.test", /*active_content=*/false) ==
            MixedContentAction::kAllow,
        "passive content may be allowed by an explicit exception");
  Check(https.ForSubresource("img.test", /*active_content=*/false) ==
            MixedContentAction::kUpgrade,
        "passive content is upgraded by default");

  // Certificate errors: honest, per host, and some are never bypassable.
  const CertError bypassable[] = {CertError::kExpired, CertError::kDateInvalid,
                                  CertError::kAuthorityInvalid,
                                  CertError::kCommonNameInvalid};
  const CertError never[] = {CertError::kRevoked, CertError::kPinnedKeyMismatch,
                             CertError::kWeakSignature};
  for (CertError error : bypassable) {
    Check(HttpsPolicy::Proceedable(error), "misconfiguration is proceedable");
    Check(https.AddCertException("broken.test", error),
          "an exception can be stored for it");
  }
  for (CertError error : never) {
    Check(!HttpsPolicy::Proceedable(error),
          "interception-grade error is not proceedable");
    Check(!https.AddCertException("evil.test", error),
          "and no exception can be stored for it");
    Check(!https.HasCertException("evil.test", error),
          "so no bypass exists");
  }

  // An exception covers the exact error it was granted for, nothing more.
  https.ClearCertExceptions();
  https.AddCertException("broken.test", CertError::kExpired);
  Check(https.HasCertException("broken.test", CertError::kExpired),
        "exception applies to the error it was granted for");
  Check(!https.HasCertException("broken.test", CertError::kAuthorityInvalid),
        "a new kind of problem on the same host still warns");
  Check(!https.HasCertException("elsewhere.test", CertError::kExpired),
        "exceptions never apply to another host");

  // Every error is explained in words a person can act on.
  for (int i = 0; i <= static_cast<int>(CertError::kWeakSignature); ++i) {
    Check(std::string(HttpsPolicy::ExplainCertError(static_cast<CertError>(i)))
              .size() > 30,
          "cert error " + std::to_string(i) + " has a real explanation");
  }

  if (failures == 0) {
    std::cout << "https_policy_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
