// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/ui/profile_menu.h"

#include <cctype>
#include <vector>

namespace bedrock {
namespace ui {
namespace {

std::string Quote(const std::string& text) {
  std::string out = "\"";
  for (char c : text) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          static const char* kHex = "0123456789abcdef";
          out += "\\u00";
          out += kHex[(c >> 4) & 0xF];
          out += kHex[c & 0xF];
        } else {
          out += c;
        }
    }
  }
  return out + "\"";
}

std::string Initial(const std::string& name) {
  if (name.empty())
    return "?";
  return std::string(
      1, static_cast<char>(std::toupper(static_cast<unsigned char>(name[0]))));
}

}  // namespace

std::string ProfileMenuJson(const session::ProfileManager& profiles,
                            WindowMode mode) {
  const session::Profile& active = profiles.active();

  std::string others = "[";
  bool first = true;
  for (const std::string& id : profiles.ProfileIds()) {
    if (id == active.id)
      continue;
    const session::Profile* profile = profiles.Find(id);
    if (profile == nullptr)
      continue;
    if (!first)
      others += ",";
    first = false;
    others += "{" + Quote("id") + ":" + Quote(profile->id) + "," +
              Quote("name") + ":" + Quote(profile->name) + "," +
              Quote("kind") + ":" +
              Quote(session::ProfileManager::KindName(profile->kind)) + "," +
              Quote("initial") + ":" + Quote(Initial(profile->name)) +
              ",\"ephemeral\":" + (profile->ephemeral ? "true" : "false") + "}";
  }
  others += "]";

  const ModeIdentity identity = IdentityFor(mode);
  std::string out = "{";
  out += Quote("active") + ":{" + Quote("id") + ":" + Quote(active.id) + "," +
         Quote("name") + ":" + Quote(active.name) + "," + Quote("kind") + ":" +
         Quote(session::ProfileManager::KindName(active.kind)) + "," +
         Quote("initial") + ":" + Quote(Initial(active.name)) +
         ",\"ephemeral\":" + (active.ephemeral ? "true" : "false") + "},";
  out += Quote("others") + ":" + others + ",";
  // No sync service exists, so the line says so rather than leaving a gap
  // where other browsers put a sign-in prompt.
  out += Quote("sync") + ":" +
         Quote("Not synced — this profile stays on this device") + ",";
  out += Quote("mode") + ":" + Quote(identity.id) + "," + Quote("modeLabel") +
         ":" + Quote(identity.label) + "," + Quote("modeSentence") + ":" +
         Quote(identity.sentence);
  return out + "}";
}

}  // namespace ui
}  // namespace bedrock
