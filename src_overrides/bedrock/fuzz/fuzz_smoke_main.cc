// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// CI driver for the fuzz harnesses. Without a fuzzing toolchain we cannot run
// a real campaign in the host suite, but we can guarantee every harness still
// builds and still survives the seed corpus and a spread of hostile-shaped
// inputs. The usual fuzzing failure is not an absent harness — it is one that
// quietly stopped compiling months ago.

#include "bedrock/fuzz/fuzz_main.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<std::string> SeedCorpus() {
  std::vector<std::string> seeds = {
      "",
      "a",
      "||example.com^$third-party",
      "@@||example.com/ads.js$script,important",
      "! comment\n\n||a^\n|http://b|\n/regex/$image\n",
      "###ad-banner\nexample.com##.promo\n",
      "https://example.com/?utm_source=x&id=1",
      "!g   ",
      ">shields off",
      "file:///etc/passwd",
      "invoice.pdf.exe",
      "\xE2\x80\xAE" "gpj.exe",
      "<!DOCTYPE NETSCAPE-Bookmark-file-1><DL><DT><A HREF=\"x\">y</A>",
      "<A HREF=\"",
      "<a href=\"&amp;&lt;\" tags=\",,,\">t</a>",
      std::string(4096, '|'),
      std::string(1024, '\0'),
      std::string("\xff\xfe\xfd\xfc", 4),
  };
  // Long, deeply repetitive inputs catch quadratic behaviour as well as
  // crashes; a parser that takes a minute on 64 KB is its own denial of
  // service.
  seeds.push_back(std::string(200, 'a') + "^" + std::string(200, 'b'));
  std::string many_rules;
  for (int i = 0; i < 500; ++i)
    many_rules += "||rule" + std::to_string(i) + ".test^\n";
  seeds.push_back(many_rules);
  return seeds;
}

}  // namespace

int main() {
  int inputs = 0;
  for (const std::string& seed : SeedCorpus()) {
    LLVMFuzzerTestOneInput(reinterpret_cast<const uint8_t*>(seed.data()),
                           seed.size());
    ++inputs;
    // Byte-flip mutations of each seed, deterministic so a failure reproduces.
    for (size_t i = 0; i < seed.size() && i < 32; ++i) {
      std::string mutated = seed;
      mutated[i] = static_cast<char>(mutated[i] ^ 0x7F);
      LLVMFuzzerTestOneInput(reinterpret_cast<const uint8_t*>(mutated.data()),
                             mutated.size());
      ++inputs;
      // Truncations: half of all parser bugs are "input ended here".
      const std::string cut = seed.substr(0, i);
      LLVMFuzzerTestOneInput(reinterpret_cast<const uint8_t*>(cut.data()),
                             cut.size());
      ++inputs;
    }
  }
  std::cout << "fuzz smoke: " << inputs << " inputs, no crash\n";
  return 0;
}
