// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/integration/network_hook.h"

#include <algorithm>
#include <mutex>

#include "bedrock/privacy/core/protection_controller.h"
#include "bedrock/privacy/network/request_headers.h"
#include "bedrock/privacy/tracker_blocker/blocking_pipeline.h"
#include "bedrock/privacy/tracker_blocker/filter_engine.h"
#include "bedrock/privacy/tracker_blocker/tracker_heuristic.h"

namespace bedrock {
namespace integration {
namespace {

// Bedrock's own starter list. Every line is a third-party host whose only
// purpose is analytics or ad delivery, written by us against the syntax in
// privacy/tracker_blocker/filter_engine.h. It is not EasyList and not derived
// from any imported list, so it carries no third-party licence — see
// docs/licensing/PROVENANCE.md.
//
// $third-party on every rule is deliberate: a site that serves its own
// analytics from its own domain is the site the user asked for, and blocking it
// would be a functional change the user did not ask for.
constexpr char kBuiltInList[] =
    "! Bedrock starter list (authored by Bedrock, MPL-2.0)\n"
    "||google-analytics.com^$third-party\n"
    "||googletagmanager.com^$third-party\n"
    "||doubleclick.net^$third-party\n"
    "||googlesyndication.com^$third-party\n"
    "||googleadservices.com^$third-party\n"
    "||connect.facebook.net^$third-party\n"
    "||scorecardresearch.com^$third-party\n"
    "||adnxs.com^$third-party\n"
    "||criteo.com^$third-party\n"
    "||criteo.net^$third-party\n"
    "||taboola.com^$third-party\n"
    "||outbrain.com^$third-party\n"
    "||hotjar.com^$third-party\n"
    "||amplitude.com^$third-party\n"
    "||branch.io^$third-party\n"
    "||bat.bing.com^$third-party\n"
    "||ads-twitter.com^$third-party\n"
    "||adroll.com^$third-party\n";

blocking::ResourceType ToResourceType(RequestKind kind) {
  switch (kind) {
    case RequestKind::kDocument:
      return blocking::ResourceType::kDocument;
    case RequestKind::kSubdocument:
      return blocking::ResourceType::kSubdocument;
    case RequestKind::kScript:
      return blocking::ResourceType::kScript;
    case RequestKind::kStylesheet:
      return blocking::ResourceType::kStylesheet;
    case RequestKind::kImage:
      return blocking::ResourceType::kImage;
    case RequestKind::kFont:
      return blocking::ResourceType::kFont;
    case RequestKind::kMedia:
      return blocking::ResourceType::kMedia;
    case RequestKind::kXhr:
      return blocking::ResourceType::kXhr;
    case RequestKind::kWebsocket:
      return blocking::ResourceType::kWebsocket;
    case RequestKind::kPing:
      return blocking::ResourceType::kPing;
    case RequestKind::kOther:
      return blocking::ResourceType::kOther;
  }
  return blocking::ResourceType::kOther;
}

// One pipeline per process, with the shipped protection defaults (ads and
// trackers blocked). The user's own settings and per-site exceptions live in a
// profile, which the network service has no access to yet — that plumbing is
// its own item, and until it exists this hook applies defaults only. That is
// stated in the startup line rather than left for someone to discover.
struct Engine {
  blocking::FilterEngine filters;
  blocking::TrackerHeuristic heuristic;
  privacy::ProtectionController controls;
  blocking::BlockingPipeline pipeline;
  net::RequestHeaderPolicy headers;
  std::size_t rules_accepted = 0;

