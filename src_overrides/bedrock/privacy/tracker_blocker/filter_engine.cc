// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/tracker_blocker/filter_engine.h"

#include <algorithm>

namespace bedrock {
namespace blocking {
namespace {

constexpr size_t kNpos = std::string::npos;

uint32_t Fnv1a(const char* data, size_t length) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < length; ++i) {
    hash ^= static_cast<unsigned char>(data[i]);
    hash *= 16777619u;
  }
  return hash;
}

bool IsTokenChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '%';
}

// A '^' in a filter matches any separator: everything that is not part of a
// name, plus end-of-URL.
bool IsSeparator(char c) {
  return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' ||
           c == '%');
}

std::vector<std::string> Split(const std::string& text, char delimiter) {
  std::vector<std::string> parts;
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find(delimiter, start);
    if (end == kNpos) {
      parts.push_back(text.substr(start));
      break;
    }
    parts.push_back(text.substr(start, end - start));
    start = end + 1;
  }
  return parts;
}

std::string ToLower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return text;
}

uint32_t TypeBit(ResourceType type) {
  return 1u << static_cast<int>(type);
}

// $option name -> resource type, for the options we can enforce.
bool TypeForOption(const std::string& name, ResourceType* out) {
  static const std::map<std::string, ResourceType> kTypes = {
      {"document", ResourceType::kDocument},
      {"subdocument", ResourceType::kSubdocument},
      {"frame", ResourceType::kSubdocument},
      {"script", ResourceType::kScript},
      {"stylesheet", ResourceType::kStylesheet},
      {"css", ResourceType::kStylesheet},
      {"image", ResourceType::kImage},
      {"font", ResourceType::kFont},
      {"media", ResourceType::kMedia},
      {"xmlhttprequest", ResourceType::kXhr},
      {"xhr", ResourceType::kXhr},
      {"websocket", ResourceType::kWebsocket},
      {"ping", ResourceType::kPing},
      {"beacon", ResourceType::kPing},
      {"other", ResourceType::kOther},
  };
  auto it = kTypes.find(name);
  if (it == kTypes.end()) {
    return false;
  }
  *out = it->second;
  return true;
}

// Matches one wildcard-free pattern segment at `pos`, honouring '^'.
// Returns the position just past the match, or kNpos.
size_t MatchSegmentAt(const std::string& segment, const std::string& url,
                      size_t pos) {
  for (char c : segment) {
    if (c == '^') {
      // '^' also matches end-of-URL, and consumes nothing there.
      if (pos == url.size()) {
        return pos;
      }
      if (!IsSeparator(url[pos])) {
        return kNpos;
      }
      ++pos;
      continue;
    }
    if (pos >= url.size() || url[pos] != c) {
      return kNpos;
    }
    ++pos;
  }
  return pos;
}

// Finds `segment` at or after `from`. Plain substring search unless the segment
// contains '^', in which case we walk candidate positions.
size_t FindSegment(const std::string& segment, const std::string& url,
                   size_t from, size_t* match_end) {
  if (segment.empty()) {
    *match_end = from;
    return from;
  }
  if (segment.find('^') == kNpos) {
    size_t at = url.find(segment, from);
    if (at == kNpos) {
      return kNpos;
    }
    *match_end = at + segment.size();
    return at;
  }
  for (size_t at = from; at <= url.size(); ++at) {
    size_t end = MatchSegmentAt(segment, url, at);
    if (end != kNpos) {
      *match_end = end;
      return at;
    }
  }
  return kNpos;
}

// True if `host` is `domain` or a subdomain of it.
bool HostMatches(const std::string& host, const std::string& domain) {
  if (host == domain) {
    return true;
  }
  return host.size() > domain.size() &&
         host.compare(host.size() - domain.size(), domain.size(), domain) == 0 &&
         host[host.size() - domain.size() - 1] == '.';
}

// Where the host starts in "scheme://host/path". kNpos if there is no authority.
size_t HostStart(const std::string& url) {
  size_t at = url.find("://");
  return at == kNpos ? kNpos : at + 3;
}

}  // namespace

