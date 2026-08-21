// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PLATFORM_PLATFORM_SUPPORT_H_
#define BEDROCK_PLATFORM_PLATFORM_SUPPORT_H_

#include <string>
#include <vector>

// Platform support and native integration (roadmap items 62, 63, 64).
//
// Windows and Linux are the platforms Bedrock is built for; macOS is best
// effort — the code stays portable and the abstraction stays honest, but a
// platform is only called supported once someone builds, tests and ships it.
// Calling macOS "supported" with nobody testing it is item 55 applied to a
// download page.
//
// The architectural rule this file exists to protect:
//
//   **Bedrock's own code never asks which platform it is on.** It asks the
//   platform layer for a capability. `#ifdef _WIN32` scattered through the
//   privacy engine is how a codebase becomes three codebases that share a
//   directory, and how the third one silently rots.
//
// `scripts/check_platform.py` enforces that: platform macros are allowed only
// under `src_overrides/bedrock/platform/`, and nothing anywhere may hardcode a
// specific Linux desktop environment.
//
// The second rule is about Linux specifically: **no desktop environment is
// assumed.** Not GNOME, not KDE, not a systemd session, not a particular portal
// implementation. Bedrock detects what is there and degrades to something that
// works — a browser that misbehaves outside one desktop is a browser most Linux
// users experience as broken.
//
// Most native behaviour here is Chromium's already: it has the Windows shell
// integration, the Wayland and X11 backends, the DPI handling, the native
// dialogs. So each integration point below records an owner. `kChromium` means
// "inherited — our job is not to break it", and the requirement text says how a
// custom browser UI typically *does* break it. `kBedrock` means the overlay has
// to do the work, because the surface is ours.

namespace bedrock {
namespace platform {

enum class Platform {
  kWindows,
  kLinux,
  kMacOS,
  kMaxValue = kMacOS,
};

enum class SupportTier {
  kSupported,   // built, tested and released
  kBestEffort,  // kept buildable, no release promise
};

enum class Owner {
  kChromium,  // Chromium provides it; the overlay must not break it
  kBedrock,   // the overlay's own surface, so the overlay's own work
};

// Windowing/session backends. Linux has two, and both are first class: X11 is
// not a legacy afterthought while it is what many users are still on, and
// Wayland is not experimental while it is the default of the biggest desktops.
enum class DisplayBackend {
  kWindowsDesktop,
  kWayland,
  kX11,
  kQuartz,
};

// The integration points items 63 and 64 name, plus the ones a privacy browser
// cannot get wrong without leaking something.
enum class IntegrationPoint {
  kNativeWindowBehavior,
  kSystemColorScheme,
  kTitlebar,
  kDpiScaling,
  kKeyboardShortcuts,
  kFileDialogs,
  kNotifications,
  kContextMenus,
  kDesktopIntegration,  // launchers, default-browser, file associations
  kSystemTheme,
  kPackaging,
  kMaxValue = kPackaging,
};

struct PlatformInfo {
  Platform platform;
  const char* name;
  SupportTier tier;
  std::vector<DisplayBackend> backends;
  // Why this tier, in one sentence. A tier without a reason becomes a promise
  // nobody remembers making.
  const char* tier_reason;
};

struct IntegrationRequirement {
  Platform platform;
  IntegrationPoint point;
  Owner owner;
  const char* requirement;  // what Bedrock must do
  const char* failure_mode;  // what browsers that skip it get wrong
};

// A Linux packaging format Bedrock produces or explicitly does not.
struct PackageFormat {
  const char* name;
  bool produced;
  const char* note;
};

class PlatformSupport {
 public:
  PlatformSupport();

  const std::vector<PlatformInfo>& Platforms() const;
  const PlatformInfo* Info(Platform platform) const;

  std::vector<IntegrationRequirement> Requirements(Platform platform) const;
  const IntegrationRequirement* Requirement(Platform platform,
                                            IntegrationPoint point) const;

  // Linux packaging (item 64). Sandboxed formats are listed with what they cost
  // as well as what they give.
  const std::vector<PackageFormat>& LinuxPackageFormats() const;

  // True when the platform's release channel is a promise the project keeps.
  bool IsSupported(Platform platform) const;

  // Every integration point a platform must answer for. Used by the test to
  // prove no platform quietly skips one.
  static std::vector<IntegrationPoint> AllPoints();

  static const char* Name(Platform platform);
  static const char* Name(IntegrationPoint point);
  static const char* Name(DisplayBackend backend);
};

}  // namespace platform
}  // namespace bedrock

#endif  // BEDROCK_PLATFORM_PLATFORM_SUPPORT_H_
