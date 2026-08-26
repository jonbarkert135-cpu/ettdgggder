// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_NETWORK_REQUEST_HEADERS_H_
#define BEDROCK_PRIVACY_NETWORK_REQUEST_HEADERS_H_

#include <string>
#include <vector>

#include "bedrock/privacy/core/protection_controller.h"
#include "bedrock/privacy/fingerprinting/fingerprint_policy.h"

// What Bedrock attaches to an outgoing request: the referrer and the client
// hints (features `referrer_control` and `client_hints`).
//
// Both are the same problem wearing two names — a header the page never asked
// for that describes the user — so one component decides both, and it decides
// them **before the request leaves the browser process**, which is the third
// rule of the privacy architecture in `docs/ARCHITECTURE.md`.
//
// Three rules run through the file:
//
//   1. **A site may ask for less, never for more.** A page-declared
//      `Referrer-Policy` is honoured when it is stricter than the profile
//      floor and ignored when it is looser: `unsafe-url` is a request to leak
//      the user's full URL and is refused, always. The same holds for the
//      per-site shields value, exactly as in `https_policy` — a site cannot
//      opt itself out of the profile's protection.
//   2. **A hint is sent only to the party that asked for it.** High-entropy
//      hints go to the first party that requested them via `Accept-CH`, and
//      are never delegated to a third party, whatever the page's
//      `Permissions-Policy` says. Delegation is how a hint the user granted
//      to one site becomes a header on twenty tracker requests.
//   3. **A header must agree with the JavaScript surface.** Every hint value
//      comes from `privacy/fingerprinting/fingerprint_policy.h`, never from
//      the raw device: a header saying 16 GB while `navigator.deviceMemory`
//      says 8 is a *more* distinctive signal than either value alone.
//
// Pure logic, no Chromium dependency. The entry points that hand a real request
// to it are phase 3 (`docs/PHASES.md`).

namespace bedrock {
namespace net {

// What survives of the referring page's URL.
enum class ReferrerScope {
  kFullUrl,     // scheme, host, port, path — never credentials or fragment
  kOriginOnly,  // "https://example.test/"
  kNone,        // no Referer header at all
};

// The page-declared policy, as spelled in the `Referrer-Policy` header or the
// `referrerpolicy` attribute. Only the values that change our decision exist
// here; anything unrecognised is `kUnset`, because a policy nobody parsed must
// not silently become a permissive one.
enum class DeclaredReferrerPolicy {
  kUnset,
  kNoReferrer,
  kSameOrigin,
  kOrigin,
  kStrictOrigin,
  kStrictOriginWhenCrossOrigin,
  kNoReferrerWhenDowngrade,
  kOriginWhenCrossOrigin,
  kUnsafeUrl,  // parsed so it can be refused, not so it can be obeyed
};

// One request about to leave the browser. `initiator_*` describe the document
// making it, `target_*` the destination. eTLD+1 values are computed by the
// caller (Chromium's registry-controlled-domain service in the real build) and
// are compared here only after normalisation.
struct OutgoingRequest {
  std::string initiator_url;    // full URL of the referring document
  std::string initiator_host;
  std::string initiator_etld1;
  std::string target_url;
  std::string target_host;
  std::string target_etld1;
  bool navigation = false;  // top-level navigation vs. subresource
  DeclaredReferrerPolicy declared = DeclaredReferrerPolicy::kUnset;

