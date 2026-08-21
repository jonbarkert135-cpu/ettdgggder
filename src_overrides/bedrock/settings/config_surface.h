// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_SETTINGS_CONFIG_SURFACE_H_
#define BEDROCK_SETTINGS_CONFIG_SURFACE_H_

#include <map>
#include <string>
#include <vector>

// The configuration surface (roadmap item 56).
//
// Every major privacy control is reachable four ways — GUI, config file,
// enterprise policy and, where it is useful, the command line — and this table
// is the one place that says so. One row per setting, with its key, its switch,
// its policy name and its allowed values.
//
// Two rules the table exists to enforce:
//
//   1. **A surface that silently ignores input is worse than one that has no
//      input.** `--disable-telemetry` on a browser that quietly ignores it
//      teaches the user a lie. So parsing is strict: an unknown switch or an
//      invalid value is an error the browser reports, never a shrug.
//   2. **One definition per setting.** The GUI, the config file, the policy and
//      the CLI all name the same row. A second table would drift, and the copy
//      that drifts is always the one nobody tests.
//
// Precedence, highest first — enterprise policy wins because an administrator
// must be able to bind a setting the user cannot undo:
//
//   policy  >  command line  >  config file  >  GUI/prefs  >  built-in default
//
// The CLI is documented in `docs/CONFIGURATION.md`; `scripts/check_config_surface.py`
// fails the build if a switch here is missing there, or the other way round.

namespace bedrock {
namespace settings {

// Where a setting can be changed. A setting that is not exposed somewhere must
// say why in `restriction_reason` — "we forgot" is not a value.
enum class Surface {
  kGui = 1 << 0,
  kConfigFile = 1 << 1,
  kPolicy = 1 << 2,
  kCommandLine = 1 << 3,
};

constexpr int kAllSurfaces = static_cast<int>(Surface::kGui) |
                             static_cast<int>(Surface::kConfigFile) |
                             static_cast<int>(Surface::kPolicy) |
                             static_cast<int>(Surface::kCommandLine);

struct SettingSpec {
  const char* key;              // config-file key, "privacy.level"
  const char* switch_name;      // CLI switch without dashes, "privacy-level" ("" = none)
  const char* policy_name;      // enterprise policy name, "PrivacyLevel" ("" = none)
  const char* description;      // one line, shown by --help
  std::vector<std::string> values;  // allowed values; empty = free-form string
  const char* default_value;
  int surfaces;                 // bitmask of Surface
  const char* restriction_reason;  // required when surfaces != kAllSurfaces
  bool takes_value;             // false for flags such as --tor-window
};

// Where a resolved value came from. Reported in the settings UI so the user is
// never left wondering why a control is greyed out.
enum class Origin {
  kDefault,
  kGui,
  kConfigFile,
  kCommandLine,
  kPolicy,
};

struct Resolved {
  std::string value;
  Origin origin = Origin::kDefault;
  bool locked = false;  // set by policy; the GUI shows it as managed
};

struct ParseResult {
  std::map<std::string, std::string> values;  // key -> value
  std::vector<std::string> errors;            // human-readable, one per problem
  bool ok() const { return errors.empty(); }
};

class ConfigSurface {
 public:
  // The table. One row per setting, in the order --help prints them.
  static const std::vector<SettingSpec>& All();
  static const SettingSpec* ByKey(const std::string& key);
  static const SettingSpec* BySwitch(const std::string& switch_name);

  // Strict command-line parsing: `--key=value`, or `--flag` for flags.
  // Unknown switches, missing values and values outside the allowed set are
  // errors, never ignored.
  static ParseResult ParseCommandLine(const std::vector<std::string>& args);

  // Resolution with the documented precedence. Later sources do not overwrite
  // a policy value; they are recorded as attempts and the value stays locked.
  static std::map<std::string, Resolved> Resolve(
      const std::map<std::string, std::string>& config_file,
      const std::map<std::string, std::string>& gui_prefs,
      const std::map<std::string, std::string>& command_line,
      const std::map<std::string, std::string>& policy);

  // `--help` text, generated from the table so it cannot go stale.
  static std::string HelpText();
};

}  // namespace settings
}  // namespace bedrock

#endif  // BEDROCK_SETTINGS_CONFIG_SURFACE_H_
