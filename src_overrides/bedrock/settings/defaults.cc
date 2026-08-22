// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/settings/defaults.h"

#include <string>
#include <vector>

namespace bedrock {
namespace settings {

namespace {

// Item 84, in the order it lists them. `docs/DEFAULTS.md` is checked against
// this table, so the documentation cannot promise a default the code does not
// set.
const std::vector<DefaultSetting>& Table() {
  static const std::vector<DefaultSetting> table = {
      {"secure_browsing", "enabled",
       "Site isolation, sandboxing and the security baseline of item 24 are on "
       "for everyone; there is no user for whom exploitation is acceptable.",
       true},
      {"https_upgrades", "enabled",
       "Upgrading is nearly free and removes on-path attacks. Strict HTTPS-only "
       "(which fails rather than falls back) is the Strict privacy choice, not "
       "the default, because unreachable local devices are a real cost.",
       true},
      {"third_party_tracking_protection", "enabled",
       "The single largest privacy improvement available without breaking the "
       "web, and the one users cannot achieve for themselves.",
       true},
      {"third_party_cookies", "restricted",
       "Restricted rather than blocked outright: blocked breaks third-party "
       "login flows that people rely on, and a browser they abandon protects "
       "nobody.",
       true},
      {"fingerprint_protection", "balanced",
       "Level 1 of the fingerprinting ladder: normalise the values that cost "
       "nothing to normalise, perturb canvas per site, leave the aggressive "
       "measures to Strict, where the user has accepted the breakage.",
       true},
      {"ad_and_tracker_blocking", "enabled",
       "Blocking at the network layer is faster than loading and hiding, and "
       "removes a common malware delivery path as a side effect.",
       true},
      {"telemetry", "disabled",
       "Not a trade-off and not a setting to negotiate: Bedrock has no "
       "reporting machinery at all (item 39). There is nothing to enable.",
       false},
      {"crash_reporting", "disabled",
       "Crash reports stay on the machine. Upload requires per-report consent "
       "from the user (item 81); no axis and no channel changes that.",
       false},
      {"webrtc_privacy", "enabled",
       "Local IP addresses are not exposed to pages. The leak surprised even "
       "technical users for years, and almost no site needs host candidates.",
       true},
      {"secure_dns", "configurable",
       "Deliberately not forced on: encrypted DNS moves trust to a resolver "
       "the user did not pick, and breaks captive portals and split-horizon "
       "networks. Bedrock asks instead of choosing a provider for you.",
       true},
      {"extension_permissions", "explicit",
       "An extension gets what it asked for at install, shown as a capability "
       "disclosure, and an update can never widen it silently (item 23).",
       true},
      {"site_permissions", "ask_when_needed",
       "Camera, microphone, location, notifications and clipboard are asked "
       "for at the moment of use, on the site that needs them — never "
       "pre-granted and never inherited by an embedded frame.",
       true},
  };
  return table;
}

Change Weaken(const std::string& id,
              const std::string& from,
              const std::string& to,
              const std::string& why) {
  return {id, from, to, true, why};
}

Change Strengthen(const std::string& id,
                  const std::string& from,
                  const std::string& to,
                  const std::string& why) {
  return {id, from, to, false, why};
}

}  // namespace

const std::vector<DefaultSetting>& FactoryDefaults() {
  return Table();
}

const DefaultSetting* FindDefault(const std::string& id) {
  for (const DefaultSetting& setting : Table()) {
    if (id == setting.id) {
      return &setting;
    }
  }
  return nullptr;
}

Profile FactoryProfile() {
  return Profile();
}

const char* FactoryProfileName() {
  return "Balanced Privacy";
}

std::vector<Change> ChangesFor(Axis axis, int choice) {
  std::vector<Change> changes;
  switch (axis) {
    case Axis::kPrivacy:
      if (choice == static_cast<int>(PrivacyChoice::kStandard)) {
        changes.push_back(Weaken("fingerprint_protection", "balanced", "off",
                                 "Sites read your real canvas, fonts and "
                                 "hardware values again."));
        changes.push_back(Weaken("third_party_cookies", "restricted", "allowed",
                                 "Third parties can follow you between sites "
                                 "with a shared cookie."));
      } else if (choice == static_cast<int>(PrivacyChoice::kStrict)) {
        changes.push_back(Strengthen("fingerprint_protection", "balanced",
                                     "strict",
                                     "Screen metrics are letterboxed and "
                                     "timers coarsened further."));
        changes.push_back(Strengthen("third_party_cookies", "restricted",
                                     "blocked",
                                     "Third-party login buttons will need the "
                                     "storage-access prompt."));
        changes.push_back(Strengthen("https_upgrades", "enabled",
                                     "https_only",
                                     "A site with no HTTPS endpoint shows an "
                                     "interstitial instead of loading."));
      }
      break;

    case Axis::kCompatibility:
      if (choice == static_cast<int>(CompatibilityChoice::kMaximum)) {
        changes.push_back(Weaken("third_party_cookies", "restricted", "allowed",
                                 "This is the setting most often behind a "
                                 "broken checkout — and the one trackers use."));
        changes.push_back(Weaken("ad_and_tracker_blocking", "enabled",
                                 "trackers_only",
                                 "Cosmetic filtering stops, so pages look as "
                                 "their authors intended, ads included."));
      } else if (choice == static_cast<int>(CompatibilityChoice::kStrict)) {
        changes.push_back(Strengthen("third_party_requests", "off", "blocked",
                                     "Most of the web will need per-site "
                                     "exceptions. This is an expert setting."));
      }
      break;

    case Axis::kPerformance:
      // Neither direction touches a protection: the performance axis moves
      // rendering and background work, not privacy. That is the point of
      // having separate axes.
      if (choice == static_cast<int>(PerformanceChoice::kSpeed)) {
        changes.push_back(Strengthen("preload_and_prerender", "conservative",
                                     "enabled",
                                     "Pages you are likely to open are fetched "
                                     "early — from sites you already visit."));
      } else if (choice == static_cast<int>(PerformanceChoice::kEfficiency)) {
        changes.push_back(Strengthen("background_tab_throttling", "standard",
                                     "aggressive",
                                     "Background tabs do less work; some "
                                     "timers fire late."));
      }
      break;

    case Axis::kAppearance:
      // Appearance never changes behaviour. A theme is a theme.
      break;
  }
  return changes;
}

}  // namespace settings
}  // namespace bedrock
