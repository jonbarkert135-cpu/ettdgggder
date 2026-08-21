// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_UI_SIDEBAR_H_
#define BEDROCK_UI_SIDEBAR_H_

#include <string>
#include <vector>

// Sidebar (roadmap item 31).
//
// Optional means optional. Every panel here is *also* reachable without the
// sidebar — through a menu item and a keyboard shortcut — and a test asserts
// that for every panel. Otherwise "optional" quietly becomes "optional unless
// you want your bookmarks", which is how a sidebar ends up mandatory in
// practice while the settings page still calls it a choice.

namespace bedrock {
namespace ui {

enum class Panel {
  kBookmarks,
  kHistory,
  kDownloads,
  kReadingList,
  kExtensions,
  kWorkspaces,
  kTabGroups,
  kNotes,
  kMaxValue = kNotes,
};

struct PanelInfo {
  Panel panel;
  const char* name;
  const char* menu_path;   // how to reach it with the sidebar off
  const char* shortcut;    // and without the mouse
  bool enabled_by_default;
};

class Sidebar {
 public:
  Sidebar();
  ~Sidebar();

  static const std::vector<PanelInfo>& Panels();
  static const PanelInfo& Info(Panel panel);

  // The sidebar is hidden until asked for. A first run that opens with a
  // sidebar has already decided for the user.
  bool visible() const { return visible_; }
  void SetVisible(bool visible) { visible_ = visible; }
  void Toggle() { visible_ = !visible_; }

  bool SetEnabled(Panel panel, bool enabled);
  bool IsEnabled(Panel panel) const;

  // Order in the rail; unknown panels keep their default position.
  void SetOrder(const std::vector<Panel>& order);
  std::vector<Panel> Order() const;

  bool Activate(Panel panel);   // shows the sidebar if it was hidden
  Panel active() const { return active_; }

  // Width in px, clamped: a sidebar that can be dragged to 3px is a bug
  // generator, and one that can eat the window is worse.
  void SetWidth(int px);
  int width() const { return width_; }
  static constexpr int kMinWidth = 200;
  static constexpr int kMaxWidth = 480;

 private:
  bool visible_ = false;
  Panel active_ = Panel::kBookmarks;
  int width_ = 280;
  std::vector<Panel> order_;
  std::vector<bool> enabled_;
};

}  // namespace ui
}  // namespace bedrock

#endif  // BEDROCK_UI_SIDEBAR_H_
