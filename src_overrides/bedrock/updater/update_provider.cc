// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/updater/update_provider.h"

#include <cstddef>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace bedrock {
namespace update {
namespace {

std::vector<long> Parts(const std::string& version) {
  std::vector<long> parts;
  std::string current;
  for (char c : version) {
    if (c == '.') {
      parts.push_back(std::atol(current.c_str()));
      current.clear();
    } else {
      current += c;
    }
  }
  parts.push_back(std::atol(current.c_str()));
  return parts;
}

}  // namespace

UpdateChecker::UpdateChecker(const SignatureVerifier* verifier,
                             std::string current_version)
    : verifier_(verifier), current_version_(std::move(current_version)) {}

UpdateChecker::~UpdateChecker() = default;

int UpdateChecker::CompareVersions(const std::string& a, const std::string& b) {
  const std::vector<long> left = Parts(a);
  const std::vector<long> right = Parts(b);
  const size_t count = left.size() > right.size() ? left.size() : right.size();
  for (size_t i = 0; i < count; ++i) {
    const long l = i < left.size() ? left[i] : 0;
    const long r = i < right.size() ? right[i] : 0;
    if (l != r)
      return l < r ? -1 : 1;
  }
  return 0;
}

UpdateStatus UpdateChecker::Check(
    const UpdateProvider& provider,
    const ReleaseManifest& manifest,
    const std::string& observed_payload_sha256) const {
  const ProviderConfig& config = provider.config();

  // The OS package manager already signs and verifies; duplicating that here
  // would mean two update paths fighting over the same binary.
  if (config.kind == ProviderKind::kPackageRepository)
    return UpdateStatus::kManagedByOperatingSystem;

  // Plain http is not an update channel. This holds for an enterprise mirror
  // as much as for a public one.
  if (config.base_url.rfind("https://", 0) != 0)
    return UpdateStatus::kRefusedInsecureTransport;

  if (manifest.signature.empty())
    return UpdateStatus::kRefusedUnsigned;
  if (!verifier_->IsTrustedKey(manifest.key_id))
    return UpdateStatus::kRefusedUnknownKey;
  if (!verifier_->Verify(manifest.version + manifest.payload_sha256,
                         manifest.signature, manifest.key_id)) {
    return UpdateStatus::kRefusedBadSignature;
  }

  if (CompareVersions(manifest.version, current_version_) <= 0) {
    // Equal is "up to date"; older is an attempt to walk the user back onto a
    // build whose bugs are public.
    return CompareVersions(manifest.version, current_version_) == 0
               ? UpdateStatus::kUpToDate
               : UpdateStatus::kRefusedDowngrade;
  }

  // The manifest is trusted only for what it claims; the bytes still have to
  // match the claim.
  if (!observed_payload_sha256.empty() &&
      observed_payload_sha256 != manifest.payload_sha256) {
    return UpdateStatus::kRefusedHashMismatch;
  }

  return UpdateStatus::kUpdateAvailable;
}

const char* UpdateChecker::StatusMessage(UpdateStatus status) {
  switch (status) {
    case UpdateStatus::kUpToDate:
      return "Bedrock is up to date.";
    case UpdateStatus::kUpdateAvailable:
      return "An update is available.";
    case UpdateStatus::kRefusedUnsigned:
      return "The update was not installed: the release is not signed.";
    case UpdateStatus::kRefusedBadSignature:
      return "The update was not installed: the signature does not match.";
    case UpdateStatus::kRefusedUnknownKey:
      return "The update was not installed: it is signed by an unknown key.";
    case UpdateStatus::kRefusedHashMismatch:
      return "The update was not installed: the downloaded file does not "
             "match the release.";
    case UpdateStatus::kRefusedDowngrade:
      return "The update was not installed: it is older than the version you "
             "are running.";
    case UpdateStatus::kRefusedInsecureTransport:
      return "The update was not installed: the update source is not HTTPS.";
    case UpdateStatus::kProviderUnavailable:
      return "Could not reach the update source. Bedrock kept the version you "
             "have.";
    case UpdateStatus::kManagedByOperatingSystem:
      return "Updates for this installation come from your system package "
             "manager.";
  }
  return "Unknown update status.";
}

}  // namespace update
}  // namespace bedrock
