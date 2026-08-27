// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_INTEGRATION_NETWORK_HOOK_H_
#define BEDROCK_INTEGRATION_NETWORK_HOOK_H_

#include <cstddef>
#include <string>

// The seam between the blocking pipeline and Chromium's network service
// (phase 7). Everything above this file is Bedrock policy; the only thing the
// patched Chromium file does is fill in a request and obey the answer.
//
// Why this exists: the pipeline, the filter engine, the heuristic and the
// shields controller were all host-tested long before anything called them, so
// a built browser blocked nothing. This is the call site that makes the
// pipeline real for network requests, and the reason we can now measure
// blocking in a running binary instead of only asserting it in a test.
//
// Rules of this seam, mirroring integration/startup.h:
//   * no Chromium type appears here, so the host tests keep building with g++;
//   * the patch computes eTLD+1 with Chromium's registry list and passes it in,
//     because guessing registrable domains is how blockers get sites wrong;
//   * the answer is advisory data, never an action — the patch performs it.
//
// The engine is process-wide (one filter list per network service) and the
// accessor is safe to call from any thread: it builds the pipeline once under a
// mutex and then only reads it.

namespace bedrock {
namespace integration {

// Chromium's RequestDestination is mapped to this by the patch; keeping our own
// spelling means an upstream enum change is a compile error in one place.
enum class RequestKind {
  kDocument,
  kSubdocument,
  kScript,
  kStylesheet,
  kImage,
  kFont,
  kMedia,
  kXhr,
  kWebsocket,
  kPing,
  kOther,
};

struct NetworkRequest {
  std::string url;        // full URL as the renderer asked for it
  std::string host;       // request host, no port
  std::string etld1;      // registrable domain of `host`, from Chromium
  std::string top_host;   // top-level document host, empty if unknown
  std::string top_etld1;  // registrable domain of `top_host`, from Chromium
  RequestKind kind = RequestKind::kOther;
};

struct NetworkDecision {
  bool blocked = false;
  // Stable, log-safe words: "filter-list", "behavioral", "script-policy",
  // "cname", "allow". Used by the [bedrock] log line and, later, the panel.
  std::string reason = "allow";
  // The rule or domain that decided, for "why was this blocked?".
  std::string detail;
};

// The single question the network service asks. A request with an empty
// `top_host` (browser-initiated: updates, navigations, favicons) is treated as
// first party and always allowed — a blocker that cannot tell which page it is
// protecting must not guess.
NetworkDecision DecideRequest(const NetworkRequest& request);

// The starter list this build ships, authored by Bedrock: a handful of
// unambiguous third-party analytics and ad hosts, no imported list. It exists
// so the seam can be measured end to end; list management (subscriptions,
// updates, the user's own rules) is a later item, and until then this is
// deliberately small rather than pretending to be complete.
const char* BuiltInFilterList();

// Rules the engine accepted from BuiltInFilterList(). Zero means the seam is
// live but blocking nothing, which the startup log says out loud.
std::size_t BuiltInRuleCount();

// Log line the network service prints once, so a running browser states what it
// will block instead of leaving it to documentation.
std::string NetworkHookStartupLine();

}  // namespace integration
}  // namespace bedrock

#endif  // BEDROCK_INTEGRATION_NETWORK_HOOK_H_
