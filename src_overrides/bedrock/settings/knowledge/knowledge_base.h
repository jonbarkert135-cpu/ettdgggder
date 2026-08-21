// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_SETTINGS_KNOWLEDGE_KNOWLEDGE_BASE_H_
#define BEDROCK_SETTINGS_KNOWLEDGE_KNOWLEDGE_BASE_H_

#include <cstdint>
#include <string>
#include <vector>

// Privacy Knowledge Center (brief items 6–11, 14–17).
//
// Two layers, and the difference between them is a licensing decision, not a
// design one:
//
//   Layer A — **native Bedrock articles**. Written here, stored offline,
//     freely readable without a network. They may cite anyone.
//   Layer B — **external reference cards**. A title, a source badge and a
//     link out. Bedrock does not mirror text it has no clear right to
//     republish, and a card that says "read this on PrivacyTools.io" is
//     honest where a copied page would not be.
//
// Every entry carries its own provenance record — source, author, license,
// published/updated dates, whether redistribution is allowed, whether
// attribution is required, whether it contains third-party material. Layer A
// entries may only exist with `redistribution_allowed`, and the test enforces
// it: that single assertion is what stops the knowledge base from quietly
// becoming a scraper.
//
// A licence covering a site does **not** cover the third-party tools, guides
// and trademarks that site links to. Each of those keeps its own record.

namespace bedrock {
namespace knowledge {

enum class Category {
  kPrivacyBasics,
  kThreatModeling,
  kBrowsers,
  kBrowserExtensions,
  kSearchEngines,
  kEmail,
  kMessaging,
  kPasswords,
  kCloudStorage,
  kDns,
  kVpn,
  kTor,
  kOperatingSystems,
  kMobilePrivacy,
  kMetadata,
  kTracking,
  kFingerprinting,
  kCookies,
  kEncryption,
  kAccountSecurity,
  kDigitalFootprint,
  kTravelPrivacy,
  kMaxValue = kTravelPrivacy,
};

enum class Layer {
  kNativeGuide,       // written by Bedrock, stored locally
  kExternalReference  // a card that links out
};

enum class ThreatLevel {
  kCovered,
  kHardened,
  kTargeted,
};

// Everything item 7 asks to check before a single word is reused.
struct Provenance {
  std::string source;            // "Bedrock" or the publication
  std::string source_url;
  std::string author;
  std::string license;           // the material's own license
  std::string published;         // ISO date
  std::string updated;           // ISO date
  bool redistribution_allowed = false;
  bool attribution_required = false;
  bool contains_third_party_material = false;
  std::string attribution_text;  // shown when required
};

struct Article {
  std::string id;
  std::string title;
  std::string summary;
  std::string body;  // empty for Layer B — we link instead of copying
  Category category = Category::kPrivacyBasics;
  Layer layer = Layer::kNativeGuide;
  ThreatLevel threat_level = ThreatLevel::kCovered;
  std::vector<std::string> tags;
  std::vector<std::string> keywords;
  Provenance provenance;
  bool available_offline = false;
  int64_t last_verified = 0;
};

// The source badge for Layer B (item 9): the user must be able to see where
// Bedrock ends and someone else's site begins.
struct SourceBadge {
  std::string source;
  std::string url;
  std::string label;          // "Original article"
  std::string action;         // "Open source website"
  bool external = true;
};

class KnowledgeBase {
 public:
  static constexpr int kVerificationMaxAgeDays = 365;

  KnowledgeBase();
  ~KnowledgeBase();

  // Returns false (and adds nothing) for a Layer A article whose provenance
  // does not permit republication.
  bool Add(const Article& article);

  const std::vector<Article>& articles() const { return articles_; }
  const Article* Get(const std::string& id) const;
  static const std::vector<Category>& Categories();
  static const char* CategoryName(Category category);
  std::vector<Article> InCategory(Category category) const;

  // Local-first search over title, tags, category, threat level, source and
  // keywords. Nothing is sent anywhere: the index is the local library.
  std::vector<Article> Search(const std::string& query) const;
  // Offline articles rank above cards that need the network — a search result
  // that cannot be opened on a plane is a worse answer.
  std::vector<Article> SearchOffline(const std::string& query) const;

  static SourceBadge BadgeFor(const Article& article);
  // Contextual help (items 16–17): a settings key -> the article that
  // explains it. Returns nullptr when there is nothing honest to link to.
  const Article* HelpFor(const std::string& setting_key) const;
  void LinkSetting(const std::string& setting_key, const std::string& id);

  bool IsStale(const Article& article, int64_t now) const;

 private:
  std::vector<Article> articles_;
  std::vector<std::pair<std::string, std::string>> setting_links_;
};

}  // namespace knowledge
}  // namespace bedrock

#endif  // BEDROCK_SETTINGS_KNOWLEDGE_KNOWLEDGE_BASE_H_
