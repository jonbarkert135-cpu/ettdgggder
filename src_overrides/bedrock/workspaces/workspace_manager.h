// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_WORKSPACES_WORKSPACE_MANAGER_H_
#define BEDROCK_WORKSPACES_WORKSPACE_MANAGER_H_

#include <string>
#include <vector>

// Workspaces (roadmap item 32).
//
// A workspace is a named set of tabs, tab groups and visual settings —
// Research, Personal, School, Development. It is an *organisational* boundary,
// not a privacy boundary: two workspaces in the same profile share cookies,
// storage, history and logins, because they are the same profile.
//
// That distinction is the whole reason this file has opinions. A user who
// believes "Personal" and "Work" workspaces keep sites apart, when they do
// not, has been misled by our UI. So:
//
//   - Workspace::privacy_summary() states plainly what is and is not
//     separated, and a test asserts unmapped workspaces never claim
//     separation.
//   - A workspace may optionally be mapped to a profile (item 21). Only then
//     is there a real data boundary — and then tabs may not be dragged
//     between workspaces on different profiles, because that would carry a
//     page across the profile boundary the user asked for.

namespace bedrock {
namespace session {

// Visual settings a workspace may override. Everything else stays global;
// per-workspace copies of every setting would just be a second settings
// system that drifts from the first.
struct WorkspaceVisuals {
  std::string theme_mode;    // empty = follow global theme (item 28)
  std::string accent_color;  // empty = follow global accent
  bool vertical_tabs = false;
  bool sidebar_visible = false;
};

struct WorkspaceTab {
  int id = 0;
  std::string url;
  std::string title;
  int group_id = 0;  // 0 = ungrouped
  bool pinned = false;
};

struct WorkspaceGroup {
  int id = 0;
  std::string name;
  std::string color;
};

class Workspace {
 public:
  Workspace(int id, std::string name);

  int id() const { return id_; }
  const std::string& name() const { return name_; }
  void SetName(std::string name) { name_ = std::move(name); }

  // Optional profile mapping. Empty = the workspace lives in whatever profile
  // the window already uses.
  const std::string& profile_id() const { return profile_id_; }
  void SetProfileId(std::string profile_id) {
    profile_id_ = std::move(profile_id);
  }
  bool has_profile() const { return !profile_id_.empty(); }

  WorkspaceVisuals& visuals() { return visuals_; }
  const WorkspaceVisuals& visuals() const { return visuals_; }

  // Tabs and groups.
  int AddTab(const std::string& url, const std::string& title);
  bool RemoveTab(int tab_id);
  const std::vector<WorkspaceTab>& tabs() const { return tabs_; }
  int tab_count() const { return static_cast<int>(tabs_.size()); }

  int AddGroup(const std::string& name, const std::string& color);
  bool AssignToGroup(int tab_id, int group_id);
  const std::vector<WorkspaceGroup>& groups() const { return groups_; }
  // Removing a group keeps its tabs; a group is a label, not an owner.
  bool RemoveGroup(int group_id);

  // The honest one-line description shown next to the workspace name.
  std::string privacy_summary() const;

 private:
  friend class WorkspaceManager;
  WorkspaceTab* Find(int tab_id);

  int id_;
  std::string name_;
  std::string profile_id_;
  WorkspaceVisuals visuals_;
  std::vector<WorkspaceTab> tabs_;
  std::vector<WorkspaceGroup> groups_;
  int next_tab_id_ = 1;
  int next_group_id_ = 1;
};

class WorkspaceManager {
 public:
  WorkspaceManager();
  ~WorkspaceManager();

  // Creates a workspace; names are not required to be unique (users rename
  // things), ids are.
  int Create(const std::string& name);
  bool Remove(int workspace_id);

  Workspace* Get(int workspace_id);
  const Workspace* Get(int workspace_id) const;
  std::vector<int> Ids() const;
  int count() const { return static_cast<int>(workspaces_.size()); }

  // Switching is a view change: the tabs of the workspace being left stay
  // open and loaded state is untouched by this model. Nothing is closed on
  // switch — a workspace switch that discards tabs is data loss with a
  // friendly name.
  bool Switch(int workspace_id);
  int active() const { return active_; }

  // Moving a tab between workspaces. Refused across a profile boundary.
  bool MoveTab(int from_workspace, int tab_id, int to_workspace);

  // Why the last MoveTab/Remove was refused ("" if it succeeded).
  const std::string& last_error() const { return last_error_; }

 private:
  std::vector<Workspace> workspaces_;
  int active_ = 0;
  int next_id_ = 1;
  std::string last_error_;
};

}  // namespace session
}  // namespace bedrock

#endif  // BEDROCK_WORKSPACES_WORKSPACE_MANAGER_H_
