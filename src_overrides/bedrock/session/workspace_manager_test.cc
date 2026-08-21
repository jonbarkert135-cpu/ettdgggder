// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/session/workspace_manager.h"

#include <iostream>
#include <string>

namespace {

using bedrock::session::Workspace;
using bedrock::session::WorkspaceManager;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

bool Mentions(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

}  // namespace

int main() {
  WorkspaceManager manager;

  // A workspace always exists.
  Check(manager.count() == 1, "one workspace exists on first run");
  Check(manager.active() == manager.Ids().front(), "it is active");
  Check(!manager.Remove(manager.active()), "the last workspace is kept");
  Check(!manager.last_error().empty(), "and the refusal is explained");

  const int research = manager.Create("Research");
  const int personal = manager.Create("Personal");
  const int development = manager.Create("Development");
  Check(manager.count() == 4, "workspaces can be created");

  // Tabs and groups.
  Workspace* ws = manager.Get(research);
  const int paper = ws->AddTab("https://arxiv.org/abs/1", "Paper");
  const int notes = ws->AddTab("https://notes.example/1", "Notes");
  const int group = ws->AddGroup("Sources", "copper");
  Check(ws->AssignToGroup(paper, group), "a tab joins a group");
  Check(!ws->AssignToGroup(notes, 999), "unknown groups are refused");
  Check(ws->RemoveGroup(group), "a group can be removed");
  Check(ws->tab_count() == 2, "removing a group keeps its tabs");
  Check(ws->tabs()[0].group_id == 0, "the tab is simply ungrouped");

  // Visual settings are per workspace and default to the global theme.
  Check(ws->visuals().theme_mode.empty(), "visuals follow the global theme");
  ws->visuals().vertical_tabs = true;
  Check(manager.Get(personal)->visuals().vertical_tabs == false,
        "visual settings do not leak between workspaces");

  // Switching never closes anything.
  Check(manager.Switch(personal), "switching works");
  Check(manager.Get(research)->tab_count() == 2,
        "the tabs of the workspace we left are still there");

  // The honest description — the point of item 32.
  const std::string unmapped = manager.Get(personal)->privacy_summary();
  Check(Mentions(unmapped, "shared"),
        "an unmapped workspace admits it shares state with the profile");
  Check(!Mentions(unmapped, "separate") && !Mentions(unmapped, "isolated"),
        "and never claims separation it does not provide");

  manager.Get(personal)->SetProfileId("profile-personal");
  Check(Mentions(manager.Get(personal)->privacy_summary(), "Separate profile"),
        "a mapped workspace says what the mapping buys");

  // Moving tabs: fine inside a profile, refused across one.
  Check(manager.MoveTab(research, paper, development),
        "a tab moves between workspaces of the same profile");
  Check(manager.Get(development)->tab_count() == 1, "it arrived");
  Check(manager.Get(research)->tab_count() == 1, "and left");
  Check(manager.Get(development)->tabs()[0].group_id == 0,
        "it arrives ungrouped rather than pointing at a foreign group");

  const int moved_id = manager.Get(development)->tabs()[0].id;
  Check(!manager.MoveTab(development, moved_id, personal),
        "a tab cannot cross a profile boundary");
  Check(Mentions(manager.last_error(), "profile"), "and the reason says why");
  Check(manager.Get(development)->tab_count() == 1,
        "the refused move changed nothing");

  // Removing the active workspace leaves a valid active one.
  Check(manager.Switch(development), "switch to the one being removed");
  Check(manager.Remove(development), "it can be removed");
  Check(manager.Get(manager.active()) != nullptr,
        "the active workspace is still a real workspace");

  if (failures == 0)
    std::cout << "workspace_manager_test: all assertions passed\n";
  return failures == 0 ? 0 : 1;
}
