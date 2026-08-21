// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/update/update_provider.h"

#include <iostream>
#include <string>

#include "bedrock/privacy/telemetry_policy.h"

namespace {

using namespace bedrock::update;  // NOLINT — test-local convenience
using bedrock::privacy::DisclosureText;
using bedrock::privacy::TelemetryCategory;
using bedrock::privacy::TelemetryPolicy;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

class FakeVerifier : public SignatureVerifier {
 public:
  bool IsTrustedKey(const std::string& key_id) const override {
    return key_id == "bedrock-release-2026";
  }
  bool Verify(const std::string& payload,
              const std::string& signature,
              const std::string& key_id) const override {
    return IsTrustedKey(key_id) && signature == "sig:" + payload;
  }
};

class FakeProvider : public UpdateProvider {
 public:
  explicit FakeProvider(ProviderConfig config) : config_(std::move(config)) {}
  ProviderKind kind() const override { return config_.kind; }
  const ProviderConfig& config() const override { return config_; }
  bool FetchManifest(ReleaseManifest*) const override { return true; }

 private:
  ProviderConfig config_;
};

ProviderConfig Config(ProviderKind kind, const std::string& url) {
  ProviderConfig config;
  config.kind = kind;
  config.name = "test";
  config.base_url = url;
  return config;
}

ReleaseManifest Manifest(const std::string& version,
                         const std::string& hash = "abc123") {
  ReleaseManifest manifest;
  manifest.version = version;
  manifest.channel = "stable";
  manifest.payload_url = "https://example.org/bedrock.tar.xz";
  manifest.payload_sha256 = hash;
  manifest.key_id = "bedrock-release-2026";
  manifest.signature = "sig:" + version + hash;
  return manifest;
}

}  // namespace

