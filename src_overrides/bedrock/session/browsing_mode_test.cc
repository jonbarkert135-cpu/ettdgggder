// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/session/browsing_mode.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <string>

#include "bedrock/profiles/new_identity.h"

namespace {

using namespace bedrock::session;  // NOLINT — test-local convenience
using bedrock::net::IsolationLevel;
using bedrock::privacy::FpLevel;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

std::string Lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return text;
}

}  // namespace

int main() {
  // Tor is a transport mode, not a personality transplant: Normal stays normal.
  {
    const ModeConfig normal =
        BrowsingModeController::ConfigFor(BrowsingMode::kNormal);
    Check(normal.isolation == IsolationLevel::kStandard &&
              normal.history_persisted && !normal.dns_through_proxy &&
              !normal.proxy_required && !normal.fingerprint_level_locked,
          "normal mode is untouched by the existence of Tor mode");
  }

  // Private: ephemeral and local-only, but not routed anywhere.
  {
    const ModeConfig priv =
        BrowsingModeController::ConfigFor(BrowsingMode::kPrivate);
    Check(priv.isolation == IsolationLevel::kEphemeralAll,
          "private mode keeps nothing");
    Check(!priv.history_persisted, "private mode records no history");
    Check(!priv.dns_through_proxy && !priv.proxy_required,
          "private mode is not secretly a proxy mode");
  }

  // Tor: proxy required, DNS at the exit, WebRTC off, fingerprinting locked.
  {
    const ModeConfig tor =
        BrowsingModeController::ConfigFor(BrowsingMode::kTor);
    Check(tor.dns_through_proxy, "names are resolved through the proxy");
    Check(tor.proxy_required, "Tor mode fails closed if the proxy is gone");
    Check(tor.webrtc_disabled, "WebRTC is off, not merely restricted");
    Check(tor.fingerprint_level == FpLevel::kMaximum &&
              tor.fingerprint_level_locked,
          "fingerprinting is forced to maximum and cannot be lowered per site");
    Check(tor.isolation == IsolationLevel::kEphemeralAll,
          "Tor sessions keep nothing");
  }

  // Circuit isolation: per site, stable within a session.
  {
    BrowsingModeController modes;
    modes.SetMode(BrowsingMode::kTor);
    const CircuitId news = modes.CircuitFor("news.test");
    const CircuitId shop = modes.CircuitFor("shop.test");
    Check(news != shop, "two sites never share a circuit");
    Check(news == modes.CircuitFor("news.test"),
          "the same site keeps its circuit, so it keeps working");

    const uint64_t before = modes.epoch();
    modes.RotateCircuits();
    Check(modes.epoch() != before, "rotating changes the epoch");
    Check(modes.CircuitFor("news.test") != news,
          "after New Identity the same site gets a new circuit");

    const uint64_t at_switch = modes.epoch();
    modes.SetMode(BrowsingMode::kNormal);
    Check(modes.epoch() != at_switch,
          "switching modes is a session boundary, not a continuation");
  }

  // The honesty rule: no user-visible string may promise anonymity.
  {
    // The ban is absolute, including in denials ("does not make you
    // untraceable"): a scanner cannot judge negation, and a screenshot of a
    // sentence containing the word travels further than the sentence does.
    const std::set<std::string> forbidden = {
        "anonymous", "anonymity", "untraceable", "100%", "completely private",
        "fully private", "no one can", "nobody can", "invisible"};
    std::vector<std::string> strings =
        BrowsingModeController::AllUserVisibleStrings();
    for (const std::string& extra : NewIdentity::AllUserVisibleStrings()) {
      strings.push_back(extra);
    }
    Check(strings.size() > 20, "there are strings to check");
    for (const std::string& text : strings) {
      const std::string lower = Lower(text);
      for (const std::string& word : forbidden) {
        Check(lower.find(word) == std::string::npos,
              "no promise of anonymity in: \"" + text + "\"");
      }
      Check(text.size() > 3, "no empty user-visible string");
    }
    Check(Lower(BrowsingModeController::StatusText(BrowsingMode::kTor))
                  .find("privacy protection") != std::string::npos,
          "Tor status says 'privacy protection enabled', as required");
  }

  // Tor mode ships its limitations, including that we are not Tor Browser.
  {
    const auto& limitations = BrowsingModeController::TorLimitations();
    Check(limitations.size() >= 5, "limitations are listed");
    bool mentions_tor_browser = false;
    bool mentions_login = false;
    for (const char* limitation : limitations) {
      const std::string text = Lower(limitation);
      mentions_tor_browser |= text.find("tor browser") != std::string::npos;
      mentions_login |= text.find("account") != std::string::npos;
    }
    Check(mentions_tor_browser,
          "we say plainly that Bedrock is not the Tor Browser");
    Check(mentions_login, "and that signing in identifies you anyway");
  }

  if (failures == 0) {
    std::cout << "browsing_mode_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
