// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/settings/settings_page.h"

namespace bedrock {
namespace settings {
namespace {

std::string Quote(const std::string& text) {
  std::string out = "\"";
  for (char c : text) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          static const char* kHex = "0123456789abcdef";
          out += "\\u00";
          out += kHex[(c >> 4) & 0xF];
          out += kHex[c & 0xF];
        } else {
          out += c;
        }
    }
  }
  return out + "\"";
}

bool StartsWith(const std::string& text, const char* prefix) {
  const std::string p(prefix);
  return text.size() >= p.size() && text.compare(0, p.size(), p) == 0;
}

// A row needs a short title and a full explanation, and `ConfigSurface` only
// carries the one line --help prints. Rather than keep a second table of
// labels that would drift from it, take the clause before the first colon or
// full stop as the title and show the whole line underneath.
std::string TitleOf(const std::string& description) {
  const std::string::size_type cut = description.find_first_of(":.");
  if (cut == std::string::npos || cut < 3)
    return description;
  return description.substr(0, cut);
}

}  // namespace

const std::vector<SectionInfo>& Sections() {
  static const std::vector<SectionInfo> kSections = {
      {Section::kGeneral, "general", "General",
       "Startup, language and how Bedrock behaves on this device."},
      {Section::kPrivacy, "privacy", "Privacy & Security",
       "What is blocked, what is allowed, and what each choice costs."},
      {Section::kSearch, "search", "Search",
       "Which provider receives your queries."},
      {Section::kAppearance, "appearance", "Appearance",
       "Theme, density and the tab layout."},
      {Section::kTabs, "tabs", "Tabs",
       "Tab layout, sleeping tabs and groups."},
      {Section::kDownloads, "downloads", "Downloads",
       "Where files go and what happens before they open."},
      {Section::kProfiles, "profiles", "Profiles",
       "Separate identities, each with its own data."},
      {Section::kExtensions, "extensions", "Extensions",
       "What each extension can read, and where it may run."},
      {Section::kAdvanced, "advanced", "Advanced",
       "Networking, updates and everything with sharp edges."},
  };
  return kSections;
}

const SectionInfo& Info(Section section) {
  for (const SectionInfo& info : Sections()) {
    if (info.section == section)
      return info;
  }
  return Sections().back();
}

Section SectionForKey(const std::string& key) {
  if (StartsWith(key, "privacy.") || StartsWith(key, "blocking.") ||
      StartsWith(key, "telemetry."))
    return Section::kPrivacy;
  if (StartsWith(key, "search."))
    return Section::kSearch;
  if (StartsWith(key, "ui.theme") || StartsWith(key, "appearance."))
    return Section::kAppearance;
  if (StartsWith(key, "tabs.") || StartsWith(key, "session."))
    return Section::kTabs;
  if (StartsWith(key, "downloads."))
    return Section::kDownloads;
  if (StartsWith(key, "profile.") || StartsWith(key, "profiles."))
    return Section::kProfiles;
  if (StartsWith(key, "extensions."))
    return Section::kExtensions;
  if (StartsWith(key, "ui.") || StartsWith(key, "startup."))
    return Section::kGeneral;
  // network.*, updates.* and anything new: visible, in Advanced.
  return Section::kAdvanced;
}

const char* OriginName(Origin origin) {
  switch (origin) {
    case Origin::kDefault: return "default";
    case Origin::kGui: return "gui";
    case Origin::kConfigFile: return "config-file";
    case Origin::kCommandLine: return "command-line";
    case Origin::kPolicy: return "policy";
  }
  return "default";
}

std::string SettingsPageJson(Section active,
                             const std::map<std::string, Resolved>& resolved,
                             const std::vector<ExtensionCard>& extensions) {
  std::string nav = "[";
  bool first = true;
  for (const SectionInfo& info : Sections()) {
    if (!first)
      nav += ",";
    first = false;
    nav += "{" + Quote("id") + ":" + Quote(info.id) + "," + Quote("title") +
           ":" + Quote(info.title) + "," + Quote("summary") + ":" +
           Quote(info.summary) + ",\"active\":" +
           (info.section == active ? "true" : "false") + "}";
  }
  nav += "]";

  std::string rows = "[";
  bool first_row = true;
  for (const SettingSpec& spec : ConfigSurface::All()) {
    if (SectionForKey(spec.key) != active)
      continue;
    const std::map<std::string, Resolved>::const_iterator it =
        resolved.find(spec.key);
    const std::string value =
        it != resolved.end() ? it->second.value : std::string(spec.default_value);
    const Origin origin = it != resolved.end() ? it->second.origin : Origin::kDefault;
    const bool locked = it != resolved.end() && it->second.locked;

    std::string values = "[";
    for (std::vector<std::string>::size_type i = 0; i < spec.values.size(); ++i) {
      if (i)
        values += ",";
      values += Quote(spec.values[i]);
    }
    values += "]";

    if (!first_row)
      rows += ",";
    first_row = false;
    // When the title is the whole line, there is nothing to add underneath;
    // repeating it would be noise pretending to be help.
    const std::string title = TitleOf(spec.description);
    const std::string detail =
        (title == spec.description || title + "." == spec.description)
            ? std::string()
            : std::string(spec.description);
    rows += "{" + Quote("key") + ":" + Quote(spec.key) + "," + Quote("label") +
            ":" + Quote(title) + "," + Quote("detail") + ":" + Quote(detail) +
            "," + Quote("value") + ":" +
            Quote(value) + "," + Quote("default") + ":" +
            Quote(spec.default_value) + ",\"values\":" + values + "," +
            Quote("origin") + ":" + Quote(OriginName(origin)) +
            ",\"locked\":" + (locked ? "true" : "false") + ",\"flag\":" +
            (spec.takes_value ? "false" : "true") + "}";
  }
  rows += "]";

  // Extension cards only travel with the section that draws them.
  std::string cards = "[";
  if (active == Section::kExtensions) {
    for (std::vector<ExtensionCard>::size_type i = 0; i < extensions.size(); ++i) {
      if (i)
        cards += ",";
      const ExtensionCard& card = extensions[i];
      cards += "{" + Quote("id") + ":" + Quote(card.id) + "," + Quote("name") +
               ":" + Quote(card.name) + "," + Quote("version") + ":" +
               Quote(card.version) + "," + Quote("risk") + ":" +
               Quote(card.risk) + "," + Quote("hostAccess") + ":" +
               Quote(card.host_access) + ",\"enabled\":" +
               (card.enabled ? "true" : "false") + ",\"privateWindows\":" +
               (card.private_windows ? "true" : "false") + "}";
    }
  }
  cards += "]";

  const SectionInfo& info = Info(active);
  return "{" + Quote("section") + ":" + Quote(info.id) + "," + Quote("title") +
         ":" + Quote(info.title) + "," + Quote("summary") + ":" +
         Quote(info.summary) + ",\"nav\":" + nav + ",\"rows\":" + rows +
         ",\"extensions\":" + cards + "}";
}

}  // namespace settings
}  // namespace bedrock