FilterEngine::FilterEngine() = default;
FilterEngine::~FilterEngine() = default;

// static
std::string FilterEngine::RedirectResource(const std::string& name) {
  // Neutered stand-ins: blocking a script outright often breaks the page,
  // while an inert replacement lets it continue. Written from scratch.
  static const std::map<std::string, std::string> kResources = {
      {"noopjs", "data:application/javascript;base64,"},  // empty script
      {"noop.js", "data:application/javascript;base64,"},
      {"noopframe", "data:text/html,"},
      {"noop.html", "data:text/html,"},
      {"nooptext", "data:text/plain,"},
      {"noop.txt", "data:text/plain,"},
      {"1x1.gif",
       "data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEA"
       "AAIBRAA7"},
      {"2x2.png",
       "data:image/png;base64,"
       "iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAQAAAD/ITsRAAAADklEQVQI12P4//8/AwAI/"
       "AL+p5qgoAAAAABJRU5ErkJggg=="},
  };
  auto it = kResources.find(name);
  return it == kResources.end() ? std::string() : it->second;
}

size_t FilterEngine::AddList(const std::string& text) {
  lists_.push_back(text);
  size_t accepted = 0;
  for (const std::string& line : Split(text, '\n')) {
    std::string rule = line;
    while (!rule.empty() && (rule.back() == '\r' || rule.back() == ' ')) {
      rule.pop_back();
    }
    size_t first = rule.find_first_not_of(' ');
    if (first == kNpos) {
      continue;
    }
    rule = rule.substr(first);
    // Comments and list metadata.
    if (rule[0] == '!' || rule[0] == '#' || rule[0] == '[') {
      // '#' alone is a comment, but '##' / '#@#' are cosmetic rules.
      if (rule.compare(0, 2, "##") != 0 && rule.compare(0, 3, "#@#") != 0) {
        continue;
      }
    }
    if (ParseRule(rule)) {
      ++accepted;
    }
  }
  Reindex();
  return accepted;
}

bool FilterEngine::AddRule(const std::string& line) {
  if (line.empty() || !ParseRule(line)) {
    return false;
  }
  user_rules_.insert(line);
  Reindex();
  return true;
}

