// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/privacy/core/protection_controller.h"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace bedrock {
namespace privacy {
namespace {

// Built-in defaults = what a fresh profile does before the user touches
// anything. Deliberately the "Standard" posture: strong, but browsable.
constexpr struct {
  Control control;
  Value value;
  const char* id;
} kDefaults[] = {
    {Control::kAds, Value::kBlock, "ads"},
    {Control::kTrackers, Value::kBlock, "trackers"},
    {Control::kFingerprinting, Value::kReduce, "fingerprinting"},  // Level 1
    {Control::kCookies, Value::kReduce, "cookies"},  // third-party blocked
    {Control::kScripts, Value::kAllow, "scripts"},   // blocking JS breaks the web
    {Control::kHttps, Value::kReduce, "https"},      // upgrade, fall back
    {Control::kReferrer, Value::kReduce, "referrer"},
};

static_assert(sizeof(kDefaults) / sizeof(kDefaults[0]) ==
                  static_cast<size_t>(Control::kMaxValue) + 1,
              "every Control needs a default");

Value LookupIn(const Overrides* overrides, Control control) {
  if (!overrides) {
    return Value::kInherit;
  }
  const auto it = overrides->find(control);
  return it == overrides->end() ? Value::kInherit : it->second;
}

}  // namespace

ProtectionController::ProtectionController() = default;

void ProtectionController::Set(Scope scope,
                               const std::string& key,
                               Control control,
                               Value value) {
  switch (scope) {
    case Scope::kGlobal:
      global_[control] = value;
      return;
    case Scope::kDomain:
      domain_[key][control] = value;
      return;
    case Scope::kSite:
      site_[key][control] = value;
      return;
  }
}

void ProtectionController::Clear(Scope scope, const std::string& key) {
  switch (scope) {
    case Scope::kGlobal:
      global_.clear();
      return;
    case Scope::kDomain:
      domain_.erase(key);
      return;
    case Scope::kSite:
      site_.erase(key);
      return;
  }
}

const Overrides* ProtectionController::Find(Scope scope,
                                            const std::string& key) const {
  const auto& table = scope == Scope::kSite ? site_ : domain_;
  const auto it = table.find(key);
  return it == table.end() ? nullptr : &it->second;
}

Value ProtectionController::Get(Control control,
                                const std::string& host,
                                const std::string& etld_plus_one) const {
  Value value = LookupIn(Find(Scope::kSite, host), control);
  if (value == Value::kInherit) {
    value = LookupIn(Find(Scope::kDomain, etld_plus_one), control);
  }
  if (value == Value::kInherit) {
    value = LookupIn(&global_, control);
  }
  if (value == Value::kInherit) {
    value = kDefaults[static_cast<size_t>(control)].value;
  }
  return value;
}

Scope ProtectionController::EffectiveScope(
    const Control control,
    const std::string& host,
    const std::string& etld_plus_one) const {
  if (LookupIn(Find(Scope::kSite, host), control) != Value::kInherit) {
    return Scope::kSite;
  }
  if (LookupIn(Find(Scope::kDomain, etld_plus_one), control) !=
      Value::kInherit) {
    return Scope::kDomain;
  }
  return Scope::kGlobal;
}

std::map<Control, Value> ProtectionController::GetProfile(
    const std::string& host,
    const std::string& etld_plus_one) const {
  std::map<Control, Value> profile;
  for (const auto& entry : kDefaults) {
    profile[entry.control] = Get(entry.control, host, etld_plus_one);
  }
  return profile;
}

std::vector<std::string> ProtectionController::CustomizedKeys(
    Scope scope) const {
  std::vector<std::string> keys;
  if (scope == Scope::kGlobal) {
    return keys;
  }
  for (const auto& entry : scope == Scope::kSite ? site_ : domain_) {
    if (!entry.second.empty()) {
      keys.push_back(entry.first);
    }
  }
  return keys;
}

FpLevel ProtectionController::ToFpLevel(Value value) {
  switch (value) {
    case Value::kAllow:
      return FpLevel::kCompatibility;
    case Value::kBlock:
      return FpLevel::kStrict;
    case Value::kBlockStrict:
      return FpLevel::kMaximum;
    case Value::kReduce:
    case Value::kInherit:
      return FpLevel::kBalanced;
  }
  return FpLevel::kBalanced;
}

const char* ProtectionController::ControlId(Control control) {
  return kDefaults[static_cast<size_t>(control)].id;
}

}  // namespace privacy
}  // namespace bedrock
