// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_EXTENSIONS_CATALOG_RECOMMENDATION_ENGINE_H_
#define BEDROCK_EXTENSIONS_CATALOG_RECOMMENDATION_ENGINE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "bedrock/extensions/catalog/extension_catalog.h"

// Smart recommendations (brief item 5) and the threat-model framing (item 18).
//
// The engine exists to say **no** more often than yes. Its output is capped,
// it refuses to suggest two extensions with the same capability, and it never
// suggests anything Bedrock already does completely. The failure mode it is
// built against is the one every privacy list falls into: a page of twelve
// extensions that feels thorough and leaves the user slower, more fingerprintable
// and no better protected.
//
// The three profiles use the Covered / Hardened / Targeted concept as a
// *model*; the wording here is Bedrock's own (see docs/LICENSING.md on why we
// do not copy PrivacyTools.io's text or branding).

namespace bedrock {
namespace catalog {

struct Recommendation {
  ThreatLevel level = ThreatLevel::kCovered;
  std::string headline;
  std::string explanation;
  // Built-in protections come first: the answer to "how do I get private" is
  // usually "you already are, here is what is on".
  std::vector<Capability> built_in_first;
  std::vector<ExtensionEntry> extensions;
  std::string caveat;  // non-empty for Targeted
};

class RecommendationEngine {
 public:
  // No profile may suggest more than this. The cap is the feature.
  static constexpr int kMaxExtensions = 3;

  explicit RecommendationEngine(const ExtensionCatalog* catalog);
  ~RecommendationEngine();

  Recommendation For(ThreatLevel level, int64_t now) const;

  // Shown next to every profile. There is no version of this product where
  // installing more extensions makes someone anonymous.
  static const char* CountDisclaimer();

 private:
  const ExtensionCatalog* catalog_;
};

}  // namespace catalog
}  // namespace bedrock

#endif  // BEDROCK_EXTENSIONS_CATALOG_RECOMMENDATION_ENGINE_H_
