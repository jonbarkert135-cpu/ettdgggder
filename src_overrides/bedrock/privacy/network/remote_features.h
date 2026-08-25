// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_NETWORK_REMOTE_FEATURES_H_
#define BEDROCK_PRIVACY_NETWORK_REMOTE_FEATURES_H_

#include <string>
#include <vector>

// Every way this browser can talk to a server that is not the page you asked
// for (roadmap items 94 and 95).
//
// Item 94 bans the hidden cloud: no cloud config, no cloud personalisation, no
// cloud profile, no remote fingerprint database, no remote rules service, no
// server-side assistant. Item 95 allows remote features to exist, on four
// conditions — optional, off by default, replaceable, documented — and adds the
// one that matters most: **the browser must be complete without them.**
//
// Both are easy to state and easy to violate a year later, by one commit that
// adds a "quick" fetch to a config endpoint. So the rule is written as data:
// every remote interaction the design permits is a row in this table, and
// `scripts/check_remote_features.py` fails the build when a module gains
// networking machinery without a row, when a row is missing a way to turn it
// off, or when anything here would be operated by us.
//
// Two things this table is careful not to claim:
//
//   1. **Nothing here is a Bedrock service.** There is no bedrock.example
//      endpoint, no list mirror, no telemetry sink, no sync server. The operator
//      of every remote interaction is either the site you navigated to or a
//      third party *you* picked, and `Operator::kBedrockOperated` exists in this
//      enum only so the validator can reject it.
//   2. **Most rows are not implemented yet** (`Status::kPolicyOnly`). This
//      overlay contains no network stack code at all — the audit of 2026-08-25
//      says so plainly and item 90 forbids dressing a policy up as a feature.
//      The table therefore describes the only remote interactions that *may* be
//      built, and the gate holds the code to it.

namespace bedrock {
namespace network {

enum class Operator {
  kSiteYouVisit,          // the page you navigated to, or the engine you chose
  kThirdPartyYouChose,    // a resolver, list author or store the user selected
  kBedrockOperated,       // never legal (item 94); present so it can be rejected
};

enum class Status {
  kPolicyOnly,   // described here, not built: no code performs this request
  kImplemented,  // code exists and can issue the request
};

struct RemoteFeature {
  const char* id;
  const char* module;        // source directory under src_overrides/bedrock/
  const char* contacts;      // whose server, in the user's own terms
  Operator op;
  Status status;
  bool on_by_default;
  bool inherent;             // the user's own navigation, not a background service
  const char* how_to_disable;
  const char* replacement;   // how the endpoint can be swapped, "" if it cannot
  const char* doc;           // path from the repository root
};

// The whole table, in the order the Privacy Center shows it.
const std::vector<RemoteFeature>& AllRemoteFeatures();

const RemoteFeature* FindRemoteFeature(const std::string& id);

// Rules 94 and 95, in one place so the host test and the gate agree:
//   * no feature is operated by us;
//   * every feature says how to turn it off, and none is required for the
//     browser to work;
//   * only an inherent interaction (your own search, your own navigation) may
//     be on by default;
//   * an unimplemented feature cannot be on by default, and cannot claim an
//     endpoint it does not contact;
//   * every feature points at a document.
// Returns one sentence per problem; empty means the table is legal.
std::vector<std::string> RemoteFeatureProblems();

}  // namespace network
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_NETWORK_REMOTE_FEATURES_H_
