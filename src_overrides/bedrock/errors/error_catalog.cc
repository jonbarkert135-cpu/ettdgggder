// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/errors/error_catalog.h"

#include "bedrock/diagnostics/scrubber.h"

namespace bedrock {
namespace errors {

namespace {

using ui::l10n::MessageId;

const ErrorEntry kEntries[] = {
    {ErrorCode::kNetworkUnreachable, "BR-NET-001",
     MessageId::kErrorNetworkUnreachable, MessageId::kErrorNetworkUnreachableAction,
     Sensitivity::kDiagnosticOnly},
    {ErrorCode::kCertificateInvalid, "BR-SEC-002",
     MessageId::kErrorCertificateInvalid, MessageId::kErrorCertificateInvalidAction,
     Sensitivity::kDiagnosticOnly},
    {ErrorCode::kProfileLocked, "BR-PRF-003",
     MessageId::kErrorProfileLocked, MessageId::kErrorProfileLockedAction,
     Sensitivity::kDiagnosticOnly},
    {ErrorCode::kDownloadRefused, "BR-DL-004",
     MessageId::kErrorDownloadRefused, MessageId::kErrorDownloadRefusedAction,
     Sensitivity::kDiagnosticOnly},
    {ErrorCode::kExtensionBlocked, "BR-EXT-005",
     MessageId::kErrorExtensionBlocked, MessageId::kErrorExtensionBlockedAction,
     Sensitivity::kDiagnosticOnly},
    // The only detail a user may see: their own configuration file, where the
    // line number is the whole point of the message.
    {ErrorCode::kConfigInvalid, "BR-CFG-006",
     MessageId::kErrorConfigInvalid, MessageId::kErrorConfigInvalidAction,
     Sensitivity::kShowOnRequest},
};

}  // namespace

ErrorCatalog::ErrorCatalog() = default;

const std::vector<ErrorEntry>& ErrorCatalog::Entries() const {
  static const std::vector<ErrorEntry> kAll(std::begin(kEntries),
                                            std::end(kEntries));
  return kAll;
}

const ErrorEntry* ErrorCatalog::Find(ErrorCode code) const {
  for (const ErrorEntry& entry : Entries()) {
    if (entry.code == code) {
      return &entry;
    }
  }
  return nullptr;
}

Presentation ErrorCatalog::Present(ErrorCode code,
                                   ui::l10n::Locale locale,
                                   const std::string& internal_detail) const {
  Presentation out;
  const ErrorEntry* entry = Find(code);
  if (!entry) {
    return out;
  }
  out.code_string = entry->code_string;

  ui::l10n::Message message;
  for (ui::l10n::Locale candidate : strings_.FallbackChain(locale)) {
    if (strings_.Lookup(candidate, entry->title, &message)) {
      out.title = message.text;
      break;
    }
  }
  for (ui::l10n::Locale candidate : strings_.FallbackChain(locale)) {
    if (strings_.Lookup(candidate, entry->action, &message)) {
      out.action = message.text;
      break;
    }
  }

  // Scrubbed once; both the log copy and any user-visible copy come from the
  // scrubbed text, so a mistake in the sensitivity classification still cannot
  // put a URL or a credential on screen.
  const diagnostics::ScrubResult scrubbed =
      diagnostics::Scrubber::Scrub(internal_detail);
  out.log_detail = scrubbed.text;
  if (entry->sensitivity == Sensitivity::kShowOnRequest) {
    out.detail = scrubbed.text;
  }
  return out;
}

}  // namespace errors
}  // namespace bedrock
