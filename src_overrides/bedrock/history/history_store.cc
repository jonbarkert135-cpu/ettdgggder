// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/history/history_store.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace bedrock {
namespace data {
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

HistoryStore::HistoryStore() = default;
HistoryStore::~HistoryStore() = default;

std::string HistoryStore::DomainOf(const std::string& url) {
  std::string rest = url;
  const size_t scheme = rest.find("://");
  if (scheme != std::string::npos)
    rest = rest.substr(scheme + 3);
  const size_t slash = rest.find('/');
  if (slash != std::string::npos)
    rest = rest.substr(0, slash);
  const size_t at = rest.find('@');
  if (at != std::string::npos)
    rest = rest.substr(at + 1);
  const size_t colon = rest.find(':');
  if (colon != std::string::npos)
    rest = rest.substr(0, colon);
  if (rest.rfind("www.", 0) == 0)
    rest = rest.substr(4);
  return Lower(rest);
}

int HistoryStore::Record(const std::string& url,
                         const std::string& title,
                         int64_t visited_at,
                         bool typed) {
  Visit visit;
  visit.id = next_id_++;
  visit.url = url;
  visit.title = title;
  visit.visited_at = visited_at;
  visit.typed = typed;
  visits_.push_back(visit);

  // A typed visit is a stronger signal than a followed link — that is the
  // whole derived-data problem in one line, and the reason Forget() exists.
  const int weight = typed ? 3 : 1;
  for (auto& entry : ranking_) {
    if (entry.first == url) {
      entry.second += weight;
      return visit.id;
    }
  }
  ranking_.emplace_back(url, weight);
  return visit.id;
}

std::vector<Visit> HistoryStore::Search(const std::string& query) const {
  std::vector<Visit> results;
  if (query.empty())
    return results;
  for (const Visit& visit : visits_) {
    if (Contains(visit.title, query) || Contains(visit.url, query))
      results.push_back(visit);
  }
  return results;
}

std::vector<Visit> HistoryStore::InRange(int64_t from, int64_t to) const {
  std::vector<Visit> results;
  for (const Visit& visit : visits_) {
    if (visit.visited_at >= from && visit.visited_at < to)
      results.push_back(visit);
  }
  return results;
}

std::vector<DomainGroup> HistoryStore::ByDomain() const {
  std::vector<DomainGroup> groups;
  for (const Visit& visit : visits_) {
    const std::string domain = DomainOf(visit.url);
    DomainGroup* group = nullptr;
    for (DomainGroup& candidate : groups) {
      if (candidate.domain == domain) {
        group = &candidate;
        break;
      }
    }
    if (!group) {
      groups.push_back({domain, 0, 0});
      group = &groups.back();
    }
    ++group->visit_count;
    group->last_visit = std::max(group->last_visit, visit.visited_at);
  }
  std::stable_sort(groups.begin(), groups.end(),
                   [](const DomainGroup& a, const DomainGroup& b) {
                     return a.visit_count > b.visit_count;
                   });
  return groups;
}

std::vector<DayGroup> HistoryStore::ByDay(int64_t day_length) const {
  if (day_length <= 0)
    day_length = 86400;
  std::vector<DayGroup> days;
  for (const Visit& visit : visits_) {
    const int64_t day = (visit.visited_at / day_length) * day_length;
    DayGroup* group = nullptr;
    for (DayGroup& candidate : days) {
      if (candidate.day_start == day) {
        group = &candidate;
        break;
      }
    }
    if (!group) {
      days.push_back({day, {}});
      group = &days.back();
    }
    group->visits.push_back(visit);
  }
  std::sort(days.begin(), days.end(),
            [](const DayGroup& a, const DayGroup& b) {
              return a.day_start > b.day_start;
            });
  for (DayGroup& day : days) {
    std::sort(day.visits.begin(), day.visits.end(),
              [](const Visit& a, const Visit& b) {
                return a.visited_at > b.visited_at;
              });
  }
  return days;
}

void HistoryStore::Forget(const std::string& url) {
  // Only drop the derived signal once no visit to this URL is left; deleting
  // one of five visits should not wipe the ranking for the other four.
  const bool still_present =
      std::any_of(visits_.begin(), visits_.end(),
                  [&url](const Visit& v) { return v.url == url; });
  if (still_present)
    return;
  ranking_.erase(std::remove_if(ranking_.begin(), ranking_.end(),
                                [&url](const std::pair<std::string, int>& e) {
                                  return e.first == url;
                                }),
                 ranking_.end());
}

int HistoryStore::DeleteVisit(int visit_id) {
  for (auto it = visits_.begin(); it != visits_.end(); ++it) {
    if (it->id != visit_id)
      continue;
    const std::string url = it->url;
    visits_.erase(it);
    Forget(url);
    return 1;
  }
  return 0;
}

int HistoryStore::DeleteUrl(const std::string& url) {
  const size_t before = visits_.size();
  visits_.erase(std::remove_if(visits_.begin(), visits_.end(),
                               [&url](const Visit& v) { return v.url == url; }),
                visits_.end());
  Forget(url);
  return static_cast<int>(before - visits_.size());
}

int HistoryStore::DeleteDomain(const std::string& domain) {
  const std::string wanted = DomainOf(domain);
  std::vector<std::string> urls;
  for (const Visit& visit : visits_) {
    if (DomainOf(visit.url) == wanted)
      urls.push_back(visit.url);
  }
  const size_t before = visits_.size();
  visits_.erase(std::remove_if(visits_.begin(), visits_.end(),
                               [&wanted](const Visit& v) {
                                 return DomainOf(v.url) == wanted;
                               }),
                visits_.end());
  for (const std::string& url : urls)
    Forget(url);
  return static_cast<int>(before - visits_.size());
}

int HistoryStore::DeleteRange(int64_t from, int64_t to) {
  std::vector<std::string> urls;
  for (const Visit& visit : visits_) {
    if (visit.visited_at >= from && visit.visited_at < to)
      urls.push_back(visit.url);
  }
  const size_t before = visits_.size();
  visits_.erase(std::remove_if(visits_.begin(), visits_.end(),
                               [from, to](const Visit& v) {
                                 return v.visited_at >= from &&
                                        v.visited_at < to;
                               }),
                visits_.end());
  for (const std::string& url : urls)
    Forget(url);
  return static_cast<int>(before - visits_.size());
}

int HistoryStore::DeleteAll() {
  const int removed = count();
  visits_.clear();
  ranking_.clear();  // derived data goes with it, or "delete all" is a lie
  return removed;
}

int HistoryStore::RankingScore(const std::string& url) const {
  for (const auto& entry : ranking_) {
    if (entry.first == url)
      return entry.second;
  }
  return 0;
}

std::vector<std::string> HistoryStore::TopSites(int limit) const {
  std::vector<DomainGroup> groups = ByDomain();
  std::vector<std::string> top;
  for (const DomainGroup& group : groups) {
    if (static_cast<int>(top.size()) >= limit)
      break;
    top.push_back(group.domain);
  }
  return top;
}

}  // namespace data
}  // namespace bedrock
