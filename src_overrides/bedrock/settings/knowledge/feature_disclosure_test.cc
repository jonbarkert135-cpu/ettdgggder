// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh. Items 82 and 85.

#include "bedrock/settings/knowledge/feature_disclosure.h"

#include <iostream>
#include <set>
#include <string>

#include "bedrock/privacy/core/privacy_engine.h"

namespace {

using bedrock::privacy::FeatureInfo;
using bedrock::privacy::GetFeatureRegistry;
using bedrock::settings::Disclosure;
using bedrock::settings::FindDisclosure;
using bedrock::settings::GetDisclosures;
using bedrock::settings::IsDefaultable;
using bedrock::settings::Tradeoff;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

void EveryFeatureIsDisclosed() {
  for (const FeatureInfo& info : GetFeatureRegistry()) {
    const Disclosure* row = FindDisclosure(info.feature);
    Check(row != nullptr, std::string(info.id) + ": has a disclosure");
    if (!row) {
      continue;
    }
    Check(std::string(row->id) == info.id,
          std::string(info.id) + ": disclosure id matches the registry id");
  }
  Check(GetDisclosures().size() == GetFeatureRegistry().size(),
        "no disclosure describes a feature that does not exist");
}

void AllFourStatementsAreThereAndSpecific() {
  for (const Disclosure& row : GetDisclosures()) {
    const std::string id = row.id;
    // A sentence, not a label. Twenty characters rules out "N/A", "None" and
    // "Protects your privacy".
    Check(std::string(row.how_it_works).size() > 40, id + ": how it works is a sentence");
    Check(std::string(row.protects).size() > 30, id + ": what it protects is a sentence");
    Check(std::string(row.cannot_protect).size() > 30,
          id + ": what it cannot protect is a sentence");
    Check(std::string(row.compatibility_impact).size() > 20,
          id + ": compatibility impact is stated");
  }
}

void LimitsAreRealLimits() {
  // The failure mode of item 82 is a "cannot protect" field that says nothing.
  const std::string kEmptyAnswers[] = {"nothing.", "n/a", "none.", "unknown"};
  for (const Disclosure& row : GetDisclosures()) {
    std::string lower;
    for (char c : std::string(row.cannot_protect)) {
      lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    for (const std::string& empty : kEmptyAnswers) {
      Check(lower != empty,
            std::string(row.id) + ": states a real limit, not " + empty);
    }
  }
}

void ScoresAreInRangeAndExplainedWhenExtreme() {
  for (const Disclosure& row : GetDisclosures()) {
    const Tradeoff& t = row.tradeoff;
    const int scores[] = {t.privacy_gain, t.security_gain, t.compatibility_loss,
                          t.performance_cost, t.complexity};
    bool extreme = false;
    for (int score : scores) {
      Check(score >= 0 && score <= 3, std::string(row.id) + ": score in 0..3");
      if (score == 3) {
        extreme = true;
      }
    }
    if (extreme) {
      Check(std::string(t.note).size() > 30,
            std::string(row.id) + ": a score of 3 carries an explanation");
    }
  }
}

void TheTwoTablesAgreeAboutBreakage() {
  for (const FeatureInfo& info : GetFeatureRegistry()) {
    const Disclosure* row = FindDisclosure(info.feature);
    if (!row) {
      continue;
    }
    const bool table_says_breaks = row->tradeoff.compatibility_loss >= 2;
    Check(table_says_breaks == info.breaks_sites,
          std::string(info.id) + ": registry breaks_sites=" +
              (info.breaks_sites ? "true" : "false") +
              " agrees with compatibility_loss=" +
              std::to_string(row->tradeoff.compatibility_loss));
  }
}

void DefaultableIsAJudgementWithATest() {
  Check(IsDefaultable({1, 0, 0, 0, 0, ""}), "a free protection is on, however small");
  Check(!IsDefaultable({1, 0, 1, 0, 0, ""}),
        "a small gain that costs something is not defaultable");
  Check(!IsDefaultable({0, 0, 0, 0, 1, ""}), "a protection that buys nothing is not");
  Check(!IsDefaultable({3, 0, 3, 1, 0, ""}),
        "a large gain that breaks the web is not defaultable");
  Check(IsDefaultable({3, 1, 1, 0, 2, ""}), "a large gain with a small cost is");
  Check(IsDefaultable({1, 3, 1, 0, 1, ""}), "a security gain counts as much");

  // The feature the registry ships off by default must be the one the scores
  // say is not defaultable.
  const Disclosure* third_party = FindDisclosure(
      bedrock::privacy::Feature::kThirdPartyRequestControl);
  Check(third_party && !IsDefaultable(third_party->tradeoff),
        "blocking all third-party requests scores as not defaultable");
}

void ShippingOnAgainstTheScoreNeedsAnArgument() {
  // Item 85's real test: when the arithmetic says "opt-in" but the registry
  // ships the feature on, that is allowed — and it must be argued in writing.
  for (const FeatureInfo& info : GetFeatureRegistry()) {
    const Disclosure* row = FindDisclosure(info.feature);
    if (!row) {
      continue;
    }
    const bool on_by_default = info.standard_default != bedrock::privacy::Setting::kOff;
    if (on_by_default && !IsDefaultable(row->tradeoff)) {
      Check(std::string(row->default_on_reason).size() > 60,
            std::string(info.id) +
                ": ships on against its own score, so it must say why");
    } else {
      Check(std::string(row->default_on_reason).empty(),
            std::string(info.id) +
                ": no exception is claimed for a default the score supports");
    }
  }
}

void IdsAreUnique() {
  std::set<std::string> ids;
  for (const Disclosure& row : GetDisclosures()) {
    Check(ids.insert(row.id).second, std::string(row.id) + ": id is unique");
  }
}

}  // namespace

int main() {
  EveryFeatureIsDisclosed();
  AllFourStatementsAreThereAndSpecific();
  LimitsAreRealLimits();
  ScoresAreInRangeAndExplainedWhenExtreme();
  TheTwoTablesAgreeAboutBreakage();
  DefaultableIsAJudgementWithATest();
  ShippingOnAgainstTheScoreNeedsAnArgument();
  IdsAreUnique();
  if (failures == 0) {
    std::cout << "feature_disclosure: ok (" << GetDisclosures().size()
              << " features disclosed)\n";
  }
  return failures == 0 ? 0 : 1;
}
