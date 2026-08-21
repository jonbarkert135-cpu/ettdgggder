// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_SESSION_BROWSING_MODE_H_
#define BEDROCK_SESSION_BROWSING_MODE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "bedrock/net/storage_isolation.h"
#include "bedrock/privacy/fingerprint_policy.h"

// Browsing modes and the optional Tor transport (roadmap items 19 and 20).
//
// Bedrock is not a Tor Browser clone and does not become one when Tor is
// switched on. Tor is a **transport mode**: the user opens a Tor window, that
// window routes through Tor, and the rest of the browser keeps working the way
// it did. A browser that silently sends everything through Tor is both slower
// than the user expects and more dangerous than they assume.
//
// The honesty rule, taken straight from the Tor Project's own guidance: never
// tell the user they are anonymous. Tor reduces what a network observer can
// learn; it cannot fix a logged-in account, a unique window size or a leaked
// document. Every user-visible string in this file says "privacy protection",
// never "anonymous" — and a test greps the strings to keep it that way.

namespace bedrock {
namespace session {

enum class BrowsingMode {
  kNormal,
  kPrivate,  // ephemeral, local only
  kTor,      // ephemeral + routed through Tor
};

// SOCKS credentials are what buy circuit isolation in Tor: two streams with
// different credentials cannot share a circuit, so two sites cannot be
// correlated by exit node. Bedrock keys them on the top-level site plus the
// identity epoch, which is the same shape Tor Browser uses.
struct CircuitId {
  std::string socks_username;  // "<top-level site>"
  std::string socks_password;  // "<epoch>"

  bool operator==(const CircuitId& other) const {
    return socks_username == other.socks_username &&
           socks_password == other.socks_password;
  }
  bool operator!=(const CircuitId& other) const { return !(*this == other); }
};

struct ModeConfig {
  BrowsingMode mode = BrowsingMode::kNormal;
  // Storage lifetime for the whole session.
  net::IsolationLevel isolation = net::IsolationLevel::kStandard;
  // Fingerprinting level is *forced* in Tor mode: a Tor user who looks unique
  // has given up the property Tor mode exists for.
  privacy::FpLevel fingerprint_level = privacy::FpLevel::kBalanced;
  bool fingerprint_level_locked = false;
  bool history_persisted = true;
  bool dns_through_proxy = false;  // Tor resolves names at the exit node
  bool webrtc_disabled = false;
  bool proxy_required = false;     // fail closed if the proxy is unreachable
};

class BrowsingModeController {
 public:
  BrowsingModeController();
  ~BrowsingModeController();

  static ModeConfig ConfigFor(BrowsingMode mode);

  void SetMode(BrowsingMode mode);
  BrowsingMode mode() const { return mode_; }
  const ModeConfig& config() const { return config_; }

  // Circuit isolation: one circuit per top-level site, per identity epoch.
  // Same site -> same circuit (the site keeps working); different site ->
  // different circuit (the two cannot be linked at the exit).
  CircuitId CircuitFor(const std::string& top_level_site) const;

  // "New Identity" for the transport half: every future stream gets fresh
  // circuits. The storage half is `NewIdentity` (roadmap item 22).
  void RotateCircuits();
  uint64_t epoch() const { return epoch_; }

  // What a Tor-mode window still leaks, in plain words. Shown once when the
  // mode is opened, not buried in a help page.
  static const std::vector<const char*>& TorLimitations();

  // Status text for the toolbar. Deliberately never contains "anonymous".
  static const char* StatusText(BrowsingMode mode);
  static const char* ModeName(BrowsingMode mode);
  static const char* Description(BrowsingMode mode);

  // Every user-visible string this class can produce, so the honesty test can
  // check all of them without enumerating call sites.
  static std::vector<std::string> AllUserVisibleStrings();

 private:
  BrowsingMode mode_ = BrowsingMode::kNormal;
  ModeConfig config_;
  uint64_t epoch_ = 1;
};

}  // namespace session
}  // namespace bedrock

#endif  // BEDROCK_SESSION_BROWSING_MODE_H_