bool FilterEngine::ParseRule(const std::string& line) {
  std::string rule = line;
  if (rule.empty()) {
    return false;
  }

  // ---- cosmetic: domains##selector / domains#@#selector ----
  size_t hash = rule.find("##");
  size_t hash_exception = rule.find("#@#");
  if (hash != kNpos || hash_exception != kNpos) {
    bool exception = hash_exception != kNpos &&
                     (hash == kNpos || hash_exception < hash);
    size_t at = exception ? hash_exception : hash;
    size_t skip = exception ? 3 : 2;
    CosmeticFilter filter;
    filter.exception = exception;
    filter.selector = rule.substr(at + skip);
    if (filter.selector.empty()) {
      return false;
    }
    // Procedural selectors need the JS evaluator, not a CSS rule.
    filter.procedural = filter.selector.find(":has-text(") != kNpos ||
                        filter.selector.find(":has(") != kNpos ||
                        filter.selector.find(":upward(") != kNpos ||
                        filter.selector.find(":xpath(") != kNpos ||
                        filter.selector.find(":matches-css") != kNpos;
    for (const std::string& domain : Split(rule.substr(0, at), ',')) {
      if (domain.empty()) {
        continue;
      }
      if (domain[0] == '~') {
        filter.not_domains.push_back(ToLower(domain.substr(1)));
      } else {
        filter.domains.push_back(ToLower(domain));
      }
    }
    cosmetics_.push_back(std::move(filter));
    return true;
  }

  // ---- network ----
  NetworkFilter filter;
  filter.raw = line;
  if (rule.compare(0, 2, "@@") == 0) {
    filter.exception = true;
    rule = rule.substr(2);
  }
  size_t dollar = rule.rfind('$');
  if (dollar != kNpos) {
    for (const std::string& option : Split(rule.substr(dollar + 1), ',')) {
      bool negated = !option.empty() && option[0] == '~';
      std::string name = ToLower(negated ? option.substr(1) : option);
      ResourceType type;
      if (name == "third-party" || name == "3p") {
        (negated ? filter.first_party_only : filter.third_party_only) = true;
      } else if (name == "first-party" || name == "1p") {
        (negated ? filter.third_party_only : filter.first_party_only) = true;
      } else if (name == "important") {
        filter.important = true;
      } else if (name.compare(0, 7, "domain=") == 0) {
        for (const std::string& domain : Split(name.substr(7), '|')) {
          if (domain.empty()) {
            continue;
          }
          if (domain[0] == '~') {
            filter.not_domains.push_back(domain.substr(1));
          } else {
            filter.domains.push_back(domain);
          }
        }
      } else if (name.compare(0, 9, "redirect=") == 0) {
        filter.redirect = name.substr(9);
      } else if (name.compare(0, 4, "csp=") == 0) {
        filter.csp_only = true;
      } else if (TypeForOption(name, &type)) {
        if (negated) {
          // ~script means "every type except script".
          if (filter.type_mask == 0) {
            for (int i = 0; i <= static_cast<int>(ResourceType::kOther); ++i) {
              filter.type_mask |= 1u << i;
            }
          }
          filter.type_mask &= ~TypeBit(type);
        } else {
          filter.type_mask |= TypeBit(type);
        }
      } else {
        // Unsupported option (generichide, badfilter, ...): skip the rule
        // rather than apply it too broadly.
        return false;
      }
    }
    rule = rule.substr(0, dollar);
  }

  if (rule.compare(0, 2, "||") == 0) {
    filter.anchor_domain = true;
    rule = rule.substr(2);
  } else if (!rule.empty() && rule[0] == '|') {
    filter.anchor_start = true;
    rule = rule.substr(1);
  }
  if (!rule.empty() && rule.back() == '|') {
    filter.anchor_end = true;
    rule.pop_back();
  }
  if (rule.empty()) {
    return false;
  }
  // Regex rules (/.../) are deliberately unsupported: they defeat the token
  // index and are the classic way to make a blocker slow.
  if (rule.front() == '/' && rule.back() == '/' && rule.size() > 2) {
    return false;
  }
  filter.pattern = ToLower(rule);
  filters_.push_back(std::move(filter));
  return true;
}

bool FilterEngine::RemoveRule(const std::string& line) {
  if (user_rules_.erase(line) == 0) {
    return false;
  }
  // Rebuild: removal is rare (a user toggling one rule), matching is not, so
  // we keep the index simple and pay on the rare path.
  std::vector<std::string> lists;
  std::set<std::string> rules;
  lists.swap(lists_);
  rules.swap(user_rules_);
  filters_.clear();
  cosmetics_.clear();
  index_.clear();
  untokenized_.clear();
  for (const std::string& list : lists) {
    AddList(list);
  }
  for (const std::string& rule : rules) {
    AddRule(rule);
  }
  Reindex();
  return true;
}

void FilterEngine::CollectTokens(const NetworkFilter& filter,
                                 std::vector<TokenCandidate>* out) {
  // Every word-aligned literal token of at least three characters is a
  // possible index key. A token must start where a URL word starts and end
  // where one ends, or the URL tokenizer would hash a longer word and never
  // look in this bucket.
  const std::string& pattern = filter.pattern;
  size_t at = 0;
  while (at < pattern.size()) {
    if (!IsTokenChar(pattern[at])) {
      ++at;
      continue;
    }
    size_t end = at;
    while (end < pattern.size() && IsTokenChar(pattern[end])) {
      ++end;
    }
    const bool starts_word =
        at > 0 || filter.anchor_domain || filter.anchor_start;
    const bool ends_word = (end < pattern.size() && pattern[end] != '*') ||
                           (end == pattern.size() && filter.anchor_end);
    if (starts_word && ends_word && end - at >= 3) {
      TokenCandidate candidate;
      candidate.hash = Fnv1a(pattern.data() + at, end - at);
      candidate.length = end - at;
      out->push_back(candidate);
    }
    at = end;
  }
}

