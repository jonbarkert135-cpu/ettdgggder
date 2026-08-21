// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_UI_TAB_MODEL_H_
#define BEDROCK_UI_TAB_MODEL_H_

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

// Tab system (roadmap item 30).
//
// One model, two layouts. Horizontal and vertical tabs are a *rendering*
// choice over the same ordered list — a browser that keeps two tab models
// eventually shows different tabs in the two layouts, and the bug is
// unreproducible for whoever does not use the other one.
//
// Ordering invariant, maintained by the model rather than by callers: pinned
// tabs first, then grouped tabs with their group contiguous, then the rest.
// The UI never has to sort, so it cannot sort differently.

namespace bedrock {
namespace ui {

enum class TabLayout { kHorizontal, kVertical };

struct Tab {
  int id = 0;
  std::string title;
  std::string url;
  int group = 0;          // 0 = ungrouped
  bool pinned = false;
  bool muted = false;
  bool audible = false;
  bool sleeping = false;  // discarded from memory, restored on activation
  int64_t last_active_ms = 0;
};

struct ClosedTab {
  Tab tab;
  size_t index = 0;  // where it was, so reopening puts it back
};

class TabModel {
 public:
  static constexpr size_t kRecentlyClosedLimit = 25;

  TabModel();
  ~TabModel();

  int Add(const std::string& title, const std::string& url, int64_t now_ms);
  bool Close(int id);
  // Reopens the most recently closed tab at its old position (Ctrl+Shift+T).
  int ReopenLastClosed();
  const std::deque<ClosedTab>& recently_closed() const { return closed_; }

  bool Activate(int id, int64_t now_ms);
  int active_id() const { return active_id_; }

  bool SetPinned(int id, bool pinned);
  bool SetMuted(int id, bool muted);
  bool SetAudible(int id, bool audible);

  // Groups.
  int CreateGroup(const std::string& name);
  bool AddToGroup(int id, int group);
  bool RemoveFromGroup(int id);
  bool CloseGroup(int group);
  std::vector<int> TabsInGroup(int group) const;
  const std::string& GroupName(int group) const;

  // Sleeping tabs: a background tab idle for longer than `idle_ms` is
  // discarded. Never the active tab, never a pinned tab, never one making
  // sound — those three exceptions are what make the feature tolerable.
  std::vector<int> SleepIdleTabs(int64_t now_ms, int64_t idle_ms);
  bool Wake(int id, int64_t now_ms);

  // Search over title and URL, case-insensitive substring, in tab order.
  std::vector<int> Search(const std::string& query) const;

  // Duplicate detection: same URL ignoring scheme, "www.", trailing slash,
  // fragment and tracking parameters. Returns one vector per duplicate set.
  std::vector<std::vector<int>> FindDuplicates() const;
  static std::string NormalizeUrl(const std::string& url);

  void set_layout(TabLayout layout) { layout_ = layout; }
  TabLayout layout() const { return layout_; }

  const std::vector<Tab>& tabs() const { return tabs_; }
  const Tab* Find(int id) const;
  size_t size() const { return tabs_.size(); }

 private:
  Tab* FindMutable(int id);
  size_t IndexOf(int id) const;
  // Re-establishes the ordering invariant after any structural change.
  void Reorder();

  std::vector<Tab> tabs_;
  std::deque<ClosedTab> closed_;
  std::map<int, std::string> groups_;
  TabLayout layout_ = TabLayout::kHorizontal;
  int next_id_ = 1;
  int next_group_ = 1;
  int active_id_ = 0;
};

}  // namespace ui
}  // namespace bedrock

#endif  // BEDROCK_UI_TAB_MODEL_H_
