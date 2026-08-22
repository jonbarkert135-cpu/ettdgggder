// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PROFILES_NEW_IDENTITY_H_
#define BEDROCK_PROFILES_NEW_IDENTITY_H_

#include <cstdint>
#include <string>
#include <vector>

#include "bedrock/session/browsing_mode.h"

// "New Identity" — the controlled session reset (roadmap items 20 and 22).
//
// The design constraint is honesty, not thoroughness. A reset that claims to
// erase your past is a lie: the sites you visited still have their logs, your
// network still saw the connections, and the account you signed into still
// knows who you are. So this class ships two lists, always shown together:
// what it clears, and what it cannot.
//
// The same machinery runs when a private window closes (item 20), with one
// difference: bookmarks, downloads on disk and the user's settings are never
// touched by either.

namespace bedrock {
namespace session {

enum class ClearTarget {
  kCookies,
  kSiteStorage,        // localStorage, IndexedDB, CacheStorage, FileSystem
  kHttpCache,
  kServiceWorkers,
  kTemporaryPermissions,
  kFormData,
  kSessionHistory,     // tabs and their back/forward entries
  kLocalHistoryEntries,
  kNetworkState,       // sockets, DNS cache, TLS/HTTP2 sessions
  kTorCircuits,
  kMediaDeviceSalts,   // so device ids do not survive the reset
  kFingerprintSeed,    // new per-session secret: new canvas/audio values
  kMaxValue = kFingerprintSeed,
};

// Things a reset explicitly does NOT do. Shown next to the list above.
enum class Preserved {
  kBookmarks,
  kSettings,
  kSavedPasswords,
  kInstalledExtensions,
  kDownloadedFiles,
  kMaxValue = kDownloadedFiles,
};

struct ResetPlan {
  std::vector<ClearTarget> targets;
  std::vector<Preserved> preserved;
  bool closes_tabs = true;
};

struct ResetReport {
  std::vector<ClearTarget> cleared;
  std::vector<ClearTarget> failed;  // reported, never hidden
  uint64_t new_epoch = 0;
  bool complete() const { return failed.empty(); }
};

class NewIdentity {
 public:
  explicit NewIdentity(BrowsingModeController* modes);
  ~NewIdentity();

  // What a "New Identity" will do, for the confirmation dialog. The user sees
  // this *before* anything happens — the whole point of item 22.
  static ResetPlan PlanForNewIdentity();

  // What closing a private window does (item 20). Same machinery, and it never
  // touches bookmarks or settings.
  static ResetPlan PlanForPrivateWindowClose();

  // Runs a plan. `unsupported` lets the caller declare targets the platform
  // cannot honour (a shared OS DNS cache, for example); those are reported as
  // failed instead of silently counted as cleared.
  ResetReport Execute(const ResetPlan& plan,
                      const std::vector<ClearTarget>& unsupported = {});

  static const char* Describe(ClearTarget target);
  static const char* Describe(Preserved preserved);

  // The sentence shown under the two lists. Says what a reset cannot undo.
  static const char* Caveat();

  // Every user-visible string, for the honesty test.
  static std::vector<std::string> AllUserVisibleStrings();

 private:
  BrowsingModeController* modes_;
};

}  // namespace session
}  // namespace bedrock

#endif  // BEDROCK_PROFILES_NEW_IDENTITY_H_