void FilterEngine::Reindex() {
  // Index each rule under its **rarest** token, not its longest.
  //
  // The longest-token heuristic looks reasonable and fails badly on real
  // lists: a few thousand rules ending in the same popular word all pick that
  // word, and every request carrying it walks the whole bucket. Measured on a
  // synthetic list of 20,000 same-suffix rules, a matching request cost 70 us
  // — 3.5x over the 20 us budget — while a non-matching one cost 0.35 us,
  // which is exactly the shape of one oversized bucket.
  //
  // Counting how many rules could use each token is a good enough frequency
  // estimate and needs no histogram shipped in the binary (uBO uses a
  // measured one). Ties go to the longer token, which is the old rule.
  index_.clear();
  untokenized_.clear();

  std::unordered_map<uint32_t, uint32_t> frequency;
  std::vector<TokenCandidate> candidates;
  for (const NetworkFilter& filter : filters_) {
    candidates.clear();
    CollectTokens(filter, &candidates);
    for (const TokenCandidate& candidate : candidates)
      ++frequency[candidate.hash];
  }

  for (size_t i = 0; i < filters_.size(); ++i) {
    candidates.clear();
    CollectTokens(filters_[i], &candidates);
    const TokenCandidate* best = nullptr;
    uint32_t best_frequency = 0;
    for (const TokenCandidate& candidate : candidates) {
      const uint32_t count = frequency[candidate.hash];
      if (!best || count < best_frequency ||
          (count == best_frequency && candidate.length > best->length)) {
        best = &candidate;
        best_frequency = count;
      }
    }
    if (best) {
      index_[best->hash].push_back(static_cast<uint32_t>(i));
    } else {
      untokenized_.push_back(static_cast<uint32_t>(i));
    }
  }
}

bool FilterEngine::Matches(const NetworkFilter& filter,
                           const Request& request) const {
  if (filter.type_mask != 0 && !(filter.type_mask & TypeBit(request.type))) {
    return false;
  }
  if (filter.third_party_only && !request.third_party()) {
    return false;
  }
  if (filter.first_party_only && request.third_party()) {
    return false;
  }
  if (!filter.domains.empty() || !filter.not_domains.empty()) {
    const std::string& context = request.top_etld1;
    for (const std::string& domain : filter.not_domains) {
      if (HostMatches(context, domain)) {
        return false;
      }
    }
    if (!filter.domains.empty()) {
      bool any = false;
      for (const std::string& domain : filter.domains) {
        any = any || HostMatches(context, domain);
      }
      if (!any) {
        return false;
      }
    }
  }

  const std::string& url = request.url;
  std::vector<std::string> segments = Split(filter.pattern, '*');
  size_t pos = 0;
  size_t first_at = kNpos;
  for (size_t i = 0; i < segments.size(); ++i) {
    const std::string& segment = segments[i];
    size_t end = 0;
    if (i == 0 && (filter.anchor_start || filter.anchor_domain)) {
      // Anchored: the first segment must match at a fixed position, not
      // wherever it happens to occur later in the URL.
      if (filter.anchor_start) {
        end = MatchSegmentAt(segment, url, 0);
        if (end == kNpos) {
          return false;
        }
        first_at = 0;
      } else {
        size_t host_at = HostStart(url);
        if (host_at == kNpos) {
          return false;
        }
        // || matches the host or any parent-label boundary inside it.
        bool matched = false;
        for (size_t start = host_at;
             start < url.size() && url[start] != '/' && !matched;) {
          end = MatchSegmentAt(segment, url, start);
          if (end != kNpos) {
            matched = true;
            first_at = start;
            break;
          }
          size_t dot = url.find('.', start);
          if (dot == kNpos || dot + 1 >= url.size()) {
            break;
          }
          start = dot + 1;
        }
        if (!matched) {
          return false;
        }
      }
      pos = end;
      continue;
    }
    if (i + 1 == segments.size() && filter.anchor_end) {
      if (segment.empty()) {
        pos = url.size();  // pattern ended with "*|": any suffix is fine
        break;
      }
      if (url.size() < segment.size()) {
        return false;
      }
      size_t start = url.size() - segment.size();
      if (start < pos || MatchSegmentAt(segment, url, start) != url.size()) {
        return false;
      }
      pos = url.size();
      continue;
    }
    size_t at = FindSegment(segment, url, pos, &end);
    if (at == kNpos) {
      return false;
    }
    if (first_at == kNpos) {
      first_at = at;
    }
    pos = end;
  }
  // An anchored pattern that also matched at a fixed start position skips the
  // per-segment end check above, so enforce it once here.
  return !filter.anchor_end || pos == url.size();
}

