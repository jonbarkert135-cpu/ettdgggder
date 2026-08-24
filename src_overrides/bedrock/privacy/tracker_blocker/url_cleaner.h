// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_TRACKER_BLOCKER_URL_CLEANER_H_
#define BEDROCK_PRIVACY_TRACKER_BLOCKER_URL_CLEANER_H_

#include <string>
#include <vector>

// Link cleaning and redirect debouncing (feature `query_param_stripping`).
//
// Two related habits of the ad ecosystem, one component:
//
//   1. Click identifiers pasted into the query string (`?fbclid=…`). They are
//      not needed to reach the page; they exist to join a click to a profile.
//   2. Redirect hops (`https://out.example/?url=https%3A%2F%2Freal.site%2F`).
//      The hop's only job is to see the click. Following the wrapped target
//      directly — "debouncing" — removes a request the user never asked for.
//
// This is the ONE place either happens. `BlockingPipeline::CleanUrl()` calls
// it; nothing else keeps a second parameter table (MEMORY.md non-negotiable 4).
//
// Judgement calls that are deliberate, not oversights:
//
// * Cleaning is refused for subresources. A tracking-looking parameter on a
//   script or XHR is frequently load-bearing, and a half-loaded page sends the
//   user to another browser — a worse privacy outcome than one stripped id.
// * Only exact parameter names from the table are removed; no prefix matching,
//   no heuristics on the value. A false positive breaks a checkout.
// * Unwrapping only accepts an absolute http(s) target, and only from a known
//   (host, parameter) rule. Anything else — relative, `javascript:`, `data:`,
//   an unlisted host — is left alone: a cleaner that can retarget navigation
//   is an open-redirect engine.
// * The fragment is never rewritten. It never leaves the browser on its own,
//   and sites keep application state in it.

namespace bedrock {
namespace blocking {

// Where the URL is about to be used. Determines whether cleaning may apply.
enum class UrlUse {
  kTopLevelNavigation,  // a click, an omnibox entry, a redirect target
  kCopiedLink,          // "Copy clean link" in the context menu
  kSubresource,         // never cleaned, see above
};

struct CleanResult {
  std::string url;                        // cleaned URL (input if unchanged)
  std::vector<std::string> stripped;      // parameter names removed, in order
  int unwrapped_hops = 0;                 // redirect wrappers skipped
  bool changed() const {
    return !stripped.empty() || unwrapped_hops > 0;
  }
};

class UrlCleaner {
 public:
  // Unwrap known redirectors first, then strip tracking parameters from the
  // URL that is actually going to be requested. At most `kMaxHops` wrappers
  // are followed, so a redirector that points at itself terminates.
  static CleanResult Clean(const std::string& url, UrlUse use);

  // True if `name` is a tracking parameter. Exposed so the privacy panel can
  // explain a stripped parameter without a copy of the table.
  static bool IsTrackingParam(const std::string& name);

  static constexpr int kMaxHops = 3;
};

}  // namespace blocking
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_TRACKER_BLOCKER_URL_CLEANER_H_
