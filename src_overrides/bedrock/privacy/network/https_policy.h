// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_NETWORK_HTTPS_POLICY_H_
#define BEDROCK_PRIVACY_NETWORK_HTTPS_POLICY_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

#include "bedrock/privacy/core/protection_controller.h"

// HTTPS upgrading, mixed content and certificate errors (roadmap item 16).
//
// Two rules run through the whole file:
//
//   1. A certificate problem is never hidden and never remembered globally.
//      There is no "ignore certificate errors" switch, not in settings, not in
//      a hidden flag. An exception is per host, and errors that mean "someone
//      is intercepting this connection" cannot be excepted at all.
//   2. Upgrading is silent, downgrading is loud. Rewriting http:// to https://
//      needs no interruption; falling back to plaintext always does.

namespace bedrock {
namespace net {

enum class HttpsMode {
  // Upgrade what can be upgraded, fall back to HTTP with a warning.
  kUpgrade,
  // HTTPS-Only: never load plaintext without an explicit, per-site decision.
  kHttpsOnly,
};

enum class UpgradeAction {
  kAlreadySecure,
  kUpgrade,        // rewrite to https://
  kAllowPlaintext, // allowed by an exception or by local-network policy
  kInterstitial,   // HTTPS-Only: ask before leaving encryption
};

// What to do with a subresource on an HTTPS page that was requested over HTTP.
enum class MixedContentAction {
  kUpgrade,  // upgradable subresource (image, media, script, style)
  kBlock,    // not upgradable, or upgrade failed
  kAllow,    // user exception for this host, active subresources excluded
};

// Certificate problems, ordered by how likely they are to be an attack.
enum class CertError {
  kNone,
  kExpired,
  kDateInvalid,
  kAuthorityInvalid,   // self-signed / unknown CA
  kCommonNameInvalid,  // wrong host
  kRevoked,
  kPinnedKeyMismatch,  // HPKP / CT violation: treat as interception
  kWeakSignature,
};

class HttpsPolicy {
 public:
  explicit HttpsPolicy(privacy::ProtectionController* controls);
  ~HttpsPolicy();

  // Navigation decision for a URL. `host` is the target host.
  UpgradeAction ForNavigation(const std::string& url,
                              const std::string& host) const;

  // Subresource decision on an HTTPS page.
  MixedContentAction ForSubresource(const std::string& host,
                                    bool active_content) const;

  // Whether the user may click through this certificate error at all, and what
  // they are told. `Proceedable` false means: no button, no bypass.
  static bool Proceedable(CertError error);
  static const char* ExplainCertError(CertError error);

  // Per-host exception after an informed decision. Scoped to one host, and
  // dropped when the profile is cleared. There is no global form of this call.
  void AllowPlaintextForHost(const std::string& host);
  void ClearPlaintextException(const std::string& host);
  bool HasPlaintextException(const std::string& host) const;

  // Certificate exceptions are also per host, and only for errors that are
  // proceedable. Returns false if the error may not be excepted.
  //
  // They **expire** (audit finding F9). Clicking through a warning once is a
  // decision about a moment — a broken certificate that is still broken next
  // week deserves the question again, and a certificate replaced in the
  // meantime should not be trusted on the strength of the broken one. `now` is
  // injected in seconds, like everywhere else in this tree, so the policy is
  // testable without a clock.
  static constexpr int64_t kCertExceptionTtlSeconds = 7 * 24 * 60 * 60;
  bool AddCertException(const std::string& host, CertError error, int64_t now);
  bool HasCertException(const std::string& host,
                        CertError error,
                        int64_t now) const;
  // Drops what has expired; returns how many went. The settings UI calls this
  // so it never lists an exception that is no longer in force.
  int PruneCertExceptions(int64_t now);
  void ClearCertExceptions();

  // Hosts that are unreachable over HTTPS by nature: a plaintext-only local
  // device should not train the user to click through warnings.
  static bool IsLocalOrOnion(const std::string& host);

  void set_mode(HttpsMode mode) { mode_ = mode; }
  HttpsMode mode() const { return mode_; }

  size_t exception_count() const {
    return plaintext_exceptions_.size() + cert_exceptions_.size();
  }

 private:
  HttpsMode ModeForHost(const std::string& host) const;

  privacy::ProtectionController* controls_;
  HttpsMode mode_ = HttpsMode::kUpgrade;
  struct CertException {
    CertError error;
    int64_t expires_at = 0;
  };

  std::map<std::string, bool> plaintext_exceptions_;
  std::map<std::string, CertException> cert_exceptions_;
};

}  // namespace net
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_NETWORK_HTTPS_POLICY_H_
