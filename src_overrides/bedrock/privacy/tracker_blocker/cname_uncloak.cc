// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/tracker_blocker/cname_uncloak.h"

#include <algorithm>

namespace bedrock {
namespace blocking {

CnameUncloaker::CnameUncloaker() = default;
CnameUncloaker::~CnameUncloaker() = default;

// static
bool CnameUncloaker::Eligible(const Request& request) {
  if (request.host.empty() || request.etld1.empty()) {
    return false;
  }
  // A top-level document load is the page the user asked for. Uncloaking it
  // would mean deciding to abandon a navigation over a DNS alias.
  if (request.type == ResourceType::kDocument) {
    return false;
  }
  // Already third party: the filter lists, the party check and the cookie
  // policy all see it for what it is. Resolving would cost a lookup and change
  // no decision.
  if (request.third_party()) {
    return false;
  }
  // Cloaking needs a subdomain the site can point elsewhere. The apex itself
  // carries the site's own web server.
  return request.host != request.etld1;
}

// static
Request CnameUncloaker::ApplyAlias(const Request& request,
                                  const UncloakResult& alias) {
  Request aliased = request;
  if (!alias.uncloaked()) {
    return aliased;
  }
  aliased.host = alias.canonical_host;
  aliased.etld1 = alias.canonical_etld1;

  const size_t scheme_end = aliased.url.find("://");
  if (scheme_end != std::string::npos) {
    const size_t host_start = scheme_end + 3;
    const size_t host_end = aliased.url.find('/', host_start);
    const std::string authority =
        aliased.url.substr(host_start, host_end == std::string::npos
                                           ? std::string::npos
                                           : host_end - host_start);
    // Keep any port; only the name was cloaked.
    const size_t colon = authority.find(':');
    const std::string port =
        colon == std::string::npos ? "" : authority.substr(colon);
    aliased.url = aliased.url.substr(0, host_start) + alias.canonical_host +
                  port +
                  (host_end == std::string::npos ? "" : aliased.url.substr(host_end));
  }
  return aliased;
}

UncloakResult CnameUncloaker::Canonical(const Request& request) {
  UncloakResult result;
  if (!Eligible(request)) {
    result.status = UncloakStatus::kNotEligible;
    return result;
  }
  if (unavailable_) {
    result.status = UncloakStatus::kUnavailable;
    return result;
  }

  const auto it = cache_.find(request.host);
  if (it == cache_.end() || it->second.expires_at <= now_) {
    if (it != cache_.end()) {
      cache_.erase(it);
    }
    // Queue the name for the embedder, deduplicated. Dropping the name when
    // the queue is full is deliberate: the alternative is letting a page with
    // ten thousand generated subdomains decide how much memory this costs.
    if (pending_.size() < kMaxPendingHosts &&
        std::find(pending_.begin(), pending_.end(), request.host) ==
            pending_.end()) {
      pending_.push_back(request.host);
    }
    result.status = UncloakStatus::kUnknown;
    return result;
  }

  const Entry& entry = it->second;
  if (entry.canonical_host.empty()) {
    result.status = UncloakStatus::kNoAlias;
    return result;
  }
  if (entry.canonical_etld1 == request.etld1) {
    result.status = UncloakStatus::kSameSite;
    return result;
  }
  result.status = UncloakStatus::kUncloaked;
  result.canonical_host = entry.canonical_host;
  result.canonical_etld1 = entry.canonical_etld1;
  return result;
}

void CnameUncloaker::Record(const std::string& host,
                            const std::string& host_etld1,
                            const std::string& canonical_host,
                            const std::string& canonical_etld1,
                            uint32_t ttl_seconds) {
  if (host.empty()) {
    return;
  }
  // A full cache drops the oldest-expiring entry rather than refusing to learn
  // anything new; uncloaking that stops working on long sessions is worse than
  // one forgotten alias.
  if (cache_.size() >= kMaxCacheEntries && cache_.find(host) == cache_.end()) {
    auto oldest = cache_.begin();
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
      if (it->second.expires_at < oldest->second.expires_at) {
        oldest = it;
      }
    }
    cache_.erase(oldest);
  }

  Entry entry;
  entry.site_etld1 = host_etld1;
  if (canonical_host != host) {
    entry.canonical_host = canonical_host;
    entry.canonical_etld1 = canonical_etld1;
  }
  const uint32_t ttl =
      std::min(kMaxTtlSeconds, std::max(kMinTtlSeconds, ttl_seconds));
  entry.expires_at = now_ + ttl;
  cache_[host] = entry;

  pending_.erase(std::remove(pending_.begin(), pending_.end(), host),
                 pending_.end());
}

std::vector<std::string> CnameUncloaker::TakePendingHosts() {
  std::vector<std::string> taken;
  taken.swap(pending_);
  return taken;
}

void CnameUncloaker::SetUnavailable(bool unavailable) {
  unavailable_ = unavailable;
  if (unavailable) {
    pending_.clear();
  }
}

void CnameUncloaker::SetNow(uint64_t now_seconds) {
  now_ = now_seconds;
}

void CnameUncloaker::Clear() {
  cache_.clear();
  pending_.clear();
}

void CnameUncloaker::ForgetSite(const std::string& etld1) {
  for (auto it = cache_.begin(); it != cache_.end();) {
    // Both directions: what the site aliased, and what aliased *to* the site.
    if (it->second.site_etld1 == etld1 || it->second.canonical_etld1 == etld1) {
      it = cache_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace blocking
}  // namespace bedrock
