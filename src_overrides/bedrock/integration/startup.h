// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_INTEGRATION_STARTUP_H_
#define BEDROCK_INTEGRATION_STARTUP_H_

#include <cstddef>
#include <string>
#include <vector>

// The seam between Chromium and Bedrock (phase 2).
//
// Phase 1 proved the overlay compiles inside Chromium and reaches the link
// line. It also proved something uncomfortable: with no Chromium call site
// calling into `bedrock::`, the linker discarded every object and
// `nm -C out/Release/chrome | grep bedrock::` printed nothing. Compiling is not
// running.
//
// This header is the first call site. A single Chromium patch
// (patches/bedrock/integration/0001-bedrock-startup-hook.patch) calls
// `ComputeStartupPlan()` once per profile initialisation, logs the summary and
// applies the returned prefs. Everything decided here is decided in
// dependency-free C++17 that the host test suite can run without a Chromium
// checkout; the patch contains no policy of its own, only the plumbing. That
// split is deliberate — policy that lives in a patch is policy nobody can test.
//
// The plan is deliberately small. A setting appears in `prefs` only when
// Bedrock can point at the Chromium pref that makes the browser behave
// differently and a test can read that pref back out of a real profile. Every
// other shipped default appears in `unenforced` instead, with the reason it is
// not enforced yet. Item 55 (NO FAKE FEATURES) is the whole design constraint:
// the honest list of what runs must be generated from the same table that
// drives the code, so it cannot drift into marketing.

namespace bedrock {
namespace integration {

// Where a preference lives. Chromium keeps per-profile prefs in the profile's
// Preferences file and browser-wide ones in Local State; the two are registered
// by different functions, so the plan has to say which.
enum class PrefScope {
  kProfile,
  kLocalState,
};

// The value type Chromium registered the pref with. Writing a boolean into a
// string pref is a crash, not a mistake to be discovered later.
enum class PrefType {
  kString,
  kBoolean,
};

// One Chromium preference Bedrock sets on a fresh profile.
struct PrefAssignment {
  std::string pref_name;    // Chromium pref path, e.g. "webrtc.ip_handling_policy"
  std::string value;        // canonical text form; for kBoolean: "true"/"false"
  std::string setting_id;   // the settings/defaults.h id this comes from
  std::string reason;       // why, copied from the defaults table
  PrefScope scope = PrefScope::kProfile;
  PrefType type = PrefType::kString;
  bool boolean_value = false;  // meaningful when type == kBoolean

  // Whether the value Bedrock registers is what the browser then *acts on*.
  // False means the assignment is real but something upstream decides the
  // behaviour instead — a build-4 measurement, not a guess (see
  // build/ENFORCEMENT.md). Only decisive assignments count as enforced, so the
  // startup summary cannot inflate itself by registering prefs nobody reads.
  bool decides_behavior = true;
};

// A shipped default Bedrock does not yet make the browser perform.
struct UnenforcedDefault {
  std::string setting_id;
  std::string documented_value;
  std::string blocked_on;  // what is missing, in one sentence
};

struct StartupPlan {
  std::string profile_name;              // settings::FactoryProfileName()
  std::string summary;                   // single line, safe for the debug log
  std::vector<PrefAssignment> prefs;     // registered; some may not be decisive
  std::vector<UnenforcedDefault> unenforced;
};

// How many assignments in `prefs` the browser actually acts on. This, not
// `prefs.size()`, is the number of enforced defaults.
size_t DecisiveCount(const StartupPlan& plan);

// Pure function of the compiled-in defaults: no I/O, no globals, no Chromium
// types. Called once per profile from the browser's PostProfileInit.
StartupPlan ComputeStartupPlan();

// The lines the browser writes to its debug log at startup, in order. First
// line is the summary; the rest name each pref that was applied. Kept here
// rather than in the patch so the wording is tested.
std::vector<std::string> StartupLogLines(const StartupPlan& plan);

// The value Bedrock wants Chromium to register as the default for
// `pref_name`, or `chromium_default` when Bedrock has nothing to say about
// that pref. Used by the registration patch, which owns no policy of its own.
std::string PrefDefaultOr(const std::string& pref_name,
                          const std::string& chromium_default);

// Same contract for boolean prefs.
bool BooleanPrefDefaultOr(const std::string& pref_name, bool chromium_default);

// The assignments of one scope, in plan order. The registration patch walks
// this list and calls PrefRegistry::SetDefaultPrefValue for each pref it finds
// already registered, so a pref renamed upstream is skipped and reported by the
// effective-value line rather than crashing the browser.
std::vector<PrefAssignment> AssignmentsForScope(PrefScope scope);

// Evidence line for a pref Chromium actually read back out of a live profile:
// names the pref, the observed value, and whether it matches the plan. The
// phase-2 verification greps a running browser's log for "match".
std::string EffectivePrefLine(const std::string& pref_name,
                              const std::string& observed);

// Same evidence line for a boolean pref. `observed` is the value the machinery
// that acts on the pref actually read — for metrics reporting that is the
// consent answer, not the raw pref, because the consent answer is what decides
// whether anything is sent.
std::string EffectiveBooleanPrefLine(const std::string& pref_name,
                                     bool observed);

// Prefix every Bedrock startup log line carries. The phase-2 verification
// script greps for it, so it is a constant rather than a literal in the patch.
extern const char kLogPrefix[];

}  // namespace integration
}  // namespace bedrock

#endif  // BEDROCK_INTEGRATION_STARTUP_H_
