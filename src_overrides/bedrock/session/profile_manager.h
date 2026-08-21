// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_SESSION_PROFILE_MANAGER_H_
#define BEDROCK_SESSION_PROFILE_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "bedrock/net/storage_isolation.h"
#include "bedrock/privacy/fingerprint_policy.h"

// Profiles (roadmap item 21).
//
// A profile owns *everything*: cookies, storage, history, bookmarks,
// extensions, permissions and privacy settings. Nothing is shared between
// profiles by default — not "mostly nothing", nothing. The moment one data
// type is shared "for convenience", the separation the user relied on is gone
// and they have no way to tell.
//
// Work and Personal are not a UI grouping; they are separate data roots.

namespace bedrock {
namespace session {

enum class ProfileKind {
  kPersonal,
  kWork,
  kSchool,
  kTemporary,  // exists in memory, gone when closed
  kCustom,
};

// Every data type a profile owns. Used by the test that asserts no type is
// shared across profiles.
enum class ProfileData {
  kCookies,
  kStorage,
  kHistory,
  kBookmarks,
  kExtensions,
  kPermissions,
  kPrivacySettings,
  kDownloads,
  kPasswords,
  kMaxValue = kPasswords,
};

struct Profile {
  std::string id;    // stable, used as the data directory name
  std::string name;  // user-visible
  ProfileKind kind = ProfileKind::kCustom;
  bool ephemeral = false;  // temporary profiles keep nothing
  privacy::FpLevel fingerprint_level = privacy::FpLevel::kBalanced;
  net::IsolationLevel isolation = net::IsolationLevel::kStandard;

  // Where a data type lives for this profile.
  std::string PathFor(ProfileData data) const;
};

class ProfileManager {
 public:
  ProfileManager();
  ~ProfileManager();

  // Creates the three named profiles a fresh install starts with. Personal is
  // active; the others exist but hold nothing until used.
  static ProfileManager WithDefaults();

  // Returns nullptr if the id already exists.
  Profile* Create(const std::string& id,
                  const std::string& name,
                  ProfileKind kind);

  bool Delete(const std::string& id);  // false for the last remaining profile
  bool SetActive(const std::string& id);

  Profile* Find(const std::string& id);
  const Profile* Find(const std::string& id) const;
  const Profile& active() const { return *active_; }

  std::vector<std::string> ProfileIds() const;
  size_t size() const { return profiles_.size(); }

  // True if the two profiles can ever see each other's copy of `data`.
  // Always false; the function exists so the rule has one home and a test.
  static bool SharesData(const Profile& a, const Profile& b, ProfileData data);

  static const char* DataName(ProfileData data);
  static const char* KindName(ProfileKind kind);

 private:
  std::map<std::string, Profile> profiles_;
  Profile* active_ = nullptr;
};

}  // namespace session
}  // namespace bedrock

#endif  // BEDROCK_SESSION_PROFILE_MANAGER_H_
