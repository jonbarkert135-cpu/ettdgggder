// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/onboarding/first_run_page.h"

#include "bedrock/privacy/core/security_levels.h"

namespace bedrock {
namespace onboarding {
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
        // A control character in a JSON string is a parse error in the page,
        // so escape rather than emit it.
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

std::string Member(const std::string& key, const std::string& value) {
  return Quote(key) + ":" + Quote(value);
}

// One selectable option: the wire id, its label, and the line under it.
std::string Option(const std::string& id, const std::string& label,
                   const std::string& detail, bool selected) {
  return "{" + Member("id", id) + "," + Member("label", label) + "," +
         Member("detail", detail) + ",\"selected\":" +
         (selected ? "true" : "false") + "}";
}

const char* StepId(Step step) {
  switch (step) {
    case Step::kWelcome: return "welcome";
    case Step::kPrivacyLevel: return "privacy";
    case Step::kSearchEngine: return "search";
    case Step::kTheme: return "theme";
    case Step::kImport: return "import";
    case Step::kFinish: return "finish";
  }
  return "welcome";
}

// The heading and the line under it, per step. They live here, next to the
// logic they describe, so the page has no copy of its own to drift.
struct StepCopy {
  const char* title;
  const char* lead;
};

StepCopy CopyFor(Step step) {
  switch (step) {
    case Step::kWelcome:
      return {"Bedrock", "A browser that shows what it is protecting, and what it cannot."};
    case Step::kPrivacyLevel:
      return {"Choose a privacy level", "You can change this at any time in Settings."};
    case Step::kSearchEngine:
      return {"Choose a search engine", "Your searches go from this browser to the provider you pick."};
    case Step::kTheme:
      return {"Choose a theme", "No restart is needed for any appearance change."};
    case Step::kImport:
      return {"Import your data", "Nothing is imported unless you choose a source."};
    case Step::kFinish:
      return {"You are set up", "These answers are your settings. Settings can change all of them."};
  }
  return {"Bedrock", ""};
}

struct PrivacyOption {
  const char* id;
  settings::PrivacyChoice choice;
  privacy::SecurityLevel level;
};

// The three levels first run offers. Maximum lives in Settings: a level whose
// compatibility cost needs a paragraph is not a first-run decision.
const PrivacyOption kPrivacyOptions[] = {
    {"standard", settings::PrivacyChoice::kStandard, privacy::SecurityLevel::kStandard},
    {"balanced", settings::PrivacyChoice::kBalanced, privacy::SecurityLevel::kBalanced},
    {"strict", settings::PrivacyChoice::kStrict, privacy::SecurityLevel::kStrict},
};

struct ThemeOption {
  const char* id;
  ui::ThemeMode mode;
  const char* label;
  const char* detail;
};

const ThemeOption kThemeOptions[] = {
    {"light", ui::ThemeMode::kLight, "Light", "A light window at any time of day."},
    {"dark", ui::ThemeMode::kDark, "Dark", "A dark window at any time of day."},
    {"system", ui::ThemeMode::kSystem, "System", "Follows the desktop setting."},
};

struct ImportOption {
  const char* id;
  ImportSource source;
  const char* label;
  const char* detail;
};

const ImportOption kImportOptions[] = {
    {"chrome", ImportSource::kChrome, "Chrome", "Bookmarks, history and passwords."},
    {"firefox", ImportSource::kFirefox, "Firefox", "Bookmarks, history and passwords."},
    {"edge", ImportSource::kEdge, "Edge", "Bookmarks, history and passwords."},
    {"chromium", ImportSource::kChromium, "Chromium", "Bookmarks, history and passwords."},
    {"html", ImportSource::kHtmlFile, "HTML file", "A bookmarks file you export yourself."},
    {"skip", ImportSource::kSkip, "Skip", "Start empty. You can import later from Settings."},
};

std::string PrivacyOptions(const FirstRun& flow) {
  std::string out;
  for (const PrivacyOption& option : kPrivacyOptions) {
    const privacy::LevelInfo& info = privacy::SecurityLevels::Info(option.level);
    std::string detail = info.summary;
    if (info.tradeoff && *info.tradeoff) {
      detail += std::string(" Cost: ") + info.tradeoff;
    }
    if (!out.empty()) out += ",";
    out += Option(option.id, info.name, detail,
                  flow.choices().privacy == option.choice);
  }
  return out;
}

std::string EngineOptions(const FirstRun& flow) {
  std::string out;
  for (const EngineFacts& engine : flow.offered()) {
    SearchDisclosure disclosure =
        FirstRun::Disclose(engine, flow.choices().search_suggestions);
    if (!out.empty()) out += ",";
    out += Option(engine.id, engine.name, disclosure.privacy,
                  flow.choices().engine_id == engine.id);
  }
  return out;
}

std::string ThemeOptions(const FirstRun& flow) {
  std::string out;
  for (const ThemeOption& option : kThemeOptions) {
    if (!out.empty()) out += ",";
    out += Option(option.id, option.label, option.detail,
                  flow.choices().theme == option.mode);
  }
  return out;
}

std::string ImportOptions(const FirstRun& flow) {
  std::string out;
  for (const ImportOption& option : kImportOptions) {
    if (!out.empty()) out += ",";
    out += Option(option.id, option.label, option.detail,
                  flow.choices().import_source == option.source);
  }
  return out;
}

std::string DisclosureJson(const FirstRun& flow) {
  SearchDisclosure disclosure = flow.Disclosure();
  return "{" + Member("provider", disclosure.provider) + "," +
         Member("suggestions", disclosure.suggestions) + "," +
         Member("safeBrowsing", disclosure.safe_browsing) + "," +
         Member("privacy", disclosure.privacy) + "}";
}

std::string PrivacyNotesJson() {
  std::string out;
  for (const char* note : FirstRun::PrivacyNotes()) {
    if (!out.empty()) out += ",";
    out += Quote(note);
  }
  return "[" + out + "]";
}

}  // namespace

std::string PageModelJson(const FirstRun& flow) {
  std::string out = "{";
  out += Member("step", StepId(flow.current()));
  out += "," + Member("title", CopyFor(flow.current()).title);
  out += "," + Member("lead", CopyFor(flow.current()).lead);
  out += ",\"done\":";
  out += flow.done() ? "true" : "false";
  out += ",\"suggestions\":";
  out += flow.choices().search_suggestions ? "true" : "false";
  out += ",\"privacyOptions\":[" + PrivacyOptions(flow) + "]";
  out += ",\"engineOptions\":[" + EngineOptions(flow) + "]";
  out += ",\"themeOptions\":[" + ThemeOptions(flow) + "]";
  out += ",\"importOptions\":[" + ImportOptions(flow) + "]";
  out += ",\"disclosure\":" + DisclosureJson(flow);
  out += "," + Member("privacyHeadline", FirstRun::PrivacyHeadline());
  out += ",\"privacyNotes\":" + PrivacyNotesJson();
  return out + "}";
}

bool ApplyPageChoice(FirstRun& flow, const std::string& field,
                     const std::string& value) {
  if (field == "privacy") {
    for (const PrivacyOption& option : kPrivacyOptions) {
      if (value == option.id) return flow.ChoosePrivacy(option.choice);
    }
    return false;
  }
  if (field == "engine") return flow.ChooseEngine(value);
  if (field == "suggestions") {
    if (value != "true" && value != "false") return false;
    flow.SetSuggestions(value == "true");
    return true;
  }
  if (field == "theme") {
    for (const ThemeOption& option : kThemeOptions) {
      if (value == option.id) return flow.ChooseTheme(option.mode);
    }
    return false;
  }
  if (field == "import") {
    for (const ImportOption& option : kImportOptions) {
      if (value == option.id) {
        flow.ChooseImport(option.source);
        return true;
      }
    }
    return false;
  }
  // The page asks for the state when it loads; that is not a change.
  if (field == "ready") return true;
  if (field == "step") {
    if (value == "next") return flow.Next();
    if (value == "back") return flow.Back();
    return false;
  }
  return false;
}

}  // namespace onboarding
}  // namespace bedrock
