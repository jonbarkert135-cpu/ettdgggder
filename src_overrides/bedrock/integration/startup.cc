// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/integration/startup.h"

#include <string>
#include <vector>

#include "bedrock/settings/defaults.h"

namespace bedrock {
namespace integration {

const char kLogPrefix[] = "[bedrock] ";

namespace {

// The only mapping that exists today: one shipped default, one Chromium pref,
// one observable effect.
//
// `webrtc.ip_handling_policy` is a profile string pref registered in
// chrome/browser/ui/browser_ui_prefs.cc with Chromium's default "default", and
// read back in chrome/browser/renderer_preferences_util.cc, which converts it
// into blink::mojom::WebRtcIpHandlingPolicy for every renderer. Setting it to
// "default_public_interface_only" (blink::kWebRTCIPHandlingDefaultPublicInterfaceOnly)
// stops WebRTC from enumerating local interfaces, which is exactly what the
// shipped `webrtc_privacy` default promises.
//
// The strings are duplicated here rather than included from Chromium on
// purpose: this file has to compile with plain g++ and no checkout. The patch
// that calls us verifies the pref exists before writing it, so a rename
// upstream shows up as a log line and a failing phase-2 check, not as a silent
// no-op or a crash.
constexpr char kWebRtcPrefName[] = "webrtc.ip_handling_policy";
constexpr char kWebRtcPublicInterfaceOnly[] = "default_public_interface_only";

// Why each remaining shipped default is not yet something a running browser
// does. Anything not listed gets a generic answer; the point is that the list
// is written down, not that every sentence is unique.
struct BlockedOn {
  const char* setting_id;
  const char* reason;
};

const BlockedOn kBlockedOn[] = {
    {"secure_browsing",
     "Chromium already provides site isolation and sandboxing; Bedrock adds "
     "nothing here until its own security baseline (item 24) has call sites."},
    {"https_upgrades",
     "privacy/network/https_policy decides the upgrade, but nothing routes "
     "Chromium's navigation through it yet."},
    {"third_party_tracking_protection",
     "needs the content-blocking pipeline attached to the network service, "
     "phase 7."},
    {"third_party_cookies",
     "needs the storage-partition work of phase 9 before a restriction can be "
     "expressed as a Chromium content setting."},
    {"fingerprint_protection",
     "requires Blink-side hooks (canvas, WebGL, navigator), phase 8."},
    {"ad_and_tracker_blocking",
     "the filter engine exists and is fast, but the URLLoaderFactory hook is "
     "phase 7."},
    {"telemetry",
     "nothing to enforce: Bedrock ships no reporting code. Chromium's own "
     "metrics are disabled by build configuration, not by this plan."},
    {"crash_reporting",
     "Crashpad is not wired up in this build at all; diagnostics/ stays local "
     "by construction."},
    {"secure_dns",
     "documented as configurable, so there is no single value to set at "
     "startup."},
    {"extension_permissions",
     "the extensions layer is not attached to Chromium's extension system "
     "yet, phase 12."},
    {"site_permissions",
     "Chromium's own default already asks; Bedrock has no override to apply."},
};

const char* BlockedOnFor(const std::string& setting_id) {
  for (const BlockedOn& entry : kBlockedOn) {
    if (setting_id == entry.setting_id) {
      return entry.reason;
    }
  }
  return "no Chromium call site yet";
}

}  // namespace

StartupPlan ComputeStartupPlan() {
  StartupPlan plan;
  plan.profile_name = settings::FactoryProfileName();

  for (const settings::DefaultSetting& setting : settings::FactoryDefaults()) {
    const std::string id = setting.id;
    if (id == "webrtc_privacy" && std::string(setting.value) == "enabled") {
      PrefAssignment assignment;
      assignment.pref_name = kWebRtcPrefName;
      assignment.value = kWebRtcPublicInterfaceOnly;
      assignment.setting_id = id;
      assignment.reason = setting.rationale;
      plan.prefs.push_back(assignment);
      continue;
    }
    UnenforcedDefault unenforced;
    unenforced.setting_id = id;
    unenforced.documented_value = setting.value;
    unenforced.blocked_on = BlockedOnFor(id);
    plan.unenforced.push_back(unenforced);
  }

  plan.summary = plan.profile_name + ": " + std::to_string(plan.prefs.size()) +
                 " of " +
                 std::to_string(plan.prefs.size() + plan.unenforced.size()) +
                 " shipped defaults enforced by this build";
  return plan;
}

std::vector<std::string> StartupLogLines(const StartupPlan& plan) {
  std::vector<std::string> lines;
  lines.push_back(std::string(kLogPrefix) + plan.summary);
  for (const PrefAssignment& pref : plan.prefs) {
    lines.push_back(std::string(kLogPrefix) + "enforcing " + pref.setting_id +
                    ": " + pref.pref_name + " = " + pref.value);
  }
  return lines;
}

std::string PrefDefaultOr(const std::string& pref_name,
                          const std::string& chromium_default) {
  for (const PrefAssignment& pref : ComputeStartupPlan().prefs) {
    if (pref.pref_name == pref_name) {
      return pref.value;
    }
  }
  return chromium_default;
}

std::string EffectivePrefLine(const std::string& pref_name,
                              const std::string& observed) {
  const std::string wanted = PrefDefaultOr(pref_name, observed);
  return std::string(kLogPrefix) + "effective " + pref_name + " = " + observed +
         " (want " + wanted + ": " + (observed == wanted ? "match" : "MISMATCH") +
         ")";
}

}  // namespace integration
}  // namespace bedrock
