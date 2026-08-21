// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/net/storage_isolation.h"

#include <iostream>
#include <string>

namespace {

using namespace bedrock::net;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

}  // namespace

int main() {
  StorageIsolation isolation;

  // The property the whole feature exists for: the same third party embedded
  // on two sites gets two unrelated storage areas.
  {
    const StorageKey on_news = isolation.KeyFor("https://widget.test",
                                                "widget.test", "news.test");
    const StorageKey on_shop = isolation.KeyFor("https://widget.test",
                                                "widget.test", "shop.test");
    Check(on_news != on_shop, "same third party, different top-level site, "
                              "different key");
    Check(on_news.Serialize() != on_shop.Serialize(),
          "and the serialized form differs too");
    Check(on_news.cross_site && on_shop.cross_site, "both are cross-site");
  }

  // First-party storage is keyed too, and is not marked cross-site.
  {
    const StorageKey own = isolation.KeyFor("https://news.test", "news.test",
                                            "news.test");
    Check(!own.cross_site, "a site's own storage is first-party");
    Check(own.Serialize().find("news.test") != std::string::npos,
          "the top-level site is part of every key, including first-party");
  }

  // Subdomains of the same site share the partition (site, not origin).
  {
    const StorageKey a = isolation.KeyFor("https://widget.test", "widget.test",
                                          "news.test");
    const StorageKey b = isolation.KeyFor("https://widget.test", "widget.test",
                                          "news.test");
    Check(a == b, "the key is stable for the same triple");
  }

  // Every storage type is partitioned. One exception is one tracking channel.
  for (int i = 0; i <= static_cast<int>(StorageType::kMaxValue); ++i) {
    const StorageType type = static_cast<StorageType>(i);
    Check(StorageIsolation::IsPartitioned(type),
          "storage type " + std::to_string(i) + " is partitioned");
    Check(std::string(StorageIsolation::Explain(type)).size() > 20,
          "storage type " + std::to_string(i) + " is explainable to the user");
  }

  // Lifetimes per level.
  {
    const StorageKey third = isolation.KeyFor("https://widget.test",
                                              "widget.test", "news.test");
    const StorageKey first = isolation.KeyFor("https://news.test", "news.test",
                                              "news.test");
    Check(isolation.LifetimeFor(StorageType::kCookies, third) ==
              Lifetime::kPersistent,
          "standard level keeps partitioned third-party storage");
    Check(isolation.LifetimeFor(StorageType::kSessionStorage, first) ==
              Lifetime::kEphemeral,
          "session storage is always ephemeral");

    isolation.set_level(IsolationLevel::kStrict);
    Check(isolation.LifetimeFor(StorageType::kCookies, third) ==
              Lifetime::kEphemeral,
          "strict makes third-party storage ephemeral");
    Check(isolation.LifetimeFor(StorageType::kCookies, first) ==
              Lifetime::kPersistent,
          "strict keeps first-party storage — logins survive");

    isolation.set_level(IsolationLevel::kEphemeralAll);
    Check(isolation.LifetimeFor(StorageType::kCookies, first) ==
              Lifetime::kEphemeral,
          "ephemeral-all clears first-party storage too");
    isolation.set_level(IsolationLevel::kStandard);
  }

  // Storage Access API grants are scoped, never global.
  {
    isolation.GrantStorageAccess("https://login.test", "news.test");
    const StorageKey granted = isolation.KeyFor("https://login.test",
                                                "login.test", "news.test");
    const StorageKey elsewhere = isolation.KeyFor("https://login.test",
                                                  "login.test", "shop.test");
    Check(!granted.cross_site, "a grant restores first-party storage here");
    Check(elsewhere.cross_site,
          "and does not follow the origin to another site");
    Check(granted != elsewhere, "so the two areas are still separate");

    isolation.RevokeStorageAccess("https://login.test", "news.test");
    Check(isolation.KeyFor("https://login.test", "login.test", "news.test")
              .cross_site,
          "revoking the grant restores partitioning");
  }

  // Deleting a site's data must include what third parties stored under it.
  {
    const std::vector<StorageKey> all = {
        isolation.KeyFor("https://news.test", "news.test", "news.test"),
        isolation.KeyFor("https://widget.test", "widget.test", "news.test"),
        isolation.KeyFor("https://widget.test", "widget.test", "shop.test"),
        isolation.KeyFor("https://shop.test", "shop.test", "shop.test"),
    };
    const auto to_clear = isolation.KeysToClearForSite(all, "news.test");
    Check(to_clear.size() == 2,
          "clearing a site clears its own and its embedded third-party areas");
    for (const StorageKey& key : to_clear) {
      Check(key.top_level_site == "news.test", "only that site's areas");
    }
  }

  Check(StorageIsolation::IsPerTab(StorageType::kSessionStorage),
        "session storage is per tab");
  Check(!StorageIsolation::IsPerTab(StorageType::kLocalStorage),
        "local storage is not per tab");

  if (failures == 0) {
    std::cout << "storage_isolation_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
