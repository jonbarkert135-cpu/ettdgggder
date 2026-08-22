// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Fuzzes the extension permission parser (roadmap item 76).
//
// Manifest permissions are third-party input by definition: whoever wrote the
// extension chose every string. The disclosure the user approves is generated
// from them, so a parser that mis-handles an odd permission string produces a
// prompt that understates what the extension can do — the one failure mode the
// whole disclosure system exists to prevent.

#include "bedrock/fuzz/fuzz_main.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "bedrock/extensions/extension_registry.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string text(reinterpret_cast<const char*>(data), size);

  std::vector<std::string> permissions;
  std::vector<std::string> hosts;
  std::string current;
  bool to_hosts = false;
  for (char c : text) {
    if (c == '\n' || c == ',') {
      (to_hosts ? hosts : permissions).push_back(current);
      current.clear();
      to_hosts = !to_hosts;  // alternate, so both lists see hostile input
    } else {
      current.push_back(c);
    }
  }
  permissions.push_back(current);

  std::vector<std::string> unknown;
  const bedrock::extensions::Disclosure disclosure =
      bedrock::extensions::ExtensionRegistry::Analyze(
          permissions, hosts, !text.empty() && (text[0] & 1), &unknown);
  bedrock::extensions::ExtensionRegistry::Risk(disclosure);

  // An unknown permission must be preserved verbatim, never dropped: the UI
  // shows it to the user precisely because we could not classify it.
  if (unknown.size() > permissions.size())
    __builtin_trap();

  // Install the fuzzed disclosure and re-analyse the same input as an update.
  // Escalation review is the security-relevant path, and it compares two
  // disclosures built by this parser.
  bedrock::extensions::ExtensionRegistry registry("fuzz-profile");
  registry.Install("id", "name", "1.0", disclosure, /*user_confirmed=*/true);
  registry.Update("id", "1.1", disclosure);
  return 0;
}
