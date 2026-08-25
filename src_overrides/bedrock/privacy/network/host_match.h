// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_NETWORK_HOST_MATCH_H_
#define BEDROCK_PRIVACY_NETWORK_HOST_MATCH_H_

#include <cstdint>
#include <string>

// The one place a host name is compared to anything.
//
// Finding **F1** of `docs/security/AUDIT-2026-08-25.md` was a prefix match on a
// host name: `StartsWith(host, "10.")` accepted `10.example.com`, a name anyone
// can register, and silently downgraded it to plaintext HTTP. The same shape of
// bug was written three times in this tree, each with its own local helper, so
// the fix is not another local helper: comparisons on host names live here, and
// `scripts/check_host_matching.py` fails the build if a new one appears
// elsewhere.
//
// Three rules every function below obeys, because each was a bug somewhere:
//
//   1. **A name is not an address.** An address check parses octets; it never
//      looks at the spelling of a name.
//   2. **A suffix must end on a label boundary.** `notonion.example.com` does
//      not end in the `.onion` label, and `evil-example.com` is not
//      `example.com`.
//   3. **Compare the normalised name.** Host names are case-insensitive and may
//      carry a trailing root dot, so `EXAMPLE.COM` and `example.com.` are the
//      same host as `example.com`. Raw `==` on the wire form says otherwise,
//      which is a filter-list bypass in one direction and a wrong block in the
//      other.

namespace bedrock {
namespace net {

// Lower-cases and drops one trailing root dot. Everything below applies this to
// both sides first; call it directly when a host is stored or used as a key.
std::string NormalizeHost(const std::string& host);

// True if `host` is `domain` itself or a subdomain of it. Empty `domain` never
// matches: a rule with no domain must not match every host.
bool IsOrSubdomainOf(const std::string& host, const std::string& domain);

// True if `host` has more than one label and its final label is `label`
// (written without a dot: "onion", "local", "internal").
bool HasFinalLabel(const std::string& host, const std::string& label);

// True only for a literal dotted-quad, and only then fills `octets`.
bool ParseIPv4(const std::string& host, uint8_t octets[4]);

// True if `host` is an address literal that cannot leave this machine or this
// network: loopback, RFC 1918, link-local, IPv6 loopback/ULA/link-local.
// A *name* is never private here, however it is spelled.
bool IsPrivateAddress(const std::string& host);

}  // namespace net
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_NETWORK_HOST_MATCH_H_
