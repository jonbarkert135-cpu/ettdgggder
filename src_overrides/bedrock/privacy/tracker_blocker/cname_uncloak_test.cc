// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/privacy/tracker_blocker/cname_uncloak.h"

#include <iostream>
#include <string>

#include "bedrock/privacy/core/protection_controller.h"
#include "bedrock/privacy/tracker_blocker/blocking_pipeline.h"
#include "bedrock/privacy/tracker_blocker/filter_engine.h"
#include "bedrock/privacy/tracker_blocker/tracker_heuristic.h"

namespace {

using bedrock::blocking::Action;
using bedrock::blocking::BlockingPipeline;
using bedrock::blocking::CnameUncloaker;
using bedrock::blocking::Decision;
using bedrock::blocking::FilterEngine;
using bedrock::blocking::Reason;
using bedrock::blocking::Request;
using bedrock::blocking::ResourceType;
using bedrock::blocking::TrackerHeuristic;
using bedrock::blocking::UncloakStatus;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

// A subresource on the visited site's own subdomain: the shape CNAME cloaking
// takes. Nothing here looks third party.
Request Cloaked(const std::string& host = "metrics.shop.test") {
  Request request;
  request.url = "https://" + host + "/collect?e=1";
  request.host = host;
  request.etld1 = "shop.test";
  request.top_host = "www.shop.test";
  request.top_etld1 = "shop.test";
  request.type = ResourceType::kScript;
  return request;
}

}  // namespace

