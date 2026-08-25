// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/network/remote_features.h"

#include <string>
#include <vector>

namespace bedrock {
namespace network {
namespace {

// clang-format off
const std::vector<RemoteFeature>& Table() {
  static const std::vector<RemoteFeature> kFeatures = {
    {"search_query", "search",
     "The search engine you picked during first run, directly. Bedrock runs no "
     "search proxy and no server of its own (item 93).",
     Operator::kSiteYouVisit, Status::kPolicyOnly, true, true,
     "Type a URL instead of a query, or choose a different engine in "
     "Settings > Search. Private windows use the same engine and send no "
     "extra identifier.",
     "Any engine in the list, or a custom search URL you enter yourself.",
     "docs/design/006-search-system.md"},

    {"search_suggestions", "search",
     "The engine you chose receives what you type, keystroke by keystroke, "
     "before you press Enter.",
     Operator::kSiteYouVisit, Status::kPolicyOnly, false, false,
     "Off unless you switch it on. Settings > Search > Suggestions, and it "
     "stays off in private windows whatever the setting says.",
     "Follows the search engine choice; there is no separate suggestion host.",
     "docs/design/006-search-system.md"},

    {"doh_resolver", "privacy/network",
     "A public DNS resolver you selected, over HTTPS or TLS. The default is "
     "your system resolver, so out of the box Bedrock contacts nobody new.",
     Operator::kThirdPartyYouChose, Status::kPolicyOnly, false, false,
     "It is off by default (DnsMode::kSystem). Settings > Privacy > DNS.",
     "Any RFC 8484 endpoint, including one you host: the presets are a "
     "convenience, not a fixed list.",
     "docs/design/017-dns.md"},

    {"filter_list_subscriptions", "privacy/tracker_blocker",
     "The list author's own URL, fetched by your machine. No Bedrock mirror, "
     "no Bedrock CDN, no rules service (invariant 1).",
     Operator::kThirdPartyYouChose, Status::kPolicyOnly, false, false,
     "Bedrock ships with no default subscription, so nothing is fetched until "
     "you add one. Settings > Shield > Filter lists.",
     "Any list URL you enter; a list can be removed without a browser update.",
     "docs/privacy/FILTER_LISTS.md"},

    {"extension_updates", "extensions",
     "The store an extension came from, for that extension's own updates. The "
     "privacy catalogue is a description of extensions, not a host for them.",
     Operator::kThirdPartyYouChose, Status::kPolicyOnly, false, false,
     "No extension is installed by default, so nothing is contacted. Remove "
     "the extension, or turn its updates off in Settings > Extensions.",
     "Whichever store the extension declares; Bedrock adds none of its own.",
     "docs/design/023-extensions.md"},

    {"tor_mode", "session",
     "The Tor network, through its own entry nodes, when you deliberately open "
     "a Tor window.",
     Operator::kThirdPartyYouChose, Status::kPolicyOnly, false, false,
     "Off until you open a Tor window; normal browsing never touches it.",
     "The Tor network only. Bedrock operates no relay, bridge or proxy.",
     "docs/design/019-browsing-modes.md"},

    {"update_check", "updater",
     "Nothing yet: this build never checks for updates. When it does, it will "
     "ask the release host for a signed manifest and send no profile data.",
     Operator::kThirdPartyYouChose, Status::kPolicyOnly, false, false,
     "Not implemented, so nothing to disable; when it lands it is a setting, "
     "and distribution packages can point it elsewhere or remove it.",
     "The release host is a build argument, so a distribution can host its own.",
     "docs/design/041-open-source.md"},
  };
  return kFeatures;
}
// clang-format on

}  // namespace

const std::vector<RemoteFeature>& AllRemoteFeatures() {
  return Table();
}

const RemoteFeature* FindRemoteFeature(const std::string& id) {
  for (const RemoteFeature& feature : Table()) {
    if (id == feature.id) return &feature;
  }
  return nullptr;
}

std::vector<std::string> RemoteFeatureProblems() {
  std::vector<std::string> problems;
  for (const RemoteFeature& feature : Table()) {
    const std::string id = feature.id;

    if (feature.op == Operator::kBedrockOperated) {
      problems.push_back(id + ": operated by Bedrock — item 94 allows no "
                              "cloud service of ours, opt-in or not");
    }
    if (std::string(feature.how_to_disable).empty()) {
      problems.push_back(id + ": no way to turn it off (item 95)");
    }
    if (std::string(feature.contacts).empty() ||
        std::string(feature.doc).empty()) {
      problems.push_back(id + ": a remote feature states whom it contacts and "
                              "where it is documented (item 95)");
    }
    if (feature.on_by_default && !feature.inherent) {
      problems.push_back(id + ": on by default without being part of your own "
                              "navigation (item 95: disabled by default)");
    }
    if (feature.status == Status::kPolicyOnly && feature.on_by_default &&
        !feature.inherent) {
      problems.push_back(id + ": not implemented, so it cannot be on");
    }
    if (feature.status == Status::kImplemented &&
        std::string(feature.replacement).empty()) {
      problems.push_back(id + ": implemented but tied to one endpoint "
                              "(item 95: replaceable)");
    }
  }
  return problems;
}

}  // namespace network
}  // namespace bedrock
