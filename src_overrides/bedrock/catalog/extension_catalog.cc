// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/catalog/extension_catalog.h"

#include <algorithm>

namespace bedrock {
namespace catalog {
namespace {

// Permissions that let an extension read or change what the user sees.
bool IsHighRisk(const std::string& permission) {
  static const char* kHigh[] = {"<all_urls>", "webRequest",
                                "webRequestBlocking", "debugger", "proxy",
                                "cookies", "history", "declarativeNetRequest",
                                "scripting", "nativeMessaging"};
  for (const char* candidate : kHigh) {
    if (permission == candidate)
      return true;
  }
  return false;
}

bool IsMediumRisk(const std::string& permission) {
  // "storage" is the extension's own storage — it says nothing about the
  // user's browsing, so it does not raise the risk level.
  static const char* kMedium[] = {"tabs", "activeTab", "browsingData",
                                  "privacy", "downloads", "bookmarks"};
  for (const char* candidate : kMedium) {
    if (permission == candidate)
      return true;
  }
  return false;
}

}  // namespace

bool BuiltInProtections::Provides(Capability capability) const {
  return std::find(capabilities.begin(), capabilities.end(), capability) !=
         capabilities.end();
}

ExtensionCatalog::ExtensionCatalog(BuiltInProtections built_in)
    : built_in_(std::move(built_in)) {}

ExtensionCatalog::~ExtensionCatalog() = default;

void ExtensionCatalog::Add(const ExtensionEntry& entry) {
  entries_.push_back(entry);
}

const ExtensionEntry* ExtensionCatalog::Get(const std::string& id) const {
  for (const ExtensionEntry& entry : entries_) {
    if (entry.id == id)
      return &entry;
  }
  return nullptr;
}

std::vector<ExtensionEntry> ExtensionCatalog::Filter(
    bool open_source_only,
    ThreatLevel max_level) const {
  std::vector<ExtensionEntry> results;
  for (const ExtensionEntry& entry : entries_) {
    if (open_source_only && !entry.open_source)
      continue;
    if (static_cast<int>(entry.threat_level) > static_cast<int>(max_level))
      continue;
    results.push_back(entry);
  }
  return results;
}

const char* ExtensionCatalog::CapabilityName(Capability capability) {
  switch (capability) {
    case Capability::kContentBlocking:
      return "Ad and content blocking";
    case Capability::kTrackerBlocking:
      return "Tracker blocking";
    case Capability::kUrlParameterCleaning:
      return "Tracking parameter filtering";
    case Capability::kCookieCleanup:
      return "Cookie cleanup";
    case Capability::kScriptControl:
      return "Script control";
    case Capability::kFingerprintDefense:
      return "Fingerprint protection";
    case Capability::kLocalCdnEmulation:
      return "Local CDN emulation";
    case Capability::kHttpsEnforcement:
      return "HTTPS enforcement";
    case Capability::kPasswordManagement:
      return "Password management";
  }
  return "Unknown";
}

const char* ExtensionCatalog::ThreatLevelName(ThreatLevel level) {
  switch (level) {
    case ThreatLevel::kCovered:
      return "Covered";
    case ThreatLevel::kHardened:
      return "Hardened";
    case ThreatLevel::kTargeted:
      return "Targeted";
  }
  return "Covered";
}

const char* ExtensionCatalog::PrivacyToolsAttribution() {
  // Their licence asks for one thing: a clear, working credit link. It is
  // cheap to honour and it is the entire price of the catalog.
  return "Privacy recommendations and selected reference materials: "
         "PrivacyTools.io";
}

const char* ExtensionCatalog::PrivacyToolsUrl() {
  return "https://www.privacytools.io";
}

std::string ExtensionCatalog::BadgeText(const ExtensionEntry& entry) {
  switch (entry.provenance) {
    case Provenance::kPrivacyToolsRecommendation:
      return "Recommended by PrivacyTools.io";
    case Provenance::kBedrockRecommendation:
      return "Recommended by BEDROCK";
    case Provenance::kBuiltIntoBedrock:
      return "Built into BEDROCK";
  }
  return "Recommended by BEDROCK";
}

namespace {

PermissionAnalysis AnalyzePermissions(const ExtensionEntry& entry) {
  PermissionAnalysis analysis;
  analysis.permissions = entry.permissions;
  int high = 0;
  int medium = 0;
  for (const std::string& permission : entry.permissions) {
    if (IsHighRisk(permission))
      ++high;
    else if (IsMediumRisk(permission))
      ++medium;
  }
  if (high > 0) {
    analysis.risk = RiskLevel::kHigh;
    analysis.why =
        "This extension can observe and change the content of pages you "
        "visit.";
  } else if (medium > 0) {
    analysis.risk = RiskLevel::kMedium;
    analysis.why =
        "This extension can see which pages are open, but not their content.";
  } else {
    analysis.risk = RiskLevel::kLow;
    analysis.why = "This extension asks for no access to your browsing.";
  }
  return analysis;
}

}  // namespace

std::string ExtensionCatalog::CoverageSummary(
    const ExtensionEntry& entry) const {
  std::string text = "Already protected by BEDROCK\n";
  bool any = false;
  for (Capability capability : built_in_.capabilities) {
    text += std::string("\u2713 ") + CapabilityName(capability) + "\n";
    any = true;
  }
  if (!any)
    text = "BEDROCK provides no built-in protection in this area.\n";
  text += "\nOptional extension:\n" + entry.name + "\n";
  text += "\nReason:\n" + entry.why_recommended + "\n";
  return text;
}

std::string ExtensionCatalog::DuplicateWarning(const OverlapReport& overlap) {
  if (!overlap.substantially_duplicate)
    return std::string();
  return "BEDROCK already provides similar protection.\n\n"
         "Installing another extension may:\n"
         "\u2022 duplicate functionality\n"
         "\u2022 increase permissions\n"
         "\u2022 increase attack surface\n"
         "\u2022 reduce compatibility\n\n"
         "Install anyway?";
}

std::vector<std::string> ExtensionCatalog::RequiredLinks(
    const ExtensionEntry& entry) {
  // The brief asks for three, and they are exactly the three that let a user
  // check us instead of trusting us.
  return {"View source: " + entry.official_source,
          "View license: " + entry.license,
          "View official repository: " + entry.official_source};
}

Analysis ExtensionCatalog::Analyze(const ExtensionEntry& entry,
                                   const std::vector<std::string>& installed_ids,
                                   int64_t now) const {
  Analysis analysis;
  analysis.permissions = AnalyzePermissions(entry);

  // Compatibility: an MV2-only extension cannot load, and saying so beats a
  // failed install.
  switch (entry.compatibility) {
    case Compatibility::kChromiumMv3:
      analysis.compatibility_note = "Chromium / current";
      break;
    case Compatibility::kChromiumMv2Only:
      analysis.compatibility_note =
          "Manifest V2 only — this cannot run in the current engine";
      analysis.installable = false;
      break;
    case Compatibility::kFirefoxOnly:
      analysis.compatibility_note = "Firefox only";
      analysis.installable = false;
      break;
    case Compatibility::kUnknown:
      analysis.compatibility_note = "Compatibility not verified";
      analysis.installable = false;
      break;
  }

  switch (entry.maintenance) {
    case Maintenance::kActive:
      analysis.maintenance_note = "Actively maintained";
      break;
    case Maintenance::kSlow:
      analysis.maintenance_note = "Updated infrequently";
      break;
    case Maintenance::kUnmaintained:
      analysis.maintenance_note =
          "No longer maintained — an unmaintained extension with page access "
          "is a risk, not a feature";
      analysis.installable = false;
      break;
  }

  // Staleness (brief item 13): an unchecked recommendation is not a current
  // one, and we say which it is.
  const int64_t age_days = (now - entry.last_verified) / 86400;
  analysis.verification_stale = age_days > kVerificationMaxAgeDays;

  // Overlap with what Bedrock already does, and with what is installed.
  for (Capability capability : entry.capabilities) {
    if (built_in_.Provides(capability))
      analysis.overlap.already_covered.push_back(capability);
  }
  for (const std::string& installed : installed_ids) {
    const ExtensionEntry* other = Get(installed);
    if (!other || other->id == entry.id)
      continue;
    for (Capability capability : entry.capabilities) {
      if (std::find(other->capabilities.begin(), other->capabilities.end(),
                    capability) != other->capabilities.end()) {
        analysis.overlap.duplicate_of.push_back(other->name);
        break;
      }
    }
  }
  // "Substantially duplicate" means every capability it offers is already
  // handled. A tool that adds one new capability is a specialised layer, not
  // a duplicate — that distinction is the difference between a curated store
  // and a pile of extensions.
  analysis.overlap.substantially_duplicate =
      !entry.capabilities.empty() &&
      analysis.overlap.already_covered.size() == entry.capabilities.size();
  if (!analysis.overlap.duplicate_of.empty())
    analysis.overlap.substantially_duplicate = true;

  if (analysis.overlap.substantially_duplicate) {
    analysis.overlap.summary =
        "Everything this extension does is already handled here.";
  } else if (!analysis.overlap.already_covered.empty()) {
    analysis.overlap.summary =
        "Partly covered by BEDROCK; adds a specialised layer.";
  } else {
    analysis.overlap.summary = "Adds protection BEDROCK does not provide.";
  }

  analysis.privacy_impact =
      analysis.permissions.risk == RiskLevel::kHigh
          ? "Gains access to page content, so it must be trusted as much as "
            "the browser itself."
          : "Limited access to browsing data.";
  switch (entry.performance_cost) {
    case 0:
      analysis.performance_impact = "Negligible";
      break;
    case 1:
      analysis.performance_impact = "Noticeable on heavy pages";
      break;
    default:
      analysis.performance_impact = "Heavy — expect slower page loads";
      break;
  }
  return analysis;
}

}  // namespace catalog
}  // namespace bedrock