int main() {
  // Eligibility: only where an alias could change the decision.
  {
    Check(CnameUncloaker::Eligible(Cloaked()),
          "a first-party-looking subdomain subresource is eligible");

    Request apex = Cloaked("shop.test");
    Check(!CnameUncloaker::Eligible(apex),
          "the site's own apex is not asked about");

    Request document = Cloaked();
    document.type = ResourceType::kDocument;
    Check(!CnameUncloaker::Eligible(document),
          "top-level document loads are never uncloaked");

    Request third_party = Cloaked();
    third_party.etld1 = "tracker.test";
    Check(!CnameUncloaker::Eligible(third_party),
          "an already-third-party request needs no alias to be seen");
  }

  // A miss never resolves anything; it queues the name for the embedder.
  {
    CnameUncloaker uncloaker;
    uncloaker.SetNow(1000);
    const auto miss = uncloaker.Canonical(Cloaked());
    Check(miss.status == UncloakStatus::kUnknown, "a cold cache reports kUnknown");
    Check(!miss.uncloaked(), "an unknown host is not treated as uncloaked");

    const auto pending = uncloaker.TakePendingHosts();
    Check(pending.size() == 1 && pending[0] == "metrics.shop.test",
          "the name is queued for the user's resolver, once");
    Check(uncloaker.TakePendingHosts().empty(),
          "taking the queue clears it");
  }

  // Deduplication and the queue ceiling: page content decides how many names
  // arrive, so it must not decide how much memory that costs.
  {
    CnameUncloaker uncloaker;
    uncloaker.SetNow(1);
    for (int i = 0; i < 4; ++i) {
      uncloaker.Canonical(Cloaked());
    }
    Check(uncloaker.TakePendingHosts().size() == 1,
          "the same host is queued once, not once per request");

    for (int i = 0; i < 400; ++i) {
      uncloaker.Canonical(Cloaked("h" + std::to_string(i) + ".shop.test"));
    }
    Check(uncloaker.TakePendingHosts().size() ==
              CnameUncloaker::kMaxPendingHosts,
          "the pending queue is bounded");
  }

  // The three resolver answers that are not cloaking.
  {
    CnameUncloaker uncloaker;
    uncloaker.SetNow(1000);

    uncloaker.Record("metrics.shop.test", "shop.test", "metrics.shop.test", "shop.test", 300);
    Check(uncloaker.Canonical(Cloaked()).status == UncloakStatus::kNoAlias,
          "a host that is its own canonical name reports kNoAlias");

    uncloaker.Record("metrics.shop.test", "shop.test", "cdn.shop.test", "shop.test", 300);
    Check(uncloaker.Canonical(Cloaked()).status == UncloakStatus::kSameSite,
          "an alias inside the same site is not cloaking");

    uncloaker.Record("metrics.shop.test", "shop.test", "", "", 300);
    Check(uncloaker.Canonical(Cloaked()).status == UncloakStatus::kNoAlias,
          "an empty canonical name means no alias");
  }

  // A real alias, and the TTL that governs it.
  {
    CnameUncloaker uncloaker;
    uncloaker.SetNow(1000);
    uncloaker.Record("metrics.shop.test", "shop.test", "collect.tracker.test",
                     "tracker.test", 300);
    const auto hit = uncloaker.Canonical(Cloaked());
    Check(hit.uncloaked(), "an alias to another site is uncloaked");
    Check(hit.canonical_host == "collect.tracker.test" &&
              hit.canonical_etld1 == "tracker.test",
          "the canonical name and its site are reported");
    Check(uncloaker.TakePendingHosts().empty(),
          "a cache hit queues nothing");

    uncloaker.SetNow(1000 + 301);
    Check(uncloaker.Canonical(Cloaked()).status == UncloakStatus::kUnknown,
          "an expired entry is a miss, not a stale hit");
  }

  // TTLs are clamped in both directions: the tracker publishes the TTL.
  {
    CnameUncloaker uncloaker;
    uncloaker.SetNow(0);
    uncloaker.Record("a.shop.test", "shop.test", "x.tracker.test", "tracker.test", 1);
    uncloaker.SetNow(CnameUncloaker::kMinTtlSeconds - 1);
    Check(uncloaker.Canonical(Cloaked("a.shop.test")).uncloaked(),
          "a one-second TTL is raised to the floor, so it is not a lookup per request");

    uncloaker.SetNow(0);
    uncloaker.Record("b.shop.test", "shop.test", "x.tracker.test", "tracker.test",
                     365 * 24 * 60 * 60);
    uncloaker.SetNow(CnameUncloaker::kMaxTtlSeconds + 1);
    Check(uncloaker.Canonical(Cloaked("b.shop.test")).status ==
              UncloakStatus::kUnknown,
          "a one-year TTL is capped, so an alias cannot outlive its truth");
  }

  // No usable resolver: degrade to no uncloaking, and queue nothing. A lookup
  // that reaches around the user's DNS choice would be a privacy bug.
  {
    CnameUncloaker uncloaker;
    uncloaker.SetNow(10);
    uncloaker.SetUnavailable(true);
    Check(uncloaker.Canonical(Cloaked()).status == UncloakStatus::kUnavailable,
          "with no usable resolver the answer is kUnavailable");
    Check(uncloaker.TakePendingHosts().empty(),
          "nothing is queued when there is no resolver to ask");
  }

  // Forgetting. Both directions, because "forget about this site" must remove
  // what the site aliased and what pointed at it.
  {
    CnameUncloaker uncloaker;
    uncloaker.SetNow(1);
    uncloaker.Record("a.shop.test", "shop.test", "x.tracker.test", "tracker.test", 600);
    uncloaker.Record("b.news.test", "news.test", "y.other.test", "other.test", 600);
    Check(uncloaker.cache_size() == 2, "two aliases cached");

    uncloaker.ForgetSite("tracker.test");
    Check(uncloaker.cache_size() == 1, "forgetting the tracker drops the alias to it");
    uncloaker.ForgetSite("news.test");
    Check(uncloaker.cache_size() == 0, "forgetting a site drops what it aliased");

    uncloaker.Record("c.shop.test", "shop.test", "z.tracker.test", "tracker.test", 600);
    uncloaker.Clear();
    Check(uncloaker.cache_size() == 0, "Clear() empties the cache (New Identity)");
  }

  // The cache has a ceiling.
  {
    CnameUncloaker uncloaker;
    uncloaker.SetNow(1);
    for (size_t i = 0; i < CnameUncloaker::kMaxCacheEntries + 50; ++i) {
      uncloaker.Record("h" + std::to_string(i) + ".shop.test", "shop.test",
                       "x.tracker.test", "tracker.test", 600);
    }
    Check(uncloaker.cache_size() == CnameUncloaker::kMaxCacheEntries,
          "the cache is bounded");
  }

  // The pipeline stage: one decision point, and uncloaking only ever blocks.
  {
    FilterEngine filters;
    filters.AddList("||collect.tracker.test^\n@@||promo.shop.test^\n");
    TrackerHeuristic heuristic;
    bedrock::privacy::ProtectionController controls;
    CnameUncloaker uncloaker;
    uncloaker.SetNow(1000);
    BlockingPipeline pipeline(&filters, &heuristic, &controls);

    // Without the stage, the cloaked request is plain first party.
    const Decision before = pipeline.Evaluate(Cloaked());
    Check(before.action == Action::kAllow,
          "without uncloaking a cloaked tracker is allowed as first party");

    pipeline.set_uncloaker(&uncloaker);
    Check(pipeline.Evaluate(Cloaked()).action == Action::kAllow,
          "an unresolved name changes nothing — no DNS on the decision path");

    uncloaker.Record("metrics.shop.test", "shop.test", "collect.tracker.test",
                     "tracker.test", 600);
    const Decision after = pipeline.Evaluate(Cloaked());
    Check(after.action == Action::kBlock && after.reason == Reason::kCnameUncloaked,
          "the alias is matched against the lists and the request is blocked");
    Check(after.detail.find("collect.tracker.test") != std::string::npos,
          "the panel is told which name the block came from");

    // An alias into a domain with an exception rule must not buy an allow: the
    // reverse direction would let a tracker CNAME its way out of blocking.
    Request listed = Cloaked("beacon.news.test");
    listed.etld1 = "news.test";
    listed.top_host = "www.news.test";
    listed.top_etld1 = "news.test";
    uncloaker.Record("beacon.news.test", "news.test", "promo.shop.test",
                     "shop.test", 600);
    const Decision allowed_alias = pipeline.Evaluate(listed);
    Check(allowed_alias.reason != Reason::kAllowRule,
          "an exception on the alias does not become an allow for the request");

    // A cached alias that is not on any list leaves the normal path intact.
    Request clean = Cloaked("assets.shop.test");
    uncloaker.Record("assets.shop.test", "shop.test", "edge.cdnhost.test",
                     "cdnhost.test", 600);
    const Decision unlisted = pipeline.Evaluate(clean);
    Check(unlisted.action == Action::kAllow &&
              unlisted.reason == Reason::kFirstParty,
          "an alias nobody lists is not a tracker by virtue of being an alias");
  }

  if (failures == 0) {
    std::cout << "cname_uncloak: ok\n";
  }
  return failures == 0 ? 0 : 1;
}
