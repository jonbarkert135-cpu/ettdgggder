// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/session/browsing_mode.h"

#include <string>
#include <vector>

namespace bedrock {
namespace session {

BrowsingModeController::BrowsingModeController()
    : config_(ConfigFor(BrowsingMode::kNormal)) {}

BrowsingModeController::~BrowsingModeController() = default;

// static
ModeConfig BrowsingModeController::ConfigFor(BrowsingMode mode) {
  ModeConfig config;
  config.mode = mode;
  switch (mode) {
    case BrowsingMode::kNormal:
      break;
    case BrowsingMode::kPrivate:
      config.isolation = net::IsolationLevel::kEphemeralAll;
      config.history_persisted = false;
      break;
    case BrowsingMode::kTor:
      config.isolation = net::IsolationLevel::kEphemeralAll;
      config.history_persisted = false;
      config.fingerprint_level = privacy::FpLevel::kMaximum;
      config.fingerprint_level_locked = true;
      config.dns_through_proxy = true;
      // WebRTC bypasses the proxy in ways Chromium does not fully control, and
      // a leaked address defeats the transport entirely. Off, not "restricted".
      config.webrtc_disabled = true;
      config.proxy_required = true;
      break;
  }
  return config;
}

void BrowsingModeController::SetMode(BrowsingMode mode) {
  if (mode == mode_) {
    return;
  }
  mode_ = mode;
  config_ = ConfigFor(mode);
  // Switching modes starts a new session boundary: nothing from the previous
  // mode may share a circuit with the next one.
  ++epoch_;
}

CircuitId BrowsingModeController::CircuitFor(
    const std::string& top_level_site) const {
  CircuitId circuit;
  circuit.socks_username = top_level_site;
  circuit.socks_password = std::to_string(epoch_);
  return circuit;
}

void BrowsingModeController::RotateCircuits() {
  ++epoch_;
}

// static
const std::vector<const char*>& BrowsingModeController::TorLimitations() {
  static const std::vector<const char*> kLimitations = {
      "Signing in to an account identifies you, whatever the network hides.",
      "Files you download can contact the internet directly when you open "
      "them. Open them offline if that matters.",
      "Bedrock is not the Tor Browser. It uses the Tor network, and its "
      "protections have not been reviewed by the Tor Project.",
      "Very large or very small windows are unusual and make you easier to "
      "recognise; Bedrock keeps the window size in fixed steps to reduce that.",
      "Pages load more slowly, and some sites block traffic from the Tor "
      "network entirely.",
      "A strong observer who can watch both ends of your connection can still "
      "correlate traffic. No browser fixes this.",
  };
  return kLimitations;
}

// static
const char* BrowsingModeController::ModeName(BrowsingMode mode) {
  switch (mode) {
    case BrowsingMode::kNormal:
      return "Normal window";
    case BrowsingMode::kPrivate:
      return "Private window";
    case BrowsingMode::kTor:
      return "Tor window";
  }
  return "";
}

// static
const char* BrowsingModeController::StatusText(BrowsingMode mode) {
  // Never "anonymous", never "untraceable", never "100%". These words promise
  // something no software can deliver, and the promise is what gets people
  // hurt — the Tor Project says as much in its own documentation.
  switch (mode) {
    case BrowsingMode::kNormal:
      return "Standard protection";
    case BrowsingMode::kPrivate:
      return "Privacy protection enabled — local traces off";
    case BrowsingMode::kTor:
      return "Privacy protection enabled — routed through Tor";
  }
  return "";
}

// static
const char* BrowsingModeController::Description(BrowsingMode mode) {
  switch (mode) {
    case BrowsingMode::kNormal:
      return "Your usual profile: history, logins and settings are kept on "
             "this device, with tracker and fingerprinting protection on.";
    case BrowsingMode::kPrivate:
      return "Nothing from this window is kept on this device after you close "
             "it. Your network, your employer and the sites you visit can "
             "still see what you do.";
    case BrowsingMode::kTor:
      return "Traffic from this window goes through the Tor network, so the "
             "sites you visit do not see your address and your network does "
             "not see which sites you visit. This reduces what others can "
             "learn about you; it does not remove every way you can be "
             "identified.";
  }
  return "";
}

// static
std::vector<std::string> BrowsingModeController::AllUserVisibleStrings() {
  std::vector<std::string> strings;
  for (BrowsingMode mode :
       {BrowsingMode::kNormal, BrowsingMode::kPrivate, BrowsingMode::kTor}) {
    strings.push_back(ModeName(mode));
    strings.push_back(StatusText(mode));
    strings.push_back(Description(mode));
  }
  for (const char* limitation : TorLimitations()) {
    strings.push_back(limitation);
  }
  return strings;
}

}  // namespace session
}  // namespace bedrock
