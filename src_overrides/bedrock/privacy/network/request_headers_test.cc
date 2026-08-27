// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/privacy/network/request_headers.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace bedrock::net;  // NOLINT — test-local convenience
using bedrock::privacy::Control;
using bedrock::privacy::FpLevel;
using bedrock::privacy::ProtectionController;
using bedrock::privacy::Scope;
using bedrock::privacy::Value;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

void CheckEq(const std::string& got,
             const std::string& want,
             const std::string& what) {
  if (got != want) {
    std::cerr << "FAIL " << what << ": got \"" << got << "\", want \"" << want
              << "\"\n";
    ++failures;
  }
}

OutgoingRequest SameSiteNav() {
  OutgoingRequest request;
  request.initiator_url = "https://news.example.test/articles/42?q=secret#note";
  request.initiator_host = "news.example.test";
  request.initiator_etld1 = "example.test";
  request.target_url = "https://shop.example.test/item";
  request.target_host = "shop.example.test";
  request.target_etld1 = "example.test";
  request.navigation = true;
  return request;
}

OutgoingRequest CrossSiteNav() {
  OutgoingRequest request = SameSiteNav();
  request.target_url = "https://tracker.other.test/pixel";
  request.target_host = "tracker.other.test";
  request.target_etld1 = "other.test";
  request.navigation = false;
  return request;
}

bool Has(const std::vector<Hint>& hints, Hint hint) {
  return std::find(hints.begin(), hints.end(), hint) != hints.end();
}

std::string HeaderOf(const std::vector<HeaderValue>& headers,
                     const std::string& name) {
  for (const HeaderValue& header : headers) {
    if (header.name == name) {
      return header.value;
    }
  }
  return std::string();
}

void TestUrlHelpers() {
  CheckEq(RequestHeaderPolicy::OriginOf("https://EXAMPLE.test./a/b?x=1"),
          "https://example.test/", "origin is normalised and path-free");
  CheckEq(RequestHeaderPolicy::OriginOf("https://user:pw@example.test/a"),
          "https://example.test/", "credentials never reach an origin");
  CheckEq(RequestHeaderPolicy::OriginOf("data:text/html,<p>hi"), "",
          "an opaque origin has no origin string");
  CheckEq(RequestHeaderPolicy::SanitizeUrl(
              "https://example.test/a/b?q=secret#fragment"),
          "https://example.test/a/b?q=secret", "the fragment is dropped");
  CheckEq(RequestHeaderPolicy::SanitizeUrl("https://u:p@example.test/a#f"),
          "https://example.test/a", "credentials are dropped from a referrer");
}

void TestReferrerFloor() {
  ProtectionController controls;
  RequestHeaderPolicy policy(&controls);

  // Default (the Balanced preset): your own site keeps the path, another site
  // learns only which site sent you.
  CheckEq(policy.ReferrerFor(SameSiteNav()),
          "https://news.example.test/articles/42?q=secret",
          "same-site keeps the full sanitised URL");
  CheckEq(policy.ReferrerFor(CrossSiteNav()), "https://news.example.test/",
          "cross-site is trimmed to the origin");

  // Strict: cross-site sends nothing, same-site loses the path too.
  controls.Set(Scope::kGlobal, "", Control::kReferrer, Value::kBlock);
  CheckEq(policy.ReferrerFor(CrossSiteNav()), "",
          "kBlock sends no cross-site referrer");
  CheckEq(policy.ReferrerFor(SameSiteNav()), "https://news.example.test/",
          "kBlock trims the same-site referrer to the origin");

  controls.Set(Scope::kGlobal, "", Control::kReferrer, Value::kBlockStrict);
  CheckEq(policy.ReferrerFor(SameSiteNav()), "",
          "kBlockStrict sends nothing at all");

  controls.Set(Scope::kGlobal, "", Control::kReferrer, Value::kAllow);
  CheckEq(policy.ReferrerFor(CrossSiteNav()),
          "https://news.example.test/articles/42?q=secret",
          "with the control off the full URL is sent, minus the fragment");
}

