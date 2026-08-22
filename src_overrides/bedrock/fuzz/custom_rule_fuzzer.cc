// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Fuzzes the custom-rule / advanced-settings validator (roadmap item 76).
//
// Advanced settings are the place where a user is deliberately handed a loaded
// weapon: custom DNS, custom proxy, per-site policies, CSP-like controls. The
// guards (G1–G9) are what keeps "advanced" from meaning "the security model is
// now optional". A validator that crashes or accepts a malformed value is a
// guard that is not there.

#include "bedrock/fuzz/fuzz_main.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "bedrock/settings/advanced_settings.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string text(reinterpret_cast<const char*>(data), size);

  // First byte selects the control, so one corpus covers every code path;
  // the rest is the value, split on the first newline into value and scope.
  const size_t split = text.find('\n');
  const std::string value = text.substr(0, split);
  const std::string scope = split == std::string::npos ? "" : text.substr(split + 1);

  using bedrock::settings::AdvancedControl;
  using bedrock::settings::AdvancedInput;
  using bedrock::settings::AdvancedSettings;

  const int count = static_cast<int>(AdvancedControl::kMaxValue) + 1;
  for (int i = 0; i < count; ++i) {
    AdvancedInput input;
    input.control = static_cast<AdvancedControl>(i);
    input.value = value;
    input.scope = scope;
    // Policy-supplied values take a different path through the guards; a
    // managed environment must not be able to bypass the security floor.
    input.from_policy = !text.empty() && (text[0] & 2);
    const bedrock::settings::Decision decision = AdvancedSettings::Evaluate(input);

    // A rejection must say why. A silent "no" in an advanced settings dialog
    // is indistinguishable from a bug, and users work around it by disabling
    // something else.
    if (!decision.ok() && decision.message.empty())
      __builtin_trap();
  }
  return 0;
}