int main() {
  // ---------- item 39: zero telemetry ----------
  TelemetryPolicy telemetry;
  Check(!telemetry.AnyEnabled(), "nothing is collected by default");
  for (TelemetryCategory category : TelemetryPolicy::Categories()) {
    Check(!telemetry.Enabled(category),
          std::string("off by default: ") +
              TelemetryPolicy::CategoryName(category));
    Check(telemetry.Endpoint(category).empty(),
          "and there is nowhere to send it");
  }
  Check(TelemetryPolicy::Categories().size() == 6,
        "all six categories from the roadmap are named");

  // Five of the six can never be turned on, by anyone.
  for (TelemetryCategory category : TelemetryPolicy::Categories()) {
    if (category == TelemetryCategory::kCrashReports)
      continue;
    Check(TelemetryPolicy::IsPermanentlyProhibited(category),
          "permanently prohibited: " +
              std::string(TelemetryPolicy::CategoryName(category)));
    Check(!telemetry.OptIn(category, TelemetryPolicy::CrashReportDisclosure()),
          "cannot be opted into even deliberately");
  }

  // Crash reporting: opt-in only, and only against the current disclosure.
  DisclosureText stale = TelemetryPolicy::CrashReportDisclosure();
  stale.version = "crash-disclosure-0";
  Check(!telemetry.OptIn(TelemetryCategory::kCrashReports, stale),
        "consent to an old disclosure is not consent to this one");
  DisclosureText empty;
  Check(!telemetry.OptIn(TelemetryCategory::kCrashReports, empty),
        "an empty disclosure cannot be agreed to");

  const DisclosureText current = TelemetryPolicy::CrashReportDisclosure();
  Check(!current.what_is_sent.empty() && !current.where_it_goes.empty() &&
            !current.how_to_turn_it_off.empty(),
        "the disclosure says what, where and how to stop");
  Check(current.what_is_sent.find("page content") != std::string::npos,
        "and admits a crash dump can contain page content");

  Check(telemetry.OptIn(TelemetryCategory::kCrashReports, current),
        "crash reporting can be turned on deliberately");
  Check(telemetry.Enabled(TelemetryCategory::kCrashReports), "and is then on");
  Check(telemetry.Endpoint(TelemetryCategory::kCrashReports).empty(),
        "but with no crash server configured there is still nowhere to send");
  telemetry.SetCrashEndpointForOptIn("https://crash.example.org/report");
  Check(!telemetry.Endpoint(TelemetryCategory::kCrashReports).empty(),
        "an endpoint appears only once the user configured one");

  DisclosureText changed = current;
  changed.version = "crash-disclosure-2";
  telemetry.OnDisclosureChanged(changed);
  Check(!telemetry.Enabled(TelemetryCategory::kCrashReports),
        "changing the terms revokes the consent instead of assuming it");

  const std::string summary = TelemetryPolicy::Summary();
  Check(summary.find("collects nothing") != std::string::npos,
        "the settings summary states the default plainly");

  // ---------- item 40: updates ----------
  FakeVerifier verifier;
  UpdateChecker checker(&verifier, "151.0.7922.173");

  Check(UpdateChecker::CompareVersions("151.0.7922.9", "151.0.7922.10") < 0,
        "versions compare numerically, not as strings");
  Check(UpdateChecker::CompareVersions("152.0.0.0", "151.0.7922.173") > 0,
        "a newer major is newer");
  Check(UpdateChecker::CompareVersions("151.0.7922.173", "151.0.7922.173") == 0,
        "equal is equal");

  const FakeProvider github(
      Config(ProviderKind::kGitHubReleases, "https://github.com/example/repo"));
  const FakeProvider static_https(
      Config(ProviderKind::kStaticHttps, "https://downloads.example.org/"));
  const FakeProvider enterprise(
      Config(ProviderKind::kEnterpriseInternal, "https://updates.corp.example/"));
  const FakeProvider distro(
      Config(ProviderKind::kPackageRepository, ""));
  const FakeProvider insecure(
      Config(ProviderKind::kStaticHttps, "http://downloads.example.org/"));

  // The same rules hold for every provider — that is the point of the
  // abstraction.
  const UpdateProvider* const kProviders[] = {&github, &static_https,
                                              &enterprise};
  for (const UpdateProvider* provider : kProviders) {
    Check(checker.Check(*provider, Manifest("151.0.7923.1"), "abc123") ==
              UpdateStatus::kUpdateAvailable,
          "a newer signed release is offered");
    Check(checker.Check(*provider, Manifest("151.0.7922.173"), "abc123") ==
              UpdateStatus::kUpToDate,
          "the running version is up to date");
    Check(checker.Check(*provider, Manifest("151.0.7900.1"), "abc123") ==
              UpdateStatus::kRefusedDowngrade,
          "an older release is refused, whatever the provider says");

    ReleaseManifest unsigned_manifest = Manifest("151.0.7923.1");
    unsigned_manifest.signature.clear();
    Check(checker.Check(*provider, unsigned_manifest, "abc123") ==
              UpdateStatus::kRefusedUnsigned,
          "an unsigned release is refused");

    ReleaseManifest wrong_key = Manifest("151.0.7923.1");
    wrong_key.key_id = "someone-elses-key";
    Check(checker.Check(*provider, wrong_key, "abc123") ==
              UpdateStatus::kRefusedUnknownKey,
          "an unknown signing key is refused");

    ReleaseManifest tampered = Manifest("151.0.7923.1");
    tampered.signature = "sig:something-else";
    Check(checker.Check(*provider, tampered, "abc123") ==
              UpdateStatus::kRefusedBadSignature,
          "a bad signature is refused");

    Check(checker.Check(*provider, Manifest("151.0.7923.1"), "different-hash") ==
              UpdateStatus::kRefusedHashMismatch,
          "a payload that does not match the manifest is refused");
  }

  Check(checker.Check(insecure, Manifest("151.0.7923.1"), "abc123") ==
            UpdateStatus::kRefusedInsecureTransport,
        "plain http is never an update channel");
  Check(checker.Check(distro, Manifest("151.0.7923.1"), "abc123") ==
            UpdateStatus::kManagedByOperatingSystem,
        "a distro package is updated by the distro, not by a second updater");

  // No domain is compiled in: the messages must not name one.
  for (UpdateStatus status :
       {UpdateStatus::kUpToDate, UpdateStatus::kUpdateAvailable,
        UpdateStatus::kRefusedUnsigned, UpdateStatus::kRefusedBadSignature,
        UpdateStatus::kRefusedUnknownKey, UpdateStatus::kRefusedHashMismatch,
        UpdateStatus::kRefusedDowngrade,
        UpdateStatus::kRefusedInsecureTransport,
        UpdateStatus::kProviderUnavailable,
        UpdateStatus::kManagedByOperatingSystem}) {
    const std::string message = UpdateChecker::StatusMessage(status);
    Check(message.find("http") == std::string::npos &&
              message.find(".io") == std::string::npos &&
              message.find(".com") == std::string::npos,
          "no update endpoint is baked into the browser: " + message);
    Check(message.size() > 10, "every status has a real message");
  }
  Check(std::string(UpdateChecker::StatusMessage(
            UpdateStatus::kProviderUnavailable))
                .find("kept the version") != std::string::npos,
        "an unreachable provider keeps the current version instead of "
        "pretending everything is fine");

  if (failures == 0)
    std::cout << "update_provider_test: all assertions passed\n";
  return failures == 0 ? 0 : 1;
}
