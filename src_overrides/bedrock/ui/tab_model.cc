// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/ui/tab_model.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace bedrock {
namespace ui {

TabStripMetrics MetricsFor(TabLayout layout) {
  switch (layout) {
    case TabLayout::kVertical:
      // A row is as tall as a comfortable hit target and the strip is wide
      // enough for a readable title, which is the entire point of going
      // vertical.
      return {32, 0, 232, true, true};
    case TabLayout::kCompact:
      // Favicon only. The title lives in the tooltip and in tab search, so
      // nothing becomes unreachable — it stops being visible, which is what
      // the user asked for by choosing compact.
      return {28, 34, 34, false, false};
    case TabLayout::kHorizontal:
      break;
  }
  return {34, 132, 42, true, false};
}

namespace {

const std::string kEmpty;

std::string ToLower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return text;
}

bool IsTrackingParam(const std::string& name) {
  return name.compare(0, 4, "utm_") == 0 || name == "fbclid" ||
         name == "gclid" || name == "msclkid" || name == "igshid" ||
         name == "mc_eid" || name == "ref";
}

}  // namespace

TabModel::TabModel() = default;
TabModel::~TabModel() = default;

// static
std::string TabModel::NormalizeUrl(const std::string& url) {
  std::string normalized = ToLower(url);
  for (const std::string scheme : {"https://", "http://"}) {
    if (normalized.compare(0, scheme.size(), scheme) == 0) {
      normalized = normalized.substr(scheme.size());
      break;
    }
  }
  if (normalized.compare(0, 4, "www.") == 0) {
    normalized = normalized.substr(4);
  }
  const size_t fragment = normalized.find('#');
  if (fragment != std::string::npos) {
    normalized = normalized.substr(0, fragment);
  }
  const size_t query_at = normalized.find('?');
  std::string path = normalized.substr(0, query_at);
  std::string kept;
  if (query_at != std::string::npos) {
    const std::string query = normalized.substr(query_at + 1);
    size_t start = 0;
    while (start <= query.size()) {
      const size_t end = query.find('&', start);
      const std::string pair = query.substr(
          start, end == std::string::npos ? std::string::npos : end - start);
      if (!pair.empty() && !IsTrackingParam(pair.substr(0, pair.find('=')))) {
        if (!kept.empty()) {
          kept += '&';
        }
        kept += pair;
      }
      if (end == std::string::npos) {
        break;
      }
      start = end + 1;
    }
  }
  while (path.size() > 1 && path.back() == '/') {
    path.pop_back();
  }
  return kept.empty() ? path : path + "?" + kept;
}

int TabModel::Add(const std::string& title,
                  const std::string& url,
                  int64_t now_ms) {
  Tab tab;
  tab.id = next_id_++;
  tab.title = title;
  tab.url = url;
  tab.last_active_ms = now_ms;
  tabs_.push_back(tab);
  if (active_id_ == 0) {
    active_id_ = tab.id;
  }
  Reorder();
  return tab.id;
}

size_t TabModel::IndexOf(int id) const {
  for (size_t i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i].id == id) {
      return i;
    }
  }
  return tabs_.size();
}

Tab* TabModel::FindMutable(int id) {
  const size_t index = IndexOf(id);
  return index == tabs_.size() ? nullptr : &tabs_[index];
}

const Tab* TabModel::Find(int id) const {
  const size_t index = IndexOf(id);
  return index == tabs_.size() ? nullptr : &tabs_[index];
}

void TabModel::Reorder() {
  // Stable, so a tab never jumps past a sibling it did not need to pass.
  std::stable_sort(tabs_.begin(), tabs_.end(), [](const Tab& a, const Tab& b) {
    if (a.pinned != b.pinned) {
      return a.pinned;
    }
    if ((a.group != 0) != (b.group != 0)) {
      return a.group != 0;  // grouped tabs stay together, before loose ones
    }
    return a.group < b.group;
  });
}

bool TabModel::Close(int id) {
  const size_t index = IndexOf(id);
  if (index == tabs_.size()) {
    return false;
  }
  closed_.push_front({tabs_[index], index});
  if (closed_.size() > kRecentlyClosedLimit) {
    closed_.pop_back();
  }
  const bool was_active = active_id_ == id;
  tabs_.erase(tabs_.begin() + static_cast<long>(index));
  if (was_active) {
    active_id_ = tabs_.empty()
                     ? 0
                     : tabs_[std::min(index, tabs_.size() - 1)].id;
  }
  return true;
}

