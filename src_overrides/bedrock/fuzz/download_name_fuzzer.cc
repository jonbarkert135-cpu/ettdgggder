// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Fuzzes download risk assessment (roadmap item 43: networking / downloads).
// The file name and MIME type come straight from a remote server and are
// attacker-chosen by definition — including the bidirectional-override tricks
// the assessor is meant to catch.

#include "bedrock/fuzz/fuzz_main.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "bedrock/downloads/download_manager.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);
  const size_t split = size ? data[0] % (size ? size : 1) : 0;
  const std::string filename = input.substr(split);
  const std::string mime = input.substr(0, split);
  bedrock::downloads::DownloadManager::Assess(filename, mime);
  return 0;
}
