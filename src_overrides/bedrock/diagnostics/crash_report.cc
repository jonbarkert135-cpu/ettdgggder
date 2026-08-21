// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/diagnostics/crash_report.h"

#include <algorithm>

#include "bedrock/diagnostics/scrubber.h"

namespace bedrock {
namespace diagnostics {

namespace {

const std::vector<std::string>& AllowedFieldList() {
  static const std::vector<std::string> kAllowed = {
      "build_id",        // which build crashed
      "channel",         // nightly / beta / stable
      "chromium_base",   // the engine version underneath
      "os",              // "Linux 6.8", "Windows 11" — no machine name
      "cpu_arch",        // x86_64, arm64
      "gpu_enabled",     // hardware acceleration on or off
      "module",          // which process type
      "signal",          // what killed it
      "uptime_seconds",  // how long the process had been running
      "tab_count",       // how many tabs, never which
      "locale",          // the UI language, already public in every request
  };
  return kAllowed;
}

const std::vector<std::string>& ForbiddenFieldList() {
  static const std::vector<std::string> kForbidden = {
      "url", "current_url", "page_url", "referrer", "page_title", "tab_titles",
      "cookies", "cookie", "password", "passwords", "form_data", "post_data",
      "session_token", "auth_token", "clipboard", "history", "bookmarks",
      "profile_path", "username", "email", "ip_address", "search_query",
      "extension_ids", "downloads",
  };
  return kForbidden;
}

}  // namespace

CrashDiagnostics::CrashDiagnostics() = default;

const std::vector<std::string>& CrashDiagnostics::AllowedFields() {
  return AllowedFieldList();
}

const std::vector<std::string>& CrashDiagnostics::ForbiddenFields() {
  return ForbiddenFieldList();
}

CrashReport CrashDiagnostics::BuildReport(
    const std::string& id,
    int64_t at,
    const std::string& signal,
    const std::string& module,
    const std::vector<std::string>& raw_frames,
    const std::map<std::string, std::string>& raw_fields) const {
  CrashReport report;
  report.id = id;
  report.at = at;
  report.signal = signal;
  report.module = module;

  for (const std::string& frame : raw_frames) {
    const ScrubResult scrubbed = Scrubber::Scrub(frame);
    report.frames.push_back(scrubbed.text);
    report.redactions += scrubbed.redactions;
  }

  const std::vector<std::string>& allowed = AllowedFieldList();
  for (const auto& entry : raw_fields) {
    const bool permitted =
        std::find(allowed.begin(), allowed.end(), entry.first) != allowed.end();
    if (!permitted) {
      report.dropped_fields.push_back(entry.first);
      continue;
    }
    // A whitelisted key can still be handed a value that carries user data —
    // "os" filled in with a hostname, say. The value is scrubbed too.
    const ScrubResult scrubbed = Scrubber::Scrub(entry.second);
    report.fields[entry.first] = scrubbed.text;
    report.redactions += scrubbed.redactions;
  }
  return report;
}

void CrashDiagnostics::Store(const CrashReport& report) {
  reports_.insert(reports_.begin(), report);
}

bool CrashDiagnostics::MarkViewed(const std::string& id) {
  for (CrashReport& report : reports_) {
    if (report.id == id) {
      report.viewed = true;
      return true;
    }
  }
  return false;
}

bool CrashDiagnostics::Delete(const std::string& id) {
  const auto it = std::find_if(
      reports_.begin(), reports_.end(),
      [&id](const CrashReport& report) { return report.id == id; });
  if (it == reports_.end()) {
    return false;
  }
  reports_.erase(it);
  return true;
}

void CrashDiagnostics::DeleteAll() {
  reports_.clear();
}

int CrashDiagnostics::DeleteOlderThan(int64_t now, int64_t max_age_seconds) {
  const std::size_t before = reports_.size();
  reports_.erase(std::remove_if(reports_.begin(), reports_.end(),
                                [&](const CrashReport& report) {
                                  return now - report.at > max_age_seconds;
                                }),
                 reports_.end());
  return static_cast<int>(before - reports_.size());
}

bool CrashDiagnostics::MayUpload(const CrashReport& report,
                                 bool user_confirmed) const {
  if (consent_ != UploadConsent::kAskEachTime) {
    return false;
  }
  // Consent in the abstract is not consent to this file. The user must have
  // opened the report and then confirmed it.
  return report.viewed && user_confirmed;
}

}  // namespace diagnostics
}  // namespace bedrock
