// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/settings/portability.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

namespace bedrock {
namespace settings {
namespace {

std::string Lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

bool Has(const std::string& haystack, const char* needle) {
  return haystack.find(needle) != std::string::npos;
}

// Keys an imported file must never be able to set. Not a blocklist of bad
// values — a list of decisions that are not the file's to make.
bool IsPrivilegeEscalation(const std::string& key, const std::string& value,
                           std::string* reason) {
  const std::string k = Lower(key);
  const std::string v = Lower(value);
  if (Has(k, "telemetry") && v != "false" && v != "off" && !v.empty()) {
    *reason = "an imported file cannot switch on reporting: there is none to switch on";
    return true;
  }
  if (Has(k, "policy") || Has(k, "managed")) {
    *reason = "enterprise policy comes from the policy system, not from a file the user opened";
    return true;
  }
  if (Has(k, "update") && Has(v, "http://")) {
    *reason = "an update source over plain HTTP would let anyone on the path ship you a browser";
    return true;
  }
  return false;
}

// Advanced values in an imported file are checked exactly as if the user had
// typed them, so a file cannot smuggle in what the dialog would refuse.
bool AdvancedValueRefused(const std::string& key, const std::string& value,
                          std::string* reason) {
  struct Mapping {
    const char* key_fragment;
    AdvancedControl control;
    bool needs_scope;
  };
  static const Mapping kMappings[] = {
      {"filter_list", AdvancedControl::kCustomFilterList, false},
      {"dns", AdvancedControl::kCustomDns, false},
      {"proxy", AdvancedControl::kCustomProxy, false},
      {"user_agent", AdvancedControl::kUserAgentPolicy, true},
      {"content_policy", AdvancedControl::kContentPolicy, true},
  };
  const std::string k = Lower(key);
  for (const Mapping& mapping : kMappings) {
    if (!Has(k, mapping.key_fragment)) {
      continue;
    }
    const Decision decision = AdvancedSettings::Evaluate(
        {mapping.control, value, mapping.needs_scope ? "imported.example" : "", false});
    if (!decision.ok()) {
      *reason = decision.message;
      return true;
    }
  }
  return false;
}

}  // namespace

// static
const std::vector<FormatSpec>& Portability::Formats() {
  static const std::vector<FormatSpec> formats = {
      {Payload::kBookmarks, "bedrock.bookmarks.v1", "Bookmarks",
       "Netscape bookmark HTML", ".html", 1, Direction::kBoth, false,
       "The format every browser reads and writes. Bedrock imports the same file from "
       "Chrome, Firefox and Safari, and exports one they can all read."},

      {Payload::kSettings, "bedrock.settings.v1", "Settings", "JSON", ".json", 1,
       Direction::kBoth, false,
       "The keys are the config-file keys from docs/CONFIGURATION.md — one vocabulary "
       "for the file you edit and the file you export. Never contains cookies, tokens "
       "or passwords."},

      {Payload::kPrivacyRules, "bedrock.privacy-rules.v1", "Privacy rules", "JSON",
       ".json", 1, Direction::kBoth, false,
       "Per-site permissions, exceptions and content policies. Exported so a careful "
       "configuration survives a reinstall, which is the point of item 58's backups."},

      {Payload::kFilterRules, "bedrock.filter-rules.v1", "Filter rules",
       "text, one rule per line", ".txt", 1, Direction::kBoth, false,
       "Your own rules only. Subscribed third-party lists are exported as their URLs, "
       "never as their contents: their licences are not ours to redistribute "
       "(docs/privacy/FILTER_LISTS.md)."},

      {Payload::kProfileBundle, "bedrock.profile.v1", "Profile", "zip archive with a "
       "manifest", ".bedrockprofile", 1, Direction::kBoth, true,
       "Settings, rules and bookmarks for one profile. Passwords are included only when "
       "asked for and only inside a separately encrypted member; history and cookies "
       "are never included."},
  };
  return formats;
}

// static
const FormatSpec& Portability::Get(Payload payload) {
  for (const FormatSpec& spec : Formats()) {
    if (spec.payload == payload) {
      return spec;
    }
  }
  return Formats().front();
}

// static
bool Portability::CanRead(Payload payload, int file_version) {
  return file_version >= 1 && file_version <= Get(payload).version;
}

// static
bool Portability::IncludesPasswords(Payload payload, bool user_requested,
                                    bool passphrase_set) {
  return Get(payload).may_contain_secrets && user_requested && passphrase_set;
}

// static
ImportReport Portability::Preview(
    Payload payload, int file_version,
    const std::vector<std::pair<std::string, std::string>>& values,
    const std::vector<std::string>& locked_keys) {
  ImportReport report;
  const FormatSpec& spec = Get(payload);
  if (spec.direction == Direction::kExportOnly) {
    report.summary = std::string(spec.human_name) + " cannot be imported.";
    return report;
  }
  if (!CanRead(payload, file_version)) {
    report.summary = "This file is version " + std::to_string(file_version) + "; " +
                     "Bedrock reads " + spec.id + " up to version " +
                     std::to_string(spec.version) +
                     ". Nothing was imported — a newer file may mean things this "
                     "version would drop without telling you.";
    return report;
  }

  for (const auto& entry : values) {
    std::string reason;
    if (std::find(locked_keys.begin(), locked_keys.end(), entry.first) !=
        locked_keys.end()) {
      report.rejected.push_back(entry.first +
                                ": locked by your organisation's policy, kept as it is");
      continue;
    }
    if (IsPrivilegeEscalation(entry.first, entry.second, &reason) ||
        AdvancedValueRefused(entry.first, entry.second, &reason)) {
      report.rejected.push_back(entry.first + ": " + reason);
      continue;
    }
    ++report.items;
  }

  report.accepted = report.items > 0;
  report.summary = std::to_string(report.items) + " item(s) will be imported, " +
                   std::to_string(report.rejected.size()) + " refused. " +
                   "Nothing changes until you confirm.";
  return report;
}

// static
std::vector<std::string> Portability::AllUserVisibleStrings() {
  std::vector<std::string> out;
  for (const FormatSpec& spec : Formats()) {
    out.push_back(spec.human_name);
    out.push_back(spec.notes);
    out.push_back(spec.id);
  }
  out.push_back(Preview(Payload::kSettings, 99, {}).summary);
  const ImportReport report = Preview(
      Payload::kSettings, 1,
      {{"privacy.level", "strict"}, {"telemetry.enabled", "true"}, {"proxy", "ftp://x"}});
  out.push_back(report.summary);
  for (const std::string& rejected : report.rejected) {
    out.push_back(rejected);
  }
  return out;
}

}  // namespace settings
}  // namespace bedrock
