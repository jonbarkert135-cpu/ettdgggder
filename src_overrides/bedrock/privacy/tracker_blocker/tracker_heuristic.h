// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_TRACKER_BLOCKER_TRACKER_HEURISTIC_H_
#define BEDROCK_PRIVACY_TRACKER_BLOCKER_TRACKER_HEURISTIC_H_

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

// Behavioral tracker detection (roadmap item 14).
//
// The idea is EFF's, from Privacy Badger's public description: a third party is
// not a tracker because a list says so, but because it is *observed* storing
// identifying state across several unrelated first-party sites. Independently
// implemented from that description — no Privacy Badger code, no EFF lists, no
// yellow list (GPL-3.0, see THIRD_PARTY_NOTICES/privacy-badger.txt).
//
// Three properties are non-negotiable, and each has a test:
//   * Local only. The table lives in the profile. Nothing is uploaded, ever —
//     Bedrock has no server to upload it to (roadmap item 4).
//   * First-party observations are never recorded. Learning from the site the
//     user is actually visiting would flag every popular site as a tracker.
//   * The stored table is domains and counters, not history: it records that
//     `tracker.example` was seen on three sites, never which sites, once the
//     threshold is reached.

namespace bedrock {
namespace blocking {

// What a third party did that makes it identifiable across sites.
enum class StateKind {
  kCookie,
  kLocalStorage,
  kIndexedDb,
  kHighEntropyFingerprint,  // canvas/audio/font probing, from the FP shims
  kRedirectTracking,        // bounce through a link-decoration redirect
};

enum class Verdict {
  kUnknown,    // not enough evidence yet
  kAllow,      // explicitly allowed (user, or honours DNT/GPC)
  kPartition,  // may load, gets no cross-site state
  kBlock,      // request is not made
};

class TrackerHeuristic {
 public:
  // Default threshold, following EFF's published heuristic: state on three
  // distinct first-party sites.
  static constexpr int kDefaultThreshold = 3;

  TrackerHeuristic();
  ~TrackerHeuristic();

  // Records that `third_party` stored identifying state while the user was on
  // `first_party`. Same-party observations and repeats are ignored. Returns the
  // verdict after the observation.
  Verdict Observe(const std::string& third_party,
                  const std::string& first_party,
                  StateKind kind);

  // Verdict without recording anything (the pipeline's read path).
  Verdict Classify(const std::string& third_party) const;

  // How many distinct first parties we have seen the domain on. Exposed for the
  // "why is this blocked?" panel; capped at the threshold, see .cc.
  int SiteCount(const std::string& third_party) const;

  // Domains that break sites when blocked outright (payment, login, CDN with
  // auth) are partitioned instead. The user manages this list; Bedrock ships a
  // small default set. There is no remote yellow list to download.
  void SetPartitionOnly(const std::string& domain, bool partition_only);

  // The site sent GPC/DNT-honouring signals we choose to trust: stop learning
  // about it and let it through (EFF's rule — a tracker that promises to stop
  // tracking is given the chance to keep the promise).
  void SetHonoursPrivacySignals(const std::string& domain, bool honours);

  // User overrides. These beat everything the heuristic learned.
  void SetUserVerdict(const std::string& domain, Verdict verdict);
  void ClearUserVerdict(const std::string& domain);

  void Forget(const std::string& domain);
  void Clear();

  // Import/export of the learned table, so the user can inspect, back up or
  // move it. Line format: "domain<TAB>count<TAB>flags".
  std::string Export() const;
  bool Import(const std::string& text);

  void set_threshold(int threshold) { threshold_ = threshold; }

 private:
  struct Entry {
    std::set<std::string> first_parties;  // cleared once the threshold is hit
    int count = 0;
    bool partition_only = false;
    bool honours_signals = false;
    Verdict user_verdict = Verdict::kUnknown;
  };

  Entry* Find(const std::string& domain);
  const Entry* Find(const std::string& domain) const;
  Verdict VerdictFor(const Entry& entry) const;

  std::map<std::string, Entry> entries_;
  int threshold_ = kDefaultThreshold;
};

}  // namespace blocking
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_TRACKER_BLOCKER_TRACKER_HEURISTIC_H_
