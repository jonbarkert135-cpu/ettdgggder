// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/platform/platform_support.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace bedrock {
namespace platform {
namespace {

std::string Lowercase(const std::string& value) {
  std::string out = value;
  for (char& character : out)
    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  return out;
}

// Item 62: every platform answers for every integration point. A gap in the
// table is a gap in the port that nobody notices until a user reports it.
void TestEveryPlatformCoversEveryIntegrationPoint() {
  const PlatformSupport support;
  for (const PlatformInfo& info : support.Platforms()) {
    for (IntegrationPoint point : PlatformSupport::AllPoints()) {
      const IntegrationRequirement* requirement =
          support.Requirement(info.platform, point);
      if (requirement == nullptr) {
        std::printf("  %s has no entry for %s\n", info.name,
                    PlatformSupport::Name(point));
      }
      assert(requirement != nullptr);
      assert(std::string(requirement->requirement).size() > 30);
      assert(std::string(requirement->failure_mode).size() > 20);
    }
  }
}

// Windows and Linux are supported; macOS is best effort and says why. The test
// exists so a later "macOS supported!" commit has to change a reason too.
void TestSupportTiersAreHonest() {
  const PlatformSupport support;
  assert(support.IsSupported(Platform::kWindows));
  assert(support.IsSupported(Platform::kLinux));
  assert(!support.IsSupported(Platform::kMacOS));
  for (const PlatformInfo& info : support.Platforms())
    assert(std::string(info.tier_reason).size() > 20);
}

// Item 64: Wayland and X11 are both first class, and neither is optional.
void TestLinuxSupportsBothDisplayBackends() {
  const PlatformSupport support;
  const PlatformInfo* linux_info = support.Info(Platform::kLinux);
  assert(linux_info != nullptr);
  const std::vector<DisplayBackend>& backends = linux_info->backends;
  assert(std::find(backends.begin(), backends.end(), DisplayBackend::kWayland) !=
         backends.end());
  assert(std::find(backends.begin(), backends.end(), DisplayBackend::kX11) !=
         backends.end());
}

// "Do not assume a specific desktop environment." A desktop environment named
// in a *requirement* is an assumption; naming one in a failure mode is a
// warning about that assumption, which is the opposite.
void TestNoRequirementAssumesADesktopEnvironment() {
  const PlatformSupport support;
  const char* kDesktops[] = {"gnome", "kde",  "xfce",     "cinnamon",
                             "mate",  "lxqt", "plasma",   "unity",
                             "i3",    "sway", "budgie"};
  for (const IntegrationRequirement& requirement :
       support.Requirements(Platform::kLinux)) {
    const std::string text = Lowercase(requirement.requirement);
    for (const char* desktop : kDesktops) {
      if (text.find(desktop) != std::string::npos) {
        std::printf("  %s requirement names %s\n",
                    PlatformSupport::Name(requirement.point), desktop);
      }
      assert(text.find(desktop) == std::string::npos);
    }
  }
}

// Item 64 asks for package formats, plural — and a distribution-agnostic one,
// because that is the only format guaranteed to work everywhere.
void TestLinuxPackagingIsPluralAndExplainsItself() {
  const PlatformSupport support;
  int produced = 0;
  bool has_plain_archive = false;
  for (const PackageFormat& format : support.LinuxPackageFormats()) {
    assert(std::string(format.note).size() > 20);
    if (!format.produced)
      continue;
    ++produced;
    if (std::string(format.name).find("tar") != std::string::npos)
      has_plain_archive = true;
  }
  assert(produced >= 4);
  assert(has_plain_archive);
}

// The abstraction rule in data form: anything Chromium owns is described as
// something we must not break, and anything we own is a surface of ours.
void TestOwnershipIsAssignedForEveryRequirement() {
  const PlatformSupport support;
  int bedrock_owned = 0;
  for (const PlatformInfo& info : support.Platforms()) {
    for (const IntegrationRequirement& requirement :
         support.Requirements(info.platform)) {
      if (requirement.owner == Owner::kBedrock)
        ++bedrock_owned;
    }
  }
  // If the overlay owned nothing, this table would be documentation of
  // Chromium rather than a specification of Bedrock.
  assert(bedrock_owned > 10);
}

void TestNamesAreUniqueAndFilled() {
  std::set<std::string> names;
  for (IntegrationPoint point : PlatformSupport::AllPoints()) {
    const std::string name = PlatformSupport::Name(point);
    assert(name != "unknown");
    assert(names.insert(name).second);
  }
  assert(names.size() == PlatformSupport::AllPoints().size());
}

}  // namespace
}  // namespace platform
}  // namespace bedrock

int main() {
  bedrock::platform::TestEveryPlatformCoversEveryIntegrationPoint();
  bedrock::platform::TestSupportTiersAreHonest();
  bedrock::platform::TestLinuxSupportsBothDisplayBackends();
  bedrock::platform::TestNoRequirementAssumesADesktopEnvironment();
  bedrock::platform::TestLinuxPackagingIsPluralAndExplainsItself();
  bedrock::platform::TestOwnershipIsAssignedForEveryRequirement();
  bedrock::platform::TestNamesAreUniqueAndFilled();
  std::printf("platform_support_test: all assertions passed\n");
  return 0;
}