void TestDowngradeAndOpaque() {
  ProtectionController controls;
  RequestHeaderPolicy policy(&controls);

  OutgoingRequest downgrade = CrossSiteNav();
  downgrade.target_url = "http://tracker.other.test/pixel";
  Check(policy.ScopeFor(downgrade) == ReferrerScope::kNone,
        "https -> http sends no referrer");

  // Even with the control off: the floor may be lowered, a downgrade may not.
  controls.Set(Scope::kGlobal, "", Control::kReferrer, Value::kAllow);
  CheckEq(policy.ReferrerFor(downgrade), "",
          "a downgraded request never carries a referrer, whatever the setting");
  downgrade.declared = DeclaredReferrerPolicy::kUnsafeUrl;
  CheckEq(policy.ReferrerFor(downgrade), "",
          "unsafe-url does not resurrect the downgrade case");

  OutgoingRequest opaque = SameSiteNav();
  opaque.initiator_url = "data:text/html,<a href=x>";
  Check(policy.ScopeFor(opaque) == ReferrerScope::kNone,
        "a document with an opaque origin sends no referrer");
}

void TestDeclaredPolicy() {
  ProtectionController controls;
  RequestHeaderPolicy policy(&controls);

  // Stricter than the floor: honoured.
  OutgoingRequest request = SameSiteNav();
  request.declared = DeclaredReferrerPolicy::kNoReferrer;
  CheckEq(policy.ReferrerFor(request), "", "no-referrer is honoured");
  Check(!policy.DeclaredPolicyRefused(request),
        "a stricter declared policy is not a refusal");

  request.declared = DeclaredReferrerPolicy::kOrigin;
  CheckEq(policy.ReferrerFor(request), "https://news.example.test/",
          "origin is honoured on a same-site request");

  // Looser than the floor: refused, and the refusal is visible.
  OutgoingRequest cross = CrossSiteNav();
  cross.declared = DeclaredReferrerPolicy::kUnsafeUrl;
  CheckEq(policy.ReferrerFor(cross), "https://news.example.test/",
          "unsafe-url cannot widen the cross-site referrer");
  Check(policy.DeclaredPolicyRefused(cross),
        "unsafe-url on a cross-site request is reported as refused");

  cross.declared = DeclaredReferrerPolicy::kNoReferrerWhenDowngrade;
  CheckEq(policy.ReferrerFor(cross), "https://news.example.test/",
          "no-referrer-when-downgrade is not looser than the floor either");

  // same-origin is about origins, not sites: a sibling subdomain is not it.
  OutgoingRequest sibling = SameSiteNav();
  sibling.declared = DeclaredReferrerPolicy::kSameOrigin;
  CheckEq(policy.ReferrerFor(sibling), "",
          "same-origin excludes a different subdomain of the same site");

  // A per-site override may tighten, never loosen (the https_policy rule).
  controls.Set(Scope::kSite, "news.example.test", Control::kReferrer,
               Value::kBlockStrict);
  OutgoingRequest tightened = SameSiteNav();
  CheckEq(policy.ReferrerFor(tightened), "",
          "a per-site strict value applies to the referring host");
}

