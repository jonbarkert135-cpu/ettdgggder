// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/settings/advanced_settings.h"

#include <cstdio>
#include <set>
#include <string>

// The interesting property of an advanced settings surface is not what it
// allows — it is what it refuses while an administrator is holding it wrong.

namespace {

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::printf("  FAIL: %s\n", what.c_str());
    ++failures;
  }
}

using namespace bedrock::settings;

Decision Eval(AdvancedControl control, const std::string& value,
              const std::string& scope = "", bool from_policy = false) {
  return AdvancedSettings::Evaluate({control, value, scope, from_policy});
}

void SecurityMechanismsAreNotConfigurable() {
  // The forum "fixes". Every one of them, from every control, including when an
  // administrator pushes it as policy — the point of guard G9.
  for (const char* value : {"ignore-certificate-errors", "disable-web-security",
                            "no-sandbox", "disable-site-isolation",
                            "allow-running-insecure-content"}) {
    for (int control = 0; control <= static_cast<int>(AdvancedControl::kMaxValue);
         ++control) {
      const Decision from_gui =
          Eval(static_cast<AdvancedControl>(control), value, "example.com", false);
      const Decision from_policy =
          Eval(static_cast<AdvancedControl>(control), value, "example.com", true);
      Check(from_gui.verdict == Verdict::kRejected,
            std::string("GUI must refuse ") + value);
      Check(from_policy.verdict == Verdict::kRejected,
            std::string("policy must refuse ") + value + " too");
    }
  }
}

void PolicyCannotSwitchOnReporting() {
  const Decision decision =
      Eval(AdvancedControl::kManagedProfile, "telemetry=on", "", true);
  Check(decision.verdict == Verdict::kRejected, "policy cannot enable telemetry");
  Check(decision.message.find("G1") != std::string::npos, "rejection names its guard");
}

void RemoteConfigurationNeedsHttps() {
  Check(!Eval(AdvancedControl::kCustomFilterList, "http://lists.example/l.txt").ok(),
        "plain-HTTP filter list refused");
  Check(Eval(AdvancedControl::kCustomFilterList, "https://lists.example/l.txt").ok(),
        "HTTPS filter list accepted");
  Check(!Eval(AdvancedControl::kCustomDns, "http://doh.example/dns-query").ok(),
        "plaintext DoH endpoint refused");
  Check(Eval(AdvancedControl::kCustomDns, "https://doh.example/dns-query").ok(),
        "HTTPS DoH endpoint accepted");
}

void AcceptedButCostlyChoicesWarn() {
  // The user is allowed to do these. They are not allowed to be surprised.
  const Decision plain_dns = Eval(AdvancedControl::kCustomDns, "9.9.9.9");
  Check(plain_dns.verdict == Verdict::kAcceptedWithWarning,
        "unencrypted DNS is accepted with a warning, not silently");
  const Decision http_proxy =
      Eval(AdvancedControl::kCustomProxy, "http://proxy.example:8080");
  Check(http_proxy.verdict == Verdict::kAcceptedWithWarning, "HTTP proxy warns");
  Check(!http_proxy.message.empty(), "a warning verdict carries text to show");
  const Decision list =
      Eval(AdvancedControl::kCustomFilterList, "https://lists.example/l.txt");
  Check(list.message.find("licence") != std::string::npos,
        "a user-added list states whose licence problem it is");
}

void CustomUserAgentIsNotSoldAsPrivacy() {
  const Decision global = Eval(AdvancedControl::kUserAgentPolicy, "Mozilla/5.0 (me)");
  Check(global.verdict == Verdict::kRejected, "no global custom UA");
  const Decision per_site =
      Eval(AdvancedControl::kUserAgentPolicy, "Mozilla/5.0 (me)", "example.com");
  Check(per_site.verdict == Verdict::kAcceptedWithWarning, "per-site UA warns");
  Check(per_site.message.find("not a privacy feature") != std::string::npos,
        "the warning says plainly that this is not privacy");
  Check(Eval(AdvancedControl::kUserAgentPolicy, "default").verdict == Verdict::kAccepted,
        "the shipped UA needs no warning");
}

