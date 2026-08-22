// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/platform/platform_support.h"

#include <vector>

namespace bedrock {
namespace platform {

namespace {

// clang-format off
const IntegrationRequirement kRequirements[] = {
    // ---------------- Windows (item 63) ----------------
    {Platform::kWindows, IntegrationPoint::kNativeWindowBehavior, Owner::kChromium,
     "Snap, Aero Shake, Win+arrow tiling, minimise/maximise/restore animations and "
     "multi-monitor placement all behave as the system's, including when the window "
     "is restored onto a monitor that no longer exists",
     "A custom-drawn frame that swallows the resize borders and drops snap"},
    {Platform::kWindows, IntegrationPoint::kSystemColorScheme, Owner::kBedrock,
     "Follow the system light/dark setting by default and change live when the user "
     "flips it, without a restart; an explicit Bedrock theme choice overrides it",
     "Reading the setting once at startup, so the browser is the only bright window "
     "on the desktop at sunset"},
    {Platform::kWindows, IntegrationPoint::kTitlebar, Owner::kBedrock,
     "The tabstrip titlebar keeps the caption buttons in the system's order and size, "
     "leaves the drag region and the system menu working, and respects the accent "
     "colour setting",
     "A titlebar that looks native until the window is maximised and the close button "
     "sits a few pixels off the corner"},
    {Platform::kWindows, IntegrationPoint::kDpiScaling, Owner::kChromium,
     "Per-monitor DPI v2: dragging the window between a 100% and a 200% monitor "
     "rescales it, and Bedrock's own surfaces rescale with it",
     "Bitmap-stretched UI on the second monitor, or icons that stay 16 px on a 4K panel"},
    {Platform::kWindows, IntegrationPoint::kKeyboardShortcuts, Owner::kBedrock,
     "The Windows shortcuts users already know keep working (Ctrl+T/W/Shift+T, F11, "
     "F6, Alt+D, Ctrl+Shift+N for a private window), and no Bedrock shortcut shadows a "
     "system one",
     "Rebinding a familiar accelerator to a house feature, so muscle memory does the "
     "wrong thing"},
    {Platform::kWindows, IntegrationPoint::kFileDialogs, Owner::kChromium,
     "The system IFileDialog is used for open, save and download-to, so shell "
     "features (OneDrive, network places, recents) work",
     "An in-page file picker that cannot see mapped drives"},
    {Platform::kWindows, IntegrationPoint::kNotifications, Owner::kBedrock,
     "Native toasts through the Action Center, honouring Focus Assist and the "
     "system's per-app notification setting; permission is per-site and revocable",
     "Custom always-on-top popups that ignore do-not-disturb"},
    {Platform::kWindows, IntegrationPoint::kContextMenus, Owner::kBedrock,
     "Native menus with system metrics, keyboard traversal, mnemonics and the "
     "shell entries the platform expects",
     "A web-rendered menu that ignores the theme and cannot be reached by keyboard"},
    {Platform::kWindows, IntegrationPoint::kDesktopIntegration, Owner::kBedrock,
     "Default-browser registration, jump lists, protocol and file associations, "
     "and pinned-taskbar behaviour go through the documented shell APIs only",
     "Writing registry keys behind the settings app's back — the trick that got other "
     "browsers into the news"},
    {Platform::kWindows, IntegrationPoint::kSystemTheme, Owner::kBedrock,
     "High-contrast themes are honoured, and accent colour is used where the system "
     "expects it",
     "A fixed dark chrome that makes high-contrast mode unreadable"},
    {Platform::kWindows, IntegrationPoint::kPackaging, Owner::kBedrock,
     "A signed installer and a portable build; the updater checks signatures and "
     "never elevates silently",
     "An unsigned installer that trains users to click through SmartScreen"},

    // ---------------- Linux (item 64) ----------------
    {Platform::kLinux, IntegrationPoint::kNativeWindowBehavior, Owner::kChromium,
     "Wayland and X11 are both first-class; the backend is detected at runtime and "
     "selectable with --ozone-platform, and window state follows the compositor "
     "rather than being drawn over it",
     "Assuming X11 and getting XWayland's blurry scaling on a HiDPI Wayland session"},
    {Platform::kLinux, IntegrationPoint::kSystemColorScheme, Owner::kBedrock,
     "Read the colour scheme from the XDG settings portal, which works without "
     "assuming any desktop environment; fall back to the toolkit setting, then to a "
     "Bedrock default",
     "Reading a GNOME-specific key and shipping a permanently light browser on every "
     "other desktop"},
    {Platform::kLinux, IntegrationPoint::kTitlebar, Owner::kBedrock,
     "Both client-side and server-side decorations are supported; the compositor's "
     "preference is honoured, and the user can override it",
     "Forcing client-side decorations onto a desktop whose users expect their window "
     "manager's titlebars"},
    {Platform::kLinux, IntegrationPoint::kDpiScaling, Owner::kChromium,
     "Wayland fractional scaling and per-output scale factors; mixed-DPI multi-monitor "
     "setups stay sharp, and text scaling from the portal is applied",
     "One global scale factor, so the laptop panel or the external monitor is always "
     "wrong"},
    {Platform::kLinux, IntegrationPoint::kKeyboardShortcuts, Owner::kBedrock,
     "The same shortcut set as other platforms, minus anything the compositor has "
     "reserved; nothing depends on a global grab, which Wayland does not offer",
     "A shortcut that works on X11 and silently does nothing on Wayland"},
    {Platform::kLinux, IntegrationPoint::kFileDialogs, Owner::kChromium,
     "The XDG desktop portal file chooser, so the dialog is the user's own and works "
     "inside a sandbox; the toolkit dialog is the fallback when no portal is present",
     "A GTK dialog hardcoded on a Qt desktop, or a picker that cannot see the "
     "filesystem inside a Flatpak"},
    {Platform::kLinux, IntegrationPoint::kNotifications, Owner::kBedrock,
     "The freedesktop notification interface, with graceful degradation when no "
     "daemon is running — a missing notification daemon is not an error dialog",
     "Crashing or spamming errors on a minimal window manager with no notification "
     "service"},
    {Platform::kLinux, IntegrationPoint::kContextMenus, Owner::kBedrock,
     "Menus follow the system font, scale factor and dark preference reported by the "
     "portal, and stay keyboard-navigable under both display backends",
     "Menus that ignore the system font size, unreadable at a 200% text scale"},
    {Platform::kLinux, IntegrationPoint::kDesktopIntegration, Owner::kBedrock,
     "A conformant .desktop entry with actions and MIME associations, an icon set "
     "following the icon theme specification, and default-browser handling through "
     "xdg-settings — all specifications, not one desktop's conventions",
     "Installing a launcher that only appears in one desktop's menu"},
    {Platform::kLinux, IntegrationPoint::kSystemTheme, Owner::kBedrock,
     "Follow the portal's colour-scheme and contrast preferences; the theme engine "
     "still validates contrast, so a broken system theme cannot produce unreadable UI",
     "Inheriting a GTK theme wholesale and rendering white text on white in the "
     "privacy panel"},
    {Platform::kLinux, IntegrationPoint::kPackaging, Owner::kBedrock,
     "Multiple formats, each with its sandbox implications documented; the tarball "
     "stays available because it is the one format every distribution can use",
     "Shipping only a snap or only a deb and calling it Linux support"},

    // ---------------- macOS (item 62, best effort) ----------------
    {Platform::kMacOS, IntegrationPoint::kNativeWindowBehavior, Owner::kChromium,
     "Kept building and behaving through Chromium's Cocoa layer; no Bedrock code may "
     "assume it is absent",
     "Letting the port rot until reviving it is a rewrite"},
    {Platform::kMacOS, IntegrationPoint::kSystemColorScheme, Owner::kBedrock,
     "Follow the system appearance through the same abstraction the other platforms "
     "use — no macOS-specific branch in shared code",
     "A third copy of the theme logic"},
    {Platform::kMacOS, IntegrationPoint::kTitlebar, Owner::kBedrock,
     "Traffic-light buttons keep their position and behaviour, including in full "
     "screen",
     "A tabstrip drawn over the traffic lights"},
    {Platform::kMacOS, IntegrationPoint::kDpiScaling, Owner::kChromium,
     "Retina backing scale factors are handled by the toolkit",
     "Non-integer scaling artefacts in overlay UI"},
    {Platform::kMacOS, IntegrationPoint::kKeyboardShortcuts, Owner::kBedrock,
     "Command replaces Control in the shared shortcut table; the table is data, not "
     "per-platform code",
     "Ctrl-based shortcuts shipped to macOS because the table was hardcoded"},
    {Platform::kMacOS, IntegrationPoint::kFileDialogs, Owner::kChromium,
     "NSOpenPanel and NSSavePanel through the toolkit",
     "A cross-platform picker nobody on the platform recognises"},
    {Platform::kMacOS, IntegrationPoint::kNotifications, Owner::kBedrock,
     "The user notification centre, honouring system focus modes",
     "Ignoring Do Not Disturb"},
    {Platform::kMacOS, IntegrationPoint::kContextMenus, Owner::kBedrock,
     "Native menus, including the application menu bar",
     "An in-window menu bar"},
    {Platform::kMacOS, IntegrationPoint::kDesktopIntegration, Owner::kBedrock,
     "Default-browser registration and URL handling through the documented APIs",
     "Prompting to be default on every launch"},
    {Platform::kMacOS, IntegrationPoint::kSystemTheme, Owner::kBedrock,
     "Increase-contrast and reduce-motion accessibility settings are honoured",
     "Animations that ignore reduce-motion"},
    {Platform::kMacOS, IntegrationPoint::kPackaging, Owner::kBedrock,
     "A build recipe exists and is exercised in CI; notarised releases wait until the "
     "platform is more than best effort",
     "Publishing an unnotarised build users must right-click to open"},
};
// clang-format on

}  // namespace

PlatformSupport::PlatformSupport() = default;

const std::vector<PlatformInfo>& PlatformSupport::Platforms() const {
  static const std::vector<PlatformInfo> kPlatforms = {
      {Platform::kWindows, "Windows", SupportTier::kSupported,
       {DisplayBackend::kWindowsDesktop},
       "Built, tested and released; Windows 10 and later"},
      {Platform::kLinux, "Linux", SupportTier::kSupported,
       {DisplayBackend::kWayland, DisplayBackend::kX11},
       "Built, tested and released on both display backends, with no desktop "
       "environment assumed"},
      {Platform::kMacOS, "macOS", SupportTier::kBestEffort,
       {DisplayBackend::kQuartz},
       "Kept buildable and portable, but not release-tested — calling it "
       "supported would be a promise the project cannot keep today"},
  };
  return kPlatforms;
}

const PlatformInfo* PlatformSupport::Info(Platform platform) const {
  for (const PlatformInfo& info : Platforms()) {
    if (info.platform == platform)
      return &info;
  }
  return nullptr;
}

std::vector<IntegrationRequirement> PlatformSupport::Requirements(
    Platform platform) const {
  std::vector<IntegrationRequirement> requirements;
  for (const IntegrationRequirement& requirement : kRequirements) {
    if (requirement.platform == platform)
      requirements.push_back(requirement);
  }
  return requirements;
}

const IntegrationRequirement* PlatformSupport::Requirement(
    Platform platform,
    IntegrationPoint point) const {
  for (const IntegrationRequirement& requirement : kRequirements) {
    if (requirement.platform == platform && requirement.point == point)
      return &requirement;
  }
  return nullptr;
}

const std::vector<PackageFormat>& PlatformSupport::LinuxPackageFormats() const {
  static const std::vector<PackageFormat> kFormats = {
      {"tar.xz", true,
       "Works on every distribution and inside none of the sandboxes; the "
       "baseline, and the one used to reproduce a build"},
      {"deb", true, "Debian and Ubuntu, with the repository signed"},
      {"rpm", true, "Fedora and openSUSE, with the repository signed"},
      {"flatpak", true,
       "Sandboxed; the file chooser and notifications go through portals, which "
       "is why those are the required implementations above"},
      {"AppImage", true,
       "Single-file, no installation; the updater is disabled in this format "
       "because the file is not ours to rewrite"},
      {"snap", false,
       "Not produced: it needs a single vendor's store, which a project with no "
       "server of its own will not depend on"},
  };
  return kFormats;
}

bool PlatformSupport::IsSupported(Platform platform) const {
  const PlatformInfo* info = Info(platform);
  return info != nullptr && info->tier == SupportTier::kSupported;
}

// static
std::vector<IntegrationPoint> PlatformSupport::AllPoints() {
  std::vector<IntegrationPoint> points;
  for (int value = 0;
       value <= static_cast<int>(IntegrationPoint::kMaxValue); ++value) {
    points.push_back(static_cast<IntegrationPoint>(value));
  }
  return points;
}

// static
const char* PlatformSupport::Name(Platform platform) {
  switch (platform) {
    case Platform::kWindows:
      return "Windows";
    case Platform::kLinux:
      return "Linux";
    case Platform::kMacOS:
      return "macOS";
  }
  return "unknown";
}

// static
const char* PlatformSupport::Name(IntegrationPoint point) {
  switch (point) {
    case IntegrationPoint::kNativeWindowBehavior:
      return "Native window behavior";
    case IntegrationPoint::kSystemColorScheme:
      return "System dark/light mode";
    case IntegrationPoint::kTitlebar:
      return "Titlebar";
    case IntegrationPoint::kDpiScaling:
      return "DPI scaling";
    case IntegrationPoint::kKeyboardShortcuts:
      return "Keyboard shortcuts";
    case IntegrationPoint::kFileDialogs:
      return "File dialogs";
    case IntegrationPoint::kNotifications:
      return "Notifications";
    case IntegrationPoint::kContextMenus:
      return "Context menus";
    case IntegrationPoint::kDesktopIntegration:
      return "Desktop integration";
    case IntegrationPoint::kSystemTheme:
      return "System theme";
    case IntegrationPoint::kPackaging:
      return "Packaging";
  }
  return "unknown";
}

// static
const char* PlatformSupport::Name(DisplayBackend backend) {
  switch (backend) {
    case DisplayBackend::kWindowsDesktop:
      return "Windows desktop";
    case DisplayBackend::kWayland:
      return "Wayland";
    case DisplayBackend::kX11:
      return "X11";
    case DisplayBackend::kQuartz:
      return "Quartz";
  }
  return "unknown";
}

}  // namespace platform
}  // namespace bedrock