void TestClientHintParties() {
  ProtectionController controls;
  RequestHeaderPolicy policy(&controls);
  const std::vector<Hint> accepted = {Hint::kUaArch, Hint::kUaPlatformVersion,
                                      Hint::kDeviceMemory, Hint::kViewportWidth,
                                      Hint::kRtt};

  // First party at the default level: low-entropy always, requested
  // high-entropy hints in reduced form, network quality never.
  const std::vector<Hint> first = policy.HintsFor(SameSiteNav(), accepted);
  Check(Has(first, Hint::kUa) && Has(first, Hint::kUaMobile) &&
            Has(first, Hint::kUaPlatform),
        "the three low-entropy hints are always sent");
  Check(Has(first, Hint::kDeviceMemory) && Has(first, Hint::kViewportWidth),
        "requested layout hints reach the first party, normalised");
  Check(!Has(first, Hint::kRtt), "network quality is not sent at kBalanced");
  Check(!Has(first, Hint::kUaArch) && !Has(first, Hint::kUaPlatformVersion),
        "no Sec-CH-UA identity hint goes on the wire from level 1 -- the claim "
        "in docs/privacy/fingerprinting/client-hints.md");
  Check(!Has(first, Hint::kPrefersColorScheme),
        "a hint nobody asked for is not sent even to the first party");

  // Third party: delegation is refused, at every level.
  const std::vector<Hint> third = policy.HintsFor(CrossSiteNav(), accepted);
  Check(Has(third, Hint::kUa), "a third party still gets the low-entropy hints");
  for (const Hint hint : {Hint::kUaArch, Hint::kUaPlatformVersion,
                          Hint::kDeviceMemory, Hint::kViewportWidth}) {
    Check(!Has(third, hint), "no high-entropy hint is delegated to a third party");
  }
  // With the shims off the identity hints are answered again -- but only to the
  // party that asked. Delegation stays refused at every level.
  policy.set_fp_level(FpLevel::kCompatibility);
  Check(Has(policy.HintsFor(SameSiteNav(), accepted), Hint::kUaArch),
        "kCompatibility answers the first party's identity hints");
  Check(!Has(policy.HintsFor(CrossSiteNav(), accepted), Hint::kUaArch),
        "delegation stays refused even with the shims off");
  Check(Has(policy.HintsFor(SameSiteNav(), accepted), Hint::kRtt),
        "network quality is answered only with the shims off");

  // Strict: no high-entropy hint at all, layout ones included. Maximum: nothing.
  policy.set_fp_level(FpLevel::kStrict);
  const std::vector<Hint> strict = policy.HintsFor(SameSiteNav(), accepted);
  Check(Has(strict, Hint::kUa) && !Has(strict, Hint::kUaArch) &&
            !Has(strict, Hint::kDeviceMemory),
        "kStrict keeps the low-entropy hints and drops the rest");
  policy.set_fp_level(FpLevel::kMaximum);
  Check(policy.HintsFor(SameSiteNav(), accepted).empty(),
        "kMaximum sends no client hints");
}

