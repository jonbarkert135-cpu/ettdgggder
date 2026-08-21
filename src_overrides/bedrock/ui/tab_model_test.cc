// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/ui/tab_model.h"

#include <algorithm>
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

bool Contains(const std::vector<int>& ids, int id) {
  return std::find(ids.begin(), ids.end(), id) != ids.end();
}

}  // namespace

int main() {
  TabModel tabs;
  const int news = tabs.Add("Morning news", "https://news.test/", 1000);
  const int docs = tabs.Add("Docs", "https://docs.test/guide", 1100);
  const int shop = tabs.Add("Shop", "https://shop.test/cart", 1200);

  // Layout is a rendering choice over one model — the same tabs either way.
  {
    const size_t horizontal = tabs.size();
    tabs.set_layout(TabLayout::kVertical);
    Check(tabs.size() == horizontal && tabs.layout() == TabLayout::kVertical,
          "switching to vertical tabs changes nothing but the layout");
    tabs.set_layout(TabLayout::kHorizontal);
  }

  // Pinned tabs come first, and the model maintains that itself.
  {
    Check(tabs.SetPinned(shop, true), "a tab can be pinned");
    Check(tabs.tabs().front().id == shop, "pinned tabs sort to the front");
    tabs.SetPinned(shop, false);
    Check(tabs.tabs().front().id == shop,
          "unpinning drops the tab at the start of the unpinned section "
          "(where the user is looking) rather than teleporting it back");
  }

  // Groups stay contiguous.
  {
    const int work = tabs.CreateGroup("Work");
    Check(tabs.GroupName(work) == "Work", "a group has a name");
    Check(tabs.AddToGroup(docs, work) && tabs.AddToGroup(shop, work),
          "tabs join a group");
    Check(!tabs.AddToGroup(news, 999), "an unknown group is refused");
    const auto& ordered = tabs.tabs();
    size_t first_grouped = ordered.size();
    size_t last_grouped = 0;
    for (size_t i = 0; i < ordered.size(); ++i) {
      if (ordered[i].group == work) {
        first_grouped = std::min(first_grouped, i);
        last_grouped = std::max(last_grouped, i);
      }
    }
    Check(last_grouped - first_grouped == 1,
          "the group's tabs are next to each other");
    Check(tabs.TabsInGroup(work).size() == 2, "the group has two tabs");
    Check(tabs.RemoveFromGroup(shop) && tabs.TabsInGroup(work).size() == 1,
          "a tab can leave the group");
  }

  // Muting and audio state.
  {
    Check(tabs.SetMuted(news, true) && tabs.Find(news)->muted,
          "a tab can be muted");
    Check(tabs.SetAudible(docs, true) && tabs.Find(docs)->audible,
          "audio state is tracked separately from muting");
  }

  // Sleeping: never the active tab, never pinned, never audible.
  {
    tabs.Activate(news, 2000);
    tabs.SetPinned(shop, true);
    const std::vector<int> slept = tabs.SleepIdleTabs(60000, 30000);
    Check(!Contains(slept, news), "the active tab never sleeps");
    Check(!Contains(slept, shop), "a pinned tab never sleeps");
    Check(!Contains(slept, docs), "a tab making sound never sleeps");
    tabs.SetAudible(docs, false);
    const std::vector<int> second = tabs.SleepIdleTabs(60000, 30000);
    Check(Contains(second, docs), "a silent idle background tab does sleep");
    Check(tabs.Find(docs)->sleeping, "and is marked as sleeping");
    tabs.Activate(docs, 61000);
    Check(!tabs.Find(docs)->sleeping, "activating wakes it");
    tabs.SetPinned(shop, false);
  }

  // Search over title and URL.
  {
    Check(tabs.Search("MORNING").size() == 1, "search is case-insensitive");
    Check(tabs.Search("shop.test").size() == 1, "search covers the URL");
    Check(tabs.Search("").empty(), "an empty query matches nothing");
    Check(tabs.Search("nothing-here").empty(), "no false matches");
  }

  // Duplicate detection ignores the noise that makes two identical pages look
  // different.
  {
    TabModel dupes;
    const int a = dupes.Add("Article", "https://news.test/article", 0);
    const int b = dupes.Add("Article", "http://www.news.test/article/", 0);
    const int c = dupes.Add("Article", "https://news.test/article?utm_source=x", 0);
    const int d = dupes.Add("Article", "https://news.test/article#section-2", 0);
    const int other = dupes.Add("Other", "https://news.test/other", 0);
    const auto sets = dupes.FindDuplicates();
    Check(sets.size() == 1, "one duplicate set");
    Check(sets[0].size() == 4 && Contains(sets[0], a) && Contains(sets[0], b) &&
              Contains(sets[0], c) && Contains(sets[0], d),
          "scheme, www., trailing slash, tracking params and fragment ignored");
    Check(!Contains(sets[0], other), "a genuinely different page is not a dupe");
    Check(TabModel::NormalizeUrl("https://news.test/a?id=7&utm_medium=x") ==
              "news.test/a?id=7",
          "real query parameters are kept");
  }

  // Closing and reopening, including position.
  {
    TabModel closing;
    const int first = closing.Add("First", "https://a.test/", 0);
    const int second = closing.Add("Second", "https://b.test/", 0);
    const int third = closing.Add("Third", "https://c.test/", 0);
    closing.Activate(second, 10);
    Check(closing.Close(second), "a tab closes");
    Check(closing.active_id() == third,
          "closing the active tab activates its neighbour, not nothing");
    Check(closing.recently_closed().size() == 1, "it is recoverable");
    Check(closing.ReopenLastClosed() == second, "and comes back");
    Check(closing.tabs()[1].id == second, "at the position it had");
    Check(closing.recently_closed().empty(), "and leaves the list");
    Check(closing.ReopenLastClosed() == 0, "reopening nothing is harmless");
    Check(!closing.Close(4242), "closing an unknown id fails cleanly");
    (void)first;

    // The list is capped rather than growing forever.
    for (int i = 0; i < 40; ++i) {
      const int id = closing.Add("T" + std::to_string(i), "https://t.test/", 0);
      closing.Close(id);
    }
    Check(closing.recently_closed().size() == TabModel::kRecentlyClosedLimit,
          "recently closed is capped");
  }

  // Closing a group keeps every tab recoverable.
  {
    TabModel grouped;
    const int group = grouped.CreateGroup("Research");
    const int one = grouped.Add("One", "https://1.test/", 0);
    const int two = grouped.Add("Two", "https://2.test/", 0);
    grouped.AddToGroup(one, group);
    grouped.AddToGroup(two, group);
    Check(grouped.CloseGroup(group), "a group closes");
    Check(grouped.size() == 0, "its tabs are gone");
    Check(grouped.recently_closed().size() == 2, "but all are recoverable");
    Check(!grouped.CloseGroup(group), "closing it again is a no-op");
  }

  if (failures == 0) {
    std::cout << "tab_model_test: all assertions passed\n";
  }
  return failures == 0 ? 0 : 1;
}
