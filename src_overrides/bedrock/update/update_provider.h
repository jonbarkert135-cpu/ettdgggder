// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_UPDATE_UPDATE_PROVIDER_H_
#define BEDROCK_UPDATE_UPDATE_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

// Update system (roadmap item 40).
//
// The browser must work without the project's infrastructure, and must still
// update safely. Those two requirements meet here: **where updates come from
// is configuration, not architecture.**
//
// `UpdateProvider` is abstract. GitHub Releases, a static HTTPS directory, a
// distribution package repository and an enterprise share are all just
// implementations. No domain is compiled into this code — the test asserts
// that the class holds no hostname at all, because "temporarily" hardcoding
// update.bedrock.example is exactly how a project ends up with mandatory
// infrastructure it did not plan.
//
// What is *not* configurable, because it is the security of the update path:
//
//   - a release manifest must be signed, and the signature verified against a
//     key that shipped with the browser (`SignatureVerifier`, injected —
//     crypto belongs to the platform, not to this file);
//   - the payload hash in the manifest must match the payload;
//   - the version must be newer than the running one (no silent downgrade to
//     a build with a known hole);
//   - a failed check keeps the current version. An update that cannot be
//     verified is not installed, ever, on any provider.

namespace bedrock {
namespace update {

enum class Channel {
  kStable,
  kBeta,
  kNightly,
};

enum class ProviderKind {
  kGitHubReleases,
  kStaticHttps,
  kPackageRepository,   // apt/dnf/pacman — the OS verifies, we do not duplicate
  kEnterpriseInternal,
  kManualOnly,          // no automatic checks at all
};

struct ReleaseManifest {
  std::string version;      // "151.0.7922.173"
  std::string channel;
  std::string payload_url;
  std::string payload_sha256;
  std::string signature;    // over the manifest bytes
  std::string key_id;
  int64_t published_at = 0;
};

struct Payload {
  std::string url;
  std::string sha256;
  int64_t size = 0;
};

enum class UpdateStatus {
  kUpToDate,
  kUpdateAvailable,
  kRefusedUnsigned,
  kRefusedBadSignature,
  kRefusedUnknownKey,
  kRefusedHashMismatch,
  kRefusedDowngrade,
  kRefusedInsecureTransport,
  kProviderUnavailable,
  kManagedByOperatingSystem,
};

class SignatureVerifier {
 public:
  virtual ~SignatureVerifier() = default;
  virtual bool IsTrustedKey(const std::string& key_id) const = 0;
  virtual bool Verify(const std::string& payload,
                      const std::string& signature,
                      const std::string& key_id) const = 0;
};

// Where to look. Everything here comes from configuration or enterprise
// policy; nothing is baked into the binary.
struct ProviderConfig {
  ProviderKind kind = ProviderKind::kManualOnly;
  std::string name;
  std::string base_url;   // must be https:// (or empty for OS-managed)
  Channel channel = Channel::kStable;
  bool automatic_checks = true;
};

class UpdateProvider {
 public:
  virtual ~UpdateProvider() = default;
  virtual ProviderKind kind() const = 0;
  virtual const ProviderConfig& config() const = 0;
  // Returns false when the provider cannot be reached; the caller keeps the
  // current version and says so, rather than treating silence as "up to date".
  virtual bool FetchManifest(ReleaseManifest* out) const = 0;
};

// Everything that must hold no matter which provider is plugged in.
class UpdateChecker {
 public:
  UpdateChecker(const SignatureVerifier* verifier,
                std::string current_version);
  ~UpdateChecker();

  UpdateStatus Check(const UpdateProvider& provider,
                     const ReleaseManifest& manifest,
                     const std::string& observed_payload_sha256) const;

  // "151.0.7922.173" < "151.0.7923.0". Compares numerically, component by
  // component: string comparison would make 9 newer than 10.
  static int CompareVersions(const std::string& a, const std::string& b);
  static const char* StatusMessage(UpdateStatus status);

 private:
  const SignatureVerifier* verifier_;
  std::string current_version_;
};

}  // namespace update
}  // namespace bedrock

#endif  // BEDROCK_UPDATE_UPDATE_PROVIDER_H_
