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
    Check(seen.insert(pref.setting_id).second,
          "setting listed twice: " + pref.setting_id);
  }
  for (const UnenforcedDefault& unenforced : plan.unenforced) {
    Check(seen.insert(unenforced.setting_id).second,
          "setting listed twice: " + unenforced.setting_id);
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
void WebRtcPrivacyIsTheOneEnforcedSetting() {
  const StartupPlan plan = ComputeStartupPlan();
  Check(plan.prefs.size() == 1, "exactly one pref is applied at startup");
  if (plan.prefs.empty()) {
    return;
  }
  const PrefAssignment& pref = plan.prefs.front();
  Check(pref.setting_id == "webrtc_privacy", "it is webrtc_privacy");
  Check(pref.pref_name == "webrtc.ip_handling_policy",
        "it writes Chromium's webrtc.ip_handling_policy");
  Check(pref.value == "default_public_interface_only",
        "to the value that hides local interfaces");
  Check(pref.value != "default",
        "and not to Chromium's own default, which would be a no-op");
  Check(!pref.reason.empty(), "with the rationale from the defaults table");
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
        "one summary line plus one line per applied pref");
  for (const std::string& line : lines) {
    Check(line.rfind(kLogPrefix, 0) == 0, "line carries the bedrock prefix");
    Check(line.find('\n') == std::string::npos, "line is a single line");
  }
  Check(lines.front().find(FactoryProfileName()) != std::string::npos,
        "summary names the shipped profile");
  Check(lines.front().find(std::to_string(FactoryDefaults().size())) !=
            std::string::npos,
        "summary states how many defaults exist, not just how many are on");
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

}  // namespace

int main() {
  PrefDefaultsFallBackToChromium();
  EffectiveLineReportsMatchAndMismatch();
  EveryShippedDefaultIsAccountedFor();
  WebRtcPrivacyIsTheOneEnforcedSetting();
  EveryUnenforcedDefaultSaysWhy();
  LogLinesAreGreppableAndHonest();
  ThePlanIsPure();
  if (failures == 0) {
    const StartupPlan plan = ComputeStartupPlan();
    std::cout << "startup: ok (" << plan.prefs.size() << " enforced, "
              << plan.unenforced.size() << " not yet)\n";
  }
  return failures == 0 ? 0 : 1;
}
