// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/tracker_blocker/url_cleaner.h"

#include <cctype>
#include <set>
#include <string>
#include <vector>

namespace bedrock {
namespace blocking {

namespace {

// Parameters that exist only to carry an identifier across a click. Kept short
// and conservative on purpose: removing a parameter a site needs breaks the
// click, and a broken click is the worse privacy outcome because the user
// retries somewhere else.
const std::set<std::string>& TrackingParams() {
  static const std::set<std::string> kParams = {
      "fbclid",      "gclid",        "gclsrc",      "dclid",
      "msclkid",     "twclid",       "igshid",      "mc_eid",
      "mkt_tok",     "yclid",        "ttclid",      "wickedid",
      "oly_enc_id",  "oly_anon_id",  "vero_id",     "_openstat",
      "utm_source",  "utm_medium",   "utm_campaign", "utm_term",
      "utm_content", "utm_id",       "utm_source_platform",
  };
  return kParams;
}

// (host suffix, parameter) pairs whose value is the real destination. A rule
// per redirector, never a generic "any parameter that looks like a URL": the
// generic version turns the cleaner into an open redirector.
struct Redirector {
  const char* host_suffix;
  const char* param;
};

const std::vector<Redirector>& Redirectors() {
  static const std::vector<Redirector> kRules = {
      {"out.reddit.com", "url"},   {"l.facebook.com", "u"},
      {"l.instagram.com", "u"},    {"lm.facebook.com", "u"},
      {"t.umblr.com", "z"},        {"href.li", ""},
      {"away.vk.com", "to"},       {"login.yahoo.com", ".done"},
      {"steamcommunity.com", "url"},
      {"www.google.com", "url"},   {"news.url.google.com", "url"},
  };
  return kRules;
}

std::string Lower(std::string value) {
  for (char& c : value) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return value;
}

// Host of an absolute http(s) URL, lowercased; empty for anything else.
std::string HostOf(const std::string& url) {
  const std::string lower = Lower(url);
  size_t start = 0;
  if (lower.rfind("https://", 0) == 0) {
    start = 8;
  } else if (lower.rfind("http://", 0) == 0) {
    start = 7;
  } else {
    return std::string();
  }
  const size_t end = lower.find_first_of("/?#", start);
  std::string host = lower.substr(start, end - start);
  const size_t at = host.rfind('@');  // userinfo is not the host
  if (at != std::string::npos) {
    host = host.substr(at + 1);
  }
  return host;
}

bool HostMatches(const std::string& host, const std::string& suffix) {
  if (host == suffix) {
    return true;
  }
  return host.size() > suffix.size() &&
         host.compare(host.size() - suffix.size() - 1, suffix.size() + 1,
                      "." + suffix) == 0;
}

std::string PercentDecode(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '+') {
      out += ' ';
    } else if (value[i] == '%' && i + 2 < value.size() &&
               std::isxdigit(static_cast<unsigned char>(value[i + 1])) &&
               std::isxdigit(static_cast<unsigned char>(value[i + 2]))) {
      const auto hex = [](char c) {
        return c <= '9' ? c - '0' : (std::tolower(c) - 'a' + 10);
      };
      out += static_cast<char>(hex(value[i + 1]) * 16 + hex(value[i + 2]));
      i += 2;
    } else {
      out += value[i];
    }
  }
  return out;
}

struct Parsed {
  std::string base;      // scheme, host, path
  std::string query;     // without '?'
  std::string fragment;  // with '#', or empty
  bool has_query = false;
};

Parsed Split(const std::string& url) {
  Parsed parsed;
  const size_t query_at = url.find('?');
  const size_t fragment_at = url.find('#', query_at == std::string::npos ? 0
                                                                         : query_at);
  const size_t base_end =
      query_at != std::string::npos
          ? query_at
          : (fragment_at != std::string::npos ? fragment_at : url.size());
  parsed.base = url.substr(0, base_end);
  if (fragment_at != std::string::npos) {
    parsed.fragment = url.substr(fragment_at);
  }
  if (query_at != std::string::npos) {
    parsed.has_query = true;
    const size_t len = fragment_at == std::string::npos
                           ? std::string::npos
                           : fragment_at - query_at - 1;
    parsed.query = url.substr(query_at + 1, len);
  }
  return parsed;
}

struct Param {
  std::string name;
  std::string value;
  std::string raw;  // exactly as it appeared, so kept parameters are untouched
};

std::vector<Param> Pairs(const std::string& query) {
  std::vector<Param> pairs;
  size_t start = 0;
  while (start <= query.size()) {
    const size_t end = query.find('&', start);
    const std::string pair = query.substr(
        start, end == std::string::npos ? std::string::npos : end - start);
    if (!pair.empty()) {
      const size_t eq = pair.find('=');
      pairs.push_back({pair.substr(0, eq),
                       eq == std::string::npos ? std::string()
                                               : pair.substr(eq + 1),
                       pair});
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return pairs;
}

// One unwrapping step. Returns the wrapped target, or an empty string when the
// URL is not a known redirector or its target is not an absolute http(s) URL.
std::string Unwrap(const std::string& url) {
  const std::string host = HostOf(url);
  if (host.empty()) {
    return std::string();
  }
  const Parsed parsed = Split(url);
  for (const Redirector& rule : Redirectors()) {
    if (!HostMatches(host, rule.host_suffix)) {
      continue;
    }
    // An empty parameter name means the target is the whole query string
    // (href.li style: https://href.li/?https://real.site/).
    const std::string candidate =
        *rule.param == '\0' ? PercentDecode(parsed.query) : std::string();
    if (!candidate.empty()) {
      return HostOf(candidate).empty() ? std::string() : candidate;
    }
    for (const auto& pair : Pairs(parsed.query)) {
      if (pair.name != rule.param) {
        continue;
      }
      const std::string target = PercentDecode(pair.value);
      // Only an absolute http(s) URL may be followed. HostOf() returns empty
      // for javascript:, data:, relative paths and anything else.
      if (!HostOf(target).empty() && HostOf(target) != host) {
        return target;
      }
    }
  }
  return std::string();
}

}  // namespace

// static
bool UrlCleaner::IsTrackingParam(const std::string& name) {
  return TrackingParams().count(Lower(name)) != 0;
}

// static
CleanResult UrlCleaner::Clean(const std::string& url, UrlUse use) {
  CleanResult result;
  result.url = url;
  if (use == UrlUse::kSubresource) {
    return result;  // never cleaned — see the header.
  }
  if (HostOf(url).empty()) {
    return result;  // only absolute http(s) URLs are rewritten
  }

  for (int hop = 0; hop < kMaxHops; ++hop) {
    const std::string target = Unwrap(result.url);
    if (target.empty()) {
      break;
    }
    result.url = target;
    ++result.unwrapped_hops;
  }

  const Parsed parsed = Split(result.url);
  if (!parsed.has_query) {
    return result;
  }
  std::string kept;
  for (const auto& pair : Pairs(parsed.query)) {
    if (IsTrackingParam(pair.name)) {
      result.stripped.push_back(pair.name);
      continue;
    }
    if (!kept.empty()) {
      kept += '&';
    }
    kept += pair.raw;
  }
  if (result.stripped.empty()) {
    return result;  // untouched, including a trailing '?' the site wrote
  }
  result.url = parsed.base;
  if (!kept.empty()) {
    result.url += '?' + kept;
  }
  result.url += parsed.fragment;
  return result;
}

}  // namespace blocking
}  // namespace bedrock
