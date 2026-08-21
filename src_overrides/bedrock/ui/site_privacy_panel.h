// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_UI_SITE_PRIVACY_PANEL_H_
#define BEDROCK_UI_SITE_PRIVACY_PANEL_H_

#include <string>
#include <vector>

#include "bedrock/privacy/protection_controller.h"
#include "bedrock/stats/privacy_event_log.h"

// Per-site privacy panel — the Privacy Shield popup (roadmap item 38).
//
//   example.com
//   Connection            HTTPS
//   Trackers              12 blocked
//   Ads                   27 blocked
//   Fingerprinting        Protected
//   Third-party cookies   Blocked
//   Scripts               Allowed
//   Site storage          Partitioned
//
// Every row comes from one of two sources and nothing else:
//
//   - a *count* from the privacy event log, i.e. actions the engine actually
//     performed on this page load, or
//   - a *state* from the protection controller, i.e. the policy in force.
//
// There is no third category. No estimates, no "typical" numbers, no counting
// a request twice because two subsystems both wanted credit for it. If the
// page has not been measured yet, the row says "Not measured" — a zero would
// claim we looked and found nothing, which is a different statement.

namespace bedrock {
namespace ui {

enum class RowKind {
  kCount,       // from the event log
  kState,       // from the protection controller
  kConnection,  // from the security state of the load
};

struct PanelRow {
  std::string label;
  std::string value;
  RowKind kind = RowKind::kState;
  bool measured = true;  // false renders as "Not measured"
};

// The connection facts, supplied by the network stack for the current load.
struct ConnectionState {
  bool https = false;
  bool certificate_valid = false;
  bool mixed_content = false;
  bool upgraded = false;  // we upgraded it from http
};

class SitePrivacyPanel {
 public:
  SitePrivacyPanel(const stats::PrivacyEventLog* log,
                   const privacy::ProtectionController* controls);
  ~SitePrivacyPanel();

  std::vector<PanelRow> Build(const std::string& host,
                              const std::string& etld_plus_one,
                              const ConnectionState& connection) const;

  // The third parties blocked on this page, for the expanded list.
  std::vector<std::string> BlockedParties(const std::string& site) const;

 private:
  const stats::PrivacyEventLog* log_;
  const privacy::ProtectionController* controls_;
};

}  // namespace ui
}  // namespace bedrock

#endif  // BEDROCK_UI_SITE_PRIVACY_PANEL_H_
