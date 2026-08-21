// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/workspaces/workspace_manager.h"

#include <algorithm>

namespace bedrock {
namespace session {

Workspace::Workspace(int id, std::string name)
    : id_(id), name_(std::move(name)) {}

WorkspaceTab* Workspace::Find(int tab_id) {
  for (WorkspaceTab& tab : tabs_) {
    if (tab.id == tab_id)
      return &tab;
  }
  return nullptr;
}

int Workspace::AddTab(const std::string& url, const std::string& title) {
  WorkspaceTab tab;
  tab.id = next_tab_id_++;
  tab.url = url;
  tab.title = title;
  tabs_.push_back(tab);
  return tab.id;
}

bool Workspace::RemoveTab(int tab_id) {
  for (auto it = tabs_.begin(); it != tabs_.end(); ++it) {
    if (it->id == tab_id) {
      tabs_.erase(it);
      return true;
    }
  }
  return false;
}

int Workspace::AddGroup(const std::string& name, const std::string& color) {
  WorkspaceGroup group;
  group.id = next_group_id_++;
  group.name = name;
  group.color = color;
  groups_.push_back(group);
  return group.id;
}

bool Workspace::AssignToGroup(int tab_id, int group_id) {
  WorkspaceTab* tab = Find(tab_id);
  if (!tab)
    return false;
  if (group_id != 0) {
    const bool known = std::any_of(
        groups_.begin(), groups_.end(),
        [group_id](const WorkspaceGroup& g) { return g.id == group_id; });
    if (!known)
      return false;
  }
  tab->group_id = group_id;
  return true;
}

bool Workspace::RemoveGroup(int group_id) {
  for (auto it = groups_.begin(); it != groups_.end(); ++it) {
    if (it->id != group_id)
      continue;
    groups_.erase(it);
    for (WorkspaceTab& tab : tabs_) {
      if (tab.group_id == group_id)
        tab.group_id = 0;  // the tabs survive their label
    }
    return true;
  }
  return false;
}

std::string Workspace::privacy_summary() const {
  // Item 32 is where a browser is most tempted to over-promise. Workspaces
  // look like separation, so the string has to say what they actually are.
  if (has_profile()) {
    return "Separate profile: cookies, storage, history and logins are kept "
           "apart from other profiles.";
  }
  return "Organises tabs only. Cookies, storage, history and logins are "
         "shared with the other workspaces of this profile.";
}

WorkspaceManager::WorkspaceManager() {
  // One workspace always exists; "no workspace" is not a state the UI should
  // ever have to render.
  active_ = Create("Default");
}

WorkspaceManager::~WorkspaceManager() = default;

int WorkspaceManager::Create(const std::string& name) {
  const int id = next_id_++;
  workspaces_.emplace_back(id, name);
  return id;
}

bool WorkspaceManager::Remove(int workspace_id) {
  last_error_.clear();
  if (workspaces_.size() <= 1) {
    last_error_ = "the last workspace cannot be removed";
    return false;
  }
  for (auto it = workspaces_.begin(); it != workspaces_.end(); ++it) {
    if (it->id() != workspace_id)
      continue;
    const bool was_active = active_ == workspace_id;
    workspaces_.erase(it);
    if (was_active)
      active_ = workspaces_.front().id();
    return true;
  }
  last_error_ = "no such workspace";
  return false;
}

Workspace* WorkspaceManager::Get(int workspace_id) {
  for (Workspace& workspace : workspaces_) {
    if (workspace.id() == workspace_id)
      return &workspace;
  }
  return nullptr;
}

const Workspace* WorkspaceManager::Get(int workspace_id) const {
  return const_cast<WorkspaceManager*>(this)->Get(workspace_id);
}

std::vector<int> WorkspaceManager::Ids() const {
  std::vector<int> ids;
  ids.reserve(workspaces_.size());
  for (const Workspace& workspace : workspaces_)
    ids.push_back(workspace.id());
  return ids;
}

bool WorkspaceManager::Switch(int workspace_id) {
  if (!Get(workspace_id))
    return false;
  active_ = workspace_id;
  return true;
}

bool WorkspaceManager::MoveTab(int from_workspace,
                               int tab_id,
                               int to_workspace) {
  last_error_.clear();
  Workspace* from = Get(from_workspace);
  Workspace* to = Get(to_workspace);
  if (!from || !to) {
    last_error_ = "no such workspace";
    return false;
  }
  if (from->profile_id() != to->profile_id()) {
    // Item 21 says profiles share nothing. A drag-and-drop that carries a
    // page (and its session) across that line would be the exception that
    // makes the rule meaningless.
    last_error_ =
        "tabs cannot move between workspaces that use different profiles";
    return false;
  }
  WorkspaceTab* tab = from->Find(tab_id);
  if (!tab) {
    last_error_ = "no such tab";
    return false;
  }
  const int new_id = to->AddTab(tab->url, tab->title);
  WorkspaceTab* moved = to->Find(new_id);
  moved->pinned = tab->pinned;
  // Groups belong to a workspace, so the tab arrives ungrouped rather than
  // pointing at a group id that means something else over here.
  moved->group_id = 0;
  from->RemoveTab(tab_id);
  return true;
}

}  // namespace session
}  // namespace bedrock
