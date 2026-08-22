// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/extensions/extension_registry.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace bedrock::extensions;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

bool Has(const std::vector<Capability>& caps, Capability capability) {
  return std::find(caps.begin(), caps.end(), capability) != caps.end();
}

}  // namespace

int main() {
  // The warning roadmap item 23 asks for, generated from the manifest.
  {
    const Disclosure all_sites =
        ExtensionRegistry::Analyze({"storage"}, {"<all_urls>"}, true);
    Check(Has(all_sites.capabilities, Capability::kAllSiteAccess),
          "<all_urls> is recognised as access to every site");
    Check(all_sites.headline_warning.find("every website") != std::string::npos,
          "and the headline says the extension can read every site's data");
    Check(all_sites.network_access && all_sites.persistent_storage &&
              all_sites.background_activity,
          "network, storage and background activity are all disclosed");
    Check(ExtensionRegistry::Risk(all_sites) == RiskLevel::kHigh,
          "all-site access is high risk");
  }

  // The dangerous ones outrank "all sites".
  {
    const Disclosure debugger =
        ExtensionRegistry::Analyze({"debugger"}, {}, false);
    Check(ExtensionRegistry::Risk(debugger) == RiskLevel::kCritical,
          "debugger access is critical");
    Check(debugger.headline_warning.find("sign in") != std::string::npos,
          "and the warning is concrete about what that means");
    Check(ExtensionRegistry::Risk(
              ExtensionRegistry::Analyze({"nativeMessaging"}, {}, false)) ==
              RiskLevel::kCritical,
          "native messaging is critical");
    Check(ExtensionRegistry::Risk(
              ExtensionRegistry::Analyze({"proxy"}, {}, false)) ==
              RiskLevel::kCritical,
          "proxy control is critical");
  }

  // A narrow extension is not scary, and the UI should not pretend it is.
  {
    const Disclosure narrow =
        ExtensionRegistry::Analyze({"activeTab"}, {}, false);
    Check(ExtensionRegistry::Risk(narrow) == RiskLevel::kLow,
          "activeTab-only is low risk");
    Check(narrow.headline_warning.empty(),
          "no warning where there is nothing to warn about");
  }

  // Unknown permissions are surfaced, never dropped.
  {
    std::vector<std::string> unknown;
    ExtensionRegistry::Analyze({"storage", "someFuturePermission"}, {}, false,
                               &unknown);
    Check(unknown.size() == 1 && unknown[0] == "someFuturePermission",
          "an unrecognised permission is reported verbatim");
  }

  // Install / disable / remove.
  ExtensionRegistry registry("personal");
  const Disclosure adblock =
      ExtensionRegistry::Analyze({"declarativeNetRequest", "storage"},
                                 {"<all_urls>"}, true);
  {
    Check(!registry.Install("x", "X", "1.0", adblock, /*user_confirmed=*/false),
          "an unconfirmed install does not happen at all");
    Extension* extension =
        registry.Install("x", "X", "1.0", adblock, /*user_confirmed=*/true);
    Check(extension != nullptr, "a confirmed install works");
    Check(!registry.Install("x", "X", "1.0", adblock, true),
          "duplicate ids are refused");
    Check(extension->storage_path.find("profiles/personal/extensions/x") !=
              std::string::npos,
          "extension storage lives inside the profile, not beside it");
    Check(registry.SetEnabled("x", false) &&
              registry.Find("x")->state == State::kDisabled,
          "an extension can be disabled");
    Check(registry.SetEnabled("x", true), "and re-enabled");
    Check(!registry.SetEnabled("missing", false),
          "an unknown id reports failure");
  }

  // Private windows are opt-in per extension.
  {
    Check(!registry.AllowedInPrivateWindows("x"),
          "extensions do not run in private windows by default");
    registry.SetAllowedInPrivateWindows("x", true);
    Check(registry.AllowedInPrivateWindows("x"), "the user can allow it");
  }

  // The user can narrow host access below what the manifest asked for.
  {
    Check(registry.HostAccess("x") == adblock.host_patterns,
          "host access starts at the manifest's patterns");
    registry.SetHostAccess("x", {"https://news.test/*"});
    Check(registry.HostAccess("x").size() == 1,
          "the user's narrowing replaces the manifest patterns");
  }

  // An update that asks for more lands disabled, pending review.
  {
    const Disclosure grown =
        ExtensionRegistry::Analyze({"declarativeNetRequest", "storage",
                                    "cookies"},
                                   {"<all_urls>"}, true);
    Check(registry.Update("x", "2.0", grown) ==
              ExtensionRegistry::UpdateResult::kNeedsReview,
          "an update that grows permissions needs review");
    Check(registry.HasPendingReview("x"), "and is flagged as pending");
    Check(registry.Find("x")->state == State::kDisabledByPolicy,
          "the extension is disabled until the user looks at it");
    Check(!registry.SetEnabled("x", true),
          "it cannot simply be re-enabled around the review");
    Check(registry.Find("x")->version == "1.0",
          "the old version stays in place meanwhile");

    Check(registry.ApprovePendingReview("x"), "the user can approve");
    Check(registry.Find("x")->version == "2.0" &&
              registry.Find("x")->state == State::kEnabled,
          "and then the new version runs");
    Check(!registry.HasPendingReview("x"), "the flag is cleared");
  }

  // An update that asks for the same or less applies straight away.
  {
    const Disclosure same =
        ExtensionRegistry::Analyze({"declarativeNetRequest", "storage",
                                    "cookies"},
                                   {"<all_urls>"}, true);
    Check(registry.Update("x", "2.1", same) ==
              ExtensionRegistry::UpdateResult::kUpdated,
          "an update with unchanged permissions applies");
    Check(registry.Update("nope", "1.0", same) ==
              ExtensionRegistry::UpdateResult::kNotFound,
          "updating an unknown extension reports not-found");
  }

  Check(registry.Remove("x") && registry.size() == 0, "removal works");

  // Every capability is named and explained.
  for (int i = 0; i <= static_cast<int>(Capability::kMaxValue); ++i) {
    const Capability capability = static_cast<Capability>(i);
    Check(std::string(ExtensionRegistry::CapabilityName(capability)).size() > 3,
          "capability " + std::to_string(i) + " has a name");
    Check(std::string(ExtensionRegistry::CapabilityExplanation(capability))
              .size() > 25,
          "capability " + std::to_string(i) + " has a real explanation");
  }

  if (failures == 0) {
    std::cout << "extension_registry_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
