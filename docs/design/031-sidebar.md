# 031 — Sidebar

**Roadmap item 31.** Status: landed and host-tested
(`src_overrides/bedrock/ui/sidebar.{h,cc}`).

Panels: bookmarks · history · downloads · reading list · extensions · workspaces · tab groups ·
notes.

**Optional means optional.** Every panel is also reachable through a menu path *and* a keyboard
shortcut, and the test asserts both for every entry in the enum. Without that rule, "optional"
quietly becomes "optional unless you want your bookmarks" while the settings page still calls it
a choice. Shortcut uniqueness is also a test: a duplicate shortcut means one of the two silently
never fires.

Behaviour worth naming:

- **Hidden on first run.** A browser that opens with a sidebar has already decided for the user.
- **Disabling the active panel** moves the selection to the first enabled one; disabling the
  *last* one hides the sidebar rather than showing an empty rail.
- **Reordering never drops a panel**: anything the caller forgets keeps its place, and duplicates
  in the request are ignored.
- **Width is clamped** to 200–480 px. A sidebar draggable to 3 px is a bug generator; one that
  can eat the window is worse.

`docs/design/mockups/vertical-tabs-light.html` shows the rail, an open panel, vertical tabs and
compact density together, in the light theme.
