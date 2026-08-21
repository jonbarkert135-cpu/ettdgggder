// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_NET_STORAGE_ISOLATION_H_
#define BEDROCK_NET_STORAGE_ISOLATION_H_

#include <string>
#include <vector>

// Cookie and storage isolation (roadmap item 15).
//
// One key decides where *every* storage backend reads and writes:
//
//     StorageKey = (origin, top-level site, is-cross-site)
//
// This is deliberately the same shape Chromium's `blink::StorageKey` /
// `net::NetworkIsolationKey` use, because agreeing with the engine is what
// makes partitioning hold across cookies, localStorage, IndexedDB, cache,
// Service Workers, DNS and connection pools instead of only the parts we
// remembered to patch. Every backend in the table below derives its key here;
// a backend that computes its own key is the bug this file exists to prevent.
//
// The property we are buying: a third party embedded on two different sites
// gets two unrelated storage areas, so it cannot recognise the user across
// them. Not "cookies are blocked" — cookies still work, they are just no
// longer a shared identifier.

namespace bedrock {
namespace net {

enum class StorageType {
  kCookies,
  kLocalStorage,
  kSessionStorage,
  kIndexedDb,
  kCacheStorage,       // CacheStorage / Cache API
  kHttpCache,
  kServiceWorker,
  kSharedWorker,
  kBlobStorage,
  kFileSystem,
  kWebSql,
  kDnsCache,           // resolver + socket pools: timing side channels
  kHstsState,          // the classic supercookie
  kMaxValue = kHstsState,
};

// How long a partitioned area survives. Ephemeral areas are destroyed when the
// last tab of the top-level site closes.
enum class Lifetime {
  kPersistent,
  kEphemeral,
};

struct StorageKey {
  std::string origin;         // the frame's origin
  std::string top_level_site; // eTLD+1 of the top-level document
  bool cross_site = false;    // origin's site != top_level_site

  bool operator==(const StorageKey& other) const {
    return origin == other.origin && top_level_site == other.top_level_site &&
           cross_site == other.cross_site;
  }
  bool operator!=(const StorageKey& other) const { return !(*this == other); }

  // Stable string form, used as the on-disk directory name and in tests.
  std::string Serialize() const;
};

// Profile-wide posture. Site-level overrides come from the Protection
// Controller (docs/design/011); this object holds the profile default.
enum class IsolationLevel {
  // Partition everything, keep third-party storage across restarts. This is
  // the floor: there is no level that turns partitioning off, because
  // unpartitioned third-party storage is the tracking mechanism itself.
  kStandard,
  // Partition everything and make third-party storage ephemeral.
  kStrict,
  // Strict, plus first-party storage is cleared when the site is closed
  // (private windows, and the fingerprinting Maximum level).
  kEphemeralAll,
};

class StorageIsolation {
 public:
  explicit StorageIsolation(IsolationLevel level = IsolationLevel::kStandard);
  ~StorageIsolation();

  // The key every backend must use. `origin_site` is the eTLD+1 of `origin`.
  StorageKey KeyFor(const std::string& origin,
                    const std::string& origin_site,
                    const std::string& top_level_site) const;

  // True if this storage type is partitioned by top-level site. Everything is;
  // the function exists so the answer lives in one place and the test can
  // assert that no type ever returns false.
  static bool IsPartitioned(StorageType type);

  // Lifetime of a given area under the current level.
  Lifetime LifetimeFor(StorageType type, const StorageKey& key) const;

  // Session storage is per-tab by definition, so it is never shared even
  // within one site. Kept explicit because "sessionStorage is already safe" is
  // an assumption worth pinning with a test.
  static bool IsPerTab(StorageType type) {
    return type == StorageType::kSessionStorage;
  }

  // Data the user asked to delete for one site: every key whose top-level site
  // matches, in every backend. Clearing "example.com" must also drop what
  // third parties stored *under* example.com, or deletion is theatre.
  std::vector<StorageKey> KeysToClearForSite(
      const std::vector<StorageKey>& all_keys,
      const std::string& site) const;

  // Sites the user allowed to keep unpartitioned access to their own state in
  // a third-party context (the Storage Access API grant), scoped to one
  // top-level site — never global.
  void GrantStorageAccess(const std::string& origin,
                          const std::string& top_level_site);
  void RevokeStorageAccess(const std::string& origin,
                           const std::string& top_level_site);
  bool HasStorageAccess(const std::string& origin,
                        const std::string& top_level_site) const;

  IsolationLevel level() const { return level_; }
  void set_level(IsolationLevel level) { level_ = level; }

  // Human-readable explanation for the settings page. Every mechanism must be
  // explainable (roadmap item 8).
  static const char* Explain(StorageType type);

 private:
  IsolationLevel level_;
  std::vector<std::string> grants_;  // "origin|top_level_site"
};

}  // namespace net
}  // namespace bedrock

#endif  // BEDROCK_NET_STORAGE_ISOLATION_H_
