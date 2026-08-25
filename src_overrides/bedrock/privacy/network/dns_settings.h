// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_NETWORK_DNS_SETTINGS_H_
#define BEDROCK_PRIVACY_NETWORK_DNS_SETTINGS_H_

#include <string>
#include <vector>

// DNS and network privacy (roadmap item 17).
//
// The one thing this file exists to guarantee: **the user can always name who
// resolves their DNS.** Not "encrypted, trust us" — a name, in the settings
// UI, next to the switch.
//
// Bedrock never routes DNS through infrastructure belonging to this project.
// There is no Bedrock resolver, no Bedrock fallback, no bootstrap through a
// Bedrock endpoint (roadmap item 4). The shipped provider list is other
// people's public resolvers, chosen by the user, and the default is the
// system's own resolver — the one the user's OS and network already use, so
// installing Bedrock does not silently move their queries to a new company.

namespace bedrock {
namespace net {

enum class DnsMode {
  // Use whatever the operating system uses. No new party learns anything.
  kSystem,
  // DoH with a provider the user picked from the list.
  kSecurePreset,
  // DoH/DoT with a URL the user typed.
  kSecureCustom,
  // DoH, and never fall back to plaintext DNS. Resolution fails instead.
  kSecureStrict,
};

// What happens when the secure resolver cannot be reached.
enum class FallbackPolicy {
  // Retry with the system resolver. Names resolve, but the query is now
  // visible to the network — this is a real privacy downgrade and the UI says
  // so, once per network, not silently.
  kSystemWithWarning,
  // Fail the navigation. No plaintext query leaves the device.
  kFailClosed,
};

struct DnsProvider {
  std::string name;         // shown in settings
  std::string operator_;    // *who* it is: the point of the whole screen
  std::string doh_template; // RFC 8484 URI template
  std::string policy_url;
  bool logs_queries = false;
  bool filters_content = false;  // blocks malware/ads at the resolver
  // ISO date this entry was last checked against the operator's own
  // documentation. A shipped resolver list is a perishable good: audit finding
  // F6b was a preset (`dns0.eu`) whose service had shut down, pointing at the
  // website instead of a DoH endpoint — with fallback enabled, that is a
  // plaintext DNS query. `scripts/check_dns_presets.py` fails the build when an
  // entry goes stale or stops looking like an endpoint.
  std::string verified;
};

class DnsSettings {
 public:
  DnsSettings();
  ~DnsSettings();

  // Providers Bedrock offers. None of them is operated by this project.
  static const std::vector<DnsProvider>& Providers();
  static const DnsProvider* FindProvider(const std::string& name);

  void UseSystemResolver();
  bool UsePreset(const std::string& provider_name);
  // Accepts https:// (DoH) and tls:// (DoT) templates only: a plaintext
  // "custom resolver" is just a different party watching, so it is refused.
  bool UseCustom(const std::string& uri_template);
  // Returns false when strict mode cannot be applied — today that means the
  // system resolver is selected, where Bedrock controls nothing and
  // "fail closed" would be a promise it cannot keep. Refusing silently let the
  // caller (and the settings UI) show strict mode as on while queries went to
  // the OS resolver: docs/security/AUDIT-2026-08-25.md (F6).
  bool SetStrict(bool strict);

  void set_fallback(FallbackPolicy policy) { fallback_ = policy; }
  FallbackPolicy fallback() const;

  DnsMode mode() const { return mode_; }
  const std::string& resolver_uri() const { return resolver_uri_; }

  // Who currently sees the user's DNS queries, in plain words, for the
  // settings page. Never empty.
  std::string WhoSeesQueries() const;

  // Known ways a query can leak around the secure resolver, so the UI can list
  // them instead of implying encrypted DNS hides everything.
  struct LeakNote {
    const char* vector;
    const char* mitigation;
  };
  static const std::vector<LeakNote>& LeakNotes();

  // A configuration is only honest if the user can be told who resolves their
  // names; this is asserted in tests for every mode.
  bool IsExplainable() const { return !WhoSeesQueries().empty(); }

 private:
  DnsMode mode_ = DnsMode::kSystem;
  std::string resolver_uri_;
  std::string provider_name_;
  FallbackPolicy fallback_ = FallbackPolicy::kSystemWithWarning;
};

}  // namespace net
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_NETWORK_DNS_SETTINGS_H_
