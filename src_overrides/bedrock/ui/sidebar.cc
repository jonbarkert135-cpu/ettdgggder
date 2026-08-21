// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/ui/sidebar.h"

#include <algorithm>

namespace bedrock {
namespace ui {

// static
const std::vector<PanelInfo>& Sidebar::Panels() {
  static const std::vector<PanelInfo> kPanels = {
      {Panel::kBookmarks, "Bookmarks", "Menu > Bookmarks", "Ctrl+Shift+O", true},
      {Panel::kHistory, "History", "Menu > History", "Ctrl+H", true},
      {Panel::kDownloads, "Downloads", "Menu > Downloads", "Ctrl+J", true},
      {Panel::kReadingList, "Reading list", "Menu > Reading list",
       "Ctrl+Shift+R", false},
      {Panel::kExtensions, "Extensions", "Menu > Extensions", "Ctrl+Shift+E",
       false},
      {Panel::kWorkspaces, "Workspaces", "Menu > Workspaces", "Ctrl+Shift+W",
       false},
      {Panel::kTabGroups, "Tab groups", "Menu > Tabs > Groups", "Ctrl+Shift+G",
       false},
      {Panel::kNotes, "Notes", "Menu > Notes", "Ctrl+Shift+N", false},
  };
  return kPanels;
}

// static
const PanelInfo& Sidebar::Info(Panel panel) {
  return Panels()[static_cast<size_t>(panel)];
}

Sidebar::Sidebar() {
  enabled_.resize(Panels().size());
  for (const PanelInfo& info : Panels()) {
    enabled_[static_cast<size_t>(info.panel)] = info.enabled_by_default;
    order_.push_back(info.panel);
  }
}

Sidebar::~Sidebar() = default;

bool Sidebar::SetEnabled(Panel panel, bool enabled) {
  enabled_[static_cast<size_t>(panel)] = enabled;
  if (!enabled && active_ == panel) {
    // Do not leave a hidden panel selected; fall back to the first enabled one,
    // and hide the sidebar entirely if nothing is left in it.
    auto it = std::find_if(order_.begin(), order_.end(),
                           [this](Panel candidate) {
                             return IsEnabled(candidate);
                           });
    if (it == order_.end()) {
      visible_ = false;
    } else {
      active_ = *it;
    }
  }
  return true;
}

bool Sidebar::IsEnabled(Panel panel) const {
  return enabled_[static_cast<size_t>(panel)];
}

void Sidebar::SetOrder(const std::vector<Panel>& order) {
  std::vector<Panel> result;
  for (Panel panel : order) {
    if (std::find(result.begin(), result.end(), panel) == result.end()) {
      result.push_back(panel);
    }
  }
  for (Panel panel : order_) {
    if (std::find(result.begin(), result.end(), panel) == result.end()) {
      result.push_back(panel);  // anything the caller forgot keeps its place
    }
  }
  order_ = result;
}

std::vector<Panel> Sidebar::Order() const {
  return order_;
}

bool Sidebar::Activate(Panel panel) {
  if (!IsEnabled(panel)) {
    return false;
  }
  active_ = panel;
  visible_ = true;
  return true;
}

void Sidebar::SetWidth(int px) {
  width_ = std::min(kMaxWidth, std::max(kMinWidth, px));
}

}  // namespace ui
}  // namespace bedrock
