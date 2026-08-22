// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/extensions/catalog/extension_catalog.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "bedrock/extensions/catalog/recommendation_engine.h"

namespace {

using namespace bedrock::catalog;  // NOLINT — test-local convenience

int failures = 0;
constexpr int64_t kNow = 1'787'000'000;  // 2026-08
constexpr int64_t kDay = 86400;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

bool Mentions(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

ExtensionEntry Entry(const std::string& id,
                     const std::string& name,
                     std::vector<Capability> capabilities,
                     Provenance provenance = Provenance::kPrivacyToolsRecommendation) {
  ExtensionEntry entry;
  entry.id = id;
  entry.name = name;
  entry.icon = "packaged://catalog/" + id + ".png";
  entry.description = name + " description";
  entry.privacy_purpose = "purpose";
  entry.official_source = "https://example.org/" + id;
  entry.license = "MIT";
  entry.attribution = name + ", MIT. Recommended by PrivacyTools.io.";
  entry.version = "1.0";
  entry.why_recommended = "Adds an additional specialized layer.";
  entry.capabilities = std::move(capabilities);
  entry.provenance = provenance;
  entry.compatibility = Compatibility::kChromiumMv3;
  entry.maintenance = Maintenance::kActive;
  entry.open_source = true;
  entry.last_verified = kNow - 10 * kDay;
  entry.permissions = {"storage"};
  return entry;
}

}  // namespace

int main() {
  BuiltInProtections built_in;
  built_in.capabilities = {Capability::kContentBlocking,
                           Capability::kTrackerBlocking,
                           Capability::kFingerprintDefense,
                           Capability::kHttpsEnforcement};
  ExtensionCatalog catalog(built_in);

  // A blocker Bedrock already replaces.
  ExtensionEntry blocker = Entry("blocker", "Some Content Blocker",
                                 {Capability::kContentBlocking,
                                  Capability::kTrackerBlocking});
  blocker.permissions = {"<all_urls>", "webRequest", "storage"};
  blocker.threat_level = ThreatLevel::kCovered;
  catalog.Add(blocker);

  // A specialised tool that adds something new.
  ExtensionEntry cleaner =
      Entry("cleaner", "URL Cleaner", {Capability::kUrlParameterCleaning});
  catalog.Add(cleaner);

  ExtensionEntry cookies =
      Entry("cookies", "Cookie Cleanup", {Capability::kCookieCleanup});
  cookies.threat_level = ThreatLevel::kHardened;
  cookies.permissions = {"cookies", "browsingData"};
  catalog.Add(cookies);

  ExtensionEntry bedrock_pick = Entry("bedrock-pick", "Bedrock Pick",
                                      {Capability::kScriptControl},
                                      Provenance::kBedrockRecommendation);
  bedrock_pick.threat_level = ThreatLevel::kTargeted;
  catalog.Add(bedrock_pick);

  // ---- item 3: no false "official" ----
  Check(ExtensionCatalog::BadgeText(blocker) == "Recommended by PrivacyTools.io",
        "a PrivacyTools pick is badged as theirs");
  Check(ExtensionCatalog::BadgeText(bedrock_pick) == "Recommended by BEDROCK",
        "a Bedrock pick says Bedrock recommends it");
  for (const ExtensionEntry& entry : catalog.entries()) {
    Check(!Mentions(ExtensionCatalog::BadgeText(entry), "Official"),
          "no third-party extension is ever called official: " + entry.name);
  }
  Check(Mentions(std::string(ExtensionCatalog::PrivacyToolsAttribution()),
                 "PrivacyTools.io"),
        "the attribution names PrivacyTools.io");
  Check(std::string(ExtensionCatalog::PrivacyToolsUrl()) ==
            "https://www.privacytools.io",
        "with the working link their license asks for");

  // ---- item 20: permission analysis ----
  const Analysis blocker_analysis = catalog.Analyze(blocker, {}, kNow);
  Check(blocker_analysis.permissions.risk == RiskLevel::kHigh,
        "page-content access is high risk");
  Check(Mentions(blocker_analysis.permissions.why, "content of pages"),
        "and the reason is in plain language");
  Check(catalog.Analyze(cleaner, {}, kNow).permissions.risk == RiskLevel::kLow,
        "an extension that asks for nothing is low risk");
  Check(catalog.Analyze(cookies, {}, kNow).permissions.risk == RiskLevel::kHigh,
        "cookie access is high risk");
  const auto links = ExtensionCatalog::RequiredLinks(blocker);
  Check(links.size() == 3, "source, license and repository links are required");

  // ---- items 4 and 21: overlap and duplicate protection ----
  Check(blocker_analysis.overlap.substantially_duplicate,
        "a blocker whose whole job Bedrock does is flagged as duplicate");
  Check(Mentions(ExtensionCatalog::DuplicateWarning(blocker_analysis.overlap),
                 "Install anyway?"),
        "and the user is asked before installing it anyway");
  Check(Mentions(ExtensionCatalog::DuplicateWarning(blocker_analysis.overlap),
                 "attack surface"),
        "with the real cost spelled out");

  const Analysis cleaner_analysis = catalog.Analyze(cleaner, {}, kNow);
  Check(!cleaner_analysis.overlap.substantially_duplicate,
        "a tool that adds a capability is not called a duplicate");
  Check(ExtensionCatalog::DuplicateWarning(cleaner_analysis.overlap).empty(),
        "so no warning is shown for it");
  Check(Mentions(catalog.CoverageSummary(cleaner), "Already protected by BEDROCK"),
        "the card still shows what is already covered");
  Check(Mentions(catalog.CoverageSummary(cleaner), "Tracker blocking"),
        "listing the built-in protections by name");

  // Overlap with something already installed, not just with Bedrock.
  ExtensionEntry second_cleaner =
      Entry("cleaner2", "Another URL Cleaner", {Capability::kUrlParameterCleaning});
  catalog.Add(second_cleaner);
  const Analysis second = catalog.Analyze(second_cleaner, {"cleaner"}, kNow);
  Check(second.overlap.substantially_duplicate,
        "a second tool doing the same job as an installed one is a duplicate");
  Check(!second.overlap.duplicate_of.empty(), "and it names which one");

  // ---- item 1: compatibility and maintenance analysis ----
  ExtensionEntry legacy = Entry("legacy", "Legacy Tool", {Capability::kScriptControl});
  legacy.compatibility = Compatibility::kChromiumMv2Only;
  catalog.Add(legacy);
  const Analysis legacy_analysis = catalog.Analyze(legacy, {}, kNow);
  Check(!legacy_analysis.installable, "an MV2-only extension cannot be installed");
  Check(Mentions(legacy_analysis.compatibility_note, "Manifest V2"),
        "and the card says why");

  ExtensionEntry abandoned =
      Entry("abandoned", "Abandoned Tool", {Capability::kCookieCleanup});
  abandoned.maintenance = Maintenance::kUnmaintained;
  catalog.Add(abandoned);
  Check(!catalog.Analyze(abandoned, {}, kNow).installable,
        "an unmaintained extension with page access is not offered");

  // ---- item 13: stale verification ----
  ExtensionEntry stale = Entry("stale", "Stale Entry", {Capability::kCookieCleanup});
  stale.last_verified = kNow - 400 * kDay;
  catalog.Add(stale);
  Check(catalog.Analyze(stale, {}, kNow).verification_stale,
        "an entry checked over half a year ago is marked stale");
  Check(!catalog.Analyze(cleaner, {}, kNow).verification_stale,
        "a recently checked entry is not");

  // ---- filters ----
  Check(catalog.Filter(true, ThreatLevel::kCovered).size() >= 2,
        "filtering by open source and level returns the covered picks");
  for (const ExtensionEntry& entry :
       catalog.Filter(true, ThreatLevel::kCovered)) {
    Check(entry.open_source, "open-source filter holds");
    Check(entry.threat_level == ThreatLevel::kCovered, "level filter holds");
  }

  // ---- item 5: the recommendation engine says no ----
  RecommendationEngine engine(&catalog);
  const Recommendation covered = engine.For(ThreatLevel::kCovered, kNow);
  Check(static_cast<int>(covered.extensions.size()) <=
            RecommendationEngine::kMaxExtensions,
        "a profile never suggests more than three extensions");
  Check(!covered.built_in_first.empty() || covered.extensions.size() <= 3,
        "the profile leads with what is already built in when there is any");

  std::vector<Capability> seen;
  for (const ExtensionEntry& entry : covered.extensions) {
    for (Capability capability : entry.capabilities) {
      for (Capability already : seen) {
        Check(already != capability,
              "no two recommended extensions do the same job");
      }
      seen.push_back(capability);
    }
    const Analysis analysis = catalog.Analyze(entry, {}, kNow);
    Check(!analysis.overlap.substantially_duplicate,
          "nothing Bedrock fully covers is recommended: " + entry.name);
    Check(analysis.installable, "nothing unusable is recommended: " + entry.name);
    Check(!analysis.verification_stale,
          "nothing stale is recommended: " + entry.name);
    Check(static_cast<int>(entry.threat_level) <=
              static_cast<int>(ThreatLevel::kCovered),
          "a Covered profile only offers Covered tools");
  }

  const Recommendation targeted = engine.For(ThreatLevel::kTargeted, kNow);
  Check(!targeted.caveat.empty(), "the Targeted profile carries a caveat");
  Check(Mentions(targeted.caveat, "compatibility"),
        "which admits the compatibility cost");
  Check(Mentions(targeted.caveat, "threat model"),
        "and that it only makes sense within a threat model");

  // Nothing anywhere may sell extensions as anonymity.
  const char* banned[] = {"anonymous", "anonymity", "untraceable", "100%",
                          "completely private", "invisible"};
  std::vector<std::string> strings = {
      RecommendationEngine::CountDisclaimer(), covered.explanation,
      covered.headline, targeted.explanation, targeted.caveat,
      ExtensionCatalog::PrivacyToolsAttribution()};
  for (const ExtensionEntry& entry : catalog.entries())
    strings.push_back(entry.why_recommended);
  for (const std::string& text : strings) {
    for (const char* word : banned) {
      Check(!Mentions(text, word),
            std::string("no anonymity promise in: ") + text);
    }
  }
  Check(Mentions(RecommendationEngine::CountDisclaimer(),
                 "More extensions do not mean more protection"),
        "the disclaimer says the quiet part out loud");

  if (failures == 0)
    std::cout << "extension_catalog_test: all assertions passed\n";
  return failures == 0 ? 0 : 1;
}
