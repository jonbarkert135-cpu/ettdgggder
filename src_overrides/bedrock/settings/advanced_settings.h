// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_SETTINGS_ADVANCED_SETTINGS_H_
#define BEDROCK_SETTINGS_ADVANCED_SETTINGS_H_

#include <string>
#include <vector>

// Advanced / enterprise controls (roadmap item 57).
//
// A power user must be able to point the browser at their own filter lists,
// their own resolver, their own proxy, and to write per-site rules. What they
// must *not* be able to do — even by asking clearly, even as an administrator —
// is switch off the parts of the browser that make the rest of it meaningful:
// certificate validation, the sandbox, site isolation.
//
// So every advanced input goes through one function, `Evaluate`, which returns
// one of three verdicts:
//
//   kAccepted            — applied.
//   kAcceptedWithWarning — applied, and the UI must show the warning text. Used
//                          where the setting is legitimate but costs the user
//                          something they would not guess (a custom user agent
//                          makes them *more* identifiable, not less).
//   kRejected            — refused, with the reason. Never silently dropped
//                          (item 56) and never shown as if it had worked
//                          (item 55).
//
// The guards are listed as data in `Guards()` so they can be read, tested and
// documented rather than being scattered through validation code.

namespace bedrock {
namespace settings {

enum class AdvancedControl {
  kCustomFilterList,
  kCustomDns,
  kCustomProxy,
  kUserAgentPolicy,
  kSitePermission,
  kSitePolicy,
  kContentPolicy,   // the CSP-like control
  kManagedProfile,
  kMaxValue = kManagedProfile,
};

enum class Verdict {
  kAccepted,
  kAcceptedWithWarning,
  kRejected,
};

struct Decision {
  Verdict verdict = Verdict::kRejected;
  std::string message;  // warning or rejection reason; empty when plainly accepted
  bool ok() const { return verdict != Verdict::kRejected; }
};

// A rule an advanced setting may never break. `id` appears in rejection
// messages so a user who hits one can look it up.
struct Guard {
  const char* id;
  const char* rule;
  const char* why;
};

struct AdvancedInput {
  AdvancedControl control;
  std::string value;    // URL, host:port, UA string, permission name…
  std::string scope;    // site pattern for per-site controls; empty = global
  bool from_policy = false;  // set when an administrator supplies it
};

class AdvancedSettings {
 public:
  // The single entry point. Pure function of its input so it can be run in the
  // settings dialog for live feedback and again at apply time.
  static Decision Evaluate(const AdvancedInput& input);

  static const std::vector<Guard>& Guards();

  static const char* Describe(AdvancedControl control);

  // Every warning and rejection string, for the honesty test.
  static std::vector<std::string> AllUserVisibleStrings();
};

}  // namespace settings
}  // namespace bedrock

#endif  // BEDROCK_SETTINGS_ADVANCED_SETTINGS_H_
