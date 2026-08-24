// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_CORE_FORGET_SITE_H_
#define BEDROCK_PRIVACY_CORE_FORGET_SITE_H_

#include <functional>
#include <map>
#include <string>
#include <vector>

// "Forget about this site" — one action that removes what one site left in
// this profile (follow-up recorded in item 49's Firefox research).
//
// The feature exists because the alternative is a user clearing "all cookies"
// to get rid of one site, which costs them every other login and teaches them
// not to use the control at all.
//
// Two rules shape the whole component, both taken from New Identity (item 22):
//
//   1. **The plan is shown before anything is deleted.** The user sees which
//      stores are included, and the two categories that are excluded by
//      default (saved passwords and bookmarks are things they wrote, not
//      things the site did to them).
//   2. **Failures are reported, never swallowed.** A store with no deleter
//      registered reports `kNotAvailable`. Deletion this browser did not
//      perform is never presented as done — the whole value of the action is
//      that its report can be trusted.
//
// Scope is the registrable domain (eTLD+1) and its subdomains: state written by
// `cdn.shop.test` is state of `shop.test`. Cross-profile data is untouched;
// each profile forgets for itself.

namespace bedrock {
namespace privacy {

// Every place a site can leave something behind. A store missing from this
// list is a store the action silently would not clear, so the list is checked
// against `StoreName()` in the test.
enum class ForgetStore {
  kHistory,
  kCookiesAndStorage,   // cookies, localStorage, IndexedDB, service workers
  kCache,               // HTTP cache, plus DNS and HSTS entries for the site
  kSiteSettings,        // permissions and per-site protection overrides
  kBehavioralLearning,  // what the tracker heuristic learned about the site
  kDownloadRecords,     // the list rows, not the downloaded files on disk
  kPasswords,           // off by default — user-authored
  kBookmarks,           // off by default — user-authored
};

struct ForgetStep {
  ForgetStore store;
  bool selected = true;
  const char* note = "";  // shown next to the row when it needs a caveat
};

struct ForgetPlan {
  std::string etld_plus_one;
  bool include_subdomains = true;
  std::vector<ForgetStep> steps;

  bool valid() const { return !etld_plus_one.empty(); }
};

enum class ForgetState {
  kDeleted,
  kNothingToDelete,
  kSkipped,       // not selected in the plan
  kNotAvailable,  // no deleter registered — this build cannot clear it
  kFailed,
};

struct ForgetOutcome {
  ForgetStore store;
  ForgetState state = ForgetState::kNotAvailable;
  std::string detail;
};

struct ForgetReport {
  std::string etld_plus_one;
  std::vector<ForgetOutcome> outcomes;

  // True only when every selected store was actually cleared. A single
  // kFailed or kNotAvailable makes it false, which is what the UI must show.
  bool complete() const;
};

class ForgetSite {
 public:
  // A store's own deleter. It returns the outcome it observed —
  // kDeleted, kNothingToDelete or kFailed — because only the store knows
  // which of the three happened, and the difference is what the report is for.
  using Deleter = std::function<ForgetState(const std::string& etld_plus_one,
                                            bool include_subdomains)>;

  ForgetSite();
  ~ForgetSite();

  void Register(ForgetStore store, Deleter deleter);

  // The plan the user confirms. Passwords and bookmarks start unselected.
  ForgetPlan MakePlan(const std::string& etld_plus_one) const;

  // Runs only the selected steps of `plan`. An invalid plan runs nothing.
  ForgetReport Run(const ForgetPlan& plan) const;

  static const char* StoreName(ForgetStore store);
  static const char* StateName(ForgetState state);

  // What the action cannot do. Shown with the plan, because a deletion feature
  // that hides its limits teaches the wrong mental model.
  static const char* Limits();

 private:
  std::map<ForgetStore, Deleter> deleters_;
};

}  // namespace privacy
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_CORE_FORGET_SITE_H_
