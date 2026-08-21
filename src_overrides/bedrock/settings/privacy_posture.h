// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_SETTINGS_PRIVACY_POSTURE_H_
#define BEDROCK_SETTINGS_PRIVACY_POSTURE_H_

#include <string>
#include <vector>

#include "bedrock/privacy/core/protection_controller.h"

// Privacy configuration view (brief item 19).
//
//   Tracking protection      Strong
//   Fingerprint protection   Balanced
//   Cookie isolation         Strong
//   Network privacy          Standard
//   Extensions               2 installed
//
// Deliberately **not** a score. "You are 97% anonymous" is not a measurement
// of anything: there is no denominator, the adversary is unspecified, and the
// number moves when we change our own formula. What a browser can honestly
// report is the state of each mechanism it controls, which is what this does.
//
// A test asserts no rendered value contains a percent sign or the word
// "anonymous" — the temptation to ship a gamified score is permanent, so the
// prohibition lives in code.

namespace bedrock {
namespace stats {

enum class Strength {
  kOff,
  kStandard,
  kBalanced,
  kStrong,
};

struct PostureRow {
  std::string label;
  std::string value;
};

class PrivacyPosture {
 public:
  explicit PrivacyPosture(const privacy::ProtectionController* controls);
  ~PrivacyPosture();

  // `installed_extensions` is a count, not an opinion: more extensions is not
  // reported as better or worse, only as a fact.
  std::vector<PostureRow> Rows(int installed_extensions,
                               bool encrypted_dns,
                               bool https_only) const;

  static const char* StrengthName(Strength strength);

 private:
  const privacy::ProtectionController* controls_;
};

}  // namespace stats
}  // namespace bedrock

#endif  // BEDROCK_SETTINGS_PRIVACY_POSTURE_H_
