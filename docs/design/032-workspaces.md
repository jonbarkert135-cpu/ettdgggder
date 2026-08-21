# 032 — Workspaces

**Roadmap item 32.** Status: landed and host-tested
(`src_overrides/bedrock/workspaces/workspace_manager.{h,cc}`).

A workspace is a named set of **tabs**, **tab groups**, **visual settings** and an **optional
profile mapping**: Research, Personal, School, Development.

## The one decision that matters

A workspace is an *organisational* boundary, not a privacy boundary. Two workspaces inside the
same profile share cookies, storage, history and logins, because they are the same profile. A
user who believes "Personal" and "Work" keep sites apart, when they do not, has been misled by
our UI — and they will only find out the day it matters.

So every workspace carries `privacy_summary()`, shown next to its name:

- unmapped: *"Organises tabs only. Cookies, storage, history and logins are shared with the other
  workspaces of this profile."*
- mapped to a profile: *"Separate profile: cookies, storage, history and logins are kept apart
  from other profiles."*

The test asserts an unmapped workspace never contains the words *separate* or *isolated*.

## Behaviour

- **Switching never closes anything.** A workspace switch that discards tabs is data loss with a
  friendly name.
- **Profile mapping is optional** (item 21). Only a mapping creates a real data boundary.
- **Tabs cannot be moved between workspaces on different profiles.** Dragging a tab across would
  carry a live page and its session over the profile line the user asked for — the exception that
  makes the rule meaningless. The refusal is returned with a reason, not swallowed.
- A moved tab arrives **ungrouped**: group ids belong to a workspace, and the same number means
  something else on the other side.
- **Removing a group keeps its tabs.** A group is a label, not an owner.
- **Visual overrides are a short list** (theme mode, accent, vertical tabs, sidebar) and default
  to "follow the global setting" (items 28–30). Per-workspace copies of every setting would be a
  second settings system that drifts from the first.
- **One workspace always exists.** "No workspace" is not a state the UI should have to render,
  and the last one cannot be removed.
