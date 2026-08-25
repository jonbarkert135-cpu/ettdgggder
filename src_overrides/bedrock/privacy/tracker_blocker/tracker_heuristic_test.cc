// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/privacy/tracker_blocker/tracker_heuristic.h"

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

void SeeOn(TrackerHeuristic* heuristic,
           const std::string& tracker,
           const std::string& site) {
  heuristic->Observe(tracker, site, StateKind::kCookie);
}

}  // namespace

int main() {
  // Learning: three distinct first parties, exactly as EFF's heuristic states.
  {
    TrackerHeuristic heuristic;
    Check(heuristic.Classify("tracker.test") == Verdict::kUnknown,
          "unseen domain is unknown, not blocked");

    SeeOn(&heuristic, "tracker.test", "news.test");
    Check(heuristic.Classify("tracker.test") == Verdict::kUnknown,
          "one site is not evidence");
    SeeOn(&heuristic, "tracker.test", "shop.test");
    Check(heuristic.Classify("tracker.test") == Verdict::kUnknown,
          "two sites are not evidence");
    SeeOn(&heuristic, "tracker.test", "forum.test");
    Check(heuristic.Classify("tracker.test") == Verdict::kBlock,
          "three distinct sites make it a tracker");
  }

  // Repeats on the same site never accumulate: a site the user reads daily
  // must not train the browser to block its own widgets.
  {
    TrackerHeuristic heuristic;
    for (int i = 0; i < 50; ++i) {
      SeeOn(&heuristic, "widget.test", "news.test");
    }
    Check(heuristic.SiteCount("widget.test") == 1, "repeats count once");
    Check(heuristic.Classify("widget.test") == Verdict::kUnknown,
          "50 visits to one site is still one site");
  }

  // First-party state is never learned from.
  {
    TrackerHeuristic heuristic;
    SeeOn(&heuristic, "news.test", "news.test");
    Check(heuristic.SiteCount("news.test") == 0,
          "the visited site itself is not recorded");
    Check(heuristic.Classify("news.test") == Verdict::kUnknown,
          "the visited site is never classified as a tracker");
  }

  // Once the verdict is reached, the sites behind it are dropped: the table
  // must not become a browsing history.
  {
    TrackerHeuristic heuristic;
    SeeOn(&heuristic, "tracker.test", "clinic.test");
    SeeOn(&heuristic, "tracker.test", "bank.test");
    SeeOn(&heuristic, "tracker.test", "dating.test");
    const std::string exported = heuristic.Export();
    Check(exported.find("clinic.test") == std::string::npos &&
              exported.find("bank.test") == std::string::npos &&
              exported.find("dating.test") == std::string::npos,
          "no first-party site names survive in the stored table");
    Check(exported.find("tracker.test\t3") == 0,
          "only the domain and the count are stored");
  }

  // Domains that break when blocked are partitioned instead.
  {
    TrackerHeuristic heuristic;
    heuristic.SetPartitionOnly("login.test", true);
    SeeOn(&heuristic, "login.test", "a.test");
    SeeOn(&heuristic, "login.test", "b.test");
    SeeOn(&heuristic, "login.test", "c.test");
    Check(heuristic.Classify("login.test") == Verdict::kPartition,
          "partition-only domain is partitioned, not blocked");
  }

  // Honouring GPC/DNT stops the learning and clears what was learned.
  {
    TrackerHeuristic heuristic;
    SeeOn(&heuristic, "polite.test", "a.test");
    SeeOn(&heuristic, "polite.test", "b.test");
    heuristic.SetHonoursPrivacySignals("polite.test", true);
    SeeOn(&heuristic, "polite.test", "c.test");
    SeeOn(&heuristic, "polite.test", "d.test");
    Check(heuristic.Classify("polite.test") == Verdict::kAllow,
          "a domain honouring the signal is allowed");
    Check(heuristic.SiteCount("polite.test") == 0,
          "and its counter is reset, not just ignored");
  }

  // User decisions beat everything the heuristic learned, in both directions.
  {
    TrackerHeuristic heuristic;
    SeeOn(&heuristic, "tracker.test", "a.test");
    SeeOn(&heuristic, "tracker.test", "b.test");
    SeeOn(&heuristic, "tracker.test", "c.test");
    heuristic.SetUserVerdict("tracker.test", Verdict::kAllow);
    Check(heuristic.Classify("tracker.test") == Verdict::kAllow,
          "user allow beats a learned block");
    heuristic.ClearUserVerdict("tracker.test");
    Check(heuristic.Classify("tracker.test") == Verdict::kBlock,
          "clearing the override restores the learned verdict");

    heuristic.SetUserVerdict("innocent.test", Verdict::kBlock);
    Check(heuristic.Classify("innocent.test") == Verdict::kBlock,
          "user block applies without any evidence");
  }

  // Threshold is configurable, and export/import round-trips.
  {
    TrackerHeuristic heuristic;
    heuristic.set_threshold(2);
    SeeOn(&heuristic, "tracker.test", "a.test");
    SeeOn(&heuristic, "tracker.test", "b.test");
    Check(heuristic.Classify("tracker.test") == Verdict::kBlock,
          "threshold of 2 blocks after two sites");
    heuristic.SetPartitionOnly("cdn.test", true);
    heuristic.SetUserVerdict("mine.test", Verdict::kAllow);

    TrackerHeuristic restored;
    restored.set_threshold(2);
    Check(restored.Import(heuristic.Export()), "import accepts our own export");
    Check(restored.Classify("tracker.test") == Verdict::kBlock,
          "learned verdict survives a round-trip");
    Check(restored.Classify("mine.test") == Verdict::kAllow,
          "user verdict survives a round-trip");
    Check(restored.Export() == heuristic.Export(), "export is stable");
    Check(!restored.Import("garbage-without-tabs\n"),
          "malformed import is reported");
  }

  // Forget and Clear.
  {
    TrackerHeuristic heuristic;
    SeeOn(&heuristic, "tracker.test", "a.test");
    SeeOn(&heuristic, "other.test", "a.test");
    heuristic.Forget("tracker.test");
    Check(heuristic.SiteCount("tracker.test") == 0, "forget drops one domain");
    Check(heuristic.SiteCount("other.test") == 1, "and leaves the rest");
    heuristic.Clear();
    Check(heuristic.Export().empty(), "clear empties the table");
  }

  // Audit F8: evidence ages out, which is what makes it safe to persist.
  {
    const int64_t day = 24 * 60 * 60;
    TrackerHeuristic heuristic;
    heuristic.SetNow(100 * day);
    SeeOn(&heuristic, "old.test", "a.test");
    heuristic.SetUserVerdict("decided.test", Verdict::kBlock);
    heuristic.SetNow(150 * day);
    SeeOn(&heuristic, "recent.test", "b.test");

    heuristic.SetNow(200 * day);
    Check(heuristic.ForgetOlderThan(60 * day) == 1,
          "an observation older than the retention window is forgotten");
    Check(heuristic.SiteCount("old.test") == 0, "and it is really gone");
    Check(heuristic.SiteCount("recent.test") == 1,
          "a recent observation is kept");
    Check(heuristic.Classify("decided.test") == Verdict::kBlock,
          "a user's decision is not evidence and never expires");
    Check(heuristic.ForgetOlderThan(60 * day) == 0, "ageing out is idempotent");

    // A table written before F8 has no timestamp column; it must still load,
    // with its entries first in line to be forgotten rather than rejected.
    TrackerHeuristic legacy;
    legacy.SetNow(200 * day);
    Check(legacy.Import("tracker.test\t3\t\n"), "a pre-F8 table still loads");
    Check(legacy.SiteCount("tracker.test") == 3, "with its counter intact");
    Check(legacy.ForgetOlderThan(60 * day) == 1,
          "and it ages out on the next sweep");
    Check(!legacy.Import("tracker.test\t3\t\tnot-a-number\n"),
          "a corrupt timestamp is rejected, not guessed");
  }

  if (failures == 0) {
    std::cout << "tracker_heuristic_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
