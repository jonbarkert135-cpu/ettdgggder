// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/session/profile_manager.h"

#include <iostream>
#include <set>
#include <string>

namespace {

using namespace bedrock::session;  // NOLINT — test-local convenience
using bedrock::net::IsolationLevel;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

}  // namespace

int main() {
  ProfileManager manager = ProfileManager::WithDefaults();
  Check(manager.size() == 3, "a fresh install has Personal, Work and School");
  Check(manager.active().id == "personal", "Personal is active");

  Profile* work = manager.Find("work");
  Profile* personal = manager.Find("personal");
  Check(work && personal, "the default profiles exist");

  // Nothing is shared. Not history, not bookmarks, not passwords.
  for (int i = 0; i <= static_cast<int>(ProfileData::kMaxValue); ++i) {
    const ProfileData data = static_cast<ProfileData>(i);
    Check(!ProfileManager::SharesData(*personal, *work, data),
          std::string("profiles never share ") + ProfileManager::DataName(data));
    Check(personal->PathFor(data) != work->PathFor(data),
          std::string("separate storage path for ") +
              ProfileManager::DataName(data));
    Check(ProfileManager::SharesData(*personal, *personal, data),
          "a profile shares data with itself");
  }

  // Every data type has a distinct path within a profile.
  {
    std::set<std::string> paths;
    for (int i = 0; i <= static_cast<int>(ProfileData::kMaxValue); ++i) {
      paths.insert(personal->PathFor(static_cast<ProfileData>(i)));
    }
    Check(paths.size() ==
              static_cast<size_t>(ProfileData::kMaxValue) + 1,
          "no two data types collide on one path");
  }

  // Temporary profiles never touch disk.
  {
    Profile* temp = manager.Create("temp1", "Temporary", ProfileKind::kTemporary);
    Check(temp && temp->ephemeral, "temporary profiles are ephemeral");
    Check(temp->isolation == IsolationLevel::kEphemeralAll,
          "and keep no storage at all");
    for (int i = 0; i <= static_cast<int>(ProfileData::kMaxValue); ++i) {
      const std::string path = temp->PathFor(static_cast<ProfileData>(i));
      Check(path.compare(0, 9, "memory://") == 0,
            "temporary profile data never gets a disk path: " + path);
    }
  }

  // Custom profiles, duplicate ids, switching, deletion.
  {
    Check(manager.Create("client-x", "Client X", ProfileKind::kCustom),
          "a custom profile can be created");
    Check(!manager.Create("client-x", "Again", ProfileKind::kCustom),
          "duplicate ids are refused");
    Check(manager.SetActive("client-x") && manager.active().id == "client-x",
          "switching profiles works");
    Check(!manager.SetActive("nope"), "switching to a missing profile fails");

    Check(manager.Delete("client-x"), "a profile can be deleted");
    Check(manager.active().id != "client-x",
          "deleting the active profile moves the user somewhere real");
    Check(!manager.Find("client-x"), "and it is gone");
  }

  // The last profile cannot be deleted: there must always be somewhere to go.
  {
    ProfileManager solo;
    solo.Create("only", "Only", ProfileKind::kPersonal);
    Check(!solo.Delete("only"), "the last profile is protected");
    Check(solo.size() == 1, "and still there");
  }

  // Every kind and data type has a name for the UI.
  for (ProfileKind kind :
       {ProfileKind::kPersonal, ProfileKind::kWork, ProfileKind::kSchool,
        ProfileKind::kTemporary, ProfileKind::kCustom}) {
    Check(std::string(ProfileManager::KindName(kind)).size() > 3,
          "profile kind has a name");
  }

  if (failures == 0) {
    std::cout << "profile_manager_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
