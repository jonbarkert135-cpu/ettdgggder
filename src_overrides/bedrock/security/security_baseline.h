// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_SECURITY_SECURITY_BASELINE_H_
#define BEDROCK_SECURITY_SECURITY_BASELINE_H_

#include <string>
#include <vector>

// Security baseline (roadmap item 24).
//
// Privacy work has a standard way of going wrong: a feature is easier to build
// if some Chromium security mechanism is out of the way, the switch gets
// flipped "temporarily", and the browser ships less safe than the Chrome it
// was derived from. A privacy browser that is easier to exploit is not a
// privacy browser.
//
// So the baseline is a list, and it is enforced by code: `Audit()` takes the
// build/runtime configuration and returns violations. The host test fails on
// any violation, which means a patch that disables the sandbox cannot land
// quietly — it has to delete an assertion, and that shows up in review.

namespace bedrock {
namespace security {

enum class Feature {
  kSandbox,
  kSiteIsolation,          // one site per process
  kStrictOriginIsolation,  // origin-keyed agent clusters where requested
  kRendererIsolation,
  kIpcValidation,          // mojo message validation
  kSecureOriginsModel,     // powerful features need a secure context
  kPermissionBoundaries,
  kMemorySafetyMitigations,  // PartitionAlloc hardening, CFI, etc.
  kV8Sandbox,
  kNetworkServiceSandbox,
  kGpuSandbox,
  kSpectreMitigations,
  kSafeBrowsingLocalLists,  // local-only lists; see the note in the .cc
  kMaxValue = kSafeBrowsingLocalLists,
};

struct Configuration {
  // Features currently enabled. Anything missing is a violation, unless it is
  // listed in `platform_unsupported` with a reason.
  std::vector<Feature> enabled;
  std::vector<Feature> platform_unsupported;
  std::vector<std::string> gn_args;      // build flags
  std::vector<std::string> switches;     // command-line switches
};

struct Violation {
  std::string what;
  std::string why_it_matters;
};

class SecurityBaseline {
 public:
  // Every feature that must be on, unless the platform genuinely lacks it.
  static const std::vector<Feature>& Required();

  // Build flags and command-line switches that are never acceptable in a
  // shipping Bedrock build, whatever privacy benefit is claimed for them.
  static const std::vector<std::string>& ForbiddenSwitches();

  static std::vector<Violation> Audit(const Configuration& configuration);

  // A configuration that is safe to ship.
  static Configuration ShippingDefault();

  static const char* FeatureName(Feature feature);
  static const char* WhyItMatters(Feature feature);
};

}  // namespace security
}  // namespace bedrock

#endif  // BEDROCK_SECURITY_SECURITY_BASELINE_H_
