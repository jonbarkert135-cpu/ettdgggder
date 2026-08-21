// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_DATA_HISTORY_STORE_H_
#define BEDROCK_DATA_HISTORY_STORE_H_

#include <cstdint>
#include <string>
#include <vector>

// History (roadmap item 35).
//
// The interesting requirement is not search, it is deletion. In most browsers
// "delete this entry" removes the row the user can see and leaves the derived
// data behind — visit counts, omnibox ranking, typed-URL scores, top sites —
// so the deleted page keeps suggesting itself. That is worse than not
// offering deletion, because the user believes it is gone.
//
// Here every delete path goes through Forget(), which drops the visits *and*
// the derived signals, and the test asserts the omnibox ranking no longer
// knows the URL. Delete by entry, by domain, by date range, or all of it.

namespace bedrock {
namespace data {

struct Visit {
  int id = 0;
  std::string url;
  std::string title;
  int64_t visited_at = 0;  // unix seconds
  bool typed = false;      // the user typed it rather than followed a link
};

struct DomainGroup {
  std::string domain;
  int visit_count = 0;
  int64_t last_visit = 0;
};

struct DayGroup {
  int64_t day_start = 0;  // unix seconds, local midnight as supplied
  std::vector<Visit> visits;
};

class HistoryStore {
 public:
  HistoryStore();
  ~HistoryStore();

  // Private-window and Tor-mode visits are never passed in; recording is the
  // caller's decision, forgetting is ours to make complete.
  int Record(const std::string& url,
             const std::string& title,
             int64_t visited_at,
             bool typed);

  const std::vector<Visit>& All() const { return visits_; }
  int count() const { return static_cast<int>(visits_.size()); }

  // Case-insensitive over title and URL.
  std::vector<Visit> Search(const std::string& query) const;
  std::vector<Visit> InRange(int64_t from, int64_t to) const;

  // Grouping for the history page.
  std::vector<DomainGroup> ByDomain() const;  // most visited first
  std::vector<DayGroup> ByDay(int64_t day_length = 86400) const;  // newest first

  // Deletion. Each returns how many visits were removed.
  int DeleteVisit(int visit_id);
  int DeleteUrl(const std::string& url);
  int DeleteDomain(const std::string& domain);
  int DeleteRange(int64_t from, int64_t to);
  int DeleteAll();

  // Derived data. The omnibox asks this, not the raw visit list, which is why
  // deletion has to reach it.
  int RankingScore(const std::string& url) const;
  std::vector<std::string> TopSites(int limit) const;

  static std::string DomainOf(const std::string& url);

 private:
  void Forget(const std::string& url);

  std::vector<Visit> visits_;
  // url -> derived score used for autocomplete ordering.
  std::vector<std::pair<std::string, int>> ranking_;
  int next_id_ = 1;
};

}  // namespace data
}  // namespace bedrock

#endif  // BEDROCK_DATA_HISTORY_STORE_H_
