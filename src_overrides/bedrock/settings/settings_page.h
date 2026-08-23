// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_SETTINGS_SETTINGS_PAGE_H_
#define BEDROCK_SETTINGS_SETTINGS_PAGE_H_

#include <map>
#include <string>
#include <vector>

#include "bedrock/settings/config_surface.h"

// The settings page (design items 21, 22, 25).
//
// The page is a view over `ConfigSurface`, never a second list of settings.
// That is the whole reason this file exists: the moment the UI keeps its own
// table, a setting exists in the config file and not in the GUI, or the GUI
// writes a key nothing reads, and neither bug shows up in a test. Here every
// key in `ConfigSurface::All()` is assigned to exactly one section, and a test
// asserts it — a new setting that nobody placed fails the build instead of
// dropping out of the GUI unnoticed.
//
// Rows also carry where their current value came from and whether policy has
// locked it, so the page can say *why* a control is greyed out. "Managed by
// your organisation" is information; a dead control is not.

namespace bedrock {
namespace settings {

enum class Section {
  kGeneral,
  kPrivacy,
  kSearch,
  kAppearance,
  kTabs,
  kDownloads,
  kProfiles,
  kExtensions,
  kAdvanced,
};

struct SectionInfo {
  Section section;
  const char* id;       // wire id, "privacy"
  const char* title;    // "Privacy & Security"
  const char* summary;  // one line under the title
};

// Navigation order, left rail top to bottom.
const std::vector<SectionInfo>& Sections();
const SectionInfo& Info(Section section);

// Which section owns a config key. Unknown keys land in Advanced rather than
// disappearing: a setting the user cannot see is a setting they cannot undo.
Section SectionForKey(const std::string& key);

// One extension as the page shows it (design item 25). Built by the host from
// ExtensionRegistry; the risk line is the registry's own classification, not a
// star rating and not a badge we invented.
struct ExtensionCard {
  std::string id;
  std::string name;
  std::string version;
  std::string risk;        // the registry's RiskLevel, as a word
  std::string host_access; // "All sites" / "3 sites" / "This site only"
  bool enabled = false;
  bool private_windows = false;
};

// The page state: the rail, the active section and its rows, each with the
// allowed values, the current value, its origin and whether it is locked.
std::string SettingsPageJson(Section active,
                             const std::map<std::string, Resolved>& resolved,
                             const std::vector<ExtensionCard>& extensions = {});

// Wire name for an origin, used by the page to explain a value.
const char* OriginName(Origin origin);

}  // namespace settings
}  // namespace bedrock

#endif  // BEDROCK_SETTINGS_SETTINGS_PAGE_H_