MatchResult FilterEngine::Match(const Request& request) const {
  // Tokenize the URL once, then look only at filters sharing a token.
  std::vector<uint32_t> tokens;
  const std::string& url = request.url;
  for (size_t at = 0; at < url.size();) {
    if (!IsTokenChar(url[at])) {
      ++at;
      continue;
    }
    size_t end = at;
    while (end < url.size() && IsTokenChar(url[end])) {
      ++end;
    }
    // Same tokenizer as Index(), so a filter token and the URL word it came
    // from hash identically. A pattern whose token cannot be word-aligned goes
    // to untokenized_ instead of silently missing.
    if (end - at >= 3) {
      tokens.push_back(Fnv1a(url.data() + at, end - at));
    }
    at = end;
  }

  const NetworkFilter* blocked = nullptr;
  const NetworkFilter* exception = nullptr;
  auto consider = [&](uint32_t filter_index) {
    const NetworkFilter& filter = filters_[filter_index];
    // An $important block cannot be undone by an exception, so once we have
    // one there is nothing left to decide.
    if (blocked && blocked->important && !filter.exception) {
      return;
    }
    if (!Matches(filter, request)) {
      return;
    }
    if (filter.exception) {
      if (!exception) {
        exception = &filter;
      }
    } else if (!blocked || (filter.important && !blocked->important)) {
      blocked = &filter;
    }
  };

  for (uint32_t token : tokens) {
    auto bucket = index_.find(token);
    if (bucket == index_.end()) {
      continue;
    }
    for (uint32_t filter_index : bucket->second) {
      consider(filter_index);
    }
  }
  for (uint32_t filter_index : untokenized_) {
    consider(filter_index);
  }

  MatchResult result;
  if (!blocked) {
    if (exception) {
      result.rule = exception->raw;
    }
    return result;
  }
  if (exception && !blocked->important) {
    result.rule = exception->raw;
    return result;
  }
  result.blocked = true;
  result.rule = blocked->raw;
  if (!blocked->redirect.empty() &&
      !RedirectResource(blocked->redirect).empty()) {
    result.redirect = blocked->redirect;
  }
  return result;
}

FilterEngine::Cosmetics FilterEngine::CosmeticsFor(
    const std::string& host) const {
  auto applies = [&host](const CosmeticFilter& filter) {
    for (const std::string& domain : filter.not_domains) {
      if (HostMatches(host, domain)) {
        return false;
      }
    }
    if (filter.domains.empty()) {
      return true;  // generic rule
    }
    for (const std::string& domain : filter.domains) {
      if (HostMatches(host, domain)) {
        return true;
      }
    }
    return false;
  };

  std::set<std::string> excluded;
  for (const CosmeticFilter& filter : cosmetics_) {
    if (filter.exception && applies(filter)) {
      excluded.insert(filter.selector);
    }
  }
  Cosmetics result;
  std::set<std::string> seen;
  for (const CosmeticFilter& filter : cosmetics_) {
    if (filter.exception || !applies(filter) ||
        excluded.count(filter.selector) != 0 ||
        !seen.insert(filter.selector).second) {
      continue;
    }
    (filter.procedural ? result.procedural : result.selectors)
        .push_back(filter.selector);
  }
  return result;
}

std::string FilterEngine::ExportRules() const {
  std::string text;
  for (const std::string& rule : user_rules_) {
    text += rule;
    text += '\n';
  }
  return text;
}

}  // namespace blocking
}  // namespace bedrock
