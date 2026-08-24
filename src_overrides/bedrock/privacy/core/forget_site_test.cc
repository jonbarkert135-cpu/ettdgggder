// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/privacy/core/forget_site.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace bedrock::privacy;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

const std::vector<ForgetStore>& AllStores() {
  static const std::vector<ForgetStore> kAll = {
      ForgetStore::kHistory,          ForgetStore::kCookiesAndStorage,
      ForgetStore::kCache,            ForgetStore::kSiteSettings,
      ForgetStore::kBehavioralLearning, ForgetStore::kDownloadRecords,
      ForgetStore::kPasswords,        ForgetStore::kBookmarks,
  };
  return kAll;
}

const ForgetOutcome* Find(const ForgetReport& report, ForgetStore store) {
  for (const ForgetOutcome& outcome : report.outcomes) {
    if (outcome.store == store) {
      return &outcome;
    }
  }
  return nullptr;
}

}  // namespace

int main() {
  // Every store is named and every store is in the plan. A store that exists
  // in the enum but not in the plan would be silently never cleared.
  {
    const ForgetSite site;
    const ForgetPlan plan = site.MakePlan("shop.test");
    Check(plan.steps.size() == AllStores().size(),
          "the plan covers every store in the enum");
    for (ForgetStore store : AllStores()) {
      Check(std::string(ForgetSite::StoreName(store)).size() > 0,
            "every store has a name a user can read");
      bool in_plan = false;
      for (const ForgetStep& step : plan.steps) {
        in_plan = in_plan || step.store == store;
      }
      Check(in_plan, "store present in the plan");
    }
  }

  // User-authored data is opt-in; everything the site did is opt-out.
  {
    const ForgetSite site;
    const ForgetPlan plan = site.MakePlan("shop.test");
    for (const ForgetStep& step : plan.steps) {
      const bool authored = step.store == ForgetStore::kPasswords ||
                            step.store == ForgetStore::kBookmarks;
      Check(step.selected != authored,
            std::string("default selection for ") +
                ForgetSite::StoreName(step.store));
    }
  }

  // A store with no deleter is reported as unavailable — never as removed.
  {
    ForgetSite site;
    site.Register(ForgetStore::kHistory,
                  [](const std::string&, bool) { return ForgetState::kDeleted; });
    const ForgetReport report = site.Run(site.MakePlan("shop.test"));
    Check(Find(report, ForgetStore::kHistory)->state == ForgetState::kDeleted,
          "the registered store reports what it did");
    Check(Find(report, ForgetStore::kCache)->state ==
              ForgetState::kNotAvailable,
          "a store with no deleter is not claimed as cleared");
    Check(!report.complete(),
          "and the report as a whole is not complete");
  }

  // Unselected stores are skipped without calling anything.
  {
    ForgetSite site;
    int password_calls = 0;
    site.Register(ForgetStore::kPasswords,
                  [&password_calls](const std::string&, bool) {
                    ++password_calls;
                    return ForgetState::kDeleted;
                  });
    site.Run(site.MakePlan("shop.test"));
    Check(password_calls == 0, "an unselected store is never asked to delete");
  }

  // The full happy path, including the difference between "removed" and
  // "nothing was stored" — a browser that reports them the same way cannot be
  // used to check whether a site stored anything at all.
  {
    ForgetSite site;
    std::string seen_site;
    bool seen_subdomains = false;
    for (ForgetStore store : AllStores()) {
      site.Register(store, [&, store](const std::string& etld1, bool subdomains) {
        seen_site = etld1;
        seen_subdomains = subdomains;
        return store == ForgetStore::kCache ? ForgetState::kNothingToDelete
                                            : ForgetState::kDeleted;
      });
    }
    ForgetPlan plan = site.MakePlan("shop.test");
    for (ForgetStep& step : plan.steps) {
      step.selected = true;  // the user ticked the two opt-in rows
    }
    const ForgetReport report = site.Run(plan);
    Check(report.complete(), "everything selected was handled");
    Check(seen_site == "shop.test" && seen_subdomains,
          "deleters get the registrable domain and the subdomain scope");
    Check(Find(report, ForgetStore::kCache)->state ==
              ForgetState::kNothingToDelete,
          "\"nothing stored\" stays distinct from \"removed\"");
  }

  // One failure is enough to make the whole run incomplete, and it says so.
  {
    ForgetSite site;
    for (ForgetStore store : AllStores()) {
      site.Register(store, [store](const std::string&, bool) {
        return store == ForgetStore::kCookiesAndStorage ? ForgetState::kFailed
                                                        : ForgetState::kDeleted;
      });
    }
    const ForgetReport report = site.Run(site.MakePlan("shop.test"));
    Check(!report.complete(), "a failed store fails the run");
    Check(!Find(report, ForgetStore::kCookiesAndStorage)->detail.empty(),
          "the failure carries a reason");
  }

  // No site, no deletion, no invented report.
  {
    ForgetSite site;
    int calls = 0;
    site.Register(ForgetStore::kHistory, [&calls](const std::string&, bool) {
      ++calls;
      return ForgetState::kDeleted;
    });
    const ForgetReport report = site.Run(site.MakePlan(""));
    Check(calls == 0 && report.outcomes.empty(),
          "an empty site name runs nothing");
    Check(!report.complete(), "and an empty report is not 'complete'");
  }

  // The limits sentence is part of the feature, and it may not overclaim.
  {
    const std::string limits = ForgetSite::Limits();
    Check(limits.find("already received") != std::string::npos,
          "the limits mention data the site already has");
    for (const char* word : {"anonymous", "untraceable", "100%", "completely",
                             "no one can", "invisible"}) {
      Check(limits.find(word) == std::string::npos,
            std::string("the limits avoid the banned claim: ") + word);
    }
  }

  if (failures == 0) {
    std::cout << "forget_site_test: OK\n";
  }
  return failures == 0 ? 0 : 1;
}
