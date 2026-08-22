// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/extensions/catalog/recommendation_engine.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace bedrock {
namespace catalog {

RecommendationEngine::RecommendationEngine(const ExtensionCatalog* catalog)
    : catalog_(catalog) {}

RecommendationEngine::~RecommendationEngine() = default;

const char* RecommendationEngine::CountDisclaimer() {
  return "More extensions do not mean more protection. Each one is more code "
         "with access to your pages, and overlapping tools mostly add "
         "breakage.";
}

Recommendation RecommendationEngine::For(ThreatLevel level, int64_t now) const {
  Recommendation recommendation;
  recommendation.level = level;

  switch (level) {
    case ThreatLevel::kCovered:
      recommendation.headline = "Basic privacy";
      recommendation.explanation =
          "Bedrock's built-in protection plus one good content blocker "
          "handles ordinary advertising and tracking. Most people need "
          "nothing else.";
      break;
    case ThreatLevel::kHardened:
      recommendation.headline = "Hardened privacy";
      recommendation.explanation =
          "Stricter cookie handling and one or two specialised tools, at the "
          "cost of some sites needing an exception.";
      break;
    case ThreatLevel::kTargeted:
      recommendation.headline = "Targeted privacy";
      recommendation.explanation =
          "For a specific adversary and a specific threat model.";
      recommendation.caveat =
          "This level is for a situation where someone specific is trying to "
          "identify you. It reduces compatibility, changes how sites behave, "
          "and only makes sense together with the rest of a threat model — "
          "the browser is one part of it.";
      break;
  }

  // What is already on. This list comes first on purpose.
  for (const ExtensionEntry& entry : catalog_->entries()) {
    if (entry.provenance != Provenance::kBuiltIntoBedrock)
      continue;
    for (Capability capability : entry.capabilities) {
      if (std::find(recommendation.built_in_first.begin(),
                    recommendation.built_in_first.end(),
                    capability) == recommendation.built_in_first.end()) {
        recommendation.built_in_first.push_back(capability);
      }
    }
  }

  std::vector<Capability> taken;
  for (const ExtensionEntry& entry : catalog_->entries()) {
    if (static_cast<int>(recommendation.extensions.size()) >= kMaxExtensions)
      break;
    if (entry.provenance == Provenance::kBuiltIntoBedrock)
      continue;
    // A profile only offers entries at or below its own level: a Targeted
    // tool has no business in a Covered list.
    if (static_cast<int>(entry.threat_level) > static_cast<int>(level))
      continue;

    const Analysis analysis = catalog_->Analyze(entry, {}, now);
    if (!analysis.installable)
      continue;
    if (analysis.verification_stale)
      continue;  // never recommend what we have not rechecked
    if (analysis.overlap.substantially_duplicate)
      continue;  // Bedrock already does all of it

    // One tool per job. If something already recommended covers the same
    // capability, this one is not added.
    bool overlaps_recommendation = false;
    for (Capability capability : entry.capabilities) {
      if (std::find(taken.begin(), taken.end(), capability) != taken.end())
        overlaps_recommendation = true;
    }
    if (overlaps_recommendation)
      continue;

    for (Capability capability : entry.capabilities)
      taken.push_back(capability);
    recommendation.extensions.push_back(entry);
  }

  return recommendation;
}

}  // namespace catalog
}  // namespace bedrock
