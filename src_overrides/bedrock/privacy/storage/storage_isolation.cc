// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/storage/storage_isolation.h"

#include <algorithm>
#include <string>
#include <vector>

namespace bedrock {
namespace net {

std::string StorageKey::Serialize() const {
  // The top-level site is always part of the key, including for same-site
  // storage. Omitting it "because it is redundant" is how a partition silently
  // becomes shared after a refactor.
  return origin + "^" + top_level_site + (cross_site ? "^3p" : "^1p");
}

StorageIsolation::StorageIsolation(IsolationLevel level) : level_(level) {}
StorageIsolation::~StorageIsolation() = default;

StorageKey StorageIsolation::KeyFor(const std::string& origin,
                                    const std::string& origin_site,
                                    const std::string& top_level_site) const {
  StorageKey key;
  key.origin = origin;
  key.top_level_site = top_level_site;
  key.cross_site = origin_site != top_level_site;
  if (key.cross_site && HasStorageAccess(origin, top_level_site)) {
    // A Storage Access API grant is *scoped*: the origin gets its first-party
    // area back, but only under this one top-level site. It never becomes a
    // cross-site identifier again.
    key.cross_site = false;
  }
  return key;
}

// static
bool StorageIsolation::IsPartitioned(StorageType type) {
  (void)type;
  // Every backend, without exception. A single unpartitioned one — the HTTP
  // cache, HSTS, the DNS cache — is enough to re-link a user across sites, and
  // each of those has been used for exactly that in published research.
  return true;
}

Lifetime StorageIsolation::LifetimeFor(StorageType type,
                                       const StorageKey& key) const {
  if (type == StorageType::kSessionStorage) {
    return Lifetime::kEphemeral;  // per tab, by definition
  }
  switch (level_) {
    case IsolationLevel::kStandard:
      return Lifetime::kPersistent;
    case IsolationLevel::kStrict:
      return key.cross_site ? Lifetime::kEphemeral : Lifetime::kPersistent;
    case IsolationLevel::kEphemeralAll:
      return Lifetime::kEphemeral;
  }
  return Lifetime::kPersistent;
}

std::vector<StorageKey> StorageIsolation::KeysToClearForSite(
    const std::vector<StorageKey>& all_keys,
    const std::string& site) const {
  std::vector<StorageKey> matches;
  for (const StorageKey& key : all_keys) {
    if (key.top_level_site == site) {
      matches.push_back(key);
    }
  }
  return matches;
}

void StorageIsolation::GrantStorageAccess(const std::string& origin,
                                          const std::string& top_level_site) {
  const std::string grant = origin + "|" + top_level_site;
  if (std::find(grants_.begin(), grants_.end(), grant) == grants_.end()) {
    grants_.push_back(grant);
  }
}

void StorageIsolation::RevokeStorageAccess(const std::string& origin,
                                           const std::string& top_level_site) {
  const std::string grant = origin + "|" + top_level_site;
  grants_.erase(std::remove(grants_.begin(), grants_.end(), grant),
                grants_.end());
}

bool StorageIsolation::HasStorageAccess(
    const std::string& origin,
    const std::string& top_level_site) const {
  const std::string grant = origin + "|" + top_level_site;
  return std::find(grants_.begin(), grants_.end(), grant) != grants_.end();
}

// static
const char* StorageIsolation::Explain(StorageType type) {
  switch (type) {
    case StorageType::kCookies:
      return "Cookies set by an embedded site are stored separately for each "
             "site you visit, so it cannot recognise you across them.";
    case StorageType::kLocalStorage:
      return "Local storage is kept per site, so embedded content cannot leave "
             "an identifier that follows you.";
    case StorageType::kSessionStorage:
      return "Session storage is per tab and is deleted when the tab closes.";
    case StorageType::kIndexedDb:
      return "Databases created by embedded content are separate for each site "
             "you visit.";
    case StorageType::kCacheStorage:
      return "Offline caches created by embedded content are separate per site.";
    case StorageType::kHttpCache:
      return "The page cache is split per site. Without this, a site can tell "
             "which other sites you visited by timing cached files.";
    case StorageType::kServiceWorker:
      return "Background workers registered by embedded content run separately "
             "for each site, and cannot share data between them.";
    case StorageType::kSharedWorker:
      return "Shared workers are limited to one site, so two sites cannot use "
             "one to talk to each other.";
    case StorageType::kBlobStorage:
      return "Temporary file objects are scoped to the site that created them.";
    case StorageType::kFileSystem:
      return "Web file storage is separate per site.";
    case StorageType::kWebSql:
      return "Legacy web databases are separate per site.";
    case StorageType::kDnsCache:
      return "Address lookups and reused connections are kept per site, so one "
             "site cannot detect connections made by another.";
    case StorageType::kHstsState:
      return "The record of which sites required HTTPS is kept per site. It is "
             "a known way to store a hidden identifier.";
  }
  return "";
}

}  // namespace net
}  // namespace bedrock
