// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/security/security_baseline.h"

#include <algorithm>
#include <string>
#include <vector>

namespace bedrock {
namespace security {

// static
const std::vector<Feature>& SecurityBaseline::Required() {
  static const std::vector<Feature> kRequired = [] {
    std::vector<Feature> features;
    for (int i = 0; i <= static_cast<int>(Feature::kMaxValue); ++i) {
      features.push_back(static_cast<Feature>(i));
    }
    return features;
  }();
  return kRequired;
}

// static
const std::vector<std::string>& SecurityBaseline::ForbiddenSwitches() {
  // Each of these has been used, in some fork or another, to make a privacy
  // feature easier to implement. None is worth it.
  static const std::vector<std::string> kForbidden = {
      "--no-sandbox",
      "--disable-site-isolation-trials",
      "--disable-web-security",
      "--allow-running-insecure-content",
      "--ignore-certificate-errors",
      "--ignore-certificate-errors-spki-list",
      "--disable-features=IsolateOrigins,site-per-process",
      "--disable-gpu-sandbox",
      "--disable-namespace-sandbox",
      "--disable-seccomp-filter-sandbox",
      "--allow-insecure-localhost",
      "--disable-blink-features=StrictMimeTypeCheck",
      "--js-flags=--no-sandbox",
      "--unsafely-treat-insecure-origin-as-secure",
  };
  return kForbidden;
}

// static
Configuration SecurityBaseline::ShippingDefault() {
  Configuration configuration;
  configuration.enabled = Required();
  configuration.gn_args = {
      "is_official_build=true",
      "symbol_level=1",
      "enable_ipc_fuzzer=false",
      "v8_enable_sandbox=true",
      "use_partition_alloc=true",
  };
  return configuration;
}

std::vector<Violation> SecurityBaseline::Audit(
    const Configuration& configuration) {
  std::vector<Violation> violations;

  for (Feature feature : Required()) {
    const bool enabled =
        std::find(configuration.enabled.begin(), configuration.enabled.end(),
                  feature) != configuration.enabled.end();
    if (enabled) {
      continue;
    }
    const bool unsupported = std::find(configuration.platform_unsupported.begin(),
                                       configuration.platform_unsupported.end(),
                                       feature) !=
                             configuration.platform_unsupported.end();
    if (unsupported) {
      // Documented platform gap, not a decision we made. Still recorded, so it
      // shows up in the security page rather than being forgotten.
      continue;
    }
    violations.push_back({std::string("disabled: ") + FeatureName(feature),
                          WhyItMatters(feature)});
  }

  for (const std::string& forbidden : ForbiddenSwitches()) {
    if (std::find(configuration.switches.begin(), configuration.switches.end(),
                  forbidden) != configuration.switches.end()) {
      violations.push_back(
          {"forbidden switch: " + forbidden,
           "This switch removes a protection every other Chromium user has. No "
           "privacy feature justifies shipping it."});
    }
    // The same strings also appear as gn args in some forks.
    if (std::find(configuration.gn_args.begin(), configuration.gn_args.end(),
                  forbidden) != configuration.gn_args.end()) {
      violations.push_back({"forbidden build flag: " + forbidden,
                            "Same as the switch: not shippable."});
    }
  }
  return violations;
}

// static
const char* SecurityBaseline::FeatureName(Feature feature) {
  switch (feature) {
    case Feature::kSandbox:               return "process sandbox";
    case Feature::kSiteIsolation:         return "site isolation";
    case Feature::kStrictOriginIsolation: return "origin isolation";
    case Feature::kRendererIsolation:     return "renderer isolation";
    case Feature::kIpcValidation:         return "IPC validation";
    case Feature::kSecureOriginsModel:    return "secure origins model";
    case Feature::kPermissionBoundaries:  return "permission boundaries";
    case Feature::kMemorySafetyMitigations: return "memory safety mitigations";
    case Feature::kV8Sandbox:             return "V8 sandbox";
    case Feature::kNetworkServiceSandbox: return "network service sandbox";
    case Feature::kGpuSandbox:            return "GPU sandbox";
    case Feature::kSpectreMitigations:    return "Spectre mitigations";
    case Feature::kSafeBrowsingLocalLists: return "local malware lists";
  }
  return "";
}

// static
const char* SecurityBaseline::WhyItMatters(Feature feature) {
  switch (feature) {
    case Feature::kSandbox:
      return "Without it, a bug in a web page becomes a bug on your computer.";
    case Feature::kSiteIsolation:
      return "Keeps each site in its own process, so one site cannot read "
             "another's memory. This is also the main defence against Spectre.";
    case Feature::kStrictOriginIsolation:
      return "Separates origins within a site where they ask for it.";
    case Feature::kRendererIsolation:
      return "Page content never runs with browser privileges.";
    case Feature::kIpcValidation:
      return "A compromised renderer cannot make the browser process do "
             "anything it likes by sending malformed messages.";
    case Feature::kSecureOriginsModel:
      return "Powerful features stay unavailable to pages loaded over plain "
             "HTTP.";
    case Feature::kPermissionBoundaries:
      return "A permission granted to one site stays with that site.";
    case Feature::kMemorySafetyMitigations:
      return "Hardened allocation and control-flow checks turn many memory "
             "bugs into crashes instead of exploits.";
    case Feature::kV8Sandbox:
      return "Contains bugs in the JavaScript engine, the most attacked code "
             "in the browser.";
    case Feature::kNetworkServiceSandbox:
      return "The component that parses hostile bytes from the network runs "
             "with the fewest privileges we can give it.";
    case Feature::kGpuSandbox:
      return "Graphics drivers are large and fragile; the GPU process is "
             "confined.";
    case Feature::kSpectreMitigations:
      return "Blocks a class of attacks that read memory across security "
             "boundaries.";
    case Feature::kSafeBrowsingLocalLists:
      return "Known malware and phishing sites are blocked using lists stored "
             "on your device. Bedrock does not send the pages you visit to any "
             "server to check them.";
  }
  return "";
}

}  // namespace security
}  // namespace bedrock
