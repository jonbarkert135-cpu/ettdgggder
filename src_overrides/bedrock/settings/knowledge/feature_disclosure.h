// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_SETTINGS_KNOWLEDGE_FEATURE_DISCLOSURE_H_
#define BEDROCK_SETTINGS_KNOWLEDGE_FEATURE_DISCLOSURE_H_

#include <string>
#include <vector>

#include "bedrock/privacy/core/privacy_engine.h"

// Privacy transparency (item 82) and the trade-off table (item 85).
//
// A checkbox labelled "Ultimate Privacy" is a marketing claim wearing a control's
// clothes. It tells the user nothing about what changes, protects nothing they
// can verify, and quietly takes the blame when a site breaks. Every protection
// in Bedrock therefore carries four statements, and the settings UI shows them
// next to the control rather than in a help centre nobody opens:
//
//   How it works            — the mechanism, in one sentence, concretely.
//   What it protects        — the specific attack or exposure it removes.
//   What it cannot protect  — the part that is still exposed. Required, and the
//                             hardest to write honestly: a protection with no
//                             stated limit is a protection nobody has thought
//                             about hard enough.
//   Compatibility impact    — what may visibly break, in the user's terms.
//
// Item 85 adds the internal half of the same honesty. Before a protection is
// switched on by default, it is scored on five axes, and the score is data in
// this table rather than an opinion in a review thread:
//
//   privacy gain · security gain · compatibility loss · performance cost ·
//   complexity                                     (each 0 = none … 3 = high)
//
// Two rules make the scores mean something:
//
//   1. `compatibility_loss >= 2` must agree with `breaks_sites` in the feature
//      registry. Two tables disagreeing about whether something breaks pages is
//      how a browser ends up shipping a default nobody tested.
//   2. Any score of 3 requires `note` to explain it. "High complexity, no
//      comment" is how a subsystem becomes unmaintainable in public.
//
// Never optimise privacy blindly: a protection that scores 1 for privacy and 3
// for compatibility loss is not shipped on by default, whatever it is called.

namespace bedrock {
namespace settings {

// 0 none · 1 low · 2 medium · 3 high. Deliberately coarse: a finer scale would
// invite arguing about a 6 versus a 7 instead of about the decision.
struct Tradeoff {
  int privacy_gain = 0;
  int security_gain = 0;
  int compatibility_loss = 0;
  int performance_cost = 0;
  int complexity = 0;
  const char* note = "";  // required whenever any score is 3
};

struct Disclosure {
  privacy::Feature feature;
  const char* id;  // matches FeatureInfo::id
  const char* how_it_works;
  const char* protects;
  const char* cannot_protect;
  const char* compatibility_impact;
  Tradeoff tradeoff;
  // Required when the registry ships the feature on although `IsDefaultable()`
  // says no. An exception is allowed — some protections are worth a cost the
  // arithmetic cannot see — but it has to be argued in writing, here.
  const char* default_on_reason = "";
};

// Every feature in the registry, in registry order.
const std::vector<Disclosure>& GetDisclosures();

// nullptr when a feature has no disclosure — a bug the test catches.
const Disclosure* FindDisclosure(privacy::Feature feature);

// True when the trade-off is acceptable as a default-on protection: it must buy
// something (privacy or security >= 2) and must not cost more than it buys.
bool IsDefaultable(const Tradeoff& tradeoff);

}  // namespace settings
}  // namespace bedrock

#endif  // BEDROCK_SETTINGS_KNOWLEDGE_FEATURE_DISCLOSURE_H_
