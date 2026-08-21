// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/settings/privacy_posture.h"

namespace bedrock {
namespace stats {
namespace {

using privacy::Control;
using privacy::Value;

Strength FromValue(Value value) {
  switch (value) {
    case Value::kAllow:
      return Strength::kOff;
    case Value::kReduce:
      return Strength::kBalanced;
    case Value::kBlock:
    case Value::kBlockStrict:
      return Strength::kStrong;
    case Value::kInherit:
      break;
  }
  return Strength::kStandard;
}

}  // namespace

PrivacyPosture::PrivacyPosture(const privacy::ProtectionController* controls)
    : controls_(controls) {}

PrivacyPosture::~PrivacyPosture() = default;

const char* PrivacyPosture::StrengthName(Strength strength) {
  switch (strength) {
    case Strength::kOff:
      return "Off";
    case Strength::kStandard:
      return "Standard";
    case Strength::kBalanced:
      return "Balanced";
    case Strength::kStrong:
      return "Strong";
  }
  return "Standard";
}

std::vector<PostureRow> PrivacyPosture::Rows(int installed_extensions,
                                             bool encrypted_dns,
                                             bool https_only) const {
  std::vector<PostureRow> rows;
  rows.push_back(
      {"Tracking protection",
       StrengthName(FromValue(controls_->Get(Control::kTrackers, "", "")))});
  rows.push_back({"Fingerprint protection",
                  StrengthName(FromValue(
                      controls_->Get(Control::kFingerprinting, "", "")))});
  rows.push_back(
      {"Cookie isolation",
       StrengthName(FromValue(controls_->Get(Control::kCookies, "", "")))});

  // Network privacy is the weakest of its parts, not their average: a strong
  // link next to a weak one does not average out in practice.
  Strength network = Strength::kStandard;
  if (encrypted_dns && https_only)
    network = Strength::kStrong;
  else if (encrypted_dns || https_only)
    network = Strength::kBalanced;
  rows.push_back({"Network privacy", StrengthName(network)});

  rows.push_back({"Extensions", std::to_string(installed_extensions) +
                                    (installed_extensions == 1 ? " installed"
                                                               : " installed")});
  return rows;
}

}  // namespace stats
}  // namespace bedrock
