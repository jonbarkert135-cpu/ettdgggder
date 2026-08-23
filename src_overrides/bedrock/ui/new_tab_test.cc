// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/ui/new_tab.h"

#include <iostream>
#include <string>

namespace {

using namespace bedrock::ui;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

bool Has(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

NewTabState Sample() {
  NewTabState state;
  state.engine_id = "duckduckgo";
  state.engines = {{"duckduckgo", "DuckDuckGo"}, {"google", "Google"}};
  state.top_sites = {"https://en.wikipedia.org/wiki/Main_Page",
                     "https://news.ycombinator.com/"};
  state.bookmarks = {{"https://www.gnu.org/", "gnu", "G", false}};
  return state;
}

}  // namespace

int main() {
  // Labels: a tile says "wikipedia", not a URL.
  Check(ShortLabel("https://www.youtube.com/watch?v=1") == "youtube",
        "www and suffix dropped");
  Check(ShortLabel("http://user:pw@news.ycombinator.com:8080/x") ==
            "news.ycombinator",
        "credentials and port are not part of a label");
  Check(ShortLabel("http://localhost:3000/") == "localhost",
        "a single-label host is kept whole");
  Check(ShortLabel("http://192.168.1.4/admin") == "192.168.1.4",
        "an address is not truncated into nonsense");
  Check(ShortLabel("") .empty(), "no url, no label");

  // Shortcut row: pinned first, no duplicates, capped.
  const std::vector<std::string> pinned = {"https://a.example/",
                                           "https://b.example/"};
  const std::vector<std::string> top = {"https://b.example/",
                                        "https://c.example/",
                                        "https://d.example/"};
  const std::vector<Tile> tiles = BuildShortcuts(pinned, top, 3);
  Check(tiles.size() == 3, "the limit is respected");
  Check(tiles[0].url == "https://a.example/" && tiles[0].pinned,
        "pinned comes first");
  Check(tiles[1].url == "https://b.example/" && tiles[1].pinned,
        "a pinned top site appears once, as pinned");
  Check(tiles[2].url == "https://c.example/" && !tiles[2].pinned,
        "then the most visited");
  Check(tiles[0].initial == "A", "a tile has a fallback initial");
  Check(BuildShortcuts(pinned, top, 0).empty(), "a zero limit shows nothing");

  // The selector reports what is actually in use.
  const std::string json = NewTabJson(Sample());
  Check(Has(json, "\"engineLabel\":\"DuckDuckGo\""), "the engine is named");
  Check(Has(json, "\"id\":\"google\",\"label\":\"Google\",\"selected\":false"),
        "the other engine is offered, unselected");
  Check(Has(json, "\"title\":\"en.wikipedia\""), "shortcuts are in the model");
  Check(Has(json, "\"historyHidden\":false"), "a normal window shows history");

  // The privacy rule this file exists for.
  NewTabState privat = Sample();
  privat.private_window = true;
  const std::string private_json = NewTabJson(privat);
  Check(Has(private_json, "\"shortcuts\":[]"),
        "a private window shows no history-derived tiles");
  Check(Has(private_json, "\"historyHidden\":true"),
        "and says so instead of looking empty");
  Check(Has(private_json, "\"title\":\"gnu\""),
        "bookmarks are the user's own choice and stay");

  // A hostile title cannot break out of the JSON string.
  NewTabState escaped = Sample();
  escaped.bookmarks = {{"https://x.example/", "a\"b\\c\nd", "A", false}};
  Check(Has(NewTabJson(escaped), "\"title\":\"a\\\"b\\\\c\\nd\""),
        "strings are escaped");

  if (failures == 0)
    std::cout << "new_tab: ok\n";
  return failures == 0 ? 0 : 1;
}