  Engine() : pipeline(&filters, &heuristic, &controls), headers(&controls) {
    rules_accepted = filters.AddList(kBuiltInList);
  }
};

// Every hint Bedrock knows, so a header on the wire can be recognised by name.
constexpr net::Hint kAllHints[] = {
    net::Hint::kUa,          net::Hint::kUaMobile,
    net::Hint::kUaPlatform,  net::Hint::kUaArch,
    net::Hint::kUaBitness,   net::Hint::kUaModel,
    net::Hint::kUaFullVersionList, net::Hint::kUaPlatformVersion,
    net::Hint::kDeviceMemory, net::Hint::kDpr,
    net::Hint::kViewportWidth, net::Hint::kWidth,
    net::Hint::kRtt,         net::Hint::kDownlink,
    net::Hint::kEct,         net::Hint::kSaveData,
    net::Hint::kPrefersColorScheme,
};

std::string LowerAscii(const std::string& text) {
  std::string out = text;
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return out;
}

const char* ScopeWord(net::ReferrerScope scope) {
  switch (scope) {
    case net::ReferrerScope::kFullUrl:
      return "full-url";
    case net::ReferrerScope::kOriginOnly:
      return "origin-only";
    case net::ReferrerScope::kNone:
      return "none";
  }
  return "unchanged";
}

Engine& GetEngine() {
  static std::once_flag once;
  static Engine* engine = nullptr;
  std::call_once(once, [] { engine = new Engine(); });
  return *engine;
}

const char* ReasonWord(blocking::Reason reason) {
  switch (reason) {
    case blocking::Reason::kFilterList:
      return "filter-list";
    case blocking::Reason::kBehavioralTracker:
      return "behavioral";
    case blocking::Reason::kScriptPolicy:
      return "script-policy";
    case blocking::Reason::kCnameUncloaked:
      return "cname";
    case blocking::Reason::kThirdPartyCookiePolicy:
      return "cookie-policy";
    case blocking::Reason::kAllowRule:
      return "allow-rule";
    case blocking::Reason::kFirstParty:
      return "first-party";
    case blocking::Reason::kShieldsDown:
      return "shields-down";
    case blocking::Reason::kUserVerdict:
      return "user-verdict";
    case blocking::Reason::kDefaultAllow:
      return "allow";
  }
  return "allow";
}

}  // namespace

NetworkDecision DecideRequest(const NetworkRequest& request) {
  NetworkDecision decision;
  // No top-level document means the network service could not tell us which
  // page this belongs to (browser-initiated fetches, service workers before a
  // client is known). Blocking on a guess is worse than not blocking.
  if (request.top_host.empty() || request.host.empty()) {
    decision.reason = "no-top-frame";
    return decision;
  }

  blocking::Request pipeline_request;
  pipeline_request.url = request.url;
  pipeline_request.host = request.host;
  pipeline_request.etld1 = request.etld1;
  pipeline_request.top_host = request.top_host;
  pipeline_request.top_etld1 = request.top_etld1;
  pipeline_request.type = ToResourceType(request.kind);

  const blocking::Decision verdict =
      GetEngine().pipeline.Evaluate(pipeline_request);
  // kPartition and kRedirect need storage partitioning and a resource server
  // that this seam does not have yet; treating them as blocks would break sites
  // and treating them as "handled" would be a lie. They load, and the reason
  // says which stage wanted more than we can do.
  decision.blocked = verdict.action == blocking::Action::kBlock;
  decision.reason = ReasonWord(verdict.reason);
  decision.detail = verdict.detail;
  return decision;
}

OutgoingHeaderDecision DecideHeaders(const OutgoingHeaderRequest& request) {
  OutgoingHeaderDecision decision;
  if (request.top_host.empty() || request.target_host.empty()) {
    return decision;
  }

  Engine& engine = GetEngine();

  // --- Referrer. Chromium has already applied its own policy; Bedrock may only
  // shorten what is left. The initiator is the referrer Chromium computed,
  // which is exactly the document whose URL would go on the wire.
  net::OutgoingRequest outgoing;
  outgoing.initiator_url = request.referrer;
  outgoing.initiator_host = request.top_host;
  outgoing.initiator_etld1 = request.top_etld1;
  outgoing.target_url = request.target_url;
  outgoing.target_host = request.target_host;
  outgoing.target_etld1 = request.target_etld1;
  outgoing.navigation = request.navigation;

  if (!request.referrer.empty()) {
    const net::ReferrerScope scope = engine.headers.ScopeFor(outgoing);
    const std::string wanted = engine.headers.ReferrerFor(outgoing);
    // Never lengthen: a longer string than Chromium's means our floor would be
    // adding information, which is the one thing this seam must not do.
    if (wanted.size() < request.referrer.size() || wanted.empty()) {
      decision.referrer_changed = true;
      decision.referrer = wanted;
      decision.referrer_scope = ScopeWord(scope);
    }
  }

  // --- Client hints. Nothing here asks the site what it wants: the accepted
  // list is the hints already on the request, and the policy answers which of
  // them this level allows. Everything else is removed by name.
  std::vector<net::Hint> present;
  for (const net::Hint hint : kAllHints) {
    const std::string name = LowerAscii(net::HintHeaderName(hint));
    for (const std::string& header : request.present_headers) {
      if (LowerAscii(header) == name) {
        present.push_back(hint);
        break;
      }
    }
  }
  if (!present.empty()) {
    const std::vector<net::Hint> allowed =
        engine.headers.HintsFor(outgoing, present);
    for (const net::Hint hint : present) {
      if (std::find(allowed.begin(), allowed.end(), hint) == allowed.end()) {
        decision.remove_headers.push_back(net::HintHeaderName(hint));
      }
    }
  }
  return decision;
}

const char* BuiltInFilterList() {
  return kBuiltInList;
}

std::size_t BuiltInRuleCount() {
  return GetEngine().rules_accepted;
}

std::string NetworkHookStartupLine() {
  return "network hook: live, " + std::to_string(BuiltInRuleCount()) +
         " built-in rules, shipped defaults only (no profile settings yet)";
}

}  // namespace integration
}  // namespace bedrock
