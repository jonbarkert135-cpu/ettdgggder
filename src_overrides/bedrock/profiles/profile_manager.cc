// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/profiles/profile_manager.h"

namespace bedrock {
namespace session {

std::string Profile::PathFor(ProfileData data) const {
  if (ephemeral) {
    // Never touches disk. Returning a path here would be the bug that leaves a
    // "temporary" profile on the filesystem.
    return "memory://" + id + "/" + ProfileManager::DataName(data);
  }
  return "profiles/" + id + "/" + ProfileManager::DataName(data);
}

ProfileManager::ProfileManager() = default;
ProfileManager::~ProfileManager() = default;

// static
ProfileManager ProfileManager::WithDefaults() {
  ProfileManager manager;
  manager.Create("personal", "Personal", ProfileKind::kPersonal);
  manager.Create("work", "Work", ProfileKind::kWork);
  manager.Create("school", "School", ProfileKind::kSchool);
  manager.SetActive("personal");
  return manager;
}

Profile* ProfileManager::Create(const std::string& id,
                                const std::string& name,
                                ProfileKind kind) {
  if (id.empty() || profiles_.count(id) != 0) {
    return nullptr;
  }
  Profile profile;
  profile.id = id;
  profile.name = name;
  profile.kind = kind;
  profile.ephemeral = kind == ProfileKind::kTemporary;
  if (profile.ephemeral) {
    profile.isolation = net::IsolationLevel::kEphemeralAll;
  }
  auto [it, inserted] = profiles_.emplace(id, std::move(profile));
  if (!active_) {
    active_ = &it->second;
  }
  return &it->second;
}

bool ProfileManager::Delete(const std::string& id) {
  if (profiles_.size() <= 1 || profiles_.count(id) == 0) {
    return false;  // there is always somewhere to browse
  }
  const bool was_active = active_ && active_->id == id;
  profiles_.erase(id);
  if (was_active) {
    active_ = &profiles_.begin()->second;
  }
  return true;
}

bool ProfileManager::SetActive(const std::string& id) {
  Profile* profile = Find(id);
  if (!profile) {
    return false;
  }
  active_ = profile;
  return true;
}

Profile* ProfileManager::Find(const std::string& id) {
  auto it = profiles_.find(id);
  return it == profiles_.end() ? nullptr : &it->second;
}

const Profile* ProfileManager::Find(const std::string& id) const {
  auto it = profiles_.find(id);
  return it == profiles_.end() ? nullptr : &it->second;
}

std::vector<std::string> ProfileManager::ProfileIds() const {
  std::vector<std::string> ids;
  for (const auto& [id, profile] : profiles_) {
    (void)profile;
    ids.push_back(id);
  }
  return ids;
}

// static
bool ProfileManager::SharesData(const Profile& a,
                                const Profile& b,
                                ProfileData data) {
  (void)data;
  // Same profile: trivially yes. Different profiles: never, for any data type.
  // Bookmarks and passwords included — "just sync the bookmarks" is how a work
  // profile leaks into a personal one.
  return a.id == b.id;
}

// static
const char* ProfileManager::DataName(ProfileData data) {
  switch (data) {
    case ProfileData::kCookies:
      return "cookies";
    case ProfileData::kStorage:
      return "storage";
    case ProfileData::kHistory:
      return "history";
    case ProfileData::kBookmarks:
      return "bookmarks";
    case ProfileData::kExtensions:
      return "extensions";
    case ProfileData::kPermissions:
      return "permissions";
    case ProfileData::kPrivacySettings:
      return "privacy-settings";
    case ProfileData::kDownloads:
      return "downloads";
    case ProfileData::kPasswords:
      return "passwords";
  }
  return "";
}

// static
const char* ProfileManager::KindName(ProfileKind kind) {
  switch (kind) {
    case ProfileKind::kPersonal:
      return "Personal";
    case ProfileKind::kWork:
      return "Work";
    case ProfileKind::kSchool:
      return "School";
    case ProfileKind::kTemporary:
      return "Temporary";
    case ProfileKind::kCustom:
      return "Custom";
  }
  return "";
}

}  // namespace session
}  // namespace bedrock