void PermissionsCannotBeGrantedEverywhere() {
  Check(!Eval(AdvancedControl::kSitePermission, "camera", "*").ok(),
        "wildcard grant refused");
  Check(!Eval(AdvancedControl::kSitePermission, "camera", "<all_urls>").ok(),
        "the other wildcard spelling is refused too");
  Check(Eval(AdvancedControl::kSitePermission, "camera", "example.com").ok(),
        "a real site is fine");
}

void ContentPolicyOnlyTightens() {
  for (const char* relaxing : {"unsafe-inline", "unsafe-eval", "relax-csp"}) {
    Check(!Eval(AdvancedControl::kContentPolicy, relaxing, "example.com").ok(),
          std::string("refuses to relax a site's CSP: ") + relaxing);
  }
  Check(Eval(AdvancedControl::kContentPolicy, "default-src 'self'", "example.com").ok(),
        "a tightening policy is accepted");
}

void ProxyCredentialsStayOutOfUrls() {
  Check(!Eval(AdvancedControl::kCustomProxy, "socks5://user:pw@proxy.example:9050").ok(),
        "credentials in a proxy URL refused");
  Check(!Eval(AdvancedControl::kCustomProxy, "ftp://proxy.example:21").ok(),
        "unknown proxy scheme refused");
  const Decision socks = Eval(AdvancedControl::kCustomProxy, "socks5://127.0.0.1:9050");
  Check(socks.ok() && socks.message.find("Tor windows") != std::string::npos,
        "the user is told a proxy does not touch Tor windows");
}

void EveryRejectionExplainsItselfAndNamesAGuard() {
  std::set<std::string> guard_ids;
  for (const Guard& guard : AdvancedSettings::Guards()) {
    Check(guard_ids.insert(guard.id).second, std::string("duplicate guard ") + guard.id);
    Check(std::string(guard.rule).size() > 20 && std::string(guard.why).size() > 20,
          std::string("guard ") + guard.id + " states its rule and its reason");
  }
  Check(guard_ids.size() >= 8, "the guard list is not a token gesture");

  int rejections = 0;
  for (int control = 0; control <= static_cast<int>(AdvancedControl::kMaxValue); ++control) {
    for (const char* value : {"", "no-sandbox", "http://x.example", "unsafe-inline",
                              "user:pw@h:1", "Mozilla/5.0 (me)"}) {
      const Decision decision =
          Eval(static_cast<AdvancedControl>(control), value, "", false);
      if (decision.verdict == Verdict::kRejected) {
        ++rejections;
        Check(decision.message.size() > 20,
              std::string("rejection of '") + value + "' explains itself");
        bool names_guard = false;
        for (const std::string& id : guard_ids) {
          names_guard = names_guard || decision.message.find(id) != std::string::npos;
        }
        Check(names_guard, "rejection names the guard it hit");
      }
    }
  }
  Check(rejections > 10, "the probes actually exercised the refusals");
}

void NoAdvancedControlPromisesAnonymity() {
  const char* banned[] = {"anonymous", "anonymity", "untraceable", "100%",
                          "completely private", "invisible"};
  for (const std::string& text : AdvancedSettings::AllUserVisibleStrings()) {
    for (const char* word : banned) {
      Check(text.find(word) == std::string::npos,
            std::string("advanced settings must not promise '") + word + "': " + text);
    }
  }
}

}  // namespace

int main() {
  std::printf("advanced_settings_test\n");
  SecurityMechanismsAreNotConfigurable();
  PolicyCannotSwitchOnReporting();
  RemoteConfigurationNeedsHttps();
  AcceptedButCostlyChoicesWarn();
  CustomUserAgentIsNotSoldAsPrivacy();
  PermissionsCannotBeGrantedEverywhere();
  ContentPolicyOnlyTightens();
  ProxyCredentialsStayOutOfUrls();
  EveryRejectionExplainsItselfAndNamesAGuard();
  NoAdvancedControlPromisesAnonymity();
  std::printf(failures == 0 ? "  ok\n" : "  %d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
