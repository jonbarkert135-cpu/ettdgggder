// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/settings/advanced_settings.h"

#include <algorithm>
#include <cctype>

namespace bedrock {
namespace settings {
namespace {

bool StartsWith(const std::string& value, const char* prefix) {
  const std::string p(prefix);
  return value.size() >= p.size() && value.compare(0, p.size(), p) == 0;
}

std::string Lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

bool Contains(const std::string& haystack, const char* needle) {
  return haystack.find(needle) != std::string::npos;
}

Decision Accept() { return {Verdict::kAccepted, "", }; }

Decision Warn(std::string message) {
  return {Verdict::kAcceptedWithWarning, std::move(message)};
}

Decision Reject(const char* guard_id, std::string reason) {
  return {Verdict::kRejected, reason + " (guard " + guard_id + ")"};
}

// An advanced setting that would turn off a security mechanism. These strings
// are the ones users find in forums as "fixes"; the browser refuses all of
// them, from the GUI and from policy alike.
bool AsksToDisableSecurity(const std::string& value) {
  const std::string v = Lower(value);
  static const char* const kNoGoes[] = {
      "ignore-certificate-errors", "allow-insecure-localhost",
      "disable-web-security",      "disable-site-isolation",
      "no-sandbox",                "disable-gpu-sandbox",
      "allow-running-insecure-content"};
  for (const char* no_go : kNoGoes) {
    if (Contains(v, no_go)) {
      return true;
    }
  }
  return false;
}

Decision EvaluateFilterList(const AdvancedInput& input) {
  if (!StartsWith(input.value, "https://")) {
    return Reject("G2", "A filter list is fetched repeatedly and rewrites what the "
                        "browser blocks; over plain HTTP anyone on the path can edit it. "
                        "Use an https:// URL");
  }
  return Warn("Bedrock will fetch this list on your behalf, which tells its host "
              "your IP on a schedule. It is your list, not a default one, and its "
              "licence is your responsibility.");
}

Decision EvaluateDns(const AdvancedInput& input) {
  const std::string value = Lower(input.value);
  if (StartsWith(value, "https://")) {
    return Warn("Your DNS queries will go to this resolver instead of your system's. "
                "It will see every hostname you visit.");
  }
  if (StartsWith(value, "http://")) {
    return Reject("G3", "A DoH endpoint must be https:// — plaintext DNS-over-HTTP "
                        "is the problem DoH exists to solve");
  }
  if (value == "system") {
    return Accept();
  }
  return Warn("Plain UDP DNS to a custom server is unencrypted: your network operator "
              "sees every hostname. Bedrock will use it because you asked, and will "
              "not describe it as private.");
}

Decision EvaluateProxy(const AdvancedInput& input) {
  const std::string value = Lower(input.value);
  if (Contains(value, "@")) {
    return Reject("G5", "Put proxy credentials in the credential prompt, not in the "
                        "URL: a URL with a password in it ends up in logs and in "
                        "exported settings");
  }
  if (!(StartsWith(value, "socks5://") || StartsWith(value, "https://") ||
        StartsWith(value, "http://"))) {
    return Reject("G4", "Proxy must be http://, https:// or socks5://");
  }
  if (StartsWith(value, "http://")) {
    return Warn("An HTTP proxy sees and can modify every unencrypted request. "
                "HTTPS pages stay encrypted end to end; nothing else does.");
  }
  return Warn("A proxy does not apply to Tor windows: their traffic keeps its own "
              "transport, so a misconfigured proxy cannot silently deanonymise them.");
}

Decision EvaluateUserAgent(const AdvancedInput& input) {
  if (input.value.empty() || Lower(input.value) == "default") {
    return Accept();
  }
  if (input.scope.empty()) {
    return Reject("G6", "A global custom user agent makes you *more* identifiable, not "
                        "less: almost nobody else sends your string. Set it per site if "
                        "a site is broken");
  }
  return Warn("A per-site user agent override is a compatibility tool. It raises your "
              "uniqueness on that site; it is not a privacy feature.");
}

Decision EvaluatePermission(const AdvancedInput& input) {
  if (input.scope == "*" || input.scope == "<all_urls>") {
    return Reject("G7", "A permission cannot be granted to every site at once. Grant it "
                        "to the sites that need it");
  }
  if (input.scope.empty()) {
    return Reject("G7", "A per-site permission needs a site");
  }
  if (input.from_policy) {
    return Warn("Set by your organisation. The setting will show as managed and you "
                "will not be able to change it.");
  }
  return Accept();
}

Decision EvaluateContentPolicy(const AdvancedInput& input) {
  const std::string value = Lower(input.value);
  if (Contains(value, "unsafe-inline") || Contains(value, "unsafe-eval") ||
      Contains(value, "relax") || Contains(value, "disable")) {
    return Reject("G8", "A user content policy may only add restrictions. Relaxing a "
                        "site's own CSP would remove protection the site asked for and "
                        "break the web's security model");
  }
  if (input.scope.empty()) {
    return Reject("G8", "A content policy needs a site scope");
  }
  return Warn("Your rules are intersected with the site's own policy, never substituted "
              "for it. Strict rules break some sites; the page will say so.");
}

Decision EvaluateManagedProfile(const AdvancedInput& input) {
  if (!input.from_policy) {
    return Reject("G9", "A managed profile is created by enterprise policy, not from "
                        "the settings dialog");
  }
  if (Contains(Lower(input.value), "telemetry")) {
    return Reject("G1", "Policy cannot switch on reporting. Bedrock has no telemetry "
                        "and no server to send it to");
  }
  return Warn("This profile is managed by your organisation: some settings are locked, "
              "and the settings page will name them. Your browsing is not sent anywhere "
              "— Bedrock has no reporting channel to send it through.");
}

}  // namespace

// static
const std::vector<Guard>& AdvancedSettings::Guards() {
  static const std::vector<Guard> guards = {
      {"G1", "No advanced setting can enable reporting or a management server",
       "Zero-telemetry is an architectural property (item 39), not a preference"},
      {"G2", "Remote configuration (filter lists, rules) must be fetched over HTTPS",
       "A list fetched over plain HTTP lets anyone on the path decide what is blocked"},
      {"G3", "A DoH endpoint must be HTTPS",
       "Plaintext DNS-over-HTTP is the problem DoH exists to solve"},
      {"G4", "Proxies are limited to http, https and socks5",
       "Unknown schemes end up handled by something that is not the network stack"},
      {"G5", "Credentials never live in a URL",
       "URLs are logged, exported and shown in the UI; passwords should be in none of those"},
      {"G6", "No global custom user agent",
       "A unique UA string raises fingerprint entropy for every site at once"},
      {"G7", "No wildcard permission grant",
       "'Allow camera everywhere' defeats the permission model it is configuring"},
      {"G8", "User content policy may only tighten, never relax, a site's CSP",
       "Relaxing a site's own policy removes protection the site asked for"},
      {"G9", "Nothing in the advanced surface can disable certificate validation, "
             "the sandbox or site isolation",
       "These are the assumptions every other protection is built on"},
  };
  return guards;
}

// static
Decision AdvancedSettings::Evaluate(const AdvancedInput& input) {
  if (AsksToDisableSecurity(input.value)) {
    return Reject("G9", "Certificate validation, the sandbox and site isolation are not "
                        "configurable. Every other protection assumes them, so no "
                        "setting — and no policy — can switch them off");
  }
  switch (input.control) {
    case AdvancedControl::kCustomFilterList:
      return EvaluateFilterList(input);
    case AdvancedControl::kCustomDns:
      return EvaluateDns(input);
    case AdvancedControl::kCustomProxy:
      return EvaluateProxy(input);
    case AdvancedControl::kUserAgentPolicy:
      return EvaluateUserAgent(input);
    case AdvancedControl::kSitePermission:
      return EvaluatePermission(input);
    case AdvancedControl::kSitePolicy:
      return input.scope.empty()
                 ? Reject("G7", "A site policy needs a site")
                 : Accept();
    case AdvancedControl::kContentPolicy:
      return EvaluateContentPolicy(input);
    case AdvancedControl::kManagedProfile:
      return EvaluateManagedProfile(input);
  }
  return Reject("G9", "Unknown advanced control");
}

// static
const char* AdvancedSettings::Describe(AdvancedControl control) {
  switch (control) {
    case AdvancedControl::kCustomFilterList:
      return "Custom filter list";
    case AdvancedControl::kCustomDns:
      return "Custom DNS resolver";
    case AdvancedControl::kCustomProxy:
      return "Custom proxy";
    case AdvancedControl::kUserAgentPolicy:
      return "User agent policy";
    case AdvancedControl::kSitePermission:
      return "Per-site permission";
    case AdvancedControl::kSitePolicy:
      return "Per-site policy";
    case AdvancedControl::kContentPolicy:
      return "Per-site content policy";
    case AdvancedControl::kManagedProfile:
      return "Managed profile";
  }
  return "Unknown";
}

// static
std::vector<std::string> AdvancedSettings::AllUserVisibleStrings() {
  std::vector<std::string> out;
  static const char* const kProbes[] = {
      "https://lists.example/list.txt", "http://lists.example/list.txt",
      "https://doh.example/dns-query",  "http://doh.example/dns-query",
      "9.9.9.9",                        "system",
      "socks5://127.0.0.1:9050",        "http://proxy.example:8080",
      "user:pw@proxy.example:8080",     "Mozilla/5.0 (custom)",
      "camera",                         "default-src 'self'",
      "unsafe-inline",                  "telemetry=on",
      "locked",                         "ftp://proxy.example"};
  for (int control = 0; control <= static_cast<int>(AdvancedControl::kMaxValue); ++control) {
    out.push_back(Describe(static_cast<AdvancedControl>(control)));
    for (const char* probe : kProbes) {
      for (const char* scope : {"", "example.com", "*"}) {
        for (bool policy : {false, true}) {
          const Decision decision = Evaluate({static_cast<AdvancedControl>(control),
                                              probe, scope, policy});
          if (!decision.message.empty()) {
            out.push_back(decision.message);
          }
        }
      }
    }
  }
  for (const Guard& guard : Guards()) {
    out.push_back(guard.rule);
    out.push_back(guard.why);
  }
  return out;
}

}  // namespace settings
}  // namespace bedrock
