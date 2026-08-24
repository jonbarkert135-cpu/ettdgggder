// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/core/forget_site.h"

#include <string>
#include <vector>

namespace bedrock {
namespace privacy {

namespace {

// The stores, in the order the plan lists them: what the site did to the
// profile first, what the user authored last.
const std::vector<ForgetStep>& DefaultSteps() {
  static const std::vector<ForgetStep> kSteps = {
      {ForgetStore::kHistory, true, ""},
      {ForgetStore::kCookiesAndStorage, true, ""},
      {ForgetStore::kCache, true, "includes cached DNS and HSTS entries"},
      {ForgetStore::kSiteSettings, true,
       "permissions and protection overrides return to the profile default"},
      {ForgetStore::kBehavioralLearning, true, ""},
      {ForgetStore::kDownloadRecords, true,
       "removes the list rows; files already on disk stay"},
      {ForgetStore::kPasswords, false, "you saved these — off by default"},
      {ForgetStore::kBookmarks, false, "you saved these — off by default"},
  };
  return kSteps;
}

}  // namespace

bool ForgetReport::complete() const {
  for (const ForgetOutcome& outcome : outcomes) {
    if (outcome.state == ForgetState::kFailed ||
        outcome.state == ForgetState::kNotAvailable) {
      return false;
    }
  }
  return !outcomes.empty();
}

ForgetSite::ForgetSite() = default;
ForgetSite::~ForgetSite() = default;

void ForgetSite::Register(ForgetStore store, Deleter deleter) {
  deleters_[store] = std::move(deleter);
}

ForgetPlan ForgetSite::MakePlan(const std::string& etld_plus_one) const {
  ForgetPlan plan;
  plan.etld_plus_one = etld_plus_one;
  plan.steps = DefaultSteps();
  return plan;
}

ForgetReport ForgetSite::Run(const ForgetPlan& plan) const {
  ForgetReport report;
  report.etld_plus_one = plan.etld_plus_one;
  if (!plan.valid()) {
    return report;  // nothing runs without a site; an empty report is honest
  }
  for (const ForgetStep& step : plan.steps) {
    ForgetOutcome outcome;
    outcome.store = step.store;
    if (!step.selected) {
      outcome.state = ForgetState::kSkipped;
      report.outcomes.push_back(outcome);
      continue;
    }
    const auto it = deleters_.find(step.store);
    if (it == deleters_.end() || !it->second) {
      // This build has no way to clear that store. Saying so is the point.
      outcome.state = ForgetState::kNotAvailable;
      outcome.detail = "no deleter registered in this build";
      report.outcomes.push_back(outcome);
      continue;
    }
    outcome.state = it->second(plan.etld_plus_one, plan.include_subdomains);
    if (outcome.state == ForgetState::kFailed) {
      outcome.detail = "the store reported a failure";
    }
    report.outcomes.push_back(outcome);
  }
  return report;
}

// static
const char* ForgetSite::StoreName(ForgetStore store) {
  switch (store) {
    case ForgetStore::kHistory:
      return "History";
    case ForgetStore::kCookiesAndStorage:
      return "Cookies and site storage";
    case ForgetStore::kCache:
      return "Cache";
    case ForgetStore::kSiteSettings:
      return "Site settings";
    case ForgetStore::kBehavioralLearning:
      return "What was learned about this site";
    case ForgetStore::kDownloadRecords:
      return "Download list entries";
    case ForgetStore::kPasswords:
      return "Saved passwords";
    case ForgetStore::kBookmarks:
      return "Bookmarks";
  }
  return "";
}

// static
const char* ForgetSite::StateName(ForgetState state) {
  switch (state) {
    case ForgetState::kDeleted:
      return "removed";
    case ForgetState::kNothingToDelete:
      return "nothing stored";
    case ForgetState::kSkipped:
      return "kept";
    case ForgetState::kNotAvailable:
      return "not available in this build";
    case ForgetState::kFailed:
      return "failed";
  }
  return "";
}

// static
const char* ForgetSite::Limits() {
  return "This clears what the site left in this profile on this device. Data "
         "the site already received, copies held on its servers, and state in "
         "your other profiles or devices are outside the browser's reach.";
}

}  // namespace privacy
}  // namespace bedrock
