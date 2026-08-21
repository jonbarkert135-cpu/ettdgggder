// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/settings/portability.h"

#include <cstdio>
#include <set>
#include <string>

// An importer is a parser for hostile input that arrives wearing the user's
// trust. These tests are mostly about what it refuses to do with that trust.

namespace {

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::printf("  FAIL: %s\n", what.c_str());
    ++failures;
  }
}

using namespace bedrock::settings;

void EveryRoadmapPayloadIsCoveredAndVersioned() {
  const std::vector<FormatSpec>& formats = Portability::Formats();
  Check(formats.size() == 5, "bookmarks, settings, privacy rules, filter rules, profiles");
  std::set<std::string> ids;
  for (const FormatSpec& spec : formats) {
    Check(ids.insert(spec.id).second, std::string("duplicate format id ") + spec.id);
    Check(spec.version >= 1, std::string(spec.id) + " is versioned");
    Check(std::string(spec.file_format).size() > 3,
          std::string(spec.id) + " names a real file format");
    Check(std::string(spec.notes).size() > 30,
          std::string(spec.id) + " documents what it is for");
    Check(std::string(spec.id).find(".v") != std::string::npos,
          std::string(spec.id) + " carries its version in its id");
  }
  // Bookmarks must be importable *and* exportable, per the roadmap.
  Check(Portability::Get(Payload::kBookmarks).direction == Direction::kBoth,
        "bookmarks go both ways");
}

void NewerFilesAreRefusedRatherThanHalfRead() {
  Check(Portability::CanRead(Payload::kSettings, 1), "the current version reads");
  Check(!Portability::CanRead(Payload::kSettings, 2), "a newer version is refused");
  Check(!Portability::CanRead(Payload::kSettings, 0), "a version-less file is refused");
  const ImportReport report =
      Portability::Preview(Payload::kSettings, 7, {{"privacy.level", "strict"}});
  Check(!report.accepted && report.items == 0, "nothing is imported from a newer file");
  Check(report.summary.find("Nothing was imported") != std::string::npos,
        "and the user is told, instead of getting a partial restore");
}

void AnImportCannotRaisePrivilege() {
  const ImportReport report = Portability::Preview(
      Payload::kSettings, 1,
      {{"privacy.level", "strict"},
       {"telemetry.enabled", "true"},
       {"managed.profile", "corp"},
       {"updates.source", "http://evil.example/build"},
       {"custom.proxy", "socks5://user:pw@host:9050"},
       {"custom.filter_list", "http://lists.example/list.txt"}});
  Check(report.items == 1, "only the honest setting is imported");
  Check(report.rejected.size() == 5, "the other five are refused, one reason each");
  for (const std::string& line : report.rejected) {
    Check(line.find(": ") != std::string::npos, "each refusal says which key and why: " + line);
  }
}

void ImportedAdvancedValuesFaceTheSameGuardsAsTypedOnes() {
  // The file is not a back door around item 57.
  const ImportReport report = Portability::Preview(
      Payload::kPrivacyRules, 1,
      {{"site.content_policy", "unsafe-inline"}, {"site.user_agent", "Mozilla/5.0 (me)"}});
  Check(report.rejected.size() == 1,
        "a CSP-relaxing rule is refused on import exactly as in the dialog");
  Check(report.items == 1, "a per-site UA override is allowed, as in the dialog");
}

void PolicyLockedKeysSurviveAnImport() {
  const ImportReport report =
      Portability::Preview(Payload::kSettings, 1,
                           {{"privacy.level", "standard"}, {"network.dns.mode", "system"}},
                           {"privacy.level"});
  Check(report.items == 1, "the unlocked key imports");
  Check(report.rejected.size() == 1 &&
            report.rejected[0].find("policy") != std::string::npos,
        "the locked key is kept and the reason names policy");
}

void SecretsLeaveOnlyWhenAskedForTwice() {
  Check(!Portability::Get(Payload::kSettings).may_contain_secrets,
        "a settings export never carries secrets");
  Check(!Portability::IncludesPasswords(Payload::kSettings, true, true),
        "even asking cannot put passwords in a settings file");
  Check(!Portability::IncludesPasswords(Payload::kProfileBundle, true, false),
        "no passphrase, no passwords");
  Check(!Portability::IncludesPasswords(Payload::kProfileBundle, false, true),
        "not requested, not included");
  Check(Portability::IncludesPasswords(Payload::kProfileBundle, true, true),
        "requested and encrypted: allowed");
}

void FilterListLicencesAreNotRedistributed() {
  const FormatSpec& filters = Portability::Get(Payload::kFilterRules);
  Check(std::string(filters.notes).find("never as their contents") != std::string::npos,
        "third-party list contents are not exported");
}

void NothingHerePromisesMoreThanItDoes() {
  const char* banned[] = {"anonymous", "untraceable", "100%", "completely private"};
  for (const std::string& text : Portability::AllUserVisibleStrings()) {
    for (const char* word : banned) {
      Check(text.find(word) == std::string::npos,
            std::string("import/export copy must not claim '") + word + "': " + text);
    }
  }
}

}  // namespace

int main() {
  std::printf("portability_test\n");
  EveryRoadmapPayloadIsCoveredAndVersioned();
  NewerFilesAreRefusedRatherThanHalfRead();
  AnImportCannotRaisePrivilege();
  ImportedAdvancedValuesFaceTheSameGuardsAsTypedOnes();
  PolicyLockedKeysSurviveAnImport();
  SecretsLeaveOnlyWhenAskedForTwice();
  FilterListLicencesAreNotRedistributed();
  NothingHerePromisesMoreThanItDoes();
  std::printf(failures == 0 ? "  ok\n" : "  %d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
