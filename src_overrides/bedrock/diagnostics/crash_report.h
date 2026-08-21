// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_DIAGNOSTICS_CRASH_REPORT_H_
#define BEDROCK_DIAGNOSTICS_CRASH_REPORT_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Local crash diagnostics (roadmap item 81).
//
// A crash report is the most sensitive file a browser ever writes. It is a
// snapshot of process memory taken at the moment the browser was doing
// something — and what it was doing was rendering a page the user opened, with
// their cookies, their session token and possibly the password manager's
// working state in nearby memory. Every browser that uploads crashes by default
// is uploading that, and every one of them has had at least one incident where
// something personal ended up in a crash bucket.
//
// Bedrock's position:
//
//   * crashes are diagnosed **locally**. A report is written into the profile,
//     listed in a UI the user can read, and deleted by the user or by age.
//   * `UploadConsent` defaults to `kNever`, and there is no build, channel or
//     policy that changes that default. Enterprise policy may lock it *more*
//     tightly, never looser (see docs/CONFIGURATION.md).
//   * even with consent, upload is per report and requires the user to have
//     seen the report contents: `MayUpload()` returns false unless the report
//     was viewed and confirmed. Consent to "send crash reports" in general is
//     not consent to send *this* file, whose contents nobody has read.
//
// The content rule is a whitelist, not a blacklist. `BuildReport()` accepts a
// map of candidate fields and keeps only the keys in `AllowedFields()`;
// everything else is dropped and counted, so a future subsystem that starts
// attaching "current_url" gets its field dropped and the test that counts
// dropped fields notices. Stack frames go through the shared scrubber, because
// a frame can carry a path, and a path carries a username.
//
// Never present, under any consent setting: page URLs, page titles, form data,
// cookies, credentials, and the profile's file contents. That list is asserted
// by the test and by scripts/check_diagnostics.py rather than promised in prose.

namespace bedrock {
namespace diagnostics {

enum class UploadConsent {
  kNever,        // default, and what a fresh profile has
  kAskEachTime,  // the most permissive setting Bedrock offers
};

struct CrashReport {
  std::string id;              // random, not derived from anything about the user
  int64_t at = 0;              // unix seconds
  std::string signal;          // "SIGSEGV", "EXCEPTION_ACCESS_VIOLATION"
  std::string module;          // "renderer", "browser", "gpu", "utility"
  std::vector<std::string> frames;  // scrubbed symbol lines
  std::map<std::string, std::string> fields;  // whitelisted metadata only
  std::vector<std::string> dropped_fields;    // keys refused, for the tests
  int redactions = 0;          // how much the scrubber removed from frames
  bool viewed = false;         // the user opened it in the crash UI
};

class CrashDiagnostics {
 public:
  CrashDiagnostics();

  // Metadata keys a report may carry. Everything here describes the *build and
  // the machine class*, never the session or its content.
  static const std::vector<std::string>& AllowedFields();

  // Keys that must never appear, whatever a caller passes. Kept explicitly so
  // the test can assert each one is refused instead of trusting the whitelist
  // to be complete for the reasons we think it is.
  static const std::vector<std::string>& ForbiddenFields();

  UploadConsent consent() const { return consent_; }
  void SetConsent(UploadConsent consent) { consent_ = consent; }

  // Builds a report from raw crash data. Frames are scrubbed; fields outside
  // the whitelist are dropped and recorded in `dropped_fields`.
  CrashReport BuildReport(const std::string& id,
                          int64_t at,
                          const std::string& signal,
                          const std::string& module,
                          const std::vector<std::string>& raw_frames,
                          const std::map<std::string, std::string>& raw_fields) const;

  // Local store, in the profile. Ordered newest first for the UI.
  void Store(const CrashReport& report);
  const std::vector<CrashReport>& Reports() const { return reports_; }
  bool MarkViewed(const std::string& id);
  bool Delete(const std::string& id);
  void DeleteAll();
  // Reports older than `max_age_seconds` are removed on startup; a crash from
  // last year helps nobody and is still a memory snapshot sitting on disk.
  int DeleteOlderThan(int64_t now, int64_t max_age_seconds);

  // The only place that may answer "yes" to sending anything, and it answers no
  // by default. `user_confirmed` is the click on *this* report, not a setting.
  bool MayUpload(const CrashReport& report, bool user_confirmed) const;

  static constexpr int64_t kDefaultMaxAgeSeconds = 30 * 24 * 60 * 60;  // 30 days

 private:
  UploadConsent consent_ = UploadConsent::kNever;
  std::vector<CrashReport> reports_;
};

}  // namespace diagnostics
}  // namespace bedrock

#endif  // BEDROCK_DIAGNOSTICS_CRASH_REPORT_H_
