// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/core/privacy_engine.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

// Host test, no Chromium. The registry drives the settings UI, the shields
// panel and the per-site override store, so what is checked here is that it
// cannot lie: unique ids, an explanation for every control, and no feature
// claiming to be enforced while the tree says otherwise.

namespace {

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::printf("  FAIL: %s\n", what.c_str());
    ++failures;
  }
}

using namespace bedrock::privacy;

void EveryFeatureHasExactlyOneRow() {
  std::set<int> seen;
  for (const FeatureInfo& info : GetFeatureRegistry()) {
    const int value = static_cast<int>(info.feature);
    Check(seen.insert(value).second, "feature enum value appears twice in the registry");
  }
  Check(GetFeatureRegistry().size() == seen.size(), "registry size does not match unique features");
}

void IdsAreStableAndUnique() {
  std::set<std::string> ids;
  for (const FeatureInfo& info : GetFeatureRegistry()) {
    const std::string id = info.id;
    Check(!id.empty(), "a feature has an empty stable id");
    Check(id.find(' ') == std::string::npos, "stable ids are used in prefs, so no spaces: " + id);
    Check(ids.insert(id).second, "duplicate stable id: " + id);
  }
}

void EveryControlExplainsItself() {
  // A switch a user cannot understand is a switch they will set wrongly.
  for (const FeatureInfo& info : GetFeatureRegistry()) {
    Check(info.explanation_string != nullptr && std::strlen(info.explanation_string) > 0,
          std::string("feature has no explanation string: ") + info.id);
    Check(info.title_string != nullptr && std::strlen(info.title_string) > 0,
          std::string("feature has no title string: ") + info.id);
  }
}

void StrictIsNeverWeakerThanStandard() {
  for (const FeatureInfo& info : GetFeatureRegistry()) {
    Check(static_cast<int>(info.strict_default) >= static_cast<int>(info.standard_default),
          std::string("strict is weaker than standard for ") + info.id);
  }
}

void UiOnlyRendersWhatTheBrowserEnforces() {
  // Item 55, mechanically: the settings page iterates UiRenderableFeatures(),
  // and that list contains only kEnforced rows. Nothing is enforced while no
  // Chromium build runs, so the honest UI today shows no protection switches.
  for (const FeatureInfo* info : UiRenderableFeatures()) {
    Check(info->status == Status::kEnforced,
          std::string("UI would render a feature that is not enforced: ") + info->id);
  }
  size_t enforced = 0;
  for (const FeatureInfo& info : GetFeatureRegistry()) {
    if (info.status == Status::kEnforced) {
      ++enforced;
    }
  }
  Check(UiRenderableFeatures().size() == enforced, "renderable list does not match enforced rows");
}

void LookupFindsEveryFeature() {
  for (const FeatureInfo& info : GetFeatureRegistry()) {
    const FeatureInfo* found = FindFeature(info.feature);
    Check(found != nullptr, std::string("FindFeature missed ") + info.id);
    if (found != nullptr) {
      Check(std::string(found->id) == info.id, "FindFeature returned the wrong row");
    }
  }
}

void DefaultsFollowTheLevel() {
  const FeatureInfo* canvas = FindFeature(Feature::kCanvasProtection);
  Check(canvas != nullptr, "canvas protection is missing from the registry");
  if (canvas != nullptr) {
    Check(DefaultFor(*canvas, Level::kStandard) == canvas->standard_default,
          "standard level does not use the standard default");
    Check(DefaultFor(*canvas, Level::kStrict) == canvas->strict_default,
          "strict level does not use the strict default");
  }
}

void BreakingFeaturesAreMarked() {
  // Anything that can break a site must say so, because the level ladder
  // prices its cost from this flag (item 45).
  const FeatureInfo* cosmetic = FindFeature(Feature::kCosmeticFiltering);
  Check(cosmetic != nullptr && cosmetic->breaks_sites,
        "cosmetic filtering can break pages and must be marked as such");
  // Changed with item 85: encrypted DNS does break things — captive portals and
  // split-horizon corporate resolvers — and the trade-off table scores it as a
  // medium compatibility loss. The two tables must agree, so the flag is true
  // and the standard default is off (item 84 says "configurable").
  const FeatureInfo* dns = FindFeature(Feature::kSecureDns);
  Check(dns != nullptr && dns->breaks_sites,
        "secure DNS is flagged as breaking: captive portals and split-horizon DNS");
  Check(dns != nullptr && dns->standard_default == Setting::kOff,
        "secure DNS is configurable rather than forced on (item 84)");
}

}  // namespace

int main() {
  EveryFeatureHasExactlyOneRow();
  IdsAreStableAndUnique();
  EveryControlExplainsItself();
  StrictIsNeverWeakerThanStandard();
  UiOnlyRendersWhatTheBrowserEnforces();
  LookupFindsEveryFeature();
  DefaultsFollowTheLevel();
  BreakingFeaturesAreMarked();

  if (failures == 0) {
    std::printf("privacy_engine: %zu features, all rows consistent\n",
                GetFeatureRegistry().size());
    return 0;
  }
  std::printf("privacy_engine: %d failure(s)\n", failures);
  return 1;
}
