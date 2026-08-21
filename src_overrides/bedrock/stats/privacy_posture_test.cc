// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/stats/privacy_posture.h"

#include <iostream>
#include <string>

namespace {

using bedrock::privacy::Control;
using bedrock::privacy::ProtectionController;
using bedrock::privacy::Scope;
using bedrock::privacy::Value;
using bedrock::stats::PostureRow;
using bedrock::stats::PrivacyPosture;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

std::string ValueOf(const std::vector<PostureRow>& rows,
                    const std::string& label) {
  for (const PostureRow& row : rows) {
    if (row.label == label)
      return row.value;
  }
  return "<missing>";
}

}  // namespace

int main() {
  ProtectionController controls;
  PrivacyPosture posture(&controls);

  const auto rows = posture.Rows(2, false, false);
  Check(rows.size() == 5, "the five rows from the brief");
  Check(ValueOf(rows, "Tracking protection") == "Strong",
        "tracker blocking is on by default, and the row says so");
  Check(ValueOf(rows, "Fingerprint protection") == "Balanced",
        "level 1 fingerprinting protection reads as balanced");
  Check(ValueOf(rows, "Cookie isolation") == "Balanced",
        "third-party cookies blocked reads as balanced");
  Check(ValueOf(rows, "Network privacy") == "Standard",
        "plain DNS and no HTTPS-only is standard");
  Check(ValueOf(rows, "Extensions") == "2 installed",
        "extensions are a count, not a verdict");

  // Network privacy is the weakest part, not an average.
  Check(ValueOf(posture.Rows(0, true, false), "Network privacy") == "Balanced",
        "encrypted DNS alone is balanced");
  Check(ValueOf(posture.Rows(0, true, true), "Network privacy") == "Strong",
        "encrypted DNS plus HTTPS-only is strong");

  controls.Set(Scope::kGlobal, "", Control::kFingerprinting, Value::kBlockStrict);
  Check(ValueOf(posture.Rows(0, false, false), "Fingerprint protection") ==
            "Strong",
        "raising the setting immediately changes the row");
  controls.Set(Scope::kGlobal, "", Control::kTrackers, Value::kAllow);
  Check(ValueOf(posture.Rows(0, false, false), "Tracking protection") == "Off",
        "and turning one off says Off rather than hiding it");

  // The prohibition that has to live in code: no score, no percentage, no
  // claim of anonymity.
  const char* banned[] = {"%", "anonymous", "anonymity", "score", "untraceable",
                          "invisible", "100"};
  for (int extensions = 0; extensions < 3; ++extensions) {
    for (const PostureRow& row : posture.Rows(extensions, true, true)) {
      for (const char* word : banned) {
        Check(row.value.find(word) == std::string::npos,
              std::string("no fabricated score in: ") + row.label + " = " +
                  row.value);
      }
    }
  }

  if (failures == 0)
    std::cout << "privacy_posture_test: all assertions passed\n";
  return failures == 0 ? 0 : 1;
}