void TestClientHintValues() {
  ProtectionController controls;
  RequestHeaderPolicy policy(&controls);
  DeviceFacts facts;
  facts.device_memory_gb = 64;
  facts.window = {1443, 907};
  facts.dpr = 2.0;
  facts.platform_version = "10.0.19045";
  facts.architecture = "arm";
  facts.model = "Pixel 8";
  facts.full_version = "134.0.6998.35";
  facts.language = "en-GB";

  const std::vector<Hint> accepted = {Hint::kUaArch,      Hint::kUaBitness,
                                      Hint::kUaModel,     Hint::kUaPlatformVersion,
                                      Hint::kDeviceMemory, Hint::kDpr,
                                      Hint::kViewportWidth};
  const std::vector<HeaderValue> headers =
      policy.HintHeadersFor(SameSiteNav(), accepted, facts);

  // Rule 3: the header value is the value the JavaScript surface reports.
  CheckEq(HeaderOf(headers, "Device-Memory"),
          std::to_string(bedrock::privacy::NormalizedDeviceMemoryGb(
              facts.device_memory_gb, policy.fp_level())),
          "Device-Memory equals the normalised JS value");
  CheckEq(HeaderOf(headers, "Viewport-Width"),
          std::to_string(bedrock::privacy::QuantizeWindowSize(
                             facts.window, policy.fp_level())
                             .width),
          "Viewport-Width equals the letterboxed width");
  CheckEq(HeaderOf(headers, "DPR"), "1", "DPR is reported as 1 once reducing");
  for (const char* name : {"Sec-CH-UA-Platform-Version", "Sec-CH-UA-Arch",
                          "Sec-CH-UA-Bitness", "Sec-CH-UA-Model"}) {
    CheckEq(HeaderOf(headers, name), "",
            std::string(name) + " does not appear on the wire from level 1");
  }
  CheckEq(HeaderOf(headers, "Sec-CH-UA"), "\"Bedrock\";v=\"134\"",
          "Sec-CH-UA carries the major version only");
  CheckEq(policy.HintValue(Hint::kUaPlatformVersion, facts), "\"\"",
          "the reduced platform version is empty, not invented");
  CheckEq(policy.HintValue(Hint::kUaArch, facts), "\"x86\"",
          "the reduced architecture is the population value");
  CheckEq(policy.HintValue(Hint::kUaModel, facts), "",
          "an emptied model is an absent header, not an empty one");

  // Compatibility level: the real values, on purpose.
  policy.set_fp_level(FpLevel::kCompatibility);
  const std::vector<HeaderValue> raw =
      policy.HintHeadersFor(SameSiteNav(), accepted, facts);
  CheckEq(HeaderOf(raw, "Sec-CH-UA-Platform-Version"), "\"10.0.19045\"",
          "kCompatibility reports the platform version as it is");
  CheckEq(HeaderOf(raw, "DPR"), "2.0", "kCompatibility reports the real DPR");

  // One decimal place, sign handled by hand (no std::abs: it is not visible in
  // the C++ modules build). A fractional and a nonsense negative DPR must both
  // round-trip as written, not silently lose their sign.
  DeviceFacts fractional = facts;
  fractional.dpr = 1.5;
  CheckEq(HeaderOf(policy.HintHeadersFor(SameSiteNav(), accepted, fractional),
                   "DPR"),
          "1.5", "a fractional DPR keeps its one decimal place");
  DeviceFacts negative = facts;
  negative.dpr = -0.05;
  CheckEq(HeaderOf(policy.HintHeadersFor(SameSiteNav(), accepted, negative),
                   "DPR"),
          "-0.1", "a negative DPR keeps its sign instead of printing as 0.1");
  CheckEq(policy.AcceptLanguage(facts), "en-GB",
          "Accept-Language is the real one with the shims off");

  policy.set_fp_level(FpLevel::kBalanced);
  CheckEq(policy.AcceptLanguage(facts),
          bedrock::privacy::NormalizedLanguage(FpLevel::kBalanced),
          "Accept-Language follows the one language normaliser");

  // Save-Data is the user's own signal: sent when they enabled it, absent
  // otherwise, never invented.
  DeviceFacts saving = facts;
  saving.save_data_enabled = true;
  const std::vector<HeaderValue> with_save = policy.HintHeadersFor(
      SameSiteNav(), {Hint::kSaveData}, saving);
  CheckEq(HeaderOf(with_save, "Save-Data"), "on", "Save-Data is sent when on");
  CheckEq(HeaderOf(policy.HintHeadersFor(SameSiteNav(), {Hint::kSaveData}, facts),
                   "Save-Data"),
          "", "Save-Data is absent when the user has not enabled it");
}

void TestPartyAnalysis() {
  OutgoingRequest unknown = SameSiteNav();
  unknown.target_etld1 = "";
  Check(unknown.third_party(),
        "an unknown eTLD+1 counts as third party, the safe direction");

  OutgoingRequest cased = SameSiteNav();
  cased.target_etld1 = "EXAMPLE.test.";
  Check(!cased.third_party(),
        "the wire form of a site name does not make it a third party");
}

}  // namespace

int main() {
  TestUrlHelpers();
  TestReferrerFloor();
  TestDowngradeAndOpaque();
  TestDeclaredPolicy();
  TestClientHintParties();
  TestClientHintValues();
  TestPartyAnalysis();

  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "request_headers_test: all assertions passed\n";
  return 0;
}
