// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh. Item 81.

#include "bedrock/diagnostics/crash_report.h"

#include <algorithm>
#include <iostream>
#include <string>

namespace {

using bedrock::diagnostics::CrashDiagnostics;
using bedrock::diagnostics::CrashReport;
using bedrock::diagnostics::UploadConsent;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

CrashReport Build(const CrashDiagnostics& crash,
                  const std::map<std::string, std::string>& fields,
                  const std::vector<std::string>& frames = {}) {
  return crash.BuildReport("f3a9", 1787000000, "SIGSEGV", "renderer", frames,
                           fields);
}

void UploadIsOffByDefault() {
  CrashDiagnostics crash;
  Check(crash.consent() == UploadConsent::kNever, "consent defaults to never");
  CrashReport report = Build(crash, {{"channel", "stable"}});
  report.viewed = true;
  Check(!crash.MayUpload(report, true),
        "no upload while consent is never, even if the user clicks send");
}

void ConsentIsPerReportAndNeedsTheReportSeen() {
  CrashDiagnostics crash;
  crash.SetConsent(UploadConsent::kAskEachTime);
  CrashReport report = Build(crash, {{"channel", "beta"}});
  Check(!crash.MayUpload(report, true), "unseen report cannot be sent");
  report.viewed = true;
  Check(!crash.MayUpload(report, false), "seen but unconfirmed cannot be sent");
  Check(crash.MayUpload(report, true), "seen and confirmed may be sent");
}

void ForbiddenFieldsAreDroppedOneByOne() {
  CrashDiagnostics crash;
  for (const std::string& key : CrashDiagnostics::ForbiddenFields()) {
    const CrashReport report = Build(crash, {{key, "whatever"}});
    Check(report.fields.find(key) == report.fields.end(),
          "field refused: " + key);
    Check(std::find(report.dropped_fields.begin(), report.dropped_fields.end(),
                    key) != report.dropped_fields.end(),
          "drop recorded: " + key);
  }
}

void UnknownFieldsAreDroppedToo() {
  // The whitelist is the rule; the forbidden list only documents the obvious
  // cases. A field nobody thought about must also be dropped.
  CrashDiagnostics crash;
  const CrashReport report =
      Build(crash, {{"last_typed_text", "my bank pin"}, {"channel", "nightly"}});
  Check(report.fields.size() == 1 && report.fields.count("channel") == 1,
        "only whitelisted keys survive");
  Check(report.dropped_fields.size() == 1, "the unknown key was dropped");
}

void WhitelistedValuesAreStillScrubbed() {
  CrashDiagnostics crash;
  const CrashReport report = Build(crash, {{"os", "Linux on /home/anna/box"}});
  Check(report.fields.at("os").find("anna") == std::string::npos,
        "a whitelisted key cannot smuggle a username through its value");
  Check(report.redactions > 0, "redaction counted for the field value");
}

void FramesKeepCodeAndLoseUserData() {
  CrashDiagnostics crash;
  const CrashReport report = Build(
      crash, {{"module", "renderer"}},
      {"#0 bedrock::net::HttpsPolicy::Upgrade(https_policy.cc:88)",
       "#1 loading https://accounts.example.com/login?session=xyz",
       "#2 /home/anna/.config/bedrock/Default/Cookies"});
  Check(report.frames.size() == 3, "all frames kept");
  Check(report.frames[0].find("https_policy.cc:88") != std::string::npos,
        "source location survives — otherwise the report is useless");
  Check(report.frames[1].find("accounts.example.com") == std::string::npos,
        "a URL in a frame is removed");
  Check(report.frames[2].find("anna") == std::string::npos,
        "a profile path in a frame loses the username");
}

void StoreIsLocalAndUserControlled() {
  CrashDiagnostics crash;
  CrashReport first = Build(crash, {{"channel", "stable"}});
  first.id = "one";
  first.at = 1000;
  CrashReport second = Build(crash, {{"channel", "stable"}});
  second.id = "two";
  second.at = 2000;
  crash.Store(first);
  crash.Store(second);
  Check(crash.Reports().size() == 2, "both reports stored");
  Check(crash.Reports().front().id == "two", "newest first");
  Check(crash.MarkViewed("one") && crash.Reports().back().viewed,
        "a report can be marked viewed");
  Check(crash.Delete("one") && crash.Reports().size() == 1,
        "the user can delete one report");
  Check(!crash.Delete("missing"), "deleting an unknown id fails cleanly");
  crash.DeleteAll();
  Check(crash.Reports().empty(), "the user can delete all reports");
}

void OldReportsExpire() {
  CrashDiagnostics crash;
  CrashReport old_report = Build(crash, {{"channel", "stable"}});
  old_report.id = "old";
  old_report.at = 1000;
  CrashReport fresh = Build(crash, {{"channel", "stable"}});
  fresh.id = "fresh";
  fresh.at = 1000 + CrashDiagnostics::kDefaultMaxAgeSeconds;
  crash.Store(old_report);
  crash.Store(fresh);
  const int removed = crash.DeleteOlderThan(
      1000 + CrashDiagnostics::kDefaultMaxAgeSeconds + 1,
      CrashDiagnostics::kDefaultMaxAgeSeconds);
  Check(removed == 1, "one report expired");
  Check(crash.Reports().size() == 1 && crash.Reports().front().id == "fresh",
        "the recent report stayed");
}

}  // namespace

int main() {
  UploadIsOffByDefault();
  ConsentIsPerReportAndNeedsTheReportSeen();
  ForbiddenFieldsAreDroppedOneByOne();
  UnknownFieldsAreDroppedToo();
  WhitelistedValuesAreStillScrubbed();
  FramesKeepCodeAndLoseUserData();
  StoreIsLocalAndUserControlled();
  OldReportsExpire();
  if (failures == 0) {
    std::cout << "crash_report: ok\n";
  }
  return failures == 0 ? 0 : 1;
}
