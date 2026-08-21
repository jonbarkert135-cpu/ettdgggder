// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/core/privacy_engine.h"

// The feature registry, defined once (roadmap items 8 and 55).
//
// Until this file existed, `GetFeatureRegistry()` was declared and never
// defined: a settings UI generated from a table that was not there. That is the
// exact shape of the problem item 55 forbids, so the table is now real, and
// every row carries a `Status` saying how far the feature actually is.
//
// Status is the honest part. `kEnforced` means the browser enforces it in a
// running build; `kPolicyLanded` means the logic and its tests are in this
// tree but nothing is wired into a Chromium build yet; `kDesigned` means a
// design document and nothing more. Today **nothing is kEnforced**, because no
// Chromium build runs in CI — and the UI is only allowed to render kEnforced
// features (`UiRenderableFeatures()`), so the browser cannot show a switch for
// a protection it does not perform.

namespace bedrock {
namespace privacy {

namespace {

// Fields: feature, module, id, title resource, explanation resource,
//         standard default, strict default, breaks sites, status.
const std::vector<FeatureInfo>& Registry() {
  static const std::vector<FeatureInfo> registry = {
      // kContentBlocker
      {Feature::kTrackerProtection, Module::kContentBlocker, "tracker_protection",
       "IDS_BEDROCK_PRIVACY_TRACKER_PROTECTION_TITLE",
       "IDS_BEDROCK_PRIVACY_TRACKER_PROTECTION_EXPLANATION",
       Setting::kStandard, Setting::kStrict, false, Status::kPolicyLanded},
      {Feature::kAdBlocking, Module::kContentBlocker, "ad_blocking",
       "IDS_BEDROCK_PRIVACY_AD_BLOCKING_TITLE",
       "IDS_BEDROCK_PRIVACY_AD_BLOCKING_EXPLANATION",
       Setting::kStandard, Setting::kStrict, false, Status::kPolicyLanded},
      {Feature::kCosmeticFiltering, Module::kContentBlocker, "cosmetic_filtering",
       "IDS_BEDROCK_PRIVACY_COSMETIC_FILTERING_TITLE",
       "IDS_BEDROCK_PRIVACY_COSMETIC_FILTERING_EXPLANATION",
       Setting::kStandard, Setting::kStrict, true, Status::kPolicyLanded},
      {Feature::kCrossSiteTrackingProtection, Module::kContentBlocker,
       "cross_site_tracking_protection",
       "IDS_BEDROCK_PRIVACY_CROSS_SITE_TRACKING_TITLE",
       "IDS_BEDROCK_PRIVACY_CROSS_SITE_TRACKING_EXPLANATION",
       Setting::kStandard, Setting::kStrict, false, Status::kPolicyLanded},

      // kNetwork
      {Feature::kReferrerControl, Module::kNetwork, "referrer_control",
       "IDS_BEDROCK_PRIVACY_REFERRER_TITLE",
       "IDS_BEDROCK_PRIVACY_REFERRER_EXPLANATION",
       Setting::kStandard, Setting::kStrict, false, Status::kDesigned},
      {Feature::kQueryParamStripping, Module::kNetwork, "query_param_stripping",
       "IDS_BEDROCK_PRIVACY_QUERY_STRIPPING_TITLE",
       "IDS_BEDROCK_PRIVACY_QUERY_STRIPPING_EXPLANATION",
       Setting::kStandard, Setting::kStrict, true, Status::kDesigned},
      {Feature::kHttpsOnly, Module::kNetwork, "https_only",
       "IDS_BEDROCK_PRIVACY_HTTPS_ONLY_TITLE",
       "IDS_BEDROCK_PRIVACY_HTTPS_ONLY_EXPLANATION",
       Setting::kStandard, Setting::kStrict, true, Status::kPolicyLanded},
      {Feature::kSecureDns, Module::kNetwork, "secure_dns",
       "IDS_BEDROCK_PRIVACY_SECURE_DNS_TITLE",
       "IDS_BEDROCK_PRIVACY_SECURE_DNS_EXPLANATION",
       Setting::kStandard, Setting::kStrict, false, Status::kPolicyLanded},
      {Feature::kThirdPartyRequestControl, Module::kNetwork, "third_party_requests",
       "IDS_BEDROCK_PRIVACY_THIRD_PARTY_REQUESTS_TITLE",
       "IDS_BEDROCK_PRIVACY_THIRD_PARTY_REQUESTS_EXPLANATION",
       Setting::kOff, Setting::kStrict, true, Status::kPolicyLanded},

      // kStorage
      {Feature::kCookieIsolation, Module::kStorage, "cookie_isolation",
       "IDS_BEDROCK_PRIVACY_COOKIE_ISOLATION_TITLE",
       "IDS_BEDROCK_PRIVACY_COOKIE_ISOLATION_EXPLANATION",
       Setting::kStandard, Setting::kStrict, false, Status::kPolicyLanded},
      {Feature::kStoragePartitioning, Module::kStorage, "storage_partitioning",
       "IDS_BEDROCK_PRIVACY_STORAGE_PARTITIONING_TITLE",
       "IDS_BEDROCK_PRIVACY_STORAGE_PARTITIONING_EXPLANATION",
       Setting::kStandard, Setting::kStrict, false, Status::kPolicyLanded},
      {Feature::kEphemeralThirdPartyStorage, Module::kStorage, "ephemeral_third_party_storage",
       "IDS_BEDROCK_PRIVACY_EPHEMERAL_STORAGE_TITLE",
       "IDS_BEDROCK_PRIVACY_EPHEMERAL_STORAGE_EXPLANATION",
       Setting::kOff, Setting::kStrict, true, Status::kDesigned},

      // kFingerprint
      {Feature::kCanvasProtection, Module::kFingerprint, "canvas_protection",
       "IDS_BEDROCK_PRIVACY_CANVAS_TITLE", "IDS_BEDROCK_PRIVACY_CANVAS_EXPLANATION",
       Setting::kStandard, Setting::kStrict, true, Status::kPolicyLanded},
      {Feature::kWebglControls, Module::kFingerprint, "webgl_controls",
       "IDS_BEDROCK_PRIVACY_WEBGL_TITLE", "IDS_BEDROCK_PRIVACY_WEBGL_EXPLANATION",
       Setting::kStandard, Setting::kStrict, true, Status::kPolicyLanded},
      {Feature::kFontExposureControl, Module::kFingerprint, "font_exposure",
       "IDS_BEDROCK_PRIVACY_FONTS_TITLE", "IDS_BEDROCK_PRIVACY_FONTS_EXPLANATION",
       Setting::kStandard, Setting::kStrict, true, Status::kPolicyLanded},
      {Feature::kClientHintsControl, Module::kFingerprint, "client_hints",
       "IDS_BEDROCK_PRIVACY_CLIENT_HINTS_TITLE",
       "IDS_BEDROCK_PRIVACY_CLIENT_HINTS_EXPLANATION",
       Setting::kStandard, Setting::kStrict, false, Status::kPolicyLanded},
      {Feature::kLanguageNormalization, Module::kFingerprint, "language_normalization",
       "IDS_BEDROCK_PRIVACY_LANGUAGE_TITLE", "IDS_BEDROCK_PRIVACY_LANGUAGE_EXPLANATION",
       Setting::kOff, Setting::kStrict, true, Status::kPolicyLanded},
      {Feature::kTimezoneNormalization, Module::kFingerprint, "timezone_normalization",
       "IDS_BEDROCK_PRIVACY_TIMEZONE_TITLE", "IDS_BEDROCK_PRIVACY_TIMEZONE_EXPLANATION",
       Setting::kOff, Setting::kStrict, true, Status::kPolicyLanded},
      {Feature::kScreenMetricNormalization, Module::kFingerprint, "screen_metrics",
       "IDS_BEDROCK_PRIVACY_SCREEN_TITLE", "IDS_BEDROCK_PRIVACY_SCREEN_EXPLANATION",
       Setting::kStandard, Setting::kStrict, true, Status::kPolicyLanded},
      {Feature::kHardwareInfoReduction, Module::kFingerprint, "hardware_info",
       "IDS_BEDROCK_PRIVACY_HARDWARE_TITLE", "IDS_BEDROCK_PRIVACY_HARDWARE_EXPLANATION",
       Setting::kStandard, Setting::kStrict, false, Status::kPolicyLanded},
      {Feature::kBatteryApiProtection, Module::kFingerprint, "battery_api",
       "IDS_BEDROCK_PRIVACY_BATTERY_TITLE", "IDS_BEDROCK_PRIVACY_BATTERY_EXPLANATION",
       Setting::kStandard, Setting::kStrict, false, Status::kPolicyLanded},
      {Feature::kWebrtcPolicy, Module::kFingerprint, "webrtc_policy",
       "IDS_BEDROCK_PRIVACY_WEBRTC_TITLE", "IDS_BEDROCK_PRIVACY_WEBRTC_EXPLANATION",
       Setting::kStandard, Setting::kStrict, true, Status::kPolicyLanded},
      {Feature::kTimerCoarsening, Module::kFingerprint, "timer_coarsening",
       "IDS_BEDROCK_PRIVACY_TIMERS_TITLE", "IDS_BEDROCK_PRIVACY_TIMERS_EXPLANATION",
       Setting::kStandard, Setting::kStrict, true, Status::kPolicyLanded},

      // kPermissions
      {Feature::kMediaDeviceEnumeration, Module::kPermissions, "media_devices",
       "IDS_BEDROCK_PRIVACY_MEDIA_DEVICES_TITLE",
       "IDS_BEDROCK_PRIVACY_MEDIA_DEVICES_EXPLANATION",
       Setting::kStandard, Setting::kStrict, true, Status::kDesigned},
      {Feature::kGamepadAndSensorExposure, Module::kPermissions, "gamepad_and_sensors",
       "IDS_BEDROCK_PRIVACY_SENSORS_TITLE", "IDS_BEDROCK_PRIVACY_SENSORS_EXPLANATION",
       Setting::kStandard, Setting::kStrict, true, Status::kDesigned},
      {Feature::kClipboardPermission, Module::kPermissions, "clipboard_permission",
       "IDS_BEDROCK_PRIVACY_CLIPBOARD_TITLE", "IDS_BEDROCK_PRIVACY_CLIPBOARD_EXPLANATION",
       Setting::kStandard, Setting::kStrict, false, Status::kDesigned},
      {Feature::kGeolocationPermission, Module::kPermissions, "geolocation_permission",
       "IDS_BEDROCK_PRIVACY_GEOLOCATION_TITLE",
       "IDS_BEDROCK_PRIVACY_GEOLOCATION_EXPLANATION",
       Setting::kStandard, Setting::kStrict, false, Status::kDesigned},
      {Feature::kNotificationPermission, Module::kPermissions, "notification_permission",
       "IDS_BEDROCK_PRIVACY_NOTIFICATIONS_TITLE",
       "IDS_BEDROCK_PRIVACY_NOTIFICATIONS_EXPLANATION",
       Setting::kStandard, Setting::kStrict, false, Status::kDesigned},
      {Feature::kAutoplayControl, Module::kPermissions, "autoplay_control",
       "IDS_BEDROCK_PRIVACY_AUTOPLAY_TITLE", "IDS_BEDROCK_PRIVACY_AUTOPLAY_EXPLANATION",
       Setting::kStandard, Setting::kStrict, false, Status::kDesigned},
      {Feature::kPermissionIsolation, Module::kPermissions, "permission_isolation",
       "IDS_BEDROCK_PRIVACY_PERMISSION_ISOLATION_TITLE",
       "IDS_BEDROCK_PRIVACY_PERMISSION_ISOLATION_EXPLANATION",
       Setting::kStandard, Setting::kStrict, false, Status::kDesigned},
  };
  return registry;
}

}  // namespace

const std::vector<FeatureInfo>& GetFeatureRegistry() {
  return Registry();
}

const FeatureInfo* FindFeature(Feature feature) {
  for (const FeatureInfo& info : Registry()) {
    if (info.feature == feature) {
      return &info;
    }
  }
  return nullptr;
}

std::vector<const FeatureInfo*> UiRenderableFeatures() {
  // Item 55: a control exists in the UI only if the browser performs it. A
  // feature whose logic is written but not wired into a build is not a control
  // the user can be shown — it is a promise, and promises do not belong in a
  // settings page.
  std::vector<const FeatureInfo*> renderable;
  for (const FeatureInfo& info : Registry()) {
    if (info.status == Status::kEnforced) {
      renderable.push_back(&info);
    }
  }
  return renderable;
}

Setting DefaultFor(const FeatureInfo& info, Level level) {
  switch (level) {
    case Level::kStandard:
      return info.standard_default;
    case Level::kStrict:
      return info.strict_default;
    case Level::kCustom:
      return info.standard_default;
  }
  return info.standard_default;
}

}  // namespace privacy
}  // namespace bedrock
