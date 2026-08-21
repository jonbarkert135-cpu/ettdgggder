// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/network/dns_settings.h"

namespace bedrock {
namespace net {
namespace {

bool StartsWith(const std::string& text, const std::string& prefix) {
  return text.compare(0, prefix.size(), prefix) == 0;
}

}  // namespace

DnsSettings::DnsSettings() = default;
DnsSettings::~DnsSettings() = default;

// static
const std::vector<DnsProvider>& DnsSettings::Providers() {
  // Public resolvers, each operated by someone other than this project. The
  // operator and the logging answer are part of the entry because a provider
  // list without them is a list of URLs, not a choice.
  static const std::vector<DnsProvider> kProviders = {
      {"Quad9", "Quad9 Foundation (Switzerland)",
       "https://dns.quad9.net/dns-query", "https://quad9.net/privacy/policy/",
       false, true},
      {"Cloudflare", "Cloudflare, Inc. (US)",
       "https://cloudflare-dns.com/dns-query",
       "https://developers.cloudflare.com/1.1.1.1/privacy/public-dns-resolver/",
       false, false},
      {"Mullvad", "Mullvad VPN AB (Sweden)",
       "https://dns.mullvad.net/dns-query", "https://mullvad.net/en/help/dns-over-https-and-dns-over-tls",
       false, false},
      {"dns0.eu", "dns0.eu (non-profit, EU)", "https://dns0.eu/",
       "https://www.dns0.eu/privacy", false, true},
      {"Google", "Google LLC (US)", "https://dns.google/dns-query",
       "https://developers.google.com/speed/public-dns/privacy", true, false},
  };
  return kProviders;
}

// static
const DnsProvider* DnsSettings::FindProvider(const std::string& name) {
  for (const DnsProvider& provider : Providers()) {
    if (provider.name == name) {
      return &provider;
    }
  }
  return nullptr;
}

void DnsSettings::UseSystemResolver() {
  mode_ = DnsMode::kSystem;
  resolver_uri_.clear();
  provider_name_.clear();
}

bool DnsSettings::UsePreset(const std::string& provider_name) {
  const DnsProvider* provider = FindProvider(provider_name);
  if (!provider) {
    return false;
  }
  mode_ = DnsMode::kSecurePreset;
  provider_name_ = provider->name;
  resolver_uri_ = provider->doh_template;
  return true;
}

bool DnsSettings::UseCustom(const std::string& uri_template) {
  if (!StartsWith(uri_template, "https://") &&
      !StartsWith(uri_template, "tls://")) {
    return false;
  }
  mode_ = DnsMode::kSecureCustom;
  provider_name_.clear();
  resolver_uri_ = uri_template;
  return true;
}

void DnsSettings::SetStrict(bool strict) {
  if (strict) {
    if (mode_ == DnsMode::kSystem) {
      return;  // nothing to be strict about; the OS resolver is the OS's
    }
    mode_ = DnsMode::kSecureStrict;
  } else if (mode_ == DnsMode::kSecureStrict) {
    mode_ = provider_name_.empty() ? DnsMode::kSecureCustom
                                   : DnsMode::kSecurePreset;
  }
}

FallbackPolicy DnsSettings::fallback() const {
  // Strict mode *is* fail-closed; letting a separate setting contradict it
  // would produce a configuration that lies to the user.
  return mode_ == DnsMode::kSecureStrict ? FallbackPolicy::kFailClosed
                                         : fallback_;
}

std::string DnsSettings::WhoSeesQueries() const {
  switch (mode_) {
    case DnsMode::kSystem:
      return "Your operating system's resolver — usually your internet "
             "provider or your router. Bedrock does not change this.";
    case DnsMode::kSecurePreset: {
      const DnsProvider* provider = FindProvider(provider_name_);
      std::string who = "Your queries are encrypted and sent to " +
                        provider_name_ + " (" +
                        (provider ? provider->operator_ : "unknown operator") +
                        "). Your internet provider no longer sees them; " +
                        provider_name_ + " does.";
      if (provider && provider->logs_queries) {
        who += " This provider states that it logs queries.";
      }
      if (provider && provider->filters_content) {
        who += " This provider also blocks some domains at the resolver.";
      }
      return who;
    }
    case DnsMode::kSecureCustom:
      return "Your queries are encrypted and sent to the resolver you "
             "configured: " + resolver_uri_ +
             ". Bedrock cannot tell you who operates it.";
    case DnsMode::kSecureStrict:
      return "Your queries are encrypted and sent only to " +
             (provider_name_.empty() ? resolver_uri_ : provider_name_) +
             ". If it cannot be reached, pages fail to load instead of falling "
             "back to unencrypted DNS.";
  }
  return "";
}

// static
const std::vector<DnsSettings::LeakNote>& DnsSettings::LeakNotes() {
  static const std::vector<LeakNote> kNotes = {
      {"Bootstrapping the resolver's own address",
       "Resolved once via the system resolver, or by IP where the provider "
       "publishes one. It reveals which provider you use, nothing else."},
      {"System-level lookups outside the browser",
       "Other applications keep using the OS resolver. Browser DoH does not "
       "cover your whole device — the settings page says this."},
      {"Captive portals and enterprise networks",
       "Detection uses the system resolver before DoH starts; the address bar "
       "shows the network is intercepting."},
      {"Server Name Indication in TLS",
       "The name is still visible on the wire unless the site supports "
       "Encrypted Client Hello. Encrypted DNS alone does not hide it."},
      {"IP addresses themselves",
       "Your provider still sees which addresses you connect to. DNS "
       "encryption hides the question, not the destination."},
      {"Local-domain and .onion names",
       "Never sent to the public resolver: they go to the system resolver or "
       "the proxy that owns them."},
  };
  return kNotes;
}

}  // namespace net
}  // namespace bedrock
