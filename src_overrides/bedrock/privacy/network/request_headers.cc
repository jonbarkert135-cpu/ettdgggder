// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/network/request_headers.h"

#include <algorithm>
#include <string>
#include <vector>

#include "bedrock/privacy/network/host_match.h"

namespace bedrock {
namespace net {
namespace {

using privacy::Control;
using privacy::FpLevel;
using privacy::Strategy;
using privacy::Surface;
using privacy::Value;

// Scheme test on a full URL. Not a host comparison: host names go through
// host_match.h (see the gate in scripts/check_host_matching.py).
bool HasScheme(const std::string& url, const std::string& scheme) {
  return url.size() >= scheme.size() && url.compare(0, scheme.size(), scheme) == 0;
}

bool IsSecure(const std::string& url) {
  return HasScheme(url, "https://") || HasScheme(url, "wss://");
}

// A URL whose origin is opaque has no referrer to send and no origin to trim
// to. `about:`, `data:`, `blob:` and `javascript:` are the ones that reach here.
bool HasHierarchicalOrigin(const std::string& url) {
  const std::string::size_type mark = url.find("://");
  return mark != std::string::npos && mark > 0;
}

// Ordering used everywhere below: a larger rank reveals less.
int Rank(ReferrerScope scope) {
  switch (scope) {
    case ReferrerScope::kFullUrl:
      return 0;
    case ReferrerScope::kOriginOnly:
      return 1;
    case ReferrerScope::kNone:
      return 2;
  }
  return 2;
}

ReferrerScope Stricter(ReferrerScope a, ReferrerScope b) {
  return Rank(a) >= Rank(b) ? a : b;
}

bool SameOrigin(const OutgoingRequest& request) {
  const std::string from = RequestHeaderPolicy::OriginOf(request.initiator_url);
  const std::string to = RequestHeaderPolicy::OriginOf(request.target_url);
  return !from.empty() && from == to;
}

// https -> http is a downgrade; so is https -> anything not secure. A referrer
// on a downgraded request is the one case every browser agrees to drop, and we
// never make it looser than that.
bool IsDowngrade(const OutgoingRequest& request) {
  return IsSecure(request.initiator_url) && !IsSecure(request.target_url);
}

std::string MajorVersion(const std::string& full_version) {
  const std::string::size_type dot = full_version.find('.');
  return dot == std::string::npos ? full_version : full_version.substr(0, dot);
}

std::string Int(int value) { return std::to_string(value); }

// One decimal place, without <sstream> or the locale-dependent printf path.
// The sign is handled by hand: std::abs() is not visible in Chromium's C++
// modules build unless <cstdlib> is imported as a module, and dividing a
// negative scaled value would otherwise print "0.5" for -0.05.
std::string OneDecimal(double value) {
  long scaled = static_cast<long>(value * 10.0 + (value < 0 ? -0.5 : 0.5));
  std::string sign;
  if (scaled < 0) {
    sign = "-";
    scaled = -scaled;
  }
  return sign + std::to_string(scaled / 10) + "." + std::to_string(scaled % 10);
}

}  // namespace

bool OutgoingRequest::third_party() const {
  const std::string from = NormalizeHost(initiator_etld1);
  const std::string to = NormalizeHost(target_etld1);
  // An unknown eTLD+1 is treated as third party: the safe direction for a
  // header that leaks, and the caller should not be guessing here anyway.
  if (from.empty() || to.empty()) {
    return true;
  }
  return from != to;
}

RequestHeaderPolicy::RequestHeaderPolicy(privacy::ProtectionController* controls)
    : controls_(controls) {}

RequestHeaderPolicy::~RequestHeaderPolicy() = default;

// static
std::string RequestHeaderPolicy::OriginOf(const std::string& url) {
  if (!HasHierarchicalOrigin(url)) {
    return std::string();
  }
  const std::string::size_type authority = url.find("://") + 3;
  std::string::size_type end = url.find('/', authority);
  if (end == std::string::npos) {
    end = url.size();
  }
  std::string authority_part = url.substr(authority, end - authority);
  // Credentials never belong in an origin, and a referrer carrying them has
  // been a real vulnerability in more than one browser.
  const std::string::size_type at = authority_part.rfind('@');
  if (at != std::string::npos) {
    authority_part = authority_part.substr(at + 1);
  }
  if (authority_part.empty()) {
    return std::string();
  }
  std::string scheme = url.substr(0, url.find("://"));
  std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                 [](unsigned char c) { return static_cast<char>(tolower(c)); });
  return scheme + "://" + NormalizeHost(authority_part) + "/";
}

// static
std::string RequestHeaderPolicy::SanitizeUrl(const std::string& url) {
  const std::string origin = OriginOf(url);
  if (origin.empty()) {
    return std::string();
  }
  const std::string::size_type authority = url.find("://") + 3;
  const std::string::size_type path = url.find('/', authority);
  std::string rest = path == std::string::npos ? std::string() : url.substr(path + 1);
  // The fragment stays in the browser; it was never part of a request.
  const std::string::size_type hash = rest.find('#');
  if (hash != std::string::npos) {
    rest = rest.substr(0, hash);
  }
  return origin + rest;
}

ReferrerScope RequestHeaderPolicy::FloorFor(const OutgoingRequest& request) const {
  // The shields scope is the *referring* document: this is a decision about
  // what that page is allowed to give away about itself.
  const Value site = controls_->Get(Control::kReferrer, request.initiator_host,
                                    request.initiator_etld1);
  const bool cross_site = request.third_party();
  switch (site) {
    case Value::kAllow:
      return ReferrerScope::kFullUrl;
    case Value::kBlockStrict:
      return ReferrerScope::kNone;
    case Value::kBlock:
      return cross_site ? ReferrerScope::kNone : ReferrerScope::kOriginOnly;
    case Value::kReduce:
    case Value::kInherit:
      break;
  }
  // The default, and the Balanced preset: your own site sees where you were,
  // everyone else sees only which site sent you.
  return cross_site ? ReferrerScope::kOriginOnly : ReferrerScope::kFullUrl;
}

ReferrerScope RequestHeaderPolicy::FromDeclared(const OutgoingRequest& request) const {
  const bool same_origin = SameOrigin(request);
  switch (request.declared) {
    case DeclaredReferrerPolicy::kUnset:
      // Nothing declared: the floor decides on its own.
      return ReferrerScope::kFullUrl;
    case DeclaredReferrerPolicy::kNoReferrer:
      return ReferrerScope::kNone;
    case DeclaredReferrerPolicy::kSameOrigin:
      return same_origin ? ReferrerScope::kFullUrl : ReferrerScope::kNone;
    case DeclaredReferrerPolicy::kOrigin:
    case DeclaredReferrerPolicy::kStrictOrigin:
      return ReferrerScope::kOriginOnly;
    case DeclaredReferrerPolicy::kStrictOriginWhenCrossOrigin:
    case DeclaredReferrerPolicy::kOriginWhenCrossOrigin:
      return same_origin ? ReferrerScope::kFullUrl : ReferrerScope::kOriginOnly;
    case DeclaredReferrerPolicy::kNoReferrerWhenDowngrade:
    case DeclaredReferrerPolicy::kUnsafeUrl:
      // Both mean "send the full URL" in the non-downgrade case. Downgrades are
      // handled once, below, for every policy — including these two.
      return ReferrerScope::kFullUrl;
  }
  return ReferrerScope::kFullUrl;
}

ReferrerScope RequestHeaderPolicy::ScopeFor(const OutgoingRequest& request) const {
  if (!HasHierarchicalOrigin(request.initiator_url)) {
    return ReferrerScope::kNone;  // opaque origin: nothing to send
  }
  if (IsDowngrade(request)) {
    return ReferrerScope::kNone;
  }
  return Stricter(FloorFor(request), FromDeclared(request));
}

bool RequestHeaderPolicy::DeclaredPolicyRefused(const OutgoingRequest& request) const {
  if (request.declared == DeclaredReferrerPolicy::kUnset) {
    return false;
  }
  return Rank(FromDeclared(request)) < Rank(FloorFor(request));
}

std::string RequestHeaderPolicy::ReferrerFor(const OutgoingRequest& request) const {
  switch (ScopeFor(request)) {
    case ReferrerScope::kNone:
      return std::string();
    case ReferrerScope::kOriginOnly:
      return OriginOf(request.initiator_url);
    case ReferrerScope::kFullUrl:
      return SanitizeUrl(request.initiator_url);
  }
  return std::string();
}

// --- Client hints ------------------------------------------------------------

const char* HintHeaderName(Hint hint) {
  switch (hint) {
    case Hint::kUa:
      return "Sec-CH-UA";
    case Hint::kUaMobile:
      return "Sec-CH-UA-Mobile";
    case Hint::kUaPlatform:
      return "Sec-CH-UA-Platform";
    case Hint::kUaArch:
      return "Sec-CH-UA-Arch";
    case Hint::kUaBitness:
      return "Sec-CH-UA-Bitness";
    case Hint::kUaModel:
      return "Sec-CH-UA-Model";
    case Hint::kUaFullVersionList:
      return "Sec-CH-UA-Full-Version-List";
    case Hint::kUaPlatformVersion:
      return "Sec-CH-UA-Platform-Version";
    case Hint::kDeviceMemory:
      return "Device-Memory";
    case Hint::kDpr:
      return "DPR";
    case Hint::kViewportWidth:
      return "Viewport-Width";
    case Hint::kWidth:
      return "Width";
    case Hint::kRtt:
      return "RTT";
    case Hint::kDownlink:
      return "Downlink";
    case Hint::kEct:
      return "ECT";
    case Hint::kSaveData:
      return "Save-Data";
    case Hint::kPrefersColorScheme:
      return "Sec-CH-Prefers-Color-Scheme";
  }
  return "";
}

bool IsHighEntropyHint(Hint hint) {
  switch (hint) {
    // The three the platform sends unconditionally. They stay, normalised: a
    // browser that sends none of them is itself distinctive, and every site
    // that does content negotiation depends on them.
    case Hint::kUa:
    case Hint::kUaMobile:
    case Hint::kUaPlatform:
      return false;
    default:
      return true;
  }
}

bool IsNetworkQualityHint(Hint hint) {
  return hint == Hint::kRtt || hint == Hint::kDownlink || hint == Hint::kEct;
}

bool IsUaIdentityHint(Hint hint) {
  switch (hint) {
    case Hint::kUaArch:
    case Hint::kUaBitness:
    case Hint::kUaModel:
    case Hint::kUaPlatformVersion:
    case Hint::kUaFullVersionList:
      return true;
    default:
      return false;
  }
}

std::string RequestHeaderPolicy::HintValue(Hint hint, const DeviceFacts& facts) const {
  const bool reduce = fp_level_ != FpLevel::kCompatibility;
  switch (hint) {
    case Hint::kUa:
      return "\"" + facts.ua_brand + "\";v=\"" +
             (reduce ? MajorVersion(facts.full_version) : facts.full_version) + "\"";
    case Hint::kUaFullVersionList:
      // Reduced to the same major version: the full version is a release-date
      // fingerprint that no site needs to lay out a page.
      return "\"" + facts.ua_brand + "\";v=\"" +
             (reduce ? MajorVersion(facts.full_version) + ".0.0.0" : facts.full_version) +
             "\"";
    case Hint::kUaMobile:
      // Truthful at every level: a desktop pretending to be a phone is served
      // a different site, which is a compatibility bug, not privacy.
      return facts.mobile ? "?1" : "?0";
    case Hint::kUaPlatform:
      return "\"" + facts.platform + "\"";
    case Hint::kUaPlatformVersion:
      // Empty is the value UA reduction settled on: it says "a supported
      // version of this platform" and nothing about the machine.
      return reduce ? "\"\"" : "\"" + facts.platform_version + "\"";
    case Hint::kUaArch:
      return reduce ? "\"x86\"" : "\"" + facts.architecture + "\"";
    case Hint::kUaBitness:
      return reduce ? "\"64\"" : "\"" + facts.bitness + "\"";
    case Hint::kUaModel:
      // A device model is the single most identifying hint and nothing about a
      // desktop page needs it: while reducing, the header is omitted entirely
      // rather than sent empty.
      return reduce || facts.model.empty() ? "" : "\"" + facts.model + "\"";
    case Hint::kDeviceMemory:
      // Same function the JavaScript surface uses (rule 3 of the header).
      return Int(privacy::NormalizedDeviceMemoryGb(facts.device_memory_gb, fp_level_));
    case Hint::kDpr:
      return reduce ? "1" : OneDecimal(facts.dpr);
    case Hint::kViewportWidth:
    case Hint::kWidth:
      return Int(reduce ? privacy::QuantizeWindowSize(facts.window, fp_level_).width
                        : facts.window.width);
    case Hint::kRtt:
    case Hint::kDownlink:
    case Hint::kEct:
      // Never reported above kCompatibility, so there is no reduced form to
      // invent; HintsFor() has already dropped them.
      return hint == Hint::kEct ? "4g" : Int(0);
    case Hint::kSaveData:
      return facts.save_data_enabled ? "on" : "";
    case Hint::kPrefersColorScheme:
      return facts.prefers_dark ? "dark" : "light";
  }
  return "";
}

std::string RequestHeaderPolicy::AcceptLanguage(const DeviceFacts& facts) const {
  const char* normalized = privacy::NormalizedLanguage(fp_level_);
  // NormalizedLanguage returns the population value at kBalanced and above and
  // the real one below it; the one place the choice is made stays there.
  return normalized != nullptr && *normalized != '\0' ? std::string(normalized)
                                                      : facts.language;
}

std::vector<Hint> RequestHeaderPolicy::HintsFor(const OutgoingRequest& request,
                                               const std::vector<Hint>& accepted) const {
  std::vector<Hint> out;
  const Strategy strategy = privacy::GetStrategy(Surface::kClientHints, fp_level_);
  if (strategy == Strategy::kBlock) {
    return out;  // kMaximum: no hint headers at all, and the UI says so
  }
  const bool third_party = request.third_party();
  const bool allow_high_entropy =
      strategy == Strategy::kAllow ||
      (fp_level_ == FpLevel::kBalanced && !third_party);

  for (int index = 0; index <= static_cast<int>(Hint::kMaxValue); ++index) {
    const Hint hint = static_cast<Hint>(index);
    const bool requested =
        std::find(accepted.begin(), accepted.end(), hint) != accepted.end();
    if (!IsHighEntropyHint(hint)) {
      out.push_back(hint);  // low entropy: always, normalised
      continue;
    }
    // Rule 2: a hint goes only to the party that asked for it. This one is not
    // relaxed at kCompatibility — it is a question of who asked, not of how
    // much the value reveals — so delegation to a third party is refused at
    // every level.
    if (!requested || third_party || !allow_high_entropy) {
      continue;
    }
    // A live measurement of the user's connection is not layout information.
    if (IsNetworkQualityHint(hint) && strategy != Strategy::kAllow) {
      continue;
    }
    // The claim in docs/privacy/fingerprinting/client-hints.md is that from
    // level 1 no `Sec-CH-UA-*` beyond the low-entropy set appears on the wire.
    // It is a claim precisely because it is checkable, so the code keeps it:
    // the identity hints are dropped rather than reduced, and a first party
    // that needs them reads the normalised values from the JS surface.
    if (IsUaIdentityHint(hint) && strategy != Strategy::kAllow) {
      continue;
    }
    // Save-Data is a signal the user turned on; absent that, sending it would
    // be inventing a preference.
    if (hint == Hint::kSaveData) {
      continue;
    }
    out.push_back(hint);
  }
  return out;
}

std::vector<HeaderValue> RequestHeaderPolicy::HintHeadersFor(
    const OutgoingRequest& request,
    const std::vector<Hint>& accepted,
    const DeviceFacts& facts) const {
  std::vector<HeaderValue> headers;
  for (const Hint hint : HintsFor(request, accepted)) {
    const std::string value = HintValue(hint, facts);
    if (value.empty()) {
      continue;  // an empty hint is an absent hint, not an empty header
    }
    headers.push_back({HintHeaderName(hint), value});
  }
  // Save-Data is the one hint the user, not the site, turns on: it is sent when
  // enabled and requested, because a site that honours it saves the user data.
  if (facts.save_data_enabled &&
      std::find(accepted.begin(), accepted.end(), Hint::kSaveData) != accepted.end() &&
      privacy::GetStrategy(Surface::kClientHints, fp_level_) != Strategy::kBlock) {
    headers.push_back({HintHeaderName(Hint::kSaveData), "on"});
  }
  return headers;
}

}  // namespace net
}  // namespace bedrock
