// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_ONBOARDING_FIRST_RUN_PAGE_H_
#define BEDROCK_ONBOARDING_FIRST_RUN_PAGE_H_

#include <string>

#include "bedrock/onboarding/first_run.h"

// The bridge between the first-run logic and the page that renders it.
//
// The WebUI is a renderer, not a second implementation: it receives the whole
// state as JSON and sends back nothing but the name of the choice the user
// made. Every option list, every disclosure line and the current step come from
// `FirstRun` here, so no privacy decision is ever made in JavaScript
// (invariant 28) and the page cannot drift from the logic it draws.
//
// The choice names are the wire format used in both directions:
//   privacy   standard | balanced | strict
//   engine    the engine id from bedrock_search_engines.json
//   theme     light | dark | system
//   import    chrome | firefox | edge | chromium | html | skip
//   step      next | back
//   ready     the page has loaded and wants the state (changes nothing)

namespace bedrock {
namespace onboarding {

// The full state of the flow, as the JSON object the page reads.
std::string PageModelJson(const FirstRun& flow);

// Applies one message from the page. `field` is a key above, `value` its
// choice. Returns false — changing nothing — for an unknown field or a value
// that is not on offer, so a broken page cannot silently unset a setting
// (invariant 35).
bool ApplyPageChoice(FirstRun& flow, const std::string& field,
                     const std::string& value);

}  // namespace onboarding
}  // namespace bedrock

#endif  // BEDROCK_ONBOARDING_FIRST_RUN_PAGE_H_
