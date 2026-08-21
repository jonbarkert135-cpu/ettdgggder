// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/ui/sidebar.h"

#include <iostream>
#include <set>
#include <string>

namespace {

using namespace bedrock::ui;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

}  // namespace

int main() {
  Sidebar sidebar;

  // The promise of item 31: optional means optional.
  Check(!sidebar.visible(), "the sidebar is hidden until asked for");
  for (const PanelInfo& info : Sidebar::Panels()) {
    Check(std::string(info.menu_path).find("Menu") == 0,
          std::string(info.name) + " is reachable from the menu without the "
                                   "sidebar");
    Check(std::string(info.shortcut).find("Ctrl") == 0,
          std::string(info.name) + " has a keyboard shortcut");
    Check(std::string(info.name).size() > 3, "the panel has a name");
  }
  Check(Sidebar::Panels().size() ==
            static_cast<size_t>(Panel::kMaxValue) + 1,
        "every panel in the enum is described");

  // Shortcuts must be unique, or one of them silently never fires.
  {
    std::set<std::string> shortcuts;
    for (const PanelInfo& info : Sidebar::Panels()) {
      Check(shortcuts.insert(info.shortcut).second,
            std::string("unique shortcut: ") + info.shortcut);
    }
  }

  // Enabling, activating, and the case nobody handles: the last panel off.
  {
    Check(sidebar.Activate(Panel::kBookmarks), "activating shows the sidebar");
    Check(sidebar.visible() && sidebar.active() == Panel::kBookmarks,
          "and selects the panel");
    Check(!sidebar.Activate(Panel::kNotes),
          "a disabled panel cannot be activated");
    Check(sidebar.SetEnabled(Panel::kNotes, true) &&
              sidebar.Activate(Panel::kNotes),
          "enabling it first works");

    sidebar.SetEnabled(Panel::kNotes, false);
    Check(sidebar.active() != Panel::kNotes,
          "disabling the active panel moves the selection");

    for (const PanelInfo& info : Sidebar::Panels()) {
      sidebar.SetEnabled(info.panel, false);
    }
    Check(!sidebar.visible(),
          "with every panel off the sidebar hides itself instead of showing an "
          "empty rail");
  }

  // Order is user-controlled, and forgetting a panel does not delete it.
  {
    Sidebar ordered;
    ordered.SetOrder({Panel::kNotes, Panel::kHistory});
    const auto order = ordered.Order();
    Check(order.size() == Sidebar::Panels().size(),
          "reordering never drops a panel");
    Check(order[0] == Panel::kNotes && order[1] == Panel::kHistory,
          "the requested order is respected");
    ordered.SetOrder({Panel::kNotes, Panel::kNotes, Panel::kHistory});
    Check(ordered.Order().size() == Sidebar::Panels().size(),
          "duplicates in the request are ignored");
  }

  // Width is clamped at both ends.
  {
    Sidebar sized;
    sized.SetWidth(3);
    Check(sized.width() == Sidebar::kMinWidth, "too narrow is clamped");
    sized.SetWidth(4000);
    Check(sized.width() == Sidebar::kMaxWidth, "too wide is clamped");
    sized.SetWidth(320);
    Check(sized.width() == 320, "a sensible width is kept");
  }

  if (failures == 0) {
    std::cout << "sidebar_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
