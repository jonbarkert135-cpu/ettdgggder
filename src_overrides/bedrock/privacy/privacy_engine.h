// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_PRIVACY_ENGINE_H_
#define BEDROCK_PRIVACY_PRIVACY_ENGINE_H_

#include <string>
#include <vector>

// The Bedrock Privacy Engine.
//
// Design rule: privacy is ONE subsystem with one profile-wide level and one
// per-site override, not a field of unrelated checkboxes. Every control below
// is a Feature with a stable id, an owning module, and a user-facing
// explanation string — the UI is generated from this registry, so a feature
// that cannot be explained cannot ship (see docs/design/008-privacy-engine.md).

namespace bedrock {
namespace privacy {

// Profile-wide posture. Per-site overrides move a single site up or down.
enum class Level {
  kStandard,  // blocks tracking, keeps sites working
  kStrict,    // adds aggressive fingerprint defence, may break some sites
  kCustom,    // user has touched individual features
};

// Which module implements a feature. Determines where the enforcement lives
// and therefore what a bug in it can affect.
enum class Module {
  kContentBlocker,   // adblock-rust engine: network + cosmetic rules
  kNetwork,          // network service: referrer, query params, HTTPS, DNS
  kStorage,          // cookie/storage partitioning and isolation
  kFingerprint,      // renderer-side Web API hardening
  kPermissions,      // permission and device-access gating
};

// Stable ids. Never renumber: they are used in prefs, sync-free export/import
// of settings, and in the per-site override records.
enum class Feature {
  // kContentBlocker
  kTrackerProtection = 0,
  kAdBlocking = 1,
  kCosmeticFiltering = 2,
  kCrossSiteTrackingProtection = 3,

  // kNetwork
  kReferrerControl = 10,
  kQueryParamStripping = 11,
  kHttpsOnly = 12,
  kSecureDns = 13,
  kThirdPartyRequestControl = 14,

  // kStorage
  kCookieIsolation = 20,
  kStoragePartitioning = 21,
  kEphemeralThirdPartyStorage = 22,

  // kFingerprint
  kCanvasProtection = 30,
  kWebglControls = 31,
  kFontExposureControl = 32,
  kClientHintsControl = 33,
  kLanguageNormalization = 34,
  kTimezoneNormalization = 35,
  kScreenMetricNormalization = 36,
  kHardwareInfoReduction = 37,
  kBatteryApiProtection = 38,
  kWebrtcPolicy = 39,
  kTimerCoarsening = 40,

  // kPermissions
  kMediaDeviceEnumeration = 50,
  kGamepadAndSensorExposure = 51,
  kClipboardPermission = 52,
  kGeolocationPermission = 53,
  kNotificationPermission = 54,
  kAutoplayControl = 55,
  kPermissionIsolation = 56,
};

// How strongly a feature is applied. Not every feature uses all three.
enum class Setting {
  kOff,
  kStandard,
  kStrict,
};

// One row of the registry that drives both enforcement and the settings UI.
struct FeatureInfo {
  Feature feature;
  Module module;
  const char* id;            // stable string id, e.g. "canvas_protection"
  int title_string_id;       // IDS_BEDROCK_PRIVACY_*_TITLE
  int explanation_string_id; // IDS_BEDROCK_PRIVACY_*_EXPLANATION — required
  Setting standard_default;
  Setting strict_default;
  bool breaks_sites;         // shown as a warning next to the control
};

// The single source of truth. Defined in privacy_engine.cc; the settings page,
// the shields panel and the per-site override store all iterate this.
const std::vector<FeatureInfo>& GetFeatureRegistry();

// Resolved value for a site: per-site override, else the profile level default.
Setting GetEffectiveSetting(Feature feature,
                            Level level,
                            const std::string& etld_plus_one);

// Human-readable reason a request/API call was modified, for the shields panel
// and bedrock://privacy-log (local only, never uploaded).
struct Action {
  Feature feature;
  std::string detail;  // "blocked doubleclick.net", "stripped ?fbclid"
};

}  // namespace privacy
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_PRIVACY_ENGINE_H_
