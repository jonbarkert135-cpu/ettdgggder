// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/security/security_baseline.h"

#include <algorithm>
#include <iostream>
#include <string>

namespace {

using namespace bedrock::security;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

}  // namespace

int main() {
  // The configuration we intend to ship must be clean. This is the assertion
  // that makes "we did not weaken Chromium's security" a checked claim.
  {
    const Configuration shipping = SecurityBaseline::ShippingDefault();
    const auto violations = SecurityBaseline::Audit(shipping);
    for (const Violation& violation : violations) {
      std::cerr << "  violation: " << violation.what << "\n";
    }
    Check(violations.empty(), "the shipping configuration has no violations");
    Check(shipping.enabled.size() ==
              static_cast<size_t>(Feature::kMaxValue) + 1,
          "every baseline feature is on by default");
  }

  // Turning any single feature off is a violation with a reason attached.
  for (int i = 0; i <= static_cast<int>(Feature::kMaxValue); ++i) {
    const Feature feature = static_cast<Feature>(i);
    Configuration configuration = SecurityBaseline::ShippingDefault();
    configuration.enabled.erase(
        std::remove(configuration.enabled.begin(), configuration.enabled.end(),
                    feature),
        configuration.enabled.end());
    const auto violations = SecurityBaseline::Audit(configuration);
    Check(violations.size() == 1,
          std::string("disabling ") + SecurityBaseline::FeatureName(feature) +
              " is reported");
    Check(!violations.empty() && violations[0].why_it_matters.size() > 30,
          std::string("and explains why it matters: ") +
              SecurityBaseline::FeatureName(feature));
  }

  // A genuine platform gap is not a decision we made, so it is not a violation.
  {
    Configuration configuration = SecurityBaseline::ShippingDefault();
    configuration.enabled.erase(
        std::remove(configuration.enabled.begin(), configuration.enabled.end(),
                    Feature::kV8Sandbox),
        configuration.enabled.end());
    configuration.platform_unsupported.push_back(Feature::kV8Sandbox);
    Check(SecurityBaseline::Audit(configuration).empty(),
          "a documented platform gap is not treated as a self-inflicted one");
  }

  // Forbidden switches are caught wherever they appear.
  {
    Check(SecurityBaseline::ForbiddenSwitches().size() >= 10,
          "the forbidden list covers the usual suspects");
    for (const std::string& forbidden : SecurityBaseline::ForbiddenSwitches()) {
      Configuration configuration = SecurityBaseline::ShippingDefault();
      configuration.switches.push_back(forbidden);
      Check(!SecurityBaseline::Audit(configuration).empty(),
            "forbidden switch is caught: " + forbidden);

      Configuration as_arg = SecurityBaseline::ShippingDefault();
      as_arg.gn_args.push_back(forbidden);
      Check(!SecurityBaseline::Audit(as_arg).empty(),
            "and is caught as a build flag too: " + forbidden);
    }
    // The two that a privacy fork is most tempted by, named explicitly.
    const auto& forbidden = SecurityBaseline::ForbiddenSwitches();
    Check(std::find(forbidden.begin(), forbidden.end(), "--no-sandbox") !=
              forbidden.end(),
          "--no-sandbox is forbidden");
    Check(std::find(forbidden.begin(), forbidden.end(),
                    "--ignore-certificate-errors") != forbidden.end(),
          "--ignore-certificate-errors is forbidden");
  }

  if (failures == 0) {
    std::cout << "security_baseline_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
