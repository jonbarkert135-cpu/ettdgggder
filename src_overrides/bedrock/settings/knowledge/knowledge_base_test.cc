// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/settings/knowledge/knowledge_base.h"

#include <cstdint>
#include <iostream>
#include <set>
#include <string>

namespace {

using namespace bedrock::knowledge;  // NOLINT — test-local convenience

int failures = 0;
constexpr int64_t kNow = 1'787'000'000;
constexpr int64_t kDay = 86400;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

Article NativeGuide(const std::string& id,
                    const std::string& title,
                    Category category) {
  Article article;
  article.id = id;
  article.title = title;
  article.summary = "A Bedrock explanation of " + title + ".";
  article.body = "Long form text written for Bedrock.";
  article.category = category;
  article.layer = Layer::kNativeGuide;
  article.available_offline = true;
  article.last_verified = kNow - 10 * kDay;
  article.tags = {"bedrock"};
  article.provenance.source = "BEDROCK";
  article.provenance.author = "The Bedrock Authors";
  article.provenance.license = "MPL-2.0";
  article.provenance.published = "2026-08-01";
  article.provenance.updated = "2026-08-21";
  article.provenance.redistribution_allowed = true;
  return article;
}

Article ExternalCard(const std::string& id,
                     const std::string& title,
                     Category category) {
  Article article;
  article.id = id;
  article.title = title;
  article.summary = "Guide hosted by the original publisher.";
  article.category = category;
  article.layer = Layer::kExternalReference;
  article.available_offline = false;
  article.last_verified = kNow - 5 * kDay;
  article.provenance.source = "PrivacyTools.io";
  article.provenance.source_url = "https://www.privacytools.io";
  article.provenance.author = "PrivacyTools.io";
  article.provenance.license = "VERNAM License";
  article.provenance.published = "2026-06-23";
  article.provenance.updated = "2026-06-23";
  article.provenance.attribution_required = true;
  article.provenance.attribution_text =
      "Privacy recommendations and selected reference materials: "
      "PrivacyTools.io";
  article.provenance.contains_third_party_material = true;
  return article;
}

}  // namespace

