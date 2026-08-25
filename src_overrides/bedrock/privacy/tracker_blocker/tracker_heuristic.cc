// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/tracker_blocker/tracker_heuristic.h"

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace bedrock {
namespace blocking {

TrackerHeuristic::TrackerHeuristic() = default;
TrackerHeuristic::~TrackerHeuristic() = default;

TrackerHeuristic::Entry* TrackerHeuristic::Find(const std::string& domain) {
  auto it = entries_.find(domain);
  return it == entries_.end() ? nullptr : &it->second;
}

const TrackerHeuristic::Entry* TrackerHeuristic::Find(
    const std::string& domain) const {
  auto it = entries_.find(domain);
  return it == entries_.end() ? nullptr : &it->second;
}

Verdict TrackerHeuristic::VerdictFor(const Entry& entry) const {
  if (entry.user_verdict != Verdict::kUnknown) {
    return entry.user_verdict;
  }
  if (entry.honours_signals) {
    return Verdict::kAllow;
  }
  if (entry.count < threshold_) {
    return Verdict::kUnknown;
  }
  return entry.partition_only ? Verdict::kPartition : Verdict::kBlock;
}

Verdict TrackerHeuristic::Observe(const std::string& third_party,
                                  const std::string& first_party,
                                  StateKind kind) {
  (void)kind;  // Every kind counts the same today; kept for the panel's "what
               // did it do?" line and for future weighting.
  if (third_party.empty() || third_party == first_party) {
    // Never learn from the site the user chose to visit.
    return Classify(third_party);
  }
  Entry& entry = entries_[third_party];
  entry.last_seen = now_;
  if (entry.honours_signals || entry.user_verdict != Verdict::kUnknown) {
    return VerdictFor(entry);
  }
  if (entry.count < threshold_) {
    if (entry.first_parties.insert(first_party).second) {
      entry.count = static_cast<int>(entry.first_parties.size());
    }
    if (entry.count >= threshold_) {
      // Threshold reached: the *decision* is all we need from here on, so the
      // list of sites the user visited is dropped. Keeping it would turn the
      // privacy feature into a browsing history nobody asked for.
      entry.first_parties.clear();
    }
  }
  return VerdictFor(entry);
}

Verdict TrackerHeuristic::Classify(const std::string& third_party) const {
  const Entry* entry = Find(third_party);
  return entry ? VerdictFor(*entry) : Verdict::kUnknown;
}

int TrackerHeuristic::SiteCount(const std::string& third_party) const {
  const Entry* entry = Find(third_party);
  return entry ? entry->count : 0;
}

void TrackerHeuristic::SetPartitionOnly(const std::string& domain,
                                        bool partition_only) {
  entries_[domain].partition_only = partition_only;
}

void TrackerHeuristic::SetHonoursPrivacySignals(const std::string& domain,
                                                bool honours) {
  Entry& entry = entries_[domain];
  entry.honours_signals = honours;
  if (honours) {
    entry.first_parties.clear();
    entry.count = 0;
  }
}

void TrackerHeuristic::SetUserVerdict(const std::string& domain,
                                      Verdict verdict) {
  entries_[domain].user_verdict = verdict;
}

void TrackerHeuristic::ClearUserVerdict(const std::string& domain) {
  if (Entry* entry = Find(domain)) {
    entry->user_verdict = Verdict::kUnknown;
  }
}

void TrackerHeuristic::Forget(const std::string& domain) {
  entries_.erase(domain);
}

void TrackerHeuristic::Clear() {
  entries_.clear();
}

int TrackerHeuristic::ForgetOlderThan(int64_t max_age_seconds) {
  int removed = 0;
  for (auto it = entries_.begin(); it != entries_.end();) {
    const bool is_decision = it->second.user_verdict != Verdict::kUnknown;
    if (!is_decision && now_ - it->second.last_seen > max_age_seconds) {
      it = entries_.erase(it);
      ++removed;
    } else {
      ++it;
    }
  }
  return removed;
}

std::string TrackerHeuristic::Export() const {
  std::string text;
  for (const auto& [domain, entry] : entries_) {
    std::string flags;
    if (entry.partition_only) {
      flags += 'p';
    }
    if (entry.honours_signals) {
      flags += 's';
    }
    if (entry.user_verdict == Verdict::kAllow) {
      flags += 'A';
    } else if (entry.user_verdict == Verdict::kPartition) {
      flags += 'P';
    } else if (entry.user_verdict == Verdict::kBlock) {
      flags += 'B';
    }
    text += domain + "\t" + std::to_string(entry.count) + "\t" + flags + "\t" +
            std::to_string(entry.last_seen) + "\n";
  }
  return text;
}

bool TrackerHeuristic::Import(const std::string& text) {
  std::istringstream stream(text);
  std::string line;
  bool ok = true;
  while (std::getline(stream, line)) {
    if (line.empty()) {
      continue;
    }
    size_t first_tab = line.find('\t');
    if (first_tab == std::string::npos) {
      ok = false;
      continue;
    }
    size_t second_tab = line.find('\t', first_tab + 1);
    const std::string domain = line.substr(0, first_tab);
    const std::string count = line.substr(
        first_tab + 1, (second_tab == std::string::npos ? line.size()
                                                        : second_tab) -
                           first_tab - 1);
    // The timestamp field was added with F8; a file without it loads with
    // last_seen 0, which simply makes those entries the first to age out.
    size_t third_tab = second_tab == std::string::npos
                           ? std::string::npos
                           : line.find('\t', second_tab + 1);
    const std::string flags =
        second_tab == std::string::npos
            ? ""
            : line.substr(second_tab + 1,
                          (third_tab == std::string::npos ? line.size()
                                                          : third_tab) -
                              second_tab - 1);
    const std::string last_seen =
        third_tab == std::string::npos ? "" : line.substr(third_tab + 1);
    Entry entry;
    // Chromium builds with -fno-exceptions, so std::stoi is unusable here: a
    // corrupt line has to be rejected by return value, not by a throw.
    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(count.c_str(), &end, 10);
    if (count.empty() || errno != 0 || end != count.c_str() + count.size() ||
        parsed < std::numeric_limits<int>::min() ||
        parsed > std::numeric_limits<int>::max()) {
      ok = false;
      continue;
    }
    entry.count = static_cast<int>(parsed);
    if (!last_seen.empty()) {
      errno = 0;
      char* stamp_end = nullptr;
      const long long stamp = std::strtoll(last_seen.c_str(), &stamp_end, 10);
      if (errno != 0 || stamp_end != last_seen.c_str() + last_seen.size() ||
          stamp < 0) {
        ok = false;
        continue;
      }
      entry.last_seen = static_cast<int64_t>(stamp);
    }
    entry.partition_only = flags.find('p') != std::string::npos;
    entry.honours_signals = flags.find('s') != std::string::npos;
    if (flags.find('A') != std::string::npos) {
      entry.user_verdict = Verdict::kAllow;
    } else if (flags.find('P') != std::string::npos) {
      entry.user_verdict = Verdict::kPartition;
    } else if (flags.find('B') != std::string::npos) {
      entry.user_verdict = Verdict::kBlock;
    }
    entries_[domain] = std::move(entry);
  }
  return ok;
}

}  // namespace blocking
}  // namespace bedrock
