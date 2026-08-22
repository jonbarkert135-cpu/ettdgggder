// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/tracker_blocker/blocking_pipeline.h"

#include <cstddef>
#include <set>
#include <string>

namespace bedrock {
namespace blocking {
namespace {

using privacy::Control;
using privacy::Value;

// Parameters that exist only to carry an identifier across a click. Stripped
// from navigations (item 14, "link cleaning"). Kept deliberately short and
// conservative: removing a parameter a site needs breaks the click, and a
// broken click is a worse privacy outcome because the user retries in another
// browser.
const std::set<std::string>& TrackingParams() {
  static const std::set<std::string> kParams = {
      "fbclid",  "gclid",     "gclsrc",  "dclid",    "msclkid",
      "twclid",  "igshid",    "mc_eid",  "mkt_tok",  "yclid",
      "ttclid",  "wickedid",  "oly_enc_id", "oly_anon_id", "vero_id",
      "_openstat", "utm_source", "utm_medium", "utm_campaign", "utm_term",
      "utm_content", "utm_id", "utm_source_platform",
  };
  return kParams;
}

}  // namespace

BlockingPipeline::BlockingPipeline(FilterEngine* filters,
                                   TrackerHeuristic* heuristic,
                                   privacy::ProtectionController* controls)
    : filters_(filters), heuristic_(heuristic), controls_(controls) {}

BlockingPipeline::~BlockingPipeline() = default;

// static
const char* BlockingPipeline::ReasonString(Reason reason) {
  switch (reason) {
    case Reason::kShieldsDown:
      return "shields down for this site";
    case Reason::kAllowRule:
      return "allowed by rule";
    case Reason::kFilterList:
      return "filter list";
    case Reason::kFirstParty:
      return "first party";
    case Reason::kBehavioralTracker:
      return "detected as a tracker on this device";
    case Reason::kUserVerdict:
      return "your rule for this domain";
    case Reason::kScriptPolicy:
      return "scripts blocked for this site";
    case Reason::kThirdPartyCookiePolicy:
      return "third-party storage blocked";
    case Reason::kDefaultAllow:
      return "allowed";
  }
  return "allowed";
}

bool BlockingPipeline::TrackersBlocked(const Request& request) const {
  return controls_->Get(Control::kTrackers, request.top_host,
                        request.top_etld1) != Value::kAllow;
}

bool BlockingPipeline::SendPrivacySignals(const Request& request) const {
  return TrackersBlocked(request);
}

// static
std::string BlockingPipeline::CleanUrl(const std::string& url) {
  size_t query_at = url.find('?');
  if (query_at == std::string::npos) {
    return url;
  }
  size_t fragment_at = url.find('#', query_at);
  const std::string query =
      url.substr(query_at + 1, fragment_at == std::string::npos
                                   ? std::string::npos
                                   : fragment_at - query_at - 1);
  const std::string fragment =
      fragment_at == std::string::npos ? "" : url.substr(fragment_at);

  std::string kept;
  size_t start = 0;
  bool stripped = false;
  while (start <= query.size()) {
    size_t end = query.find('&', start);
    const std::string pair = query.substr(
        start, end == std::string::npos ? std::string::npos : end - start);
    if (!pair.empty()) {
      const std::string name = pair.substr(0, pair.find('='));
      if (TrackingParams().count(name) != 0) {
        stripped = true;
      } else {
        if (!kept.empty()) {
          kept += '&';
        }
        kept += pair;
      }
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  if (!stripped) {
    return url;
  }
  std::string cleaned = url.substr(0, query_at);
  if (!kept.empty()) {
    cleaned += '?' + kept;
  }
  return cleaned + fragment;
}

Decision BlockingPipeline::Evaluate(const Request& request) const {
  Decision decision;

  // Stage 0 — shields. If the user turned protection off for this site, no
  // later stage gets to override that. One switch, honoured everywhere.
  const bool ads_blocked =
      controls_->Get(Control::kAds, request.top_host, request.top_etld1) !=
      Value::kAllow;
  const bool trackers_blocked = TrackersBlocked(request);
  if (!ads_blocked && !trackers_blocked) {
    decision.reason = Reason::kShieldsDown;
    return decision;
  }

  // Stage 1 — filter lists (network rules, exceptions, redirects).
  const MatchResult match = filters_->Match(request);
  if (match.blocked) {
    decision.detail = match.rule;
    if (!match.redirect.empty()) {
      decision.action = Action::kRedirect;
      decision.reason = Reason::kFilterList;
      decision.redirect_url = FilterEngine::RedirectResource(match.redirect);
      return decision;
    }
    decision.action = Action::kBlock;
    decision.reason = Reason::kFilterList;
    return decision;
  }
  if (!match.rule.empty()) {
    // An exception rule matched: an explicit allow, and the heuristic does not
    // get to second-guess it.
    decision.reason = Reason::kAllowRule;
    decision.detail = match.rule;
    return decision;
  }

  // Stage 2 — party analysis. First-party requests are the site the user asked
  // for; the behavioral layer has no business judging them.
  if (!request.third_party()) {
    // ...but the site's own script policy still applies.
    if (request.type == ResourceType::kScript &&
        controls_->Get(Control::kScripts, request.top_host,
                       request.top_etld1) == Value::kBlock) {
      decision.action = Action::kBlock;
      decision.reason = Reason::kScriptPolicy;
      return decision;
    }
    decision.reason = Reason::kFirstParty;
    return decision;
  }

  // Stage 3 — script policy for third parties (kReduce = third-party scripts
  // only are blocked).
  if (request.type == ResourceType::kScript) {
    const Value scripts = controls_->Get(Control::kScripts, request.top_host,
                                         request.top_etld1);
    if (scripts == Value::kBlock || scripts == Value::kReduce) {
      decision.action = Action::kBlock;
      decision.reason = Reason::kScriptPolicy;
      return decision;
    }
  }

  // Stage 4 — behavioral classification (item 14).
  const Verdict verdict = heuristic_->Classify(request.etld1);
  if (verdict == Verdict::kBlock && trackers_blocked) {
    decision.action = Action::kBlock;
    decision.reason = Reason::kBehavioralTracker;
    decision.detail = request.etld1;
    return decision;
  }
  if (verdict == Verdict::kPartition) {
    decision.action = Action::kPartition;
    decision.reason = Reason::kBehavioralTracker;
    decision.detail = request.etld1;
    return decision;
  }
  if (verdict == Verdict::kAllow) {
    decision.reason = Reason::kUserVerdict;
    decision.detail = request.etld1;
    return decision;
  }

  // Stage 5 — cookie policy. Unknown third parties still load, but with
  // partitioned storage: the default is "may work, may not track".
  if (controls_->Get(Control::kCookies, request.top_host, request.top_etld1) !=
      Value::kAllow) {
    decision.action = Action::kPartition;
    decision.reason = Reason::kThirdPartyCookiePolicy;
    return decision;
  }
  return decision;
}

void BlockingPipeline::NoteStoredState(const Request& request, StateKind kind) {
  if (!request.third_party()) {
    return;
  }
  heuristic_->Observe(request.etld1, request.top_etld1, kind);
}

}  // namespace blocking
}  // namespace bedrock
