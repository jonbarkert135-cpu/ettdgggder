// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_ERRORS_ERROR_CATALOG_H_
#define BEDROCK_ERRORS_ERROR_CATALOG_H_

#include <string>
#include <vector>

#include "bedrock/ui/l10n/string_catalog.h"

// Error presentation (roadmap item 80).
//
// Four properties were asked for, and each one rules out a habit browsers have:
//
//   * **meaningful** — the message says what failed, in the user's terms. Not
//     "ERR_SSL_PROTOCOL_ERROR", which tells a user nothing and a developer
//     almost nothing.
//   * **actionable** — every error carries a next step. If there is genuinely
//     nothing the user can do, the step says so and offers the one thing that
//     is available (go back, retry later, copy the code for a bug report).
//     An error with no exit is a dead end the user has to guess their way out of.
//   * **localized** — the text lives in the string catalog under an id, like
//     every other string (item 61). Error paths are exactly where developers
//     hardcode English, which is why the gate checks these ids in all locales.
//   * **security-conscious** — the internal detail (a path, a certificate
//     chain, an exception string, a URL) is *not* shown by default. It goes to
//     the debug log, scrubbed. What the user sees is the code, the sentence and
//     the step; what they can copy for a report is the code.
//
// The split is the point of this class. A single `ShowError(message)` API is
// how internal detail reaches screenshots on support forums. Here the caller
// cannot mix them up: `Present()` returns one struct with `user_text` and a
// separate `log_detail`, and the surfaces render only the first.

namespace bedrock {
namespace errors {

enum class ErrorCode {
  kNetworkUnreachable,
  kCertificateInvalid,
  kProfileLocked,
  kDownloadRefused,
  kExtensionBlocked,
  kConfigInvalid,
  kMaxValue = kConfigInvalid,
};

// Whether the internal detail may ever be put in front of the user.
enum class Sensitivity {
  // Detail is safe to show on request behind a "Details" disclosure — it
  // describes the user's own input, e.g. which line of their config file.
  kShowOnRequest,
  // Detail never reaches the UI: it can carry a URL, a path or a chain.
  kDiagnosticOnly,
};

struct ErrorEntry {
  ErrorCode code;
  const char* code_string;  // "BR-NET-001" — stable, quotable in a bug report
  ui::l10n::MessageId title;
  ui::l10n::MessageId action;
  Sensitivity sensitivity;
};

struct Presentation {
  std::string code_string;
  std::string title;        // localized
  std::string action;       // localized, always non-empty
  std::string detail;       // shown only when sensitivity allows; else empty
  std::string log_detail;   // scrubbed, for the local debug log
};

class ErrorCatalog {
 public:
  ErrorCatalog();

  const std::vector<ErrorEntry>& Entries() const;
  const ErrorEntry* Find(ErrorCode code) const;

  // Builds what the UI renders. `internal_detail` is whatever the failing
  // subsystem knows — it is scrubbed for the log and withheld from the user
  // unless the entry is kShowOnRequest.
  Presentation Present(ErrorCode code,
                       ui::l10n::Locale locale,
                       const std::string& internal_detail) const;

 private:
  ui::l10n::StringCatalog strings_;
};

}  // namespace errors
}  // namespace bedrock

#endif  // BEDROCK_ERRORS_ERROR_CATALOG_H_
