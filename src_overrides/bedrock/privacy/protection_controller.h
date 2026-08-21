// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PRIVACY_PROTECTION_CONTROLLER_H_
#define BEDROCK_PRIVACY_PROTECTION_CONTROLLER_H_

#include <map>
#include <string>
#include <vector>

#include "bedrock/privacy/fingerprint_policy.h"

// Protection Controller — the per-site protection profile behind the shields
// panel (roadmap item 11).
//
// One resolver, three scopes, one rule: the most specific setting wins.
//
//   site (news.example.com)  ->  domain (example.com)  ->  global  ->  built-in
//
// Anything left kInherit falls through. That is the whole model: no priority
// numbers, no rule engine, no per-feature special cases — because every special
// case here becomes a security hole someone has to reason about later.

namespace bedrock {
namespace privacy {

enum class Control {
  kAds,
  kTrackers,
  kFingerprinting,
  kCookies,
  kScripts,
  kHttps,
  kReferrer,
  kMaxValue = kReferrer,
};

// Shared value ladder. Not every control uses every rung; the meaning per
// control is in docs/design/011-protection-controller.md.
//   kAds/kTrackers:      kAllow (off)      | kReduce (standard lists) | kBlock (aggressive)
//   kFingerprinting:     kAllow (Level 0)  | kReduce (Level 1)        | kBlock (Level 2)
//                        kBlockStrict maps to Level 3
//   kCookies:            kAllow (all)      | kReduce (3rd-party blocked) | kBlock (all)
//   kScripts:            kAllow            | kReduce (3rd-party only) | kBlock
//   kHttps:              kAllow (off)      | kReduce (upgrade, fall back) | kBlock (HTTPS-only)
//   kReferrer:           kAllow (full)     | kReduce (origin only)    | kBlock (none)
enum class Value {
  kInherit,
  kAllow,
  kReduce,
  kBlock,
  kBlockStrict,
};

enum class Scope {
  kGlobal,
  kDomain,  // eTLD+1, applies to every subdomain
  kSite,    // exact host
};

// A user-visible set of overrides for one scope key ("" for global).
using Overrides = std::map<Control, Value>;

class ProtectionController {
 public:
  ProtectionController();

  // `key` is "" for global, an eTLD+1 for kDomain, an exact host for kSite.
  void Set(Scope scope, const std::string& key, Control control, Value value);
  // Removing an override restores inheritance — this is the "reset" button.
  void Clear(Scope scope, const std::string& key);

  // Effective value for a host. `etld_plus_one` must be the registrable domain
  // of `host` (Chromium computes it; passing it in keeps this class pure).
  Value Get(Control control,
            const std::string& host,
            const std::string& etld_plus_one) const;

  // Everything the shields panel needs for one site, in one call.
  std::map<Control, Value> GetProfile(const std::string& host,
                                      const std::string& etld_plus_one) const;

  // Which scope actually decided this value — the panel says "set for this
  // site" / "set for example.com" / "default" instead of leaving users guessing.
  Scope EffectiveScope(Control control,
                       const std::string& host,
                       const std::string& etld_plus_one) const;

  // Sites the user has customised, for Settings -> Privacy -> Site exceptions.
  std::vector<std::string> CustomizedKeys(Scope scope) const;

  // Fingerprinting value -> the level the renderer applies.
  static FpLevel ToFpLevel(Value value);

  static const char* ControlId(Control control);

 private:
  const Overrides* Find(Scope scope, const std::string& key) const;

  std::map<std::string, Overrides> site_;
  std::map<std::string, Overrides> domain_;
  Overrides global_;
};

}  // namespace privacy
}  // namespace bedrock

#endif  // BEDROCK_PRIVACY_PROTECTION_CONTROLLER_H_
