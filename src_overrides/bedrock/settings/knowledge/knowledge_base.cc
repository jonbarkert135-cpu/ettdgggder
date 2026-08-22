// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/settings/knowledge/knowledge_base.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace bedrock {
namespace knowledge {
namespace {

std::string Lower(const std::string& text) {
  std::string out = text;
  for (char& c : out)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

bool Contains(const std::string& haystack, const std::string& needle) {
  return Lower(haystack).find(Lower(needle)) != std::string::npos;
}

}  // namespace

KnowledgeBase::KnowledgeBase() = default;
KnowledgeBase::~KnowledgeBase() = default;

const std::vector<Category>& KnowledgeBase::Categories() {
  static const std::vector<Category> kAll = {
      Category::kPrivacyBasics,     Category::kThreatModeling,
      Category::kBrowsers,          Category::kBrowserExtensions,
      Category::kSearchEngines,     Category::kEmail,
      Category::kMessaging,         Category::kPasswords,
      Category::kCloudStorage,      Category::kDns,
      Category::kVpn,               Category::kTor,
      Category::kOperatingSystems,  Category::kMobilePrivacy,
      Category::kMetadata,          Category::kTracking,
      Category::kFingerprinting,    Category::kCookies,
      Category::kEncryption,        Category::kAccountSecurity,
      Category::kDigitalFootprint,  Category::kTravelPrivacy};
  return kAll;
}

const char* KnowledgeBase::CategoryName(Category category) {
  switch (category) {
    case Category::kPrivacyBasics: return "Privacy Basics";
    case Category::kThreatModeling: return "Threat Modeling";
    case Category::kBrowsers: return "Browsers";
    case Category::kBrowserExtensions: return "Browser Extensions";
    case Category::kSearchEngines: return "Search Engines";
    case Category::kEmail: return "Email";
    case Category::kMessaging: return "Messaging";
    case Category::kPasswords: return "Passwords";
    case Category::kCloudStorage: return "Cloud Storage";
    case Category::kDns: return "DNS";
    case Category::kVpn: return "VPN";
    case Category::kTor: return "Tor";
    case Category::kOperatingSystems: return "Operating Systems";
    case Category::kMobilePrivacy: return "Mobile Privacy";
    case Category::kMetadata: return "Metadata";
    case Category::kTracking: return "Tracking";
    case Category::kFingerprinting: return "Fingerprinting";
    case Category::kCookies: return "Cookies";
    case Category::kEncryption: return "Encryption";
    case Category::kAccountSecurity: return "Account Security";
    case Category::kDigitalFootprint: return "Digital Footprint";
    case Category::kTravelPrivacy: return "Travel Privacy";
  }
  return "Privacy Basics";
}

bool KnowledgeBase::Add(const Article& article) {
  if (article.layer == Layer::kNativeGuide) {
    // Storing the text locally *is* republication. If the provenance does not
    // allow it, the entry belongs in Layer B as a link, not in our library.
    if (!article.provenance.redistribution_allowed)
      return false;
    if (article.body.empty())
      return false;
  } else {
    // A reference card must have somewhere to point, and must not smuggle a
    // copy of the article in its "summary".
    if (article.provenance.source_url.empty())
      return false;
    if (!article.body.empty())
      return false;
  }
  if (article.provenance.attribution_required &&
      article.provenance.attribution_text.empty()) {
    return false;
  }
  articles_.push_back(article);
  return true;
}

const Article* KnowledgeBase::Get(const std::string& id) const {
  for (const Article& article : articles_) {
    if (article.id == id)
      return &article;
  }
  return nullptr;
}

std::vector<Article> KnowledgeBase::InCategory(Category category) const {
  std::vector<Article> found;
  for (const Article& article : articles_) {
    if (article.category == category)
      found.push_back(article);
  }
  return found;
}

std::vector<Article> KnowledgeBase::Search(const std::string& query) const {
  std::vector<Article> results;
  if (query.empty())
    return results;
  for (const Article& article : articles_) {
    bool hit = Contains(article.title, query) ||
               Contains(article.summary, query) ||
               Contains(CategoryName(article.category), query) ||
               Contains(article.provenance.source, query);
    for (const std::string& tag : article.tags)
      hit = hit || Contains(tag, query);
    for (const std::string& keyword : article.keywords)
      hit = hit || Contains(keyword, query);
    if (!hit) {
      // Threat level is searchable as a word: "targeted" should find the
      // Targeted material.
      const char* level = article.threat_level == ThreatLevel::kTargeted
                              ? "targeted"
                              : article.threat_level == ThreatLevel::kHardened
                                    ? "hardened"
                                    : "covered";
      hit = Contains(level, query);
    }
    if (hit)
      results.push_back(article);
  }
  return results;
}

std::vector<Article> KnowledgeBase::SearchOffline(
    const std::string& query) const {
  std::vector<Article> results = Search(query);
  std::stable_sort(results.begin(), results.end(),
                   [](const Article& a, const Article& b) {
                     return a.available_offline && !b.available_offline;
                   });
  return results;
}

SourceBadge KnowledgeBase::BadgeFor(const Article& article) {
  SourceBadge badge;
  if (article.layer == Layer::kNativeGuide) {
    badge.source = "BEDROCK";
    badge.url = "";
    badge.label = "Bedrock guide";
    badge.action = "";
    badge.external = false;
    return badge;
  }
  badge.source = article.provenance.source;
  badge.url = article.provenance.source_url;
  badge.label = "Original article";
  badge.action = "Open source website";
  badge.external = true;
  return badge;
}

void KnowledgeBase::LinkSetting(const std::string& setting_key,
                                const std::string& id) {
  setting_links_.emplace_back(setting_key, id);
}

const Article* KnowledgeBase::HelpFor(const std::string& setting_key) const {
  for (const auto& link : setting_links_) {
    if (link.first == setting_key)
      return Get(link.second);
  }
  return nullptr;
}

bool KnowledgeBase::IsStale(const Article& article, int64_t now) const {
  return (now - article.last_verified) / 86400 > kVerificationMaxAgeDays;
}

}  // namespace knowledge
}  // namespace bedrock
