// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/ui/site_privacy_panel.h"

#include <string>
#include <vector>

namespace bedrock {
namespace ui {
namespace {

using privacy::Control;
using privacy::Value;

const char kNotMeasured[] = "Not measured";

std::string CountRow(int count, const char* suffix) {
  if (count < 0)
    return kNotMeasured;
  return std::to_string(count) + " " + suffix;
}

std::string FingerprintingState(Value value) {
  switch (value) {
    case Value::kAllow:
      return "Off";
    case Value::kReduce:
      return "Protected (standard)";
    case Value::kBlock:
      return "Protected (strict)";
    case Value::kBlockStrict:
      return "Protected (maximum)";
    case Value::kInherit:
      break;
  }
  return "Protected (standard)";
}

std::string CookieState(Value value) {
  switch (value) {
    case Value::kAllow:
      return "Allowed";
    case Value::kReduce:
      return "Blocked";  // third-party cookies blocked, first-party kept
    case Value::kBlock:
    case Value::kBlockStrict:
      return "Blocked";
    case Value::kInherit:
      break;
  }
  return "Blocked";
}

std::string StorageState(Value cookies) {
  // Storage isolation is on for every site (item 15); the row says what the
  // partitioning means here rather than claiming a per-site setting exists.
  if (cookies == Value::kBlockStrict)
    return "Partitioned, cleared on close";
  return "Partitioned";
}

}  // namespace

SitePrivacyPanel::SitePrivacyPanel(const stats::PrivacyEventLog* log,
                                   const privacy::ProtectionController* controls)
    : log_(log), controls_(controls) {}

SitePrivacyPanel::~SitePrivacyPanel() = default;

std::vector<PanelRow> SitePrivacyPanel::Build(
    const std::string& host,
    const std::string& etld_plus_one,
    const ConnectionState& connection) const {
  const stats::SiteCounters counters = log_->ForSite(etld_plus_one);
  std::vector<PanelRow> rows;

  // Connection — facts about this load, not a policy.
  PanelRow security;
  security.label = "Connection";
  security.kind = RowKind::kConnection;
  if (!connection.https) {
    security.value = "Not secure (HTTP)";
  } else if (!connection.certificate_valid) {
    security.value = "Certificate problem";
  } else if (connection.mixed_content) {
    security.value = "HTTPS with blocked mixed content";
  } else if (connection.upgraded) {
    security.value = "HTTPS (upgraded)";
  } else {
    security.value = "HTTPS";
  }
  rows.push_back(security);

  PanelRow trackers;
  trackers.label = "Trackers";
  trackers.kind = RowKind::kCount;
  trackers.measured = counters.measured;
  trackers.value = CountRow(counters.trackers_blocked, "blocked");
  rows.push_back(trackers);

  PanelRow ads;
  ads.label = "Ads";
  ads.kind = RowKind::kCount;
  ads.measured = counters.measured;
  ads.value = CountRow(counters.ads_blocked, "blocked");
  rows.push_back(ads);

  const Value fp = controls_->Get(Control::kFingerprinting, host, etld_plus_one);
  PanelRow fingerprinting;
  fingerprinting.label = "Fingerprinting";
  fingerprinting.value = FingerprintingState(fp);
  if (counters.measured && counters.fingerprint_attempts > 0) {
    fingerprinting.value +=
        " — " + std::to_string(counters.fingerprint_attempts) +
        " attempts blocked";
  }
  rows.push_back(fingerprinting);

  const Value cookies = controls_->Get(Control::kCookies, host, etld_plus_one);
  PanelRow cookie_row;
  cookie_row.label = "Third-party cookies";
  cookie_row.value = CookieState(cookies);
  rows.push_back(cookie_row);

  const Value scripts = controls_->Get(Control::kScripts, host, etld_plus_one);
  PanelRow script_row;
  script_row.label = "Scripts";
  script_row.value = scripts == Value::kAllow ? "Allowed" : "Blocked";
  rows.push_back(script_row);

  PanelRow storage;
  storage.label = "Site storage";
  storage.value = StorageState(cookies);
  rows.push_back(storage);

  return rows;
}

std::vector<std::string> SitePrivacyPanel::BlockedParties(
    const std::string& site) const {
  return log_->BlockedPartiesFor(site);
}



namespace {

std::string JsonQuote(const std::string& text) {
  std::string out = "\"";
  for (char ch : text) {
    switch (ch) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          static const char* kHex = "0123456789abcdef";
          out += "\\u00";
          out += kHex[(ch >> 4) & 0xF];
          out += kHex[ch & 0xF];
        } else {
          out += ch;
        }
    }
  }
  return out + "\"";
}

const char* KindName(RowKind kind) {
  switch (kind) {
    case RowKind::kCount: return "count";
    case RowKind::kState: return "state";
    case RowKind::kConnection: return "connection";
  }
  return "state";
}

}  // namespace

std::string PanelJson(const std::string& host,
                      const std::vector<PanelRow>& rows,
                      const std::vector<std::string>& blocked_parties) {
  std::string out = "{\"host\":" + JsonQuote(host) + ",\"rows\":[";
  for (std::vector<PanelRow>::size_type i = 0; i < rows.size(); ++i) {
    if (i)
      out += ",";
    out += "{\"label\":" + JsonQuote(rows[i].label) +
           ",\"value\":" + JsonQuote(rows[i].value) +
           ",\"kind\":" + JsonQuote(KindName(rows[i].kind)) +
           ",\"measured\":" + (rows[i].measured ? "true" : "false") + "}";
  }
  out += "],\"blockedParties\":[";
  for (std::vector<std::string>::size_type i = 0; i < blocked_parties.size(); ++i) {
    if (i)
      out += ",";
    out += JsonQuote(blocked_parties[i]);
  }
  return out + "]}";
}

}  // namespace ui
}  // namespace bedrock
