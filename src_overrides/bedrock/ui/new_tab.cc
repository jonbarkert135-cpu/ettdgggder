// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/ui/new_tab.h"

#include <algorithm>
#include <cctype>

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

std::string Member(const std::string& key, const std::string& value) {
  return Quote(key) + ":" + Quote(value);
}

std::string Host(const std::string& url) {
  std::string rest = url;
  const std::string::size_type scheme = rest.find("://");
  if (scheme != std::string::npos)
    rest = rest.substr(scheme + 3);
  rest = rest.substr(0, rest.find_first_of("/?#"));
  // Credentials and ports are not part of a label.
  const std::string::size_type at = rest.rfind('@');
  if (at != std::string::npos)
    rest = rest.substr(at + 1);
  rest = rest.substr(0, rest.find(':'));
  if (rest.compare(0, 4, "www.") == 0)
    rest = rest.substr(4);
  return rest;
}

std::string TileJson(const Tile& tile) {
  return "{" + Member("url", tile.url) + "," + Member("title", tile.title) +
         "," + Member("initial", tile.initial) + ",\"pinned\":" +
         (tile.pinned ? "true" : "false") + "}";
}

Tile MakeTile(const std::string& url, bool pinned) {
  Tile tile;
  tile.url = url;
  tile.title = ShortLabel(url);
  tile.initial = tile.title.empty()
                     ? std::string("?")
                     : std::string(1, static_cast<char>(std::toupper(
                           static_cast<unsigned char>(tile.title[0]))));
  tile.pinned = pinned;
  return tile;
}

}  // namespace

std::string ShortLabel(const std::string& url) {
  std::string host = Host(url);
  if (host.empty())
    return std::string();
  // Drop one trailing label ("youtube.com" -> "youtube"), but keep the whole
  // host when that would leave nothing readable ("localhost", an IP address).
  const std::string::size_type dot = host.rfind('.');
  if (dot == std::string::npos || dot == 0)
    return host;
  const bool numeric =
      host.find_first_not_of("0123456789.") == std::string::npos;
  if (numeric)
    return host;
  return host.substr(0, dot);
}

std::vector<Tile> BuildShortcuts(const std::vector<std::string>& pinned,
                                 const std::vector<std::string>& top_sites,
                                 int limit) {
  std::vector<Tile> tiles;
  if (limit <= 0)
    return tiles;
  const auto already_there = [&tiles](const std::string& url) {
    return std::any_of(tiles.begin(), tiles.end(),
                       [&url](const Tile& t) { return t.url == url; });
  };
  for (const std::string& url : pinned) {
    if (static_cast<int>(tiles.size()) >= limit)
      return tiles;
    if (!url.empty() && !already_there(url))
      tiles.push_back(MakeTile(url, true));
  }
  for (const std::string& url : top_sites) {
    if (static_cast<int>(tiles.size()) >= limit)
      break;
    if (!url.empty() && !already_there(url))
      tiles.push_back(MakeTile(url, false));
  }
  return tiles;
}

std::string NewTabJson(const NewTabState& state) {
  std::string engine_label;
  std::string engines = "[";
  bool first = true;
  for (const EngineChoice& engine : state.engines) {
    const bool selected = engine.id == state.engine_id;
    if (selected)
      engine_label = engine.label;
    if (!first)
      engines += ",";
    first = false;
    engines += "{" + Member("id", engine.id) + "," +
               Member("label", engine.label) + ",\"selected\":" +
               (selected ? "true" : "false") + "}";
  }
  engines += "]";

  // History-derived tiles never reach a private window.
  const std::vector<Tile> shortcuts =
      state.private_window
          ? std::vector<Tile>()
          : BuildShortcuts(state.pinned, state.top_sites, state.shortcut_limit);

  std::string out = "{";
  out += Member("engine", state.engine_id) + ",";
  out += Member("engineLabel", engine_label) + ",";
  out += "\"engines\":" + engines + ",";
  out += "\"privateWindow\":";
  out += state.private_window ? "true," : "false,";
  out += "\"historyHidden\":";
  out += state.private_window ? "true," : "false,";
  out += "\"shortcuts\":[";
  for (std::vector<Tile>::size_type i = 0; i < shortcuts.size(); ++i) {
    if (i)
      out += ",";
    out += TileJson(shortcuts[i]);
  }
  out += "],\"bookmarks\":[";
  for (std::vector<Tile>::size_type i = 0; i < state.bookmarks.size(); ++i) {
    if (i)
      out += ",";
    out += TileJson(state.bookmarks[i]);
  }
  out += "]}";
  return out;
}

}  // namespace ui
}  // namespace bedrock
