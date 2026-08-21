// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test: no Chromium, no gtest. Build and run with
//   scripts/run_host_tests.sh
// It is also compiled into the browser's unit test binary later; keeping it
// dependency-free is what lets CI run it without a 100 GB checkout.

#include "bedrock/omnibox/input_parser.h"

#include <cassert>
#include <iostream>
#include <string>

namespace {

const std::vector<std::string> kBangs = {"!g", "!ddg", "!w", "!br"};

void Expect(std::string_view input,
            bedrock::InputType type,
            std::string_view text,
            std::string_view bang = "") {
  const bedrock::ParsedInput got = bedrock::ParseOmniboxInput(input, kBangs);
  const bedrock::ParsedInput want{type, std::string(text), std::string(bang)};
  if (!(got == want)) {
    std::cerr << "FAIL  input=[" << input << "]\n"
              << "  got  type=" << static_cast<int>(got.type) << " text=["
              << got.text << "] bang=[" << got.bang << "]\n"
              << "  want type=" << static_cast<int>(want.type) << " text=["
              << want.text << "] bang=[" << want.bang << "]\n";
    std::exit(1);
  }
}

}  // namespace

int main() {
  using bedrock::InputType;

  // URLs
  Expect("example.com", InputType::kUrl, "example.com");
  Expect("  https://example.com/a?b=c  ", InputType::kUrl,
         "https://example.com/a?b=c");
  Expect("localhost:8080", InputType::kUrl, "localhost:8080");
  Expect("127.0.0.1:8000", InputType::kUrl, "127.0.0.1:8000");
  Expect("bedrock://settings/search", InputType::kUrl,
         "bedrock://settings/search");
  Expect("/etc/hosts", InputType::kUrl, "/etc/hosts");
  Expect("file:///tmp/x.html", InputType::kUrl, "file:///tmp/x.html");

  // Searches
  Expect("best laptops", InputType::kSearch, "best laptops");
  Expect("example. com", InputType::kSearch, "example. com");
  Expect("c++ move semantics", InputType::kSearch, "c++ move semantics");
  Expect("what is 2.5", InputType::kSearch, "what is 2.5");
  Expect("mail.", InputType::kSearch, "mail.");

  // Bangs, leading and trailing
  Expect("!g linux kernel", InputType::kSearch, "linux kernel", "!g");
  Expect("!ddg privacy browser", InputType::kSearch, "privacy browser", "!ddg");
  Expect("linux kernel !g", InputType::kSearch, "linux kernel", "!g");
  Expect("!w  Chromium ", InputType::kSearch, "Chromium", "!w");
  // A bang alone: search the empty string, provider still selected.
  Expect("!br", InputType::kSearch, "", "!br");
  // An explicit bang beats URL detection: "!g example.com" is a search.
  Expect("!g example.com", InputType::kSearch, "example.com", "!g");

  // Unknown bangs stay part of the query — never silently dropped.
  Expect("!unknown thing", InputType::kSearch, "!unknown thing");
  Expect("!!!", InputType::kSearch, "!!!");
  Expect("hello !notabang", InputType::kSearch, "hello !notabang");

  // Commands
  Expect(">clear history", InputType::kCommand, "clear history");
  Expect("> shields off ", InputType::kCommand, "shields off");

  // Degenerate input
  Expect("", InputType::kSearch, "");
  Expect("   ", InputType::kSearch, "");

  std::cout << "input_parser_test: all assertions passed\n";
  return 0;
}
