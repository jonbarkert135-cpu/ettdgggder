// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_UI_NEW_TAB_H_
#define BEDROCK_UI_NEW_TAB_H_

#include <string>
#include <vector>

// The new tab page (design items 11-14).
//
// The page is a renderer, like the first-run page: it receives this state as
// JSON and sends back an id. The shortcut row, the bookmark row and the engine
// selector are built here so that the rules around them are testable — and the
// rule that matters is a privacy one: shortcuts are derived from history, so a
// private window must not show them. A "new tab" that quietly displays the
// most-visited sites of the normal profile is a leak to whoever is looking at
// the screen, and it is exactly the kind of thing that ships unnoticed when the
// decision lives in JavaScript.
//
// Bookmarks are not history: the user placed them there deliberately, so the
// bookmark row stays in private windows.

namespace bedrock {
namespace ui {

struct Tile {
  std::string url;
  std::string title;    // what the tile is labelled with
  std::string initial;  // one uppercase character for the fallback favicon
  bool pinned = false;
};

struct EngineChoice {
  std::string id;
  std::string label;
};

struct NewTabState {
  std::string engine_id;                 // must be one of `engines`
  std::vector<EngineChoice> engines;     // what the selector offers
  std::vector<std::string> pinned;       // urls the user pinned, in order
  std::vector<std::string> top_sites;    // from HistoryStore::TopSites()
  std::vector<Tile> bookmarks;           // the bookmark bar, already ordered
  bool private_window = false;
  int shortcut_limit = 10;
};

// The label a tile carries when the site has no title: the registrable-ish host
// with "www." and the trailing public suffix removed, so "https://youtube.com/"
// reads as "youtube" rather than as a URL.
std::string ShortLabel(const std::string& url);

// Pinned first, then the most visited, without duplicates, capped at `limit`.
// A pinned url that is also a top site appears once, as pinned.
std::vector<Tile> BuildShortcuts(const std::vector<std::string>& pinned,
                                 const std::vector<std::string>& top_sites,
                                 int limit);

// The whole page state as JSON. In a private window the shortcut row is empty
// and `historyHidden` is true, so the page can say why instead of looking
// broken.
std::string NewTabJson(const NewTabState& state);

}  // namespace ui
}  // namespace bedrock

#endif  // BEDROCK_UI_NEW_TAB_H_
