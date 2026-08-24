// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_TRACKER_BLOCKER_CNAME_UNCLOAK_H_
#define BEDROCK_PRIVACY_TRACKER_BLOCKER_CNAME_UNCLOAK_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "bedrock/privacy/tracker_blocker/filter_engine.h"

// CNAME uncloaking (part of feature `tracker_protection`).
//
// A tracker asks the site to publish `metrics.shop.test` as a CNAME to
// `collect.tracker.test`. The request then looks first-party to every filter
// list, to the party check, and to the cookie policy — so the tracker gets a
// first-party cookie and the request is never examined. Resolving the alias
// before matching is the only way to see it.
//
// Four decisions shape this component. Each is a refusal to do the obvious
// thing, and each has a test.
//
// 1. **No DNS lookup on the decision path.** `Canonical()` reads the cache and
//    nothing else. The blocking pipeline has a 30 microsecond budget; a DNS
//    round trip is four to five orders of magnitude larger, and stalling every
//    request behind a resolver would be paid by every user on every page in
//    exchange for catching a tracker on the *first* request only. A miss
//    records the name as pending, the embedder resolves it out of band, and
//    the next request for that host is matched with the alias. The honest
//    consequence — the first request to an unseen cloaked host is not blocked —
//    is written in the design doc and in the feature disclosure, not hidden.
//
// 2. **The user's resolver, or nothing.** This component never resolves
//    anything itself. It hands pending names to the embedder, which must use
//    the resolver the user chose in `privacy/network/dns_settings`. An
//    uncloaking lookup that reached around a fail-closed DNS setting, or that
//    fell back to plaintext DNS, would be a privacy bug committed in the name
//    of privacy. `SetUnavailable()` exists so a browser with no usable
//    resolver degrades to "no uncloaking" instead of quietly leaking.
//
// 3. **Uncloaking can only block, never allow.** The alias is used to re-match
//    against the filter lists; a decision to allow is never taken because of
//    it. So a tracker cannot publish a CNAME into a whitelisted domain to buy
//    itself an exception.
//
// 4. **Only where cloaking is possible.** Names that already look third party
//    are matched normally, so uncloaking would buy nothing; a bare eTLD+1 is
//    not asked about (a site's apex is not somebody else's tracker); and
//    top-level document loads are never uncloaked, because retargeting a
//    navigation is not this component's job.

namespace bedrock {
namespace blocking {

// Why a host was, or was not, uncloaked. Reported so the privacy panel can
// explain the outcome instead of showing an unexplained block.
enum class UncloakStatus {
  kNotEligible,   // first-party matching already sees this host
  kUnknown,       // nothing cached yet; the name is now pending
  kNoAlias,       // resolved, and the host is its own canonical name
  kSameSite,      // resolved to an alias inside the same eTLD+1 — not cloaking
  kUncloaked,     // resolved to another site's name; matching should use it
  kUnavailable,   // no usable resolver, so no lookup was made
};

struct UncloakResult {
  UncloakStatus status = UncloakStatus::kUnknown;
  std::string canonical_host;   // set only when status == kUncloaked
  std::string canonical_etld1;  // set only when status == kUncloaked

  bool uncloaked() const { return status == UncloakStatus::kUncloaked; }
};

class CnameUncloaker {
 public:
  CnameUncloaker();
  ~CnameUncloaker();

  // Cache-only. Never blocks, never resolves. On a miss the host is queued in
  // `PendingHosts()` for the embedder to resolve with the user's resolver.
  UncloakResult Canonical(const Request& request);

  // Records a resolver answer. `canonical_host` equal to `host` (or empty)
  // means "no alias". `canonical_etld1` is supplied by the caller because
  // eTLD+1 needs the public suffix list, which lives in the Chromium tree.
  void Record(const std::string& host,
              const std::string& host_etld1,
              const std::string& canonical_host,
              const std::string& canonical_etld1,
              uint32_t ttl_seconds);

  // Names waiting for a lookup, oldest first, then cleared. The embedder is
  // expected to resolve these and call Record().
  std::vector<std::string> TakePendingHosts();

  // No resolver is usable (fail-closed DNS with the resolver unreachable, for
  // instance). Every later Canonical() reports kUnavailable and nothing is
  // queued: no lookup is better than a lookup the user did not agree to.
  void SetUnavailable(bool unavailable);
  bool unavailable() const { return unavailable_; }

  // Cache maintenance. `now_seconds` is injected rather than read from a clock
  // so the tests can age entries without sleeping.
  void SetNow(uint64_t now_seconds);
  void Clear();                              // New Identity, "forget site"
  void ForgetSite(const std::string& etld1);  // one site only
  size_t cache_size() const { return cache_.size(); }

  // Bounds. A cache without a ceiling is a memory leak with a hit rate, and
  // the pending list is fed by untrusted page content.
  static constexpr size_t kMaxCacheEntries = 4096;
  static constexpr size_t kMaxPendingHosts = 256;
  // Resolver TTLs are honoured, but clamped: a tracker controls its own TTL,
  // so a one-second TTL must not turn uncloaking into a lookup per request,
  // and a one-year TTL must not outlive the alias it describes.
  static constexpr uint32_t kMinTtlSeconds = 60;
  static constexpr uint32_t kMaxTtlSeconds = 24 * 60 * 60;

  // True when uncloaking could change the outcome for this request.
  static bool Eligible(const Request& request);

  // The same request as the filter lists would see it if the site had not
  // hidden the tracker behind an alias: host, eTLD+1 and the host inside the
  // URL, all replaced together. Rewriting only the `host` field would leave
  // `||collect.tracker.test^` unable to match the URL it is meant to match —
  // which is the quiet way this whole feature would end up doing nothing.
  static Request ApplyAlias(const Request& request, const UncloakResult& alias);

 private:
  struct Entry {
    std::string canonical_host;   // empty = no alias
    std::string canonical_etld1;
    std::string site_etld1;       // the queried host's own eTLD+1
    uint64_t expires_at = 0;
  };

  std::unordered_map<std::string, Entry> cache_;
  std::vector<std::string> pending_;
  uint64_t now_ = 0;
  bool unavailable_ = false;
};

}  // namespace blocking
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_TRACKER_BLOCKER_CNAME_UNCLOAK_H_
