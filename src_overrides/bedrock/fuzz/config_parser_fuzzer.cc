// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Fuzzes the configuration parser (roadmap item 76).
//
// The command line and the config file are the two ways a privacy setting can
// be set by something other than a click, which makes them the two ways a
// setting can be *silently mis-set*. A parser that accepts `--privacy-level=`
// with an empty value, or splits `--proxy=http://a=b` on the wrong `=`, hands
// the user a browser that is configured differently from what they wrote.

#include "bedrock/fuzz/fuzz_main.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "bedrock/settings/config_surface.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string text(reinterpret_cast<const char*>(data), size);

  // One argument per line: the corpus is then also readable as a config file.
  std::vector<std::string> args;
  std::string current;
  for (char c : text) {
    if (c == '\n') {
      args.push_back(current);
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  args.push_back(current);

  const bedrock::settings::ParseResult parsed =
      bedrock::settings::ConfigSurface::ParseCommandLine(args);

  // Resolution is where a partially-parsed value would do damage: it decides
  // which source wins and whether a value is locked by policy.
  std::map<std::string, std::string> as_config;
  for (const auto& [key, value] : parsed.values)
    as_config[key] = value;
  bedrock::settings::ConfigSurface::Resolve(as_config, {}, parsed.values, {});

  // Cheap invariant, checked on every input: a parse either reports an error
  // or produces only keys the table knows. A silently invented key would mean
  // a switch that does nothing while looking accepted.
  if (parsed.ok()) {
    for (const auto& [key, value] : parsed.values) {
      (void)value;
      if (!bedrock::settings::ConfigSurface::ByKey(key))
        __builtin_trap();
    }
  }
  return 0;
}