  bool third_party() const;  // different eTLD+1, normalised on both sides
};

// The client hints Bedrock knows how to answer. Low-entropy hints are the three
// the platform sends by default; everything else is high-entropy and needs an
// explicit request.
enum class Hint {
  kUa,                  // Sec-CH-UA
  kUaMobile,            // Sec-CH-UA-Mobile
  kUaPlatform,          // Sec-CH-UA-Platform
  kUaArch,              // Sec-CH-UA-Arch
  kUaBitness,           // Sec-CH-UA-Bitness
  kUaModel,             // Sec-CH-UA-Model
  kUaFullVersionList,   // Sec-CH-UA-Full-Version-List
  kUaPlatformVersion,   // Sec-CH-UA-Platform-Version
  kDeviceMemory,        // Device-Memory
  kDpr,                 // DPR
  kViewportWidth,       // Viewport-Width
  kWidth,               // Width
  kRtt,                 // RTT
  kDownlink,            // Downlink
  kEct,                 // ECT
  kSaveData,            // Save-Data
  kPrefersColorScheme,  // Sec-CH-Prefers-Color-Scheme
  kMaxValue = kPrefersColorScheme,
};

const char* HintHeaderName(Hint hint);
bool IsHighEntropyHint(Hint hint);
// RTT / Downlink / ECT: a live measurement of *this* connection, which is a
// property of the user's network rather than of the page's layout needs.
bool IsNetworkQualityHint(Hint hint);
// The UA-identity hints -- architecture, bitness, model, platform version, full
// version list. These name the machine, and `docs/privacy/fingerprinting/client-hints.md`
// commits to the testable claim that no `Sec-CH-UA-*` beyond the low-entropy
// set appears on the wire from level 1 on. Kept apart from the layout hints
// (device memory, DPR, viewport width), which describe how to render a page and
// are answered with the normalised values the JS surface reports.
bool IsUaIdentityHint(Hint hint);

// What the device would report with no policy applied. Supplied by the caller
// so this component stays testable and free of platform code.
struct DeviceFacts {
  int hardware_concurrency = 8;
  int device_memory_gb = 8;
  double dpr = 1.0;
  privacy::Size window = {1280, 800};
  std::string platform = "Linux";
  std::string platform_version = "6.1.0";
  std::string architecture = "x86";
  std::string bitness = "64";
  std::string model;  // empty on desktop
  // Brand and version of the engine, supplied by the build rather than written
  // here: no vendor string is compiled into this file (`docs/IDENTITY.md`).
  std::string ua_brand = "Bedrock";
  std::string full_version = "0.0.1";
  std::string language = "en-GB";
  bool mobile = false;
  bool prefers_dark = true;
  bool save_data_enabled = false;
};

// One header the request will carry.
struct HeaderValue {
  std::string name;
  std::string value;
};

class RequestHeaderPolicy {
 public:
  explicit RequestHeaderPolicy(privacy::ProtectionController* controls);
  ~RequestHeaderPolicy();

  // --- Referrer -------------------------------------------------------------

  // The decision, for the UI and the event log.
  ReferrerScope ScopeFor(const OutgoingRequest& request) const;
  // The actual header value; empty string means "send no Referer header".
  std::string ReferrerFor(const OutgoingRequest& request) const;

  // True when the declared policy was refused for being looser than the floor.
  // The site privacy panel can then say so instead of pretending it applied.
  bool DeclaredPolicyRefused(const OutgoingRequest& request) const;

  // --- Client hints ---------------------------------------------------------

  // `accepted` is what the **top-level** document asked for via `Accept-CH`.
  // A hint requested by a third-party subresource's own response is not in it,
  // and cannot be: the browser decides, not the requester.
  std::vector<Hint> HintsFor(const OutgoingRequest& request,
                             const std::vector<Hint>& accepted) const;

  // Header list for a request, values already normalised for the level.
  std::vector<HeaderValue> HintHeadersFor(const OutgoingRequest& request,
                                          const std::vector<Hint>& accepted,
                                          const DeviceFacts& facts) const;

  // Value of one hint at the active level. Public because the DevTools privacy
  // panel shows the before/after pair.
  std::string HintValue(Hint hint, const DeviceFacts& facts) const;

  // `Accept-Language`, which is a client hint in everything but name. Delegates
  // to the language normalisation in fingerprint_policy so the header and
  // `navigator.languages` cannot disagree.
  std::string AcceptLanguage(const DeviceFacts& facts) const;

  // --- Configuration --------------------------------------------------------

  void set_fp_level(privacy::FpLevel level) { fp_level_ = level; }
  privacy::FpLevel fp_level() const { return fp_level_; }

  // Origin of a URL: "scheme://host[:port]/". Empty for a URL with no
  // hierarchical origin (data:, blob:, javascript:, about:), which is also the
  // referrer for such a document — an opaque origin has nothing to send.
  static std::string OriginOf(const std::string& url);
  // The full URL with credentials and fragment removed. A referrer has never
  // needed either, and both have leaked in real browsers.
  static std::string SanitizeUrl(const std::string& url);

 private:
  // The floor from the shields value for this site, then narrowed by the
  // declared policy if the declared policy is stricter.
  ReferrerScope FloorFor(const OutgoingRequest& request) const;
  ReferrerScope FromDeclared(const OutgoingRequest& request) const;

  privacy::ProtectionController* controls_;
  privacy::FpLevel fp_level_ = privacy::FpLevel::kBalanced;
};

}  // namespace net
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_NETWORK_REQUEST_HEADERS_H_
