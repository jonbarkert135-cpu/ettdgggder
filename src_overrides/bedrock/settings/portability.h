// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_SETTINGS_PORTABILITY_H_
#define BEDROCK_SETTINGS_PORTABILITY_H_

#include <string>
#include <utility>
#include <vector>

#include "bedrock/settings/advanced_settings.h"

// Import and export (roadmap item 59).
//
// A browser you cannot leave is a browser you cannot trust, so everything the
// user made — bookmarks, settings, privacy rules, filter rules, whole profiles
// — comes back out in a documented format. `docs/FORMATS.md` describes each one
// and `scripts/check_config_surface.py` fails the build if this table and that
// document disagree.
//
// Three rules the table exists to enforce:
//
//   1. **An export never carries a secret by accident.** Passwords leave only
//      when the user asks for them specifically, in their own encrypted file;
//      cookies and session tokens never ride along inside a settings export.
//   2. **An import can only lower privilege, never raise it.** An imported file
//      is untrusted input: it cannot switch on telemetry, cannot override a
//      locked policy, and every advanced value in it goes through the same
//      guards as if it had been typed by hand (item 57).
//   3. **Every payload names its format and version**, and an unknown or newer
//      version is refused rather than half-read. Silently ignoring the parts we
//      do not understand is how a "restore" quietly loses your rules.

namespace bedrock {
namespace settings {

enum class Payload {
  kBookmarks,
  kSettings,
  kPrivacyRules,
  kFilterRules,
  kProfileBundle,
  kMaxValue = kProfileBundle,
};

enum class Direction {
  kExportOnly,
  kImportOnly,
  kBoth,
};

struct FormatSpec {
  Payload payload;
  const char* id;            // "bedrock.settings.v1"
  const char* human_name;    // "Settings"
  const char* file_format;   // "JSON", "Netscape bookmark HTML", "text"
  const char* extension;     // ".json"
  int version;
  Direction direction;
  bool may_contain_secrets;  // true only where the user opted in explicitly
  const char* notes;
};

// What an import would do, shown before anything is applied.
struct ImportReport {
  bool accepted = false;
  int items = 0;
  std::vector<std::string> rejected;  // one line per refused item, with the reason
  std::string summary;
};

class Portability {
 public:
  static const std::vector<FormatSpec>& Formats();
  static const FormatSpec& Get(Payload payload);

  // Version handling. An older file is upgraded, the current one is read, a
  // newer one is refused — we cannot know what it added.
  static bool CanRead(Payload payload, int file_version);

  // Whether this export includes passwords. Only ever true when the user asked
  // for it *and* supplied a passphrase; the flag alone is not enough.
  static bool IncludesPasswords(Payload payload, bool user_requested,
                                bool passphrase_set);

  // Dry-run an import: `values` are (key, value) pairs from the file. Anything
  // that would raise privilege is refused with a reason, and the rest is
  // reported before it is applied. `locked_keys` come from enterprise policy.
  static ImportReport Preview(Payload payload, int file_version,
                              const std::vector<std::pair<std::string, std::string>>& values,
                              const std::vector<std::string>& locked_keys = {});

  static std::vector<std::string> AllUserVisibleStrings();
};

}  // namespace settings
}  // namespace bedrock

#endif  // BEDROCK_SETTINGS_PORTABILITY_H_
