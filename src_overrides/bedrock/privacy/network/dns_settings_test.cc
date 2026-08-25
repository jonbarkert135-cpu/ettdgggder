// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/privacy/network/dns_settings.h"

#include <iostream>
#include <string>

namespace {

using namespace bedrock::net;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

bool Contains(const std::string& text, const std::string& needle) {
  return text.find(needle) != std::string::npos;
}

}  // namespace

int main() {
  DnsSettings dns;

  // Default: the system resolver. Installing a browser must not silently move
  // the user's DNS to a new company.
  Check(dns.mode() == DnsMode::kSystem, "system resolver is the default");
  Check(Contains(dns.WhoSeesQueries(), "operating system"),
        "and the user is told what that means");

  // No provider in the shipped list belongs to this project.
  Check(!DnsSettings::Providers().empty(), "providers are offered");
  Check(DnsSettings::FindProvider("dns0.eu") == nullptr,
        "the dns0.eu preset is gone: the service shut down in October 2025");
  for (const DnsProvider& provider : DnsSettings::Providers()) {
    Check(!provider.operator_.empty(),
          provider.name + " names its operator");
    Check(!provider.policy_url.empty(), provider.name + " links its policy");
    Check(provider.doh_template.compare(0, 8, "https://") == 0,
          provider.name + " uses an encrypted endpoint");
    Check(!Contains(provider.doh_template, "bedrock"),
          "no Bedrock-operated resolver: " + provider.name);
    // Audit F6b: the dns0.eu preset pointed at the operator's *website*. With
    // fallback enabled that is a plaintext DNS query, so a preset must be an
    // endpoint with a path, not a bare origin.
    const size_t path = provider.doh_template.find('/', 8);
    Check(path != std::string::npos &&
              path + 1 < provider.doh_template.size(),
          provider.name + " names an endpoint, not a website");
    Check(provider.verified.size() == 10,
          provider.name + " records when it was last checked");
  }

  // Presets name who sees the queries, including logging and filtering.
  Check(dns.UsePreset("Quad9"), "a known preset is accepted");
  Check(Contains(dns.WhoSeesQueries(), "Quad9") &&
            Contains(dns.WhoSeesQueries(), "Switzerland"),
        "the operator is named, not just the brand");
  Check(Contains(dns.WhoSeesQueries(), "blocks some domains"),
        "resolver-side filtering is disclosed");
  Check(dns.UsePreset("Google") && Contains(dns.WhoSeesQueries(), "logs"),
        "a provider that logs is disclosed as logging");
  Check(!dns.UsePreset("Nonexistent"), "an unknown preset is rejected");

  // Custom resolvers must be encrypted: a plaintext "custom resolver" is just
  // a different party watching in the clear.
  Check(dns.UseCustom("https://dns.example.test/dns-query"),
        "custom DoH accepted");
  Check(dns.mode() == DnsMode::kSecureCustom, "mode reflects the custom URI");
  Check(Contains(dns.WhoSeesQueries(), "dns.example.test"),
        "the custom endpoint is shown to the user");
  Check(dns.UseCustom("tls://dns.example.test"), "custom DoT accepted");
  Check(!dns.UseCustom("http://dns.example.test/dns-query"),
        "plaintext custom resolver rejected");
  Check(!dns.UseCustom("8.8.8.8"), "bare IP resolver rejected");

  // Fallback policy, and strict mode that cannot be contradicted.
  dns.UsePreset("Mullvad");
  dns.set_fallback(FallbackPolicy::kSystemWithWarning);
  Check(dns.fallback() == FallbackPolicy::kSystemWithWarning,
        "fallback to the system resolver is configurable");
  Check(dns.SetStrict(true), "strict mode can be applied to a secure resolver");
  Check(dns.mode() == DnsMode::kSecureStrict, "strict mode set");
  Check(dns.fallback() == FallbackPolicy::kFailClosed,
        "strict mode is fail-closed no matter what the fallback setting says");
  Check(Contains(dns.WhoSeesQueries(), "fail to load"),
        "and the consequence is stated plainly");
  dns.SetStrict(false);
  Check(dns.mode() == DnsMode::kSecurePreset, "leaving strict restores preset");
  Check(dns.fallback() == FallbackPolicy::kSystemWithWarning,
        "and the user's fallback choice was not overwritten");

  // Strict is meaningless without a secure resolver, so it is not offered.
  dns.UseSystemResolver();
  // Audit F6: refusing is fine, refusing *silently* is not — the caller showed
  // strict mode as on while queries went to the OS resolver.
  Check(!dns.SetStrict(true),
        "strict is refused, and the refusal is reported to the caller");
  Check(dns.mode() == DnsMode::kSystem,
        "strict does nothing while using the system resolver");

  // Every mode can answer "who sees my DNS?".
  for (const char* preset : {"Quad9", "Cloudflare", "dns0.eu"}) {
    dns.UsePreset(preset);
    Check(dns.IsExplainable(), std::string("explainable: ") + preset);
  }
  dns.UseCustom("https://dns.example.test/dns-query");
  Check(dns.IsExplainable(), "custom mode is explainable");
  dns.UseSystemResolver();
  Check(dns.IsExplainable(), "system mode is explainable");

  // Leak notes: encrypted DNS is not a cloak, and the UI must say so.
  Check(DnsSettings::LeakNotes().size() >= 5, "leak vectors are documented");
  for (const auto& note : DnsSettings::LeakNotes()) {
    Check(std::string(note.vector).size() > 5 &&
              std::string(note.mitigation).size() > 20,
          "each leak note explains the vector and what we do about it");
  }

  if (failures == 0) {
    std::cout << "dns_settings_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
