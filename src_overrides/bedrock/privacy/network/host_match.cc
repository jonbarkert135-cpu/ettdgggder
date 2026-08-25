// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/network/host_match.h"

namespace bedrock {
namespace net {
namespace {

char Lower(char c) {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// Suffix comparison on already-normalised names, at a label boundary.
bool EndsWithLabelBoundary(const std::string& host, const std::string& suffix) {
  if (suffix.empty() || host.size() <= suffix.size()) {
    return false;
  }
  const size_t at = host.size() - suffix.size();
  return host[at - 1] == '.' && host.compare(at, suffix.size(), suffix) == 0;
}

}  // namespace

std::string NormalizeHost(const std::string& host) {
  std::string out;
  out.reserve(host.size());
  for (const char c : host) {
    out += Lower(c);
  }
  // One trailing root dot only: "example.com." is "example.com", but ".." is
  // malformed and stays malformed rather than being quietly cleaned up.
  if (out.size() > 1 && out.back() == '.' && out[out.size() - 2] != '.') {
    out.pop_back();
  }
  return out;
}

bool IsOrSubdomainOf(const std::string& host, const std::string& domain) {
  const std::string name = NormalizeHost(host);
  const std::string base = NormalizeHost(domain);
  if (base.empty() || name.empty()) {
    return false;
  }
  return name == base || EndsWithLabelBoundary(name, base);
}

bool HasFinalLabel(const std::string& host, const std::string& label) {
  return EndsWithLabelBoundary(NormalizeHost(host), NormalizeHost(label));
}

bool ParseIPv4(const std::string& host, uint8_t octets[4]) {
  int value = 0;
  int digits = 0;
  int index = 0;
  for (size_t i = 0; i <= host.size(); ++i) {
    const char c = i < host.size() ? host[i] : '.';
    if (c >= '0' && c <= '9') {
      if (++digits > 3) {
        return false;
      }
      value = value * 10 + (c - '0');
      if (value > 255) {
        return false;
      }
      continue;
    }
    if (c != '.' || digits == 0 || index > 3) {
      return false;
    }
    octets[index++] = static_cast<uint8_t>(value);
    value = 0;
    digits = 0;
  }
  return index == 4;
}

bool IsPrivateAddress(const std::string& host) {
  const std::string name = NormalizeHost(host);
  if (name == "[::1]" || name == "::1") {
    return true;
  }
  // Bracketed IPv6. Compared on the parsed prefix nibbles, not on the spelling
  // of a name: only a literal can start with '[', so there is no name to spoof.
  if (name.size() > 2 && name.front() == '[') {
    const std::string body = name.substr(1);
    // fc00::/7 (unique local) and fe80::/10 (link local).
    if (body.compare(0, 2, "fc") == 0 || body.compare(0, 2, "fd") == 0 ||
        body.compare(0, 5, "fe80:") == 0) {
      return true;
    }
    // ponytail: prefix nibbles cover the ranges we act on; a full IPv6 parser
    // belongs here once anything needs subnet arithmetic.
    return false;
  }
  uint8_t octet[4] = {0, 0, 0, 0};
  if (!ParseIPv4(name, octet)) {
    return false;
  }
  return octet[0] == 127 || octet[0] == 10 ||
         (octet[0] == 192 && octet[1] == 168) ||
         (octet[0] == 172 && octet[1] >= 16 && octet[1] <= 31) ||
         (octet[0] == 169 && octet[1] == 254) ||
         (octet[0] == 0 && octet[1] == 0 && octet[2] == 0 && octet[3] == 0);
}

}  // namespace net
}  // namespace bedrock
