// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/history/history_store.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

using bedrock::data::DayGroup;
using bedrock::data::DomainGroup;
using bedrock::data::HistoryStore;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

constexpr int64_t kDay = 86400;
constexpr int64_t kMonday = 1'787'000'000LL / kDay * kDay;

}  // namespace

int main() {
  HistoryStore history;

  Check(HistoryStore::DomainOf("https://www.Example.com:8443/path?q=1") ==
            "example.com",
        "the domain drops scheme, www, port and path");

  const int first = history.Record("https://example.com/a", "A", kMonday, true);
  history.Record("https://example.com/a", "A", kMonday + 60, false);
  history.Record("https://example.com/b", "B", kMonday + 120, false);
  history.Record("https://news.example.org/x", "Story", kMonday + kDay, false);
  history.Record("https://docs.example.net/y", "Docs", kMonday + 2 * kDay,
                 true);
  Check(history.count() == 5, "visits are recorded");

  // Search.
  Check(history.Search("story").size() == 1, "title search works");
  Check(history.Search("example.com").size() == 3, "URL search works");
  Check(history.Search("").empty(), "an empty query returns nothing");

  // Grouping.
  const std::vector<DomainGroup> domains = history.ByDomain();
  Check(domains.size() == 3, "three domains");
  Check(domains[0].domain == "example.com" && domains[0].visit_count == 3,
        "the most visited domain comes first");
  Check(domains[0].last_visit == kMonday + 120, "with its latest visit");

  const std::vector<DayGroup> days = history.ByDay();
  Check(days.size() == 3, "three days");
  Check(days[0].day_start > days[1].day_start, "newest day first");
  Check(days[2].visits.size() == 3, "the first day holds three visits");
  Check(days[2].visits[0].visited_at > days[2].visits[1].visited_at,
        "and its visits are newest first");

  Check(history.InRange(kMonday, kMonday + kDay).size() == 3,
        "a date range selects the right visits");

  // Derived data exists...
  Check(history.RankingScore("https://example.com/a") == 4,
        "a typed visit counts more than a followed link");
  Check(history.TopSites(2).size() == 2, "top sites are derived from history");

  // ...and deletion has to reach it. This is the point of item 35.
  Check(history.DeleteVisit(first) == 1, "a single visit is deleted");
  Check(history.RankingScore("https://example.com/a") == 4,
        "the URL keeps its score while another visit to it remains");
  Check(history.DeleteUrl("https://example.com/a") == 1,
        "deleting the URL removes the remaining visit");
  Check(history.RankingScore("https://example.com/a") == 0,
        "and the omnibox ranking forgets it — no ghost suggestions");
  Check(history.Search("example.com/a").empty(), "search forgets it too");

  Check(history.DeleteDomain("https://news.example.org/anything") == 1,
        "delete by domain accepts a URL and matches the host");
  Check(history.Search("news.example.org").empty(), "the domain is gone");
  Check(history.RankingScore("https://news.example.org/x") == 0,
        "with its derived score");

  const int in_range = history.DeleteRange(kMonday, kMonday + kDay);
  Check(in_range == 1, "delete by date range removes that day only");
  Check(history.count() == 1, "one visit is left");

  Check(history.DeleteAll() == 1, "delete all reports what it removed");
  Check(history.count() == 0, "history is empty");
  Check(history.RankingScore("https://docs.example.net/y") == 0,
        "and no derived score survived it");
  Check(history.TopSites(5).empty(), "top sites are empty too");

  if (failures == 0)
    std::cout << "history_store_test: all assertions passed\n";
  return failures == 0 ? 0 : 1;
}
