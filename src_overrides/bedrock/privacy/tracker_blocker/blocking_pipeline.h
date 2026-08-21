// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_TRACKER_BLOCKER_BLOCKING_PIPELINE_H_
#define BEDROCK_PRIVACY_TRACKER_BLOCKER_BLOCKING_PIPELINE_H_

#include <string>

#include "bedrock/privacy/tracker_blocker/filter_engine.h"
#include "bedrock/privacy/tracker_blocker/tracker_heuristic.h"
#include "bedrock/privacy/core/protection_controller.h"

// The one blocking pipeline (roadmap item 13).
//
// Exactly one component decides whether a request is made. Filter lists, the
// behavioral heuristic, the shields settings and the cookie policy are *stages*
// of this pipeline, not independent blockers stacked on top of each other.
// Four blockers that each veto a request produce four sources of truth, a
// shields panel that cannot explain itself, and rules that silently fight one
// another — so there is a single entry point, `Evaluate()`, and everything else
// contributes to it.
//
// Stage order (roadmap item 14's diagram, with the party check moved ahead of
// the heuristic because the heuristic only ever applies to third parties):
//
//   request -> shields -> filter lists -> party analysis -> heuristic
//           -> cookie/script policy -> Allow | Partition | Block

namespace bedrock {
namespace blocking {

enum class Action {
  kAllow,
  kPartition,  // loads, but with no access to cross-site state
  kRedirect,   // replaced with a neutered resource
  kBlock,
};

enum class Reason {
  kShieldsDown,
  kAllowRule,           // @@ exception or user allow rule
  kFilterList,
  kFirstParty,
  kBehavioralTracker,   // learned locally, item 14
  kUserVerdict,
  kScriptPolicy,
  kThirdPartyCookiePolicy,
  kDefaultAllow,
};

struct Decision {
  Action action = Action::kAllow;
  Reason reason = Reason::kDefaultAllow;
  std::string detail;        // rule text or domain, for the panel
  std::string redirect_url;  // when action == kRedirect

  bool allowed() const { return action != Action::kBlock; }
};

class BlockingPipeline {
 public:
  BlockingPipeline(FilterEngine* filters,
                   TrackerHeuristic* heuristic,
                   privacy::ProtectionController* controls);
  ~BlockingPipeline();

  // The single decision point for every network request.
  Decision Evaluate(const Request& request) const;

  // Called when an allowed third party turned out to store identifying state.
  // This is the only path into the heuristic's learning, so learning can never
  // disagree with what the pipeline actually let through.
  void NoteStoredState(const Request& request, StateKind kind);

  // Global Privacy Control / Do Not Track (item 14). Sent unless the user turned
  // tracker protection off for this site — a signal that contradicts the user's
  // own setting is worse than no signal.
  bool SendPrivacySignals(const Request& request) const;

  // Link cleaning: strips known tracking parameters from a URL (item 14).
  // Returns the cleaned URL, unchanged if there was nothing to strip.
  static std::string CleanUrl(const std::string& url);

  static const char* ReasonString(Reason reason);

 private:
  bool TrackersBlocked(const Request& request) const;

  FilterEngine* filters_;
  TrackerHeuristic* heuristic_;
  privacy::ProtectionController* controls_;
};

}  // namespace blocking
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_TRACKER_BLOCKER_BLOCKING_PIPELINE_H_
