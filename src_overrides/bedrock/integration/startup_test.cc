// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh. Phase 2.

#include "bedrock/integration/startup.h"

#include <iostream>
#include <set>
#include <string>

#include "bedrock/settings/defaults.h"

namespace {

using bedrock::integration::ComputeStartupPlan;
using bedrock::integration::PrefAssignment;
using bedrock::integration::PrefScope;
using bedrock::integration::PrefType;
using bedrock::integration::StartupLogLines;
using bedrock::integration::StartupPlan;
using bedrock::integration::UnenforcedDefault;
using bedrock::integration::kLogPrefix;
using bedrock::settings::FactoryDefaults;
using bedrock::settings::FactoryProfileName;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

// Every shipped default has to appear exactly once, either as something this
// build performs or as something it admits it does not. A default that falls
// out of both lists is how "NO FAKE FEATURES" quietly stops being true.
void EveryShippedDefaultIsAccountedFor() {
  const StartupPlan plan = ComputeStartupPlan();
  std::set<std::string> seen;
  for (const PrefAssignment& pref : plan.prefs) {
    if (pref.decides_behavior) {
      Check(seen.insert(pref.setting_id).second,
            "enforced setting listed twice: " + pref.setting_id);
    } else {
      seen.insert(pref.setting_id);
    }
  }
  for (const UnenforcedDefault& unenforced : plan.unenforced) {
    Check(seen.insert(unenforced.setting_id).second ||
              !unenforced.blocked_on.empty(),
          "setting listed twice with no reason: " + unenforced.setting_id);
    seen.insert(unenforced.setting_id);
  }
  Check(seen.size() == FactoryDefaults().size(),
        "plan covers every shipped default");
  for (const auto& setting : FactoryDefaults()) {
    Check(seen.count(setting.id) == 1,
          std::string("shipped default is in the plan: ") + setting.id);
  }
}

// The claim this whole phase exists to make true, spelled out so it cannot be
// weakened by accident.
void OneSettingIsEnforcedAndTwoAreOnlyRegistered() {
  using bedrock::integration::DecisiveCount;
  const StartupPlan plan = ComputeStartupPlan();
  Check(plan.prefs.size() == 3, "three prefs are registered at startup");
  Check(DecisiveCount(plan) == 1,
        "but only one of them decides what the browser does");
  std::set<std::string> decisive;
  for (const PrefAssignment& pref : plan.prefs) {
    Check(!pref.reason.empty(), "rationale kept for " + pref.setting_id);
    if (pref.decides_behavior) {
      decisive.insert(pref.setting_id);
    }
  }
  Check(decisive.size() == 1 && decisive.count("webrtc_privacy") == 1,
        "webrtc_privacy is the enforced one");
}

// The trap this exists to keep shut: an assignment that does not decide the
// behaviour must also be listed as unenforced, with the measurement that says
// so. Registering a pref nobody reads is not a protection.
void RegisteredButNotDecisiveIsAlsoUnenforced() {
  const StartupPlan plan = ComputeStartupPlan();
  for (const PrefAssignment& pref : plan.prefs) {
    if (pref.decides_behavior) {
      continue;
    }
    bool listed = false;
    for (const UnenforcedDefault& unenforced : plan.unenforced) {
      if (unenforced.setting_id == pref.setting_id) {
        listed = true;
        Check(unenforced.blocked_on.find("unbranded") != std::string::npos,
              "and the reason names the measurement: " + pref.setting_id);
      }
    }
    Check(listed, pref.setting_id + " is registered but not claimed as enforced");
  }
}

void WebRtcPrivacyWritesTheInterfacePolicy() {
  for (const PrefAssignment& pref : ComputeStartupPlan().prefs) {
    if (pref.setting_id != "webrtc_privacy") {
      continue;
    }
    Check(pref.pref_name == "webrtc.ip_handling_policy",
          "it writes Chromium's webrtc.ip_handling_policy");
    Check(pref.value == "default_public_interface_only",
          "to the value that hides local interfaces");
    Check(pref.value != "default",
          "and not to Chromium's own default, which would be a no-op");
    Check(pref.type == PrefType::kString, "as a string pref");
    Check(pref.scope == PrefScope::kProfile, "in the profile");
  }
}

// Telemetry and crash upload are one consent bit in Chromium, and Bedrock says
// so out loud rather than inventing a second pref. Neither is decisive here.
void ReportingConsentIsOffInLocalState() {
  int seen = 0;
  for (const PrefAssignment& pref : ComputeStartupPlan().prefs) {
    if (pref.setting_id != "telemetry" && pref.setting_id != "crash_reporting") {
      continue;
    }
    ++seen;
    Check(pref.pref_name == "user_experience_metrics.reporting_enabled",
          "it writes Chromium's reporting-consent pref");
    Check(pref.type == PrefType::kBoolean, "as a boolean pref");
    Check(pref.boolean_value == false, "and the value is off");
    Check(pref.value == "false", "with a text form that says so");
    Check(pref.scope == PrefScope::kLocalState, "in Local State, not a profile");
    Check(!pref.decides_behavior,
          "and it is not claimed to decide anything in an unbranded build");
  }
  Check(seen == 2, "both non-negotiable defaults land on that pref");
}

// The registration patch takes one scope at a time, because Chromium registers
// the two scopes in two different functions.
void ScopesSplitTheWayChromiumRegistersThem() {
  using bedrock::integration::AssignmentsForScope;
  const auto profile = AssignmentsForScope(PrefScope::kProfile);
  const auto local_state = AssignmentsForScope(PrefScope::kLocalState);
  Check(profile.size() == 1, "one profile pref");
  Check(local_state.size() == 2, "two Local State assignments");
  Check(profile.size() + local_state.size() == ComputeStartupPlan().prefs.size(),
        "every assignment belongs to exactly one scope");
  for (const PrefAssignment& pref : local_state) {
    Check(pref.scope == PrefScope::kLocalState, "scope filter is honest");
  }
}

// An unenforced default without a reason is a TODO nobody will ever read.
void EveryUnenforcedDefaultSaysWhy() {
  const StartupPlan plan = ComputeStartupPlan();
  Check(!plan.unenforced.empty(),
        "this build does not yet claim to enforce everything");
  for (const UnenforcedDefault& unenforced : plan.unenforced) {
    Check(!unenforced.blocked_on.empty(),
          "reason given for " + unenforced.setting_id);
    Check(!unenforced.documented_value.empty(),
          "documented value kept for " + unenforced.setting_id);
  }
}

// The log lines are the phase-2 evidence: the verification script greps a
// running browser's stderr for them.
void LogLinesAreGreppableAndHonest() {
  const StartupPlan plan = ComputeStartupPlan();
  const auto lines = StartupLogLines(plan);
  Check(lines.size() == plan.prefs.size() + 1,
        "one summary line plus one line per registered pref");
  int registering = 0;
  for (const std::string& line : lines) {
    if (line.find("registering (not decisive in this build)") !=
        std::string::npos) {
      ++registering;
    }
  }
  Check(registering == 2,
        "the two non-decisive assignments say so in the log, not 'enforcing'");
  for (const std::string& line : lines) {
    Check(line.rfind(kLogPrefix, 0) == 0, "line carries the bedrock prefix");
    Check(line.find('\n') == std::string::npos, "line is a single line");
  }
  Check(lines.front().find(FactoryProfileName()) != std::string::npos,
        "summary names the shipped profile");
  Check(lines.front().find(std::to_string(FactoryDefaults().size())) !=
            std::string::npos,
        "summary states how many defaults exist, not just how many are on");
  Check(lines.front().find("1 of ") != std::string::npos,
        "and counts only what the browser acts on");
}

// No state, no I/O: the browser may call this once per profile and must get
// the same answer.
void ThePlanIsPure() {
  const StartupPlan first = ComputeStartupPlan();
  const StartupPlan second = ComputeStartupPlan();
  Check(first.summary == second.summary, "summary is stable");
  Check(first.prefs.size() == second.prefs.size(), "pref count is stable");
}

// The registration patch asks Bedrock for a default and must get Chromium's
// own value back for anything Bedrock does not own.
void PrefDefaultsFallBackToChromium() {
  using bedrock::integration::PrefDefaultOr;
  Check(PrefDefaultOr("webrtc.ip_handling_policy", "default") ==
            "default_public_interface_only",
        "Bedrock overrides the WebRTC pref default");
  Check(PrefDefaultOr("some.other.pref", "chromium-value") == "chromium-value",
        "and leaves every other pref to Chromium");
  // A boolean pref must not be answered by the string accessor, or the patch
  // would write "false" into a boolean pref and Chromium would CHECK.
  Check(PrefDefaultOr("user_experience_metrics.reporting_enabled", "keep") ==
            "keep",
        "the string accessor ignores boolean assignments");
}

void BooleanPrefDefaultsFallBackToChromium() {
  using bedrock::integration::BooleanPrefDefaultOr;
  Check(!BooleanPrefDefaultOr("user_experience_metrics.reporting_enabled", true),
        "Bedrock turns reporting consent off");
  Check(BooleanPrefDefaultOr("some.other.bool", true),
        "and leaves every other boolean to Chromium");
  Check(!BooleanPrefDefaultOr("webrtc.ip_handling_policy", false),
        "the boolean accessor ignores string assignments");
}

// The evidence line has to be able to say MISMATCH, or it proves nothing.
void EffectiveLineReportsMatchAndMismatch() {
  using bedrock::integration::EffectivePrefLine;
  const std::string good = EffectivePrefLine("webrtc.ip_handling_policy",
                                             "default_public_interface_only");
  Check(good.find("match") != std::string::npos, "matching value reads match");
  Check(good.find("MISMATCH") == std::string::npos, "and not MISMATCH");
  const std::string bad =
      EffectivePrefLine("webrtc.ip_handling_policy", "default");
  Check(bad.find("MISMATCH") != std::string::npos,
        "Chromium's own value reads MISMATCH");
}

void EffectiveBooleanLineReportsMatchAndMismatch() {
  using bedrock::integration::EffectiveBooleanPrefLine;
  const std::string good = EffectiveBooleanPrefLine(
      "user_experience_metrics.reporting_enabled", false);
  Check(good.find("= false") != std::string::npos, "it prints the value read");
  Check(good.find("match") != std::string::npos, "off reads match");
  Check(good.find("MISMATCH") == std::string::npos, "and not MISMATCH");
  const std::string bad = EffectiveBooleanPrefLine(
      "user_experience_metrics.reporting_enabled", true);
  Check(bad.find("MISMATCH") != std::string::npos,
        "consent given by a live browser reads MISMATCH");
  Check(bad.rfind(kLogPrefix, 0) == 0, "and carries the bedrock prefix");
}

}  // namespace

int main() {
  PrefDefaultsFallBackToChromium();
  BooleanPrefDefaultsFallBackToChromium();
  EffectiveLineReportsMatchAndMismatch();
  EffectiveBooleanLineReportsMatchAndMismatch();
  EveryShippedDefaultIsAccountedFor();
  OneSettingIsEnforcedAndTwoAreOnlyRegistered();
  RegisteredButNotDecisiveIsAlsoUnenforced();
  WebRtcPrivacyWritesTheInterfacePolicy();
  ReportingConsentIsOffInLocalState();
  ScopesSplitTheWayChromiumRegistersThem();
  EveryUnenforcedDefaultSaysWhy();
  LogLinesAreGreppableAndHonest();
  ThePlanIsPure();
  if (failures == 0) {
    const StartupPlan plan = ComputeStartupPlan();
    std::cout << "startup: ok (" << bedrock::integration::DecisiveCount(plan)
              << " enforced, " << plan.prefs.size() << " prefs registered, "
              << plan.unenforced.size() << " defaults not yet enforced)\n";
  }
  return failures == 0 ? 0 : 1;
}
