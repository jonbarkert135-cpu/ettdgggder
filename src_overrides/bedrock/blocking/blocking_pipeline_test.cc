// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/blocking/blocking_pipeline.h"

#include <iostream>
#include <string>

namespace {

using namespace bedrock::blocking;         // NOLINT — test-local convenience
using bedrock::privacy::Control;
using bedrock::privacy::ProtectionController;
using bedrock::privacy::Scope;
using bedrock::privacy::Value;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

Request Req(const std::string& url,
            const std::string& etld1,
            ResourceType type = ResourceType::kOther,
            const std::string& top = "news.test") {
  Request request;
  request.url = url;
  request.host = etld1;
  request.etld1 = etld1;
  request.top_host = top;
  request.top_etld1 = top;
  request.type = type;
  return request;
}

}  // namespace

int main() {
  FilterEngine filters;
  filters.AddList(
      "||ads.test^\n"
      "||partner.test/widget.js$redirect=noopjs\n"
      "@@||ads.test/needed.js\n");
  TrackerHeuristic heuristic;
  ProtectionController controls;
  BlockingPipeline pipeline(&filters, &heuristic, &controls);

  // Stage 1: filter lists.
  {
    const Decision decision = pipeline.Evaluate(Req("https://ads.test/x.js", "ads.test"));
    Check(decision.action == Action::kBlock && decision.reason == Reason::kFilterList,
          "list rule blocks");
    Check(decision.detail == "||ads.test^", "the deciding rule is reported");
  }
  {
    const Decision decision =
        pipeline.Evaluate(Req("https://ads.test/needed.js", "ads.test"));
    Check(decision.action == Action::kAllow && decision.reason == Reason::kAllowRule,
          "exception rule allows and says so");
  }
  {
    const Decision decision =
        pipeline.Evaluate(Req("https://partner.test/widget.js", "partner.test"));
    Check(decision.action == Action::kRedirect &&
              decision.redirect_url.compare(0, 5, "data:") == 0,
          "redirect rule serves an inert resource instead of blocking");
  }

  // Stage 0: shields down wins over every later stage — one switch, honoured.
  {
    controls.Set(Scope::kSite, "news.test", Control::kAds, Value::kAllow);
    controls.Set(Scope::kSite, "news.test", Control::kTrackers, Value::kAllow);
    const Decision decision = pipeline.Evaluate(Req("https://ads.test/x.js", "ads.test"));
    Check(decision.action == Action::kAllow && decision.reason == Reason::kShieldsDown,
          "shields down allows a list-blocked request");
    Check(!pipeline.SendPrivacySignals(Req("https://ads.test/x.js", "ads.test")),
          "no GPC/DNT signal when the user turned protection off here");
    controls.Clear(Scope::kSite, "news.test");
    Check(pipeline.SendPrivacySignals(Req("https://ads.test/x.js", "ads.test")),
          "GPC/DNT sent by default");
  }

  // Stage 2: first party. The behavioral layer must not judge the site the
  // user asked for, and it must not learn from it either.
  {
    const Request first_party = Req("https://news.test/app.js", "news.test",
                                    ResourceType::kScript);
    Check(pipeline.Evaluate(first_party).reason == Reason::kFirstParty,
          "first-party request is allowed as first party");
    pipeline.NoteStoredState(first_party, StateKind::kCookie);
    Check(heuristic.SiteCount("news.test") == 0,
          "first-party state is never learned from");
  }

  // Stage 4: behavioral detection, learned only through the pipeline.
  {
    const Request tracker = Req("https://spy.test/p.gif", "spy.test");
    Check(pipeline.Evaluate(tracker).allowed(), "unknown third party may load");
    pipeline.NoteStoredState(tracker, StateKind::kCookie);
    pipeline.NoteStoredState(Req("https://spy.test/p.gif", "spy.test",
                                 ResourceType::kOther, "shop.test"),
                             StateKind::kCookie);
    Check(pipeline.Evaluate(tracker).action == Action::kPartition,
          "two sites: still not blocked, only partitioned by cookie policy");
    pipeline.NoteStoredState(Req("https://spy.test/p.gif", "spy.test",
                                 ResourceType::kOther, "forum.test"),
                             StateKind::kCookie);
    const Decision decision = pipeline.Evaluate(tracker);
    Check(decision.action == Action::kBlock &&
              decision.reason == Reason::kBehavioralTracker,
          "third site: blocked by the local heuristic");
    Check(decision.detail == "spy.test", "the panel is told which domain");

    // Turning tracker protection off for a site releases the heuristic too:
    // the learned verdict is not a second, independent blocker.
    controls.Set(Scope::kSite, "news.test", Control::kTrackers, Value::kAllow);
    Check(pipeline.Evaluate(tracker).allowed(),
          "heuristic verdict respects the per-site tracker setting");
    controls.Clear(Scope::kSite, "news.test");
  }

  // Stage 3: script policy comes from the same shields settings.
  {
    const Request script = Req("https://cdn.test/lib.js", "cdn.test",
                               ResourceType::kScript);
    Check(pipeline.Evaluate(script).allowed(), "third-party script allowed by default");
    controls.Set(Scope::kDomain, "news.test", Control::kScripts, Value::kReduce);
    Check(pipeline.Evaluate(script).reason == Reason::kScriptPolicy &&
              !pipeline.Evaluate(script).allowed(),
          "scripts=reduce blocks third-party scripts");
    Check(pipeline.Evaluate(Req("https://news.test/app.js", "news.test",
                                ResourceType::kScript))
              .allowed(),
          "scripts=reduce still allows first-party scripts");
    controls.Set(Scope::kDomain, "news.test", Control::kScripts, Value::kBlock);
    Check(!pipeline.Evaluate(Req("https://news.test/app.js", "news.test",
                                 ResourceType::kScript))
               .allowed(),
          "scripts=block also blocks first-party scripts");
    controls.Clear(Scope::kDomain, "news.test");
  }

  // Stage 5: unknown third parties load with partitioned storage.
  {
    const Decision decision =
        pipeline.Evaluate(Req("https://cdn.test/logo.png", "cdn.test",
                              ResourceType::kImage));
    Check(decision.action == Action::kPartition &&
              decision.reason == Reason::kThirdPartyCookiePolicy,
          "unknown third party is partitioned, not blocked");
    controls.Set(Scope::kSite, "news.test", Control::kCookies, Value::kAllow);
    Check(pipeline.Evaluate(Req("https://cdn.test/logo.png", "cdn.test",
                                ResourceType::kImage))
                  .action == Action::kAllow,
          "allowing cookies for the site stops the partitioning");
    controls.Clear(Scope::kSite, "news.test");
  }

  // Link cleaning.
  {
    Check(BlockingPipeline::CleanUrl(
              "https://shop.test/item?id=7&utm_source=news&fbclid=abc") ==
              "https://shop.test/item?id=7",
          "tracking params stripped, real params kept");
    Check(BlockingPipeline::CleanUrl("https://shop.test/item?gclid=1") ==
              "https://shop.test/item",
          "query dropped entirely when only tracking params remain");
    Check(BlockingPipeline::CleanUrl("https://shop.test/item?id=7#frag") ==
              "https://shop.test/item?id=7#frag",
          "URL without tracking params is returned unchanged");
    Check(BlockingPipeline::CleanUrl("https://shop.test/i?utm_id=1&x=2#top") ==
              "https://shop.test/i?x=2#top",
          "fragment preserved");
    Check(BlockingPipeline::CleanUrl("https://shop.test/item") ==
              "https://shop.test/item",
          "URL without a query is untouched");
  }

  // Every reason has a human-readable string for the shields panel.
  for (int i = 0; i <= static_cast<int>(Reason::kDefaultAllow); ++i) {
    Check(std::string(BlockingPipeline::ReasonString(static_cast<Reason>(i)))
              .size() > 3,
          "reason " + std::to_string(i) + " is explainable");
  }

  if (failures == 0) {
    std::cout << "blocking_pipeline_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
