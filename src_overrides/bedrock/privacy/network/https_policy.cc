// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/network/https_policy.h"

namespace bedrock {
namespace net {
namespace {

using privacy::Control;
using privacy::Value;

bool StartsWith(const std::string& text, const std::string& prefix) {
  return text.compare(0, prefix.size(), prefix) == 0;
}

bool EndsWith(const std::string& text, const std::string& suffix) {
  return text.size() >= suffix.size() &&
         text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

HttpsPolicy::HttpsPolicy(privacy::ProtectionController* controls)
    : controls_(controls) {}

HttpsPolicy::~HttpsPolicy() = default;

// static
bool HttpsPolicy::IsLocalOrOnion(const std::string& host) {
  return host == "localhost" || StartsWith(host, "127.") ||
         StartsWith(host, "192.168.") || StartsWith(host, "10.") ||
         EndsWith(host, ".local") || EndsWith(host, ".onion") ||
         host == "[::1]";
}

HttpsMode HttpsPolicy::ModeForHost(const std::string& host) const {
  // The per-site shields setting can only make HTTPS *stronger* than the
  // profile default, never weaker: a site cannot opt itself out of HTTPS-Only.
  const Value site = controls_->Get(Control::kHttps, host, host);
  if (site == Value::kBlock || site == Value::kBlockStrict) {
    return HttpsMode::kHttpsOnly;
  }
  return mode_;
}

UpgradeAction HttpsPolicy::ForNavigation(const std::string& url,
                                         const std::string& host) const {
  if (StartsWith(url, "https://")) {
    return UpgradeAction::kAlreadySecure;
  }
  if (!StartsWith(url, "http://")) {
    return UpgradeAction::kAlreadySecure;  // non-web scheme, not ours to judge
  }
  if (controls_->Get(Control::kHttps, host, host) == Value::kAllow) {
    return UpgradeAction::kAllowPlaintext;  // user turned upgrading off here
  }
  if (HasPlaintextException(host)) {
    return UpgradeAction::kAllowPlaintext;
  }
  if (IsLocalOrOnion(host)) {
    // A printer on the LAN and a .onion address have no public certificate.
    // Warning here would teach the user that the warning means nothing.
    return UpgradeAction::kAllowPlaintext;
  }
  if (ModeForHost(host) == HttpsMode::kHttpsOnly) {
    return UpgradeAction::kInterstitial;
  }
  return UpgradeAction::kUpgrade;
}

MixedContentAction HttpsPolicy::ForSubresource(const std::string& host,
                                               bool active_content) const {
  if (active_content) {
    // Scripts, iframes, XHR: never allowed over plaintext on a secure page,
    // and no exception can enable them. An attacker who can rewrite one script
    // owns the page, so there is nothing to weigh here.
    return MixedContentAction::kBlock;
  }
  if (HasPlaintextException(host)) {
    return MixedContentAction::kAllow;
  }
  // Passive content is upgraded; if the upgrade fails the caller blocks it
  // rather than falling back, which is what "mixed-content protection" means.
  return MixedContentAction::kUpgrade;
}

// static
bool HttpsPolicy::Proceedable(CertError error) {
  switch (error) {
    case CertError::kNone:
      return true;
    case CertError::kExpired:
    case CertError::kDateInvalid:
    case CertError::kAuthorityInvalid:
    case CertError::kCommonNameInvalid:
      // Common on misconfigured but honest servers. The user may proceed after
      // reading what is wrong — per host, once.
      return true;
    case CertError::kRevoked:
    case CertError::kPinnedKeyMismatch:
    case CertError::kWeakSignature:
      // These do not happen by accident. Revocation means the key is known to
      // be compromised, a pin mismatch means someone is between the user and
      // the site. There is no button.
      return false;
  }
  return false;
}

// static
const char* HttpsPolicy::ExplainCertError(CertError error) {
  switch (error) {
    case CertError::kNone:
      return "The connection is encrypted and the site proved its identity.";
    case CertError::kExpired:
      return "This site's certificate expired. The site is probably just "
             "misconfigured, but nobody is checking its identity right now.";
    case CertError::kDateInvalid:
      return "This site's certificate is not valid yet, or your device clock "
             "is wrong. Check the date on your computer first.";
    case CertError::kAuthorityInvalid:
      return "Nobody your browser trusts vouches for this site's identity. On "
             "a public website this can mean the connection is being "
             "intercepted.";
    case CertError::kCommonNameInvalid:
      return "This certificate was issued for a different address. You may not "
             "be talking to the site you typed.";
    case CertError::kRevoked:
      return "This certificate was revoked by its issuer, which usually means "
             "its key was stolen. Bedrock will not open this site.";
    case CertError::kPinnedKeyMismatch:
      return "This site's certificate does not match the one it is pinned to. "
             "That is what an intercepted connection looks like. Bedrock will "
             "not open this site.";
    case CertError::kWeakSignature:
      return "This certificate uses cryptography that is considered broken. "
             "Bedrock will not open this site.";
  }
  return "";
}

void HttpsPolicy::AllowPlaintextForHost(const std::string& host) {
  plaintext_exceptions_[host] = true;
}

void HttpsPolicy::ClearPlaintextException(const std::string& host) {
  plaintext_exceptions_.erase(host);
}

bool HttpsPolicy::HasPlaintextException(const std::string& host) const {
  return plaintext_exceptions_.count(host) != 0;
}

bool HttpsPolicy::AddCertException(const std::string& host, CertError error) {
  if (!Proceedable(error)) {
    return false;
  }
  cert_exceptions_[host] = error;
  return true;
}

bool HttpsPolicy::HasCertException(const std::string& host,
                                   CertError error) const {
  auto it = cert_exceptions_.find(host);
  // The exception covers the exact error it was granted for. A site excepted
  // for an expired certificate must still warn if the authority changes.
  return it != cert_exceptions_.end() && it->second == error;
}

void HttpsPolicy::ClearCertExceptions() {
  cert_exceptions_.clear();
}

}  // namespace net
}  // namespace bedrock
