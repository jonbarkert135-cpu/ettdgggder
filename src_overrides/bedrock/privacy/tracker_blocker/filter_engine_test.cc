// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/privacy/tracker_blocker/filter_engine.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>

namespace {

using namespace bedrock::blocking;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

Request Req(const std::string& url,
            const std::string& etld1,
            const std::string& top_etld1,
            ResourceType type = ResourceType::kOther) {
  Request request;
  request.url = url;
  request.etld1 = etld1;
  request.host = etld1;
  request.top_host = top_etld1;
  request.top_etld1 = top_etld1;
  request.type = type;
  return request;
}

bool Blocks(const FilterEngine& engine, const Request& request) {
  return engine.Match(request).blocked;
}

}  // namespace

int main() {
  // ---- network rules ----
  {
    FilterEngine engine;
    const size_t added = engine.AddList(
        "! Title: test list\n"
        "\n"
        "[Adblock Plus 2.0]\n"
        "||ads.example.com^\n"
        "||tracker.test/pixel.gif\n"
        "/banner-ad.\n"
        "@@||ads.example.com/allowed.js\n"
        "||cdn.test/analytics.js$script,third-party\n"
        "||shop.test/track$image,domain=news.test|~sports.test\n"
        "||partner.test/widget.js$redirect=noopjs\n"
        "|https://exact.test/one|\n"
        "/regex-not-supported/$badoption\n");
    Check(added == 8, "8 rules accepted, unsupported option skipped");

    Check(Blocks(engine, Req("https://ads.example.com/x.js", "example.com",
                             "news.test")),
          "|| matches the host");
    Check(Blocks(engine, Req("https://sub.ads.example.com/x.js", "example.com",
                             "news.test")),
          "|| matches a subdomain boundary");
    Check(!Blocks(engine, Req("https://notads.example.com/x.js", "example.com",
                              "news.test")),
          "|| does not match mid-label");
    Check(!Blocks(engine, Req("https://ads.example.com.evil.test/x.js",
                              "evil.test", "news.test")),
          "^ does not match a letter, so the lookalike domain is not blocked");
    Check(Blocks(engine, Req("https://tracker.test/pixel.gif?id=1",
                             "tracker.test", "news.test")),
          "path pattern matches with a query string");
    Check(Blocks(engine, Req("https://cdn.test/img/banner-ad.png", "cdn.test",
                             "news.test")),
          "unanchored substring rule matches");

    // Exceptions.
    Check(!Blocks(engine, Req("https://ads.example.com/allowed.js",
                              "example.com", "news.test")),
          "@@ exception wins over a block rule");

    // Type and party options.
    Check(Blocks(engine, Req("https://cdn.test/analytics.js", "cdn.test",
                             "news.test", ResourceType::kScript)),
          "$script,third-party matches a third-party script");
    Check(!Blocks(engine, Req("https://cdn.test/analytics.js", "cdn.test",
                              "news.test", ResourceType::kImage)),
          "$script does not match an image");
    Check(!Blocks(engine, Req("https://cdn.test/analytics.js", "cdn.test",
                              "cdn.test", ResourceType::kScript)),
          "$third-party does not match a first-party request");

    // $domain=, including the negated form.
    Check(Blocks(engine, Req("https://shop.test/track", "shop.test",
                             "news.test", ResourceType::kImage)),
          "$domain= matches the listed site");
    Check(!Blocks(engine, Req("https://shop.test/track", "shop.test",
                              "other.test", ResourceType::kImage)),
          "$domain= does not match an unlisted site");
    Check(!Blocks(engine, Req("https://shop.test/track", "shop.test",
                              "sports.test", ResourceType::kImage)),
          "$domain=~ excludes the site");

    // Anchors.
    Check(Blocks(engine, Req("https://exact.test/one", "exact.test",
                             "news.test")),
          "|...| matches the exact URL");
    Check(!Blocks(engine, Req("https://exact.test/one/two", "exact.test",
                              "news.test")),
          "trailing | requires the URL to end there");

    // Redirects.
    const MatchResult redirect = engine.Match(
        Req("https://partner.test/widget.js", "partner.test", "news.test"));
    Check(redirect.blocked && redirect.redirect == "noopjs",
          "$redirect= reports the resource");
    Check(FilterEngine::RedirectResource("noopjs").compare(0, 5, "data:") == 0,
          "redirect resource is an inert data URL");
    Check(FilterEngine::RedirectResource("nope").empty(),
          "unknown redirect resource is empty");
  }

  // ---- $important beats an exception ----
  {
    FilterEngine engine;
    engine.AddList("@@||site.test^\n||site.test/ads$important\n");
    Check(Blocks(engine, Req("https://site.test/ads", "site.test", "news.test")),
          "$important overrides @@");
    Check(!Blocks(engine, Req("https://site.test/news", "site.test",
                              "news.test")),
          "the exception still applies elsewhere");
  }

  // ---- cosmetic rules ----
  {
    FilterEngine engine;
    engine.AddList(
        "##.generic-ad\n"
        "news.test##.sidebar-ad\n"
        "news.test##div.promo:has-text(Sponsored)\n"
        "shop.test##.cart-ad\n"
        "news.test#@#.generic-ad\n");
    Check(engine.cosmetic_rule_count() == 5, "5 cosmetic rules parsed");

    const auto news = engine.CosmeticsFor("news.test");
    const auto has = [](const std::vector<std::string>& list,
                        const std::string& value) {
      return std::find(list.begin(), list.end(), value) != list.end();
    };
    Check(has(news.selectors, ".sidebar-ad"), "site-specific selector applies");
    Check(!has(news.selectors, ".generic-ad"),
          "#@# removes the generic selector on this site");
    Check(!has(news.selectors, ".cart-ad"), "other site's selector stays out");
    Check(has(news.procedural, "div.promo:has-text(Sponsored)"),
          "procedural selector is separated from plain CSS");
    Check(!has(news.selectors, "div.promo:has-text(Sponsored)"),
          "procedural selector is not injected as CSS");

    const auto other = engine.CosmeticsFor("other.test");
    Check(has(other.selectors, ".generic-ad"),
          "generic selector applies where it is not excepted");
    Check(other.procedural.empty(), "no procedural rules for other sites");

    const auto subdomain = engine.CosmeticsFor("www.news.test");
    Check(has(subdomain.selectors, ".sidebar-ad"),
          "cosmetic domain rules cover subdomains");
  }

  // ---- user rules: add, export, remove ----
  {
    FilterEngine engine;
    engine.AddList("||fromlist.test^\n");
    engine.AddRule("||user-blocked.test^");
    engine.AddRule("shop.test##.user-hidden");
    Check(engine.ExportRules() ==
              "shop.test##.user-hidden\n||user-blocked.test^\n",
          "export contains user rules only, not list rules");

    FilterEngine imported;
    imported.AddList(engine.ExportRules());
    Check(Blocks(imported, Req("https://user-blocked.test/x", "user-blocked.test",
                               "news.test")),
          "exported rules round-trip through import");

    Check(engine.RemoveRule("||user-blocked.test^"), "rule removed");
    Check(!Blocks(engine, Req("https://user-blocked.test/x", "user-blocked.test",
                              "news.test")),
          "removed rule no longer blocks");
    Check(Blocks(engine, Req("https://fromlist.test/x", "fromlist.test",
                             "news.test")),
          "removing a user rule does not drop the loaded lists");
    Check(!engine.RemoveRule("||never-added.test^"),
          "removing an unknown rule reports false");
  }

  // ---- the index has to actually make matching cheap ----
  {
    FilterEngine engine;
    std::string list;
    for (int i = 0; i < 50000; ++i) {
      list += "||tracker" + std::to_string(i) + ".test/pixel-" +
              std::to_string(i) + ".gif\n";
    }
    engine.AddList(list);
    Check(engine.network_rule_count() == 50000, "50k rules loaded");

    const Request hit =
        Req("https://tracker49999.test/pixel-49999.gif", "tracker49999.test",
            "news.test");
    const Request miss =
        Req("https://example.test/some/normal/path.html?a=1", "example.test",
            "news.test");
    Check(Blocks(engine, hit), "the last rule of 50k still matches");
    Check(!Blocks(engine, miss), "an unrelated URL is not blocked");

    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 20000; ++i) {
      engine.Match(miss);
    }
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();
    const double per_match = static_cast<double>(micros) / 20000.0;
    std::cout << "  50k rules, " << per_match << " us per non-matching URL\n";
    // A linear scan of 50k patterns is ~1000x slower than this. The bound is
    // loose so the test does not flake on a busy CI box, but it still fails if
    // someone replaces the index with a loop.
    Check(per_match < 20.0, "matching stays sub-20us against 50k rules");
  }

  if (failures == 0) {
    std::cout << "filter_engine_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