int main() {
  KnowledgeBase kb;

  // All 22 categories from the brief exist and are named.
  Check(KnowledgeBase::Categories().size() == 22, "22 categories");
  std::set<std::string> names;
  for (Category category : KnowledgeBase::Categories())
    names.insert(KnowledgeBase::CategoryName(category));
  Check(names.size() == 22, "each category has a distinct name");
  Check(names.count("Threat Modeling") == 1, "including threat modeling");
  Check(names.count("Travel Privacy") == 1, "and travel privacy");

  // ---- item 7: nothing is republished without the right to republish ----
  Article copied = NativeGuide("copied", "Someone Else's Guide",
                               Category::kTor);
  copied.provenance.source = "Someone Else";
  copied.provenance.redistribution_allowed = false;
  Check(!kb.Add(copied),
        "a local copy is refused when redistribution is not allowed");

  Article no_credit = ExternalCard("nc", "Card", Category::kTor);
  no_credit.provenance.attribution_text.clear();
  Check(!kb.Add(no_credit),
        "an entry that requires attribution must carry the attribution text");

  Article smuggled = ExternalCard("smuggled", "Card", Category::kTor);
  smuggled.body = "The whole article, pasted in.";
  Check(!kb.Add(smuggled),
        "a reference card may not carry a copy of the article body");

  // ---- Layer A ----
  Check(kb.Add(NativeGuide("fingerprinting", "How Browser Fingerprinting Works",
                           Category::kFingerprinting)),
        "a native guide is accepted");
  Check(kb.Add(NativeGuide("third-party", "How Third-Party Tracking Works",
                           Category::kTracking)),
        "and another");
  Check(kb.Add(NativeGuide("cookies", "How Cookies Track Users",
                           Category::kCookies)),
        "and another");
  Check(kb.Add(NativeGuide("tor-limits", "What Tor Does and Does Not Protect",
                           Category::kTor)),
        "including the one about limits");

  // ---- Layer B ----
  Check(kb.Add(ExternalCard("pt-tor", "Installing and using Desktop Tor Browser",
                            Category::kTor)),
        "an external reference card is accepted");
  const Article* card = kb.Get("pt-tor");
  Check(card->body.empty(), "the card stores no copied text");

  const SourceBadge badge = KnowledgeBase::BadgeFor(*card);
  Check(badge.external, "the card is marked external");
  Check(badge.source == "PrivacyTools.io", "the badge names the source");
  Check(badge.url == "https://www.privacytools.io", "and links to it");
  Check(badge.label == "Original article" && badge.action == "Open source website",
        "with the wording from the brief");

  const SourceBadge own = KnowledgeBase::BadgeFor(*kb.Get("cookies"));
  Check(!own.external && own.source == "BEDROCK",
        "a Bedrock guide is clearly ours, so the boundary is visible");

  // Every third-party entry keeps its own provenance record.
  for (const Article& article : kb.articles()) {
    Check(!article.provenance.source.empty(), "every entry names a source");
    Check(!article.provenance.license.empty(), "and a license");
    Check(!article.provenance.updated.empty(), "and an update date");
    if (article.layer == Layer::kExternalReference) {
      Check(!article.provenance.source_url.empty(),
            "an external entry links to the original");
      Check(article.provenance.license != "MPL-2.0",
            "we do not relabel someone else's material with our license");
    }
  }

  // ---- item 10: local-first search ----
  Check(kb.Search("fingerprinting").size() >= 1, "title search works");
  Check(kb.Search("Tor").size() == 2, "category and title both match");
  Check(kb.Search("privacytools.io").size() == 1, "source is searchable");
  Check(kb.Search("bedrock").size() == 4, "tags are searchable");
  Check(kb.Search("").empty(), "an empty query returns nothing");

  const auto offline_first = kb.SearchOffline("tor");
  Check(offline_first.size() == 2, "both Tor entries match");
  Check(offline_first[0].available_offline,
        "offline material ranks above a card that needs the network");

  // ---- item 11: offline-first ----
  int offline = 0;
  for (const Article& article : kb.articles())
    offline += article.available_offline ? 1 : 0;
  Check(offline == 4, "every native guide is available without a network");

  // ---- items 16–17: contextual help ----
  kb.LinkSetting("privacy.cookies.third_party", "cookies");
  kb.LinkSetting("privacy.fingerprinting", "fingerprinting");
  Check(kb.HelpFor("privacy.cookies.third_party") != nullptr,
        "a setting can link to the article that explains it");
  Check(kb.HelpFor("privacy.cookies.third_party")->title ==
            "How Cookies Track Users",
        "and it is the right article");
  Check(kb.HelpFor("privacy.unknown") == nullptr,
        "a setting with nothing honest to link to links to nothing");
  Check(kb.HelpFor("privacy.fingerprinting")->available_offline,
        "contextual help works offline, where the user actually is");

  // ---- staleness ----
  Article old_card = ExternalCard("old", "Old guide", Category::kVpn);
  old_card.last_verified = kNow - 500 * kDay;
  kb.Add(old_card);
  Check(kb.IsStale(*kb.Get("old"), kNow), "an unchecked entry is stale");
  Check(!kb.IsStale(*kb.Get("pt-tor"), kNow), "a recent one is not");

  // Nothing in the knowledge base promises anonymity.
  const char* banned[] = {"anonymous", "anonymity", "untraceable", "100%",
                          "completely private", "invisible"};
  for (const Article& article : kb.articles()) {
    for (const char* word : banned) {
      Check(article.title.find(word) == std::string::npos &&
                article.summary.find(word) == std::string::npos,
            std::string("no anonymity promise in: ") + article.title);
    }
  }

  if (failures == 0)
    std::cout << "knowledge_base_test: all assertions passed\n";
  return failures == 0 ? 0 : 1;
}
