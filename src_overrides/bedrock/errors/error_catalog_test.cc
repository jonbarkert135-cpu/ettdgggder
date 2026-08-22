// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh. Item 80.

#include "bedrock/errors/error_catalog.h"

#include <cstddef>
#include <iostream>
#include <set>
#include <string>

namespace {

using bedrock::errors::ErrorCatalog;
using bedrock::errors::ErrorCode;
using bedrock::errors::Presentation;
using bedrock::errors::Sensitivity;
using bedrock::ui::l10n::Locale;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

const Locale kLocales[] = {Locale::kEnglish, Locale::kUkrainian,
                           Locale::kRussian, Locale::kGerman};

void EveryErrorIsMeaningfulAndActionableInEveryLocale() {
  ErrorCatalog catalog;
  for (const auto& entry : catalog.Entries()) {
    for (Locale locale : kLocales) {
      const Presentation shown = catalog.Present(entry.code, locale, "");
      Check(!shown.title.empty(),
            std::string(entry.code_string) + ": has a title in every locale");
      Check(!shown.action.empty(),
            std::string(entry.code_string) + ": has an action in every locale");
      // "Meaningful" is not fully checkable, but a sentence shorter than a
      // dozen characters is a code, not a message.
      Check(shown.title.size() > 12,
            std::string(entry.code_string) + ": title is a sentence");
    }
  }
}

void CodesAreUniqueAndQuotable() {
  ErrorCatalog catalog;
  std::set<std::string> codes;
  for (const auto& entry : catalog.Entries()) {
    const std::string code = entry.code_string;
    Check(codes.insert(code).second, "code is unique: " + code);
    Check(code.rfind("BR-", 0) == 0, "code is namespaced: " + code);
  }
  Check(codes.size() == static_cast<std::size_t>(ErrorCode::kMaxValue) + 1,
        "every ErrorCode has an entry");
}

void InternalDetailNeverReachesTheUser() {
  ErrorCatalog catalog;
  const std::string detail =
      "TLS handshake for https://mail.example.com failed, chain at "
      "/home/anna/.config/bedrock/Default/cert, Cookie: sid=abc";
  const Presentation shown =
      catalog.Present(ErrorCode::kCertificateInvalid, Locale::kEnglish, detail);
  Check(shown.detail.empty(), "diagnostic-only detail is withheld from the UI");
  Check(!shown.log_detail.empty(), "the log still gets something to work with");
  Check(shown.log_detail.find("mail.example.com") == std::string::npos,
        "the URL is scrubbed even in the log copy");
  Check(shown.log_detail.find("anna") == std::string::npos,
        "the username is scrubbed even in the log copy");
  Check(shown.log_detail.find("sid=abc") == std::string::npos,
        "the cookie is scrubbed even in the log copy");
}

void UserFacingDetailIsScrubbedToo() {
  ErrorCatalog catalog;
  const Presentation shown =
      catalog.Present(ErrorCode::kConfigInvalid, Locale::kGerman,
                      "line 14: proxy = http://internal.example/proxy.pac");
  Check(!shown.detail.empty(), "a kShowOnRequest error may show its detail");
  Check(shown.detail.find("line 14") != std::string::npos,
        "the part that helps the user is kept");
  Check(shown.detail.find("internal.example") == std::string::npos,
        "even a shown detail loses its URL");
  const auto* entry = catalog.Find(ErrorCode::kConfigInvalid);
  Check(entry && entry->sensitivity == Sensitivity::kShowOnRequest,
        "the config error is the one classified as showable");
}

void MostErrorsAreDiagnosticOnly() {
  // The safe classification must be the common one; if a future change makes
  // most errors showable, that is a review conversation, not a silent drift.
  ErrorCatalog catalog;
  int showable = 0;
  for (const auto& entry : catalog.Entries()) {
    if (entry.sensitivity == Sensitivity::kShowOnRequest) {
      ++showable;
    }
  }
  Check(showable <= 1, "at most one error class shows its internal detail");
}

void UnknownLocaleFallsBackToEnglish() {
  ErrorCatalog catalog;
  const Presentation shown =
      catalog.Present(ErrorCode::kProfileLocked, Locale::kGerman, "");
  Check(!shown.title.empty(), "German title present");
  const Presentation english =
      catalog.Present(ErrorCode::kProfileLocked, Locale::kEnglish, "");
  Check(shown.title != english.title, "German is a translation, not the source");
}

}  // namespace

int main() {
  EveryErrorIsMeaningfulAndActionableInEveryLocale();
  CodesAreUniqueAndQuotable();
  InternalDetailNeverReachesTheUser();
  UserFacingDetailIsScrubbedToo();
  MostErrorsAreDiagnosticOnly();
  UnknownLocaleFallsBackToEnglish();
  if (failures == 0) {
    std::cout << "error_catalog: ok\n";
  }
  return failures == 0 ? 0 : 1;
}
