// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Fuzzes the outgoing-header policy (roadmap item 43: networking). Every input
// here is attacker-chosen: the referring URL comes from the page the user was
// on, the declared referrer policy is a header field, and both are parsed
// before a request leaves the browser. A crash in this path is remotely
// triggerable by any site.

#include "bedrock/fuzz/fuzz_main.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "bedrock/privacy/core/protection_controller.h"
#include "bedrock/privacy/network/request_headers.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);

  bedrock::privacy::ProtectionController controls;
  bedrock::net::RequestHeaderPolicy policy(&controls);

  // The first byte selects the declared policy and the level, so the fuzzer can
  // reach every branch instead of only the default one.
  const uint8_t selector = size > 0 ? data[0] : 0;
  policy.set_fp_level(static_cast<bedrock::privacy::FpLevel>(selector % 4));

  bedrock::net::OutgoingRequest request;
  request.initiator_url = input;
  request.initiator_host = input;
  request.initiator_etld1 = input;
  request.target_url = "https://target.test/x";
  request.target_host = "target.test";
  request.target_etld1 = "target.test";
  request.declared = static_cast<bedrock::net::DeclaredReferrerPolicy>(
      (selector / 4) % 9);

  policy.ReferrerFor(request);
  policy.DeclaredPolicyRefused(request);
  bedrock::net::RequestHeaderPolicy::OriginOf(input);
  bedrock::net::RequestHeaderPolicy::SanitizeUrl(input);

  // The same URL as the *target*: a referrer decision is made in both
  // directions, and only one of them is usually tested.
  bedrock::net::OutgoingRequest reversed = request;
  reversed.initiator_url = "https://source.test/page";
  reversed.initiator_host = "source.test";
  reversed.initiator_etld1 = "source.test";
  reversed.target_url = input;
  reversed.target_host = input;
  reversed.target_etld1 = input;
  policy.ReferrerFor(reversed);

  bedrock::net::DeviceFacts facts;
  facts.platform = input;
  facts.model = input;
  facts.full_version = input;
  facts.ua_brand = input;
  policy.HintHeadersFor(request, {bedrock::net::Hint::kUaModel,
                                  bedrock::net::Hint::kUaPlatformVersion,
                                  bedrock::net::Hint::kDeviceMemory},
                        facts);
  policy.AcceptLanguage(facts);
  return 0;
}