int TabModel::ReopenLastClosed() {
  if (closed_.empty()) {
    return 0;
  }
  ClosedTab entry = closed_.front();
  closed_.pop_front();
  const size_t index = std::min(entry.index, tabs_.size());
  tabs_.insert(tabs_.begin() + static_cast<long>(index), entry.tab);
  Reorder();
  return entry.tab.id;
}

bool TabModel::Activate(int id, int64_t now_ms) {
  Tab* tab = FindMutable(id);
  if (!tab) {
    return false;
  }
  active_id_ = id;
  tab->last_active_ms = now_ms;
  tab->sleeping = false;  // activating a sleeping tab wakes it
  return true;
}

bool TabModel::SetPinned(int id, bool pinned) {
  Tab* tab = FindMutable(id);
  if (!tab) {
    return false;
  }
  tab->pinned = pinned;
  Reorder();
  return true;
}

bool TabModel::SetMuted(int id, bool muted) {
  Tab* tab = FindMutable(id);
  if (!tab) {
    return false;
  }
  tab->muted = muted;
  return true;
}

bool TabModel::SetAudible(int id, bool audible) {
  Tab* tab = FindMutable(id);
  if (!tab) {
    return false;
  }
  tab->audible = audible;
  return true;
}

int TabModel::CreateGroup(const std::string& name) {
  const int group = next_group_++;
  groups_[group] = name;
  return group;
}

bool TabModel::AddToGroup(int id, int group) {
  Tab* tab = FindMutable(id);
  if (!tab || groups_.count(group) == 0) {
    return false;
  }
  tab->group = group;
  Reorder();
  return true;
}

bool TabModel::RemoveFromGroup(int id) {
  Tab* tab = FindMutable(id);
  if (!tab) {
    return false;
  }
  tab->group = 0;
  Reorder();
  return true;
}

bool TabModel::CloseGroup(int group) {
  const std::vector<int> ids = TabsInGroup(group);
  if (ids.empty()) {
    return false;
  }
  for (int id : ids) {
    Close(id);  // each goes to recently-closed, so the group is recoverable
  }
  groups_.erase(group);
  return true;
}

std::vector<int> TabModel::TabsInGroup(int group) const {
  std::vector<int> ids;
  for (const Tab& tab : tabs_) {
    if (tab.group == group) {
      ids.push_back(tab.id);
    }
  }
  return ids;
}

const std::string& TabModel::GroupName(int group) const {
  auto it = groups_.find(group);
  return it == groups_.end() ? kEmpty : it->second;
}

std::vector<int> TabModel::SleepIdleTabs(int64_t now_ms, int64_t idle_ms) {
  std::vector<int> slept;
  for (Tab& tab : tabs_) {
    if (tab.sleeping || tab.id == active_id_ || tab.pinned || tab.audible) {
      continue;
    }
    if (now_ms - tab.last_active_ms >= idle_ms) {
      tab.sleeping = true;
      slept.push_back(tab.id);
    }
  }
  return slept;
}

bool TabModel::Wake(int id, int64_t now_ms) {
  Tab* tab = FindMutable(id);
  if (!tab) {
    return false;
  }
  tab->sleeping = false;
  tab->last_active_ms = now_ms;
  return true;
}

std::vector<int> TabModel::Search(const std::string& query) const {
  std::vector<int> matches;
  if (query.empty()) {
    return matches;
  }
  const std::string needle = ToLower(query);
  for (const Tab& tab : tabs_) {
    if (ToLower(tab.title).find(needle) != std::string::npos ||
        ToLower(tab.url).find(needle) != std::string::npos) {
      matches.push_back(tab.id);
    }
  }
  return matches;
}

std::vector<std::vector<int>> TabModel::FindDuplicates() const {
  std::map<std::string, std::vector<int>> by_url;
  std::vector<std::string> order;
  for (const Tab& tab : tabs_) {
    const std::string key = NormalizeUrl(tab.url);
    if (by_url.find(key) == by_url.end()) {
      order.push_back(key);
    }
    by_url[key].push_back(tab.id);
  }
  std::vector<std::vector<int>> duplicates;
  for (const std::string& key : order) {
    if (by_url[key].size() > 1) {
      duplicates.push_back(by_url[key]);
    }
  }
  return duplicates;
}

}  // namespace ui
}  // namespace bedrock
