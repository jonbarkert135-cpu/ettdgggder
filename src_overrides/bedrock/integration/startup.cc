// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/integration/startup.h"

#include <cstddef>
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

// The reporting-consent pref, and why setting it is not the same as enforcing
// anything in this build.
//
// `user_experience_metrics.reporting_enabled`
// (components/metrics/metrics_pref_names.h) is the single consent bit Chromium
// checks for *both* metrics upload and crash upload:
// ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled reads it and
// the EnabledStateProvider that gates the metrics and Crashpad services asks
// exactly that question. So the shipped `telemetry` and `crash_reporting`
// defaults land on one pref, and Bedrock registers its default from the
// defaults table rather than inheriting whatever a build configuration happens
// to produce.
//
// It is *not* what decides the behaviour here. Build 4 (2026-08-27) measured it
// directly: with Bedrock asking for `true`, a running browser still reported
// `false` — `MetricsServiceAccessor::IsMetricsReportingEnabled` returns false
// unconditionally unless `GOOGLE_CHROME_BRANDING` is set, so in an unbranded
// build nothing is uploaded no matter what the pref says. Bedrock therefore
// keeps both defaults in the *unenforced* list with that as the reason: the
// browser is quiet because it is unbranded, not because of us, and claiming
// otherwise is exactly the fake feature item 55 forbids. The assignment stays,
// because it is the value a branded build would then act on, and the
// effective-value line stays, because it is what caught this.
constexpr char kMetricsReportingPrefName[] =
    "user_experience_metrics.reporting_enabled";

constexpr char kReportingBlockedOn[] =
    "Bedrock registers user_experience_metrics.reporting_enabled = false, but "
    "an unbranded build disables reporting in code regardless of the pref "
    "(MetricsServiceAccessor::IsMetricsReportingEnabled), so this build's "
    "silence is not Bedrock's doing — measured by build 4, 2026-08-27.";

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
      assignment.scope = PrefScope::kProfile;
      assignment.type = PrefType::kString;
      plan.prefs.push_back(assignment);
      continue;
    }
    if ((id == "telemetry" || id == "crash_reporting") &&
        std::string(setting.value) == "disabled") {
      // Registered, and honest about not being the thing that decides: see the
      // comment on kMetricsReportingPrefName. Build 4 measured it.
      PrefAssignment assignment;
      assignment.pref_name = kMetricsReportingPrefName;
      assignment.value = "false";
      assignment.setting_id = id;
      assignment.reason = setting.rationale;
      assignment.scope = PrefScope::kLocalState;
      assignment.type = PrefType::kBoolean;
      assignment.boolean_value = false;
      assignment.decides_behavior = false;
      plan.prefs.push_back(assignment);
      UnenforcedDefault unenforced;
      unenforced.setting_id = id;
      unenforced.documented_value = setting.value;
      unenforced.blocked_on = kReportingBlockedOn;
      plan.unenforced.push_back(unenforced);
      continue;
    }
    UnenforcedDefault unenforced;
    unenforced.setting_id = id;
    unenforced.documented_value = setting.value;
    unenforced.blocked_on = BlockedOnFor(id);
    plan.unenforced.push_back(unenforced);
  }

  plan.summary = plan.profile_name + ": " +
                 std::to_string(DecisiveCount(plan)) + " of " +
                 std::to_string(settings::FactoryDefaults().size()) +
                 " shipped defaults enforced by this build";
  return plan;
}

size_t DecisiveCount(const StartupPlan& plan) {
  size_t count = 0;
  for (const PrefAssignment& pref : plan.prefs) {
    if (pref.decides_behavior) {
      ++count;
    }
  }
  return count;
}

std::vector<std::string> StartupLogLines(const StartupPlan& plan) {
  std::vector<std::string> lines;
  lines.push_back(std::string(kLogPrefix) + plan.summary);
  for (const PrefAssignment& pref : plan.prefs) {
    lines.push_back(std::string(kLogPrefix) +
                    (pref.decides_behavior
                         ? "enforcing "
                         : "registering (not decisive in this build) ") +
                    pref.setting_id + ": " + pref.pref_name + " = " +
                    pref.value);
  }
  return lines;
}

std::string PrefDefaultOr(const std::string& pref_name,
                          const std::string& chromium_default) {
  for (const PrefAssignment& pref : ComputeStartupPlan().prefs) {
    if (pref.pref_name == pref_name && pref.type == PrefType::kString) {
      return pref.value;
    }
  }
  return chromium_default;
}

bool BooleanPrefDefaultOr(const std::string& pref_name, bool chromium_default) {
  for (const PrefAssignment& pref : ComputeStartupPlan().prefs) {
    if (pref.pref_name == pref_name && pref.type == PrefType::kBoolean) {
      return pref.boolean_value;
    }
  }
  return chromium_default;
}

std::vector<PrefAssignment> AssignmentsForScope(PrefScope scope) {
  std::vector<PrefAssignment> out;
  for (const PrefAssignment& pref : ComputeStartupPlan().prefs) {
    if (pref.scope == scope) {
      out.push_back(pref);
    }
  }
  return out;
}

std::string EffectivePrefLine(const std::string& pref_name,
                              const std::string& observed) {
  const std::string wanted = PrefDefaultOr(pref_name, observed);
  return std::string(kLogPrefix) + "effective " + pref_name + " = " + observed +
         " (want " + wanted + ": " + (observed == wanted ? "match" : "MISMATCH") +
         ")";
}

std::string EffectiveBooleanPrefLine(const std::string& pref_name,
                                     bool observed) {
  const bool wanted = BooleanPrefDefaultOr(pref_name, observed);
  const std::string observed_text = observed ? "true" : "false";
  const std::string wanted_text = wanted ? "true" : "false";
  return std::string(kLogPrefix) + "effective " + pref_name + " = " +
         observed_text + " (want " + wanted_text + ": " +
         (observed == wanted ? "match" : "MISMATCH") + ")";
}

}  // namespace integration
}  // namespace bedrock
