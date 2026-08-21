// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/settings/config_surface.h"

#include <algorithm>

namespace bedrock {
namespace settings {

namespace {

constexpr int kGuiConfigPolicy = static_cast<int>(Surface::kGui) |
                                 static_cast<int>(Surface::kConfigFile) |
                                 static_cast<int>(Surface::kPolicy);

const std::vector<SettingSpec>& Table() {
  static const std::vector<SettingSpec> table = {
      {"privacy.level", "privacy-level", "PrivacyLevel",
       "Global protection preset.",
       {"standard", "balanced", "strict", "maximum"}, "balanced", kAllSurfaces, "", true},

      {"privacy.fingerprinting", "fingerprinting", "FingerprintProtection",
       "Anti-fingerprinting level for this profile.",
       {"off", "standard", "strict", "maximum"}, "standard", kAllSurfaces, "", true},

      {"privacy.cookies", "cookies", "CookiePolicy",
       "Cookie policy: allow all, block third-party, or block all.",
       {"allow", "block-third-party", "block"}, "block-third-party", kAllSurfaces, "", true},

      {"privacy.https", "https", "HttpsPolicy",
       "HTTPS handling: upgrade with fallback, or HTTPS-only.",
       {"upgrade", "only"}, "upgrade", kAllSurfaces, "", true},

      {"privacy.referrer", "referrer", "ReferrerPolicy",
       "Cross-site referrer handling.",
       {"full", "origin", "none"}, "origin", kAllSurfaces, "", true},

      {"network.dns.mode", "dns-mode", "DnsMode",
       "Resolver: the system one, a named DoH provider, or strict fail-closed DoH.",
       {"system", "doh", "doh-strict"}, "system", kAllSurfaces, "", true},

      {"network.dns.provider", "dns-provider", "DnsProvider",
       "Named DoH provider id, when dns.mode is doh or doh-strict.",
       {}, "", kAllSurfaces, "", true},

      {"network.webrtc", "webrtc-policy", "WebRtcPolicy",
       "WebRTC IP exposure: default, privacy (no local addresses), or strict.",
       {"default", "privacy", "strict"}, "privacy", kAllSurfaces, "", true},

      {"blocking.lists", "filter-lists", "FilterLists",
       "Comma-separated filter list ids to subscribe to (see docs/privacy/FILTER_LISTS.md).",
       {}, "", kAllSurfaces, "", true},

      {"search.default_engine", "search-engine", "DefaultSearchEngine",
       "Default search engine id, for example duckduckgo.",
       {}, "duckduckgo", kAllSurfaces, "", true},

      {"profile.name", "profile", "", "Profile to start with.",
       {}, "", static_cast<int>(Surface::kGui) | static_cast<int>(Surface::kConfigFile) |
               static_cast<int>(Surface::kCommandLine),
       "No policy control: an administrator pinning which profile a person opens is user "
       "management, not configuration, and profiles are a local privacy boundary.",
       true},

      {"session.tor_window", "tor-window", "",
       "Open a Tor transport window at startup. Not an anonymity guarantee (see item 51).",
       {}, "false",
       static_cast<int>(Surface::kGui) | static_cast<int>(Surface::kCommandLine),
       "Not a persisted setting: a Tor window is an action, not a state, and a config file "
       "that silently starts one would surprise the user.",
       false},

      {"telemetry.enabled", "disable-telemetry", "TelemetryEnabled",
       "Telemetry is off and cannot be switched on; the switch is accepted so scripts that "
       "pass it are not told it is unknown.",
       {"false"}, "false", kGuiConfigPolicy | static_cast<int>(Surface::kCommandLine),
       "Value is fixed: Bedrock has no telemetry to enable (item 39), so 'true' is rejected "
       "rather than accepted and ignored.",
       false},

      {"updates.channel", "update-channel", "UpdateChannel",
       "Update channel to check.", {"stable", "beta", "none"}, "stable", kAllSurfaces, "", true},
  };
  return table;
}

std::string Trim(const std::string& text) {
  const size_t start = text.find_first_not_of(" \t");
  if (start == std::string::npos) {
    return "";
  }
  const size_t end = text.find_last_not_of(" \t");
  return text.substr(start, end - start + 1);
}

bool ValueAllowed(const SettingSpec& spec, const std::string& value) {
  if (spec.values.empty()) {
    return true;  // free-form (a provider id, an engine id, a profile name)
  }
  return std::find(spec.values.begin(), spec.values.end(), value) != spec.values.end();
}

std::string AllowedList(const SettingSpec& spec) {
  std::string out;
  for (const std::string& value : spec.values) {
    if (!out.empty()) {
      out += ", ";
    }
    out += value;
  }
  return out;
}

}  // namespace

const std::vector<SettingSpec>& ConfigSurface::All() {
  return Table();
}

const SettingSpec* ConfigSurface::ByKey(const std::string& key) {
  for (const SettingSpec& spec : Table()) {
    if (key == spec.key) {
      return &spec;
    }
  }
  return nullptr;
}

const SettingSpec* ConfigSurface::BySwitch(const std::string& switch_name) {
  if (switch_name.empty()) {
    return nullptr;
  }
  for (const SettingSpec& spec : Table()) {
    if (switch_name == spec.switch_name) {
      return &spec;
    }
  }
  return nullptr;
}

ParseResult ConfigSurface::ParseCommandLine(const std::vector<std::string>& args) {
  ParseResult result;
  for (const std::string& raw : args) {
    const std::string arg = Trim(raw);
    if (arg.empty()) {
      continue;
    }
    if (arg.rfind("--", 0) != 0) {
      result.errors.push_back("not a switch: " + arg);
      continue;
    }
    const size_t equals = arg.find('=');
    const std::string name = arg.substr(2, equals == std::string::npos
                                              ? std::string::npos
                                              : equals - 2);
    const SettingSpec* spec = BySwitch(name);
    if (spec == nullptr) {
      result.errors.push_back("unknown switch --" + name);
      continue;
    }
    if (spec->takes_value) {
      if (equals == std::string::npos) {
        result.errors.push_back("--" + name + " needs a value (--" + name + "=<value>)");
        continue;
      }
      const std::string value = arg.substr(equals + 1);
      if (value.empty()) {
        result.errors.push_back("--" + name + " was given an empty value");
        continue;
      }
      if (!ValueAllowed(*spec, value)) {
        result.errors.push_back("--" + name + "=" + value + " is not allowed; expected one of: " +
                                AllowedList(*spec));
        continue;
      }
      result.values[spec->key] = value;
    } else {
      if (equals != std::string::npos) {
        // A flag with a value is almost always a user expecting it to toggle
        // something. Saying so beats guessing which way they meant it.
        const std::string value = arg.substr(equals + 1);
        if (!ValueAllowed(*spec, value)) {
          result.errors.push_back("--" + name + " does not accept the value " + value);
          continue;
        }
        result.values[spec->key] = value;
      } else {
        // `--disable-telemetry` asserts the shipped state; `--tor-window` acts.
        result.values[spec->key] = spec->values.empty() ? "true" : spec->values.front();
      }
    }
  }
  return result;
}

std::map<std::string, Resolved> ConfigSurface::Resolve(
    const std::map<std::string, std::string>& config_file,
    const std::map<std::string, std::string>& gui_prefs,
    const std::map<std::string, std::string>& command_line,
    const std::map<std::string, std::string>& policy) {
  std::map<std::string, Resolved> resolved;
  for (const SettingSpec& spec : Table()) {
    Resolved value{spec.default_value, Origin::kDefault, false};
    const std::string key = spec.key;

    auto apply = [&](const std::map<std::string, std::string>& source, Origin origin) {
      const auto it = source.find(key);
      if (it != source.end() && ValueAllowed(spec, it->second)) {
        value.value = it->second;
        value.origin = origin;
      }
    };

    apply(gui_prefs, Origin::kGui);
    apply(config_file, Origin::kConfigFile);
    apply(command_line, Origin::kCommandLine);

    const auto managed = policy.find(key);
    if (managed != policy.end() && ValueAllowed(spec, managed->second)) {
      value.value = managed->second;
      value.origin = Origin::kPolicy;
      value.locked = true;  // the GUI shows this as managed, not as a broken control
    }
    resolved[key] = value;
  }
  return resolved;
}

std::string ConfigSurface::HelpText() {
  std::string help = "Bedrock configuration switches (see docs/CONFIGURATION.md):\n";
  for (const SettingSpec& spec : Table()) {
    if ((spec.surfaces & static_cast<int>(Surface::kCommandLine)) == 0) {
      continue;
    }
    help += "  --";
    help += spec.switch_name;
    if (spec.takes_value) {
      help += spec.values.empty() ? "=<value>" : "=" + AllowedList(spec);
    }
    help += "\n      ";
    help += spec.description;
    help += "\n";
  }
  return help;
}

}  // namespace settings
}  // namespace bedrock
