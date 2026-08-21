// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_EXTENSIONS_CATALOG_EXTENSION_CATALOG_H_
#define BEDROCK_EXTENSIONS_CATALOG_EXTENSION_CATALOG_H_

#include <cstdint>
#include <string>
#include <vector>

// Bedrock Privacy Extensions — the curated catalog (roadmap items 1–4, 13,
// 20, 21 of the PrivacyTools.io brief).
//
// The catalog is *data*, not code: entries live in
// `catalog/bedrock_privacy_catalog.json`, are validated by
// `scripts/check_catalog.py`, and can be refreshed from a signed static
// manifest without shipping a new browser. This file is the logic that turns
// an entry into an honest recommendation.
//
// Three rules the code enforces rather than documents:
//
//  1. **Nothing third-party is ever labelled "Official BEDROCK Extension".**
//     The badge is derived from *why* the entry is in the catalog
//     (recommended by PrivacyTools.io / recommended by Bedrock / built into
//     Bedrock), never from who ships the UI it appears in. Bedrock does not
//     get to borrow other people's work as its own.
//  2. **Overlap is stated before installation.** A privacy browser that
//     already blocks trackers should say so instead of selling a second
//     tracker blocker. More extensions is more code with page access, more
//     breakage, and a bigger attack surface — not more privacy.
//  3. **A recommendation with a stale verification is marked stale.** Every
//     entry carries `last_verified`; past `kVerificationMaxAgeDays` the UI
//     shows "Verification outdated" instead of pretending the check is
//     current.

namespace bedrock {
namespace catalog {

// What an extension actually does — used for overlap detection.
enum class Capability {
  kContentBlocking,
  kTrackerBlocking,
  kUrlParameterCleaning,
  kCookieCleanup,
  kScriptControl,
  kFingerprintDefense,
  kLocalCdnEmulation,
  kHttpsEnforcement,
  kPasswordManagement,
  kMaxValue = kPasswordManagement,
};

// PrivacyTools.io's three-tier model, used as a *concept* (their wording and
// branding stay theirs — see docs/LICENSING.md).
enum class ThreatLevel {
  kCovered,
  kHardened,
  kTargeted,
};

// Why this entry is in the catalog. Drives the badge text.
enum class Provenance {
  kPrivacyToolsRecommendation,
  kBedrockRecommendation,
  kBuiltIntoBedrock,
};

enum class RiskLevel {
  kLow,
  kMedium,
  kHigh,
};

enum class Compatibility {
  kChromiumMv3,       // works as-is
  kChromiumMv2Only,   // will not load in current Chromium
  kFirefoxOnly,
  kUnknown,
};

enum class Maintenance {
  kActive,
  kSlow,
  kUnmaintained,
};

struct ExtensionEntry {
  std::string id;
  std::string name;
  std::string icon;            // packaged asset path, not a remote URL
  std::string description;
  std::string privacy_purpose;
  std::string official_source; // upstream repository or store page
  std::string license;         // the extension's own license, never ours
  std::string attribution;     // required credit line for this entry
  std::string version;
  std::string why_recommended;
  std::vector<std::string> permissions;
  std::vector<Capability> capabilities;
  ThreatLevel threat_level = ThreatLevel::kCovered;
  Provenance provenance = Provenance::kPrivacyToolsRecommendation;
  Compatibility compatibility = Compatibility::kUnknown;
  Maintenance maintenance = Maintenance::kActive;
  bool open_source = false;
  int64_t last_verified = 0;  // unix seconds
  int performance_cost = 0;   // 0 negligible, 1 noticeable, 2 heavy
};

// What Bedrock already does without any extension.
struct BuiltInProtections {
  std::vector<Capability> capabilities;
  bool Provides(Capability capability) const;
};

struct PermissionAnalysis {
  RiskLevel risk = RiskLevel::kLow;
  std::string why;                       // plain language, no jargon
  std::vector<std::string> permissions;  // as the user will be asked
};

struct OverlapReport {
  std::vector<Capability> already_covered;   // by Bedrock itself
  std::vector<std::string> duplicate_of;     // installed extensions
  bool substantially_duplicate = false;      // triggers the "Install anyway?"
  std::string summary;
};

struct Analysis {
  PermissionAnalysis permissions;
  OverlapReport overlap;
  bool installable = true;      // false when it cannot work in this browser
  bool verification_stale = false;
  std::string compatibility_note;
  std::string maintenance_note;
  std::string privacy_impact;
  std::string performance_impact;
};

class ExtensionCatalog {
 public:
  static constexpr int kVerificationMaxAgeDays = 180;

  explicit ExtensionCatalog(BuiltInProtections built_in);
  ~ExtensionCatalog();

  void Add(const ExtensionEntry& entry);
  const std::vector<ExtensionEntry>& entries() const { return entries_; }
  const ExtensionEntry* Get(const std::string& id) const;

  // Filters PrivacyTools.io-style: open source, no account, free,
  // self-hosted, threat level. Only the ones Bedrock can actually verify from
  // catalog data are offered — a filter the data cannot answer is a lie.
  std::vector<ExtensionEntry> Filter(bool open_source_only,
                                     ThreatLevel max_level) const;

  Analysis Analyze(const ExtensionEntry& entry,
                   const std::vector<std::string>& installed_ids,
                   int64_t now) const;

  // The badge under the name. Never "Official BEDROCK Extension" for work
  // Bedrock did not write.
  static std::string BadgeText(const ExtensionEntry& entry);
  // The "Already protected by BEDROCK" block from the brief.
  std::string CoverageSummary(const ExtensionEntry& entry) const;
  // The pre-install dialog for a duplicate; empty when there is no overlap.
  static std::string DuplicateWarning(const OverlapReport& overlap);

  // Links every entry must expose before install.
  static std::vector<std::string> RequiredLinks(const ExtensionEntry& entry);

  static const char* CapabilityName(Capability capability);
  static const char* ThreatLevelName(ThreatLevel level);
  // The credit PrivacyTools.io's licence asks for, shown wherever their
  // recommendations are used.
  static const char* PrivacyToolsAttribution();
  static const char* PrivacyToolsUrl();

 private:
  BuiltInProtections built_in_;
  std::vector<ExtensionEntry> entries_;
};

}  // namespace catalog
}  // namespace bedrock

#endif  // BEDROCK_EXTENSIONS_CATALOG_EXTENSION_CATALOG_H_
