# Platform support

**Roadmap items 62, 63, 64.** Windows and Linux are what Bedrock is built for. macOS is best
effort: kept buildable and kept out of the abstraction's way, but not promised.

| Platform | Tier | Backends | Why |
| --- | --- | --- | --- |
| Windows | supported | Windows desktop | Built, tested and released; Windows 10 and later |
| Linux | supported | Wayland **and** X11 | Both backends released, no desktop environment assumed |
| macOS | best effort | Quartz | Kept buildable, not release-tested — calling it supported would be a promise the project cannot keep |

Tiers are honest on purpose. A download page listing a platform nobody tests is item 55 in a
different room.

## The architectural rule

**Bedrock's own code never asks which platform it is on.** It asks the platform layer for a
capability. `#ifdef _WIN32` scattered through the privacy engine is how one codebase becomes three
that share a directory — and how the third one rots unnoticed.

`scripts/check_platform.py` enforces it: platform macros (`_WIN32`, `__linux__`, `__APPLE__`,
`OS_WIN`, …) are allowed only under `src_overrides/bedrock/platform/`.

## Windows (item 63)

Owner **C** = Chromium provides it and our job is not to break it; **B** = Bedrock's own surface.

| Point | Owner | What Bedrock must do |
| --- | --- | --- |
| Native window behavior | C | Snap, Aero Shake, Win+arrow tiling, multi-monitor restore all behave as the system's |
| System dark/light mode | B | Follow the system setting and change **live**, no restart; an explicit theme choice overrides |
| Titlebar | B | Caption buttons in system order and size; drag region, system menu and accent colour intact |
| DPI scaling | C | Per-monitor DPI v2 — dragging between a 100% and a 200% monitor rescales our surfaces too |
| Keyboard shortcuts | B | The Windows shortcuts users already know keep working; nothing shadows a system one |
| File dialogs | C | The system `IFileDialog`, so shell places and network drives work |
| Notifications | B | Action Center toasts honouring Focus Assist and per-app settings |
| Context menus | B | Native menus, system metrics, keyboard traversal, mnemonics |
| Desktop integration | B | Default-browser, jump lists, associations — documented shell APIs only, never registry tricks |
| System theme | B | High-contrast themes honoured; accent colour where the system expects it |
| Packaging | B | Signed installer plus a portable build; the updater verifies signatures and never elevates silently |

## Linux (item 64)

**No desktop environment is assumed.** Not GNOME, not KDE, not a systemd session, not one portal
implementation. Bedrock detects what is present and degrades to something that works — a browser
that misbehaves outside one desktop is a browser most Linux users experience as broken.

| Point | Owner | What Bedrock must do |
| --- | --- | --- |
| Native window behavior | C | Wayland and X11 both first class, detected at runtime, selectable with `--ozone-platform` |
| System dark/light mode | B | XDG settings portal first, toolkit setting second, Bedrock default last |
| Titlebar | B | Client- and server-side decorations both supported; compositor preference honoured, user can override |
| DPI scaling | C | Fractional scaling, per-output scale factors, portal text scaling |
| Keyboard shortcuts | B | Same set as elsewhere minus compositor-reserved keys; nothing depends on a global grab (Wayland has none) |
| File dialogs | C | XDG desktop portal chooser, so the dialog works inside a sandbox; toolkit dialog as fallback |
| Notifications | B | freedesktop notifications, degrading quietly when no daemon is running |
| Context menus | B | System font, scale and contrast from the portal; keyboard-navigable on both backends |
| Desktop integration | B | Conformant `.desktop` entry, icon theme spec, `xdg-settings` — specifications, not one desktop's conventions |
| System theme | B | Portal colour-scheme and contrast preferences, still validated by the theme engine so a broken system theme cannot produce unreadable UI |
| Packaging | B | Several formats, each documenting its sandbox implications |

### Package formats

| Format | Produced | Note |
| --- | --- | --- |
| tar.xz | yes | Works on every distribution; the baseline and the reproducible-build target |
| deb | yes | Debian/Ubuntu, signed repository |
| rpm | yes | Fedora/openSUSE, signed repository |
| flatpak | yes | Sandboxed — which is why portals are the required file-chooser and notification path above |
| AppImage | yes | Single file, no install; the updater is disabled because the file is not ours to rewrite |
| snap | **no** | Needs a single vendor's store; a project with no server of its own will not depend on one |

## macOS (item 62)

Best effort means: the code stays portable, the shared tables stay data (the shortcut table swaps
Control for Command without a per-platform branch), and CI keeps the build recipe exercised.
Notarised releases wait until the tier changes. The port is not allowed to become a third codebase
while nobody is looking.
