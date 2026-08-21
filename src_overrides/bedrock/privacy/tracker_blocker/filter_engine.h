// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_TRACKER_BLOCKER_FILTER_ENGINE_H_
#define BEDROCK_PRIVACY_TRACKER_BLOCKER_FILTER_ENGINE_H_

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// Filter engine — Adblock Plus / uBlock Origin filter syntax (roadmap item 12).
//
// Independently implemented from the publicly documented filter syntax. No uBO
// code is present: uBO is GPL-3.0 and Bedrock is MPL-2.0 (see
// THIRD_PARTY_NOTICES/ublock-origin.txt). List *data* is fetched from the list
// authors at runtime under their own terms.
//
// Speed comes from the index, not from cutting features. A URL is tokenized and
// only the filters sharing a token are examined, so a 300k-rule list costs a
// handful of candidate checks per request instead of 300k substring searches —
// the same idea uBO documents. Everything is arrays and interned strings; no
// per-request allocation on the hot path beyond the token vector.

namespace bedrock {
namespace blocking {

enum class ResourceType {
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

struct Request {
  std::string url;        // full URL, lowercased by the caller
  std::string host;       // request host
  std::string etld1;      // request eTLD+1
  std::string top_host;   // top-level document host (shields scope)
  std::string top_etld1;  // top-level document eTLD+1
  ResourceType type = ResourceType::kOther;

  bool third_party() const { return !etld1.empty() && etld1 != top_etld1; }
};

// One parsed network rule.
struct NetworkFilter {
  std::string pattern;      // with ||, |, ^ and * markers intact
  std::string raw;          // original line, for the UI and for export
  bool exception = false;   // @@
  bool important = false;   // $important beats exceptions
  bool anchor_domain = false;  // ||
  bool anchor_start = false;   // |http...
  bool anchor_end = false;     // ...|
  // Party constraint: unset = both.
  bool first_party_only = false;
  bool third_party_only = false;
  uint32_t type_mask = 0;   // 0 = any type
  std::vector<std::string> domains;      // $domain=a.com|b.com
  std::vector<std::string> not_domains;  // $domain=~a.com
  std::string redirect;     // $redirect=noopjs
  bool csp_only = false;    // parsed, not applied without a Chromium tree
};

// One parsed cosmetic rule.
struct CosmeticFilter {
  std::string selector;
  std::vector<std::string> domains;      // empty = generic
  std::vector<std::string> not_domains;
  bool exception = false;   // #@#
  bool procedural = false;  // :has-text(), :upward(), ...
};

struct MatchResult {
  bool blocked = false;
  std::string redirect;   // non-empty: serve this resource instead
  std::string rule;       // the raw rule that decided, for the shields panel
};

class FilterEngine {
 public:
  FilterEngine();
  ~FilterEngine();

  // Parses one filter list. Comments, empty lines and unsupported syntax are
  // skipped rather than fatal: real lists always contain rules we do not
  // implement, and dropping the whole list over one line is worse than
  // dropping the line. Returns the number of rules accepted.
  size_t AddList(const std::string& text);

  // A single rule from the user's dynamic/per-site rules. Unlike AddList
  // rules, these are included in ExportRules().
  bool AddRule(const std::string& line);

  // Removes a previously added rule by its exact text. Returns true if found.
  bool RemoveRule(const std::string& line);

  // Network matching. Exceptions win, $important wins over exceptions.
  MatchResult Match(const Request& request) const;

  // Cosmetic selectors that apply to a document host, generic + host-specific,
  // minus #@# exceptions. Procedural selectors are returned separately because
  // they need the JS-side evaluator, not the CSS injector.
  struct Cosmetics {
    std::vector<std::string> selectors;
    std::vector<std::string> procedural;
  };
  Cosmetics CosmeticsFor(const std::string& host) const;

  // Built-in neutered resources for $redirect (roadmap item 12). Returns an
  // empty string for unknown names.
  static std::string RedirectResource(const std::string& name);

  // Rules added via AddRule, as a filter list. Round-trips through AddList.
  std::string ExportRules() const;

  size_t network_rule_count() const { return filters_.size(); }
  size_t cosmetic_rule_count() const { return cosmetics_.size(); }

 private:
  bool ParseRule(const std::string& line);
  struct TokenCandidate {
    uint32_t hash = 0;
    size_t length = 0;
  };
  // All word-aligned tokens a rule could be indexed under.
  static void CollectTokens(const NetworkFilter& filter,
                            std::vector<TokenCandidate>* out);
  // Rebuilds the token index, assigning each rule to its rarest token.
  void Reindex();
  bool Matches(const NetworkFilter& filter, const Request& request) const;

  std::vector<NetworkFilter> filters_;
  std::vector<CosmeticFilter> cosmetics_;
  // token hash -> filter indices. Filters with no usable token land in
  // untokenized_ and are checked on every request, so parsing keeps that set
  // small on purpose.
  std::unordered_map<uint32_t, std::vector<uint32_t>> index_;
  std::vector<uint32_t> untokenized_;
  // Kept so RemoveRule() can rebuild. Lists are text as fetched; user rules are
  // the dynamic/per-site rules the user added, and only those are exported.
  std::vector<std::string> lists_;
  std::set<std::string> user_rules_;
};

}  // namespace blocking
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_TRACKER_BLOCKER_FILTER_ENGINE_H_
