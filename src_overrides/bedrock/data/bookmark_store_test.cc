// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/data/bookmark_store.h"

#include <iostream>
#include <string>

namespace {

using bedrock::data::Bookmark;
using bedrock::data::BookmarkStore;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

}  // namespace

int main() {
  BookmarkStore store;

  const int reading = store.AddFolder("Reading", 0);
  const int papers = store.AddFolder("Papers", reading);
  Check(store.PathOf(papers) == "Bookmarks/Reading/Papers",
        "folder paths read from the root down");

  const int paper = store.Add("https://arxiv.org/abs/2401.00001",
                              "Fingerprinting surfaces", papers);
  const int news = store.Add("https://example.org/news", "Daily news", 0);
  store.AddTag(paper, "Privacy");
  store.AddTag(paper, "research");
  Check(!store.AddTag(paper, "privacy"),
        "tags are case-insensitive, so the same tag is not stored twice");
  Check(store.Get(paper)->tags.size() == 2, "two tags stick");

  // Search covers title, URL and tags.
  Check(store.Search("fingerprinting").size() == 1, "title search works");
  Check(store.Search("ARXIV").size() == 1, "URL search is case-insensitive");
  Check(store.Search("research").size() == 1, "tags are searchable too");
  Check(store.Search("#priv").size() == 1, "#tag limits the search to tags");
  Check(store.Search("#news").empty(),
        "a #tag query does not fall back to titles");
  Check(store.WithTag("privacy").size() == 1, "exact tag lookup works");
  Check(store.AllTags().size() == 2, "the tag list is de-duplicated");

  // Folders.
  Check(store.InFolder(papers).size() == 1, "the bookmark is in its folder");
  Check(store.Move(news, reading), "a bookmark can move");
  Check(!store.Move(news, 999), "into an existing folder only");
  Check(store.RemoveFolder(papers), "a folder can be removed");
  Check(store.count() == 2, "removing a folder keeps its bookmarks");
  Check(store.Get(paper)->folder_id == reading,
        "and moves them up to the parent");
  Check(!store.RemoveFolder(0), "the root folder stays");

  // Export/import round trip in the format other browsers read.
  const std::string html = store.ExportHtml();
  Check(html.find("<!DOCTYPE NETSCAPE-Bookmark-file-1>") == 0,
        "the export is a Netscape bookmark file, not a Bedrock-only format");
  Check(html.find("https://arxiv.org/abs/2401.00001") != std::string::npos,
        "the URL is in there");
  Check(html.find("TAGS=\"privacy,research\"") != std::string::npos,
        "so are the tags");

  BookmarkStore imported;
  const int existing = imported.Add("https://keep.example", "Keep me", 0);
  Check(imported.ImportHtml(html) == 2, "both bookmarks are imported");
  Check(imported.Get(existing) != nullptr,
        "an import adds to the collection instead of replacing it");
  Check(imported.Search("fingerprinting").size() == 1,
        "the imported bookmark is searchable");
  Check(imported.WithTag("research").size() == 1, "and kept its tags");

  // Escaping survives the round trip.
  BookmarkStore tricky;
  tricky.Add("https://example.org/?a=1&b=2", "Tom & \"Jerry\" <b>", 0);
  BookmarkStore back;
  back.ImportHtml(tricky.ExportHtml());
  Check(back.count() == 1, "the escaped bookmark comes back");
  Check(back.All()[0].url == "https://example.org/?a=1&b=2",
        "with its ampersand intact");
  Check(back.All()[0].title == "Tom & \"Jerry\" <b>",
        "and its quotes and angle brackets");

  Check(store.Remove(news), "a bookmark can be deleted");
  Check(store.count() == 1, "and is gone");

  if (failures == 0)
    std::cout << "bookmark_store_test: all assertions passed\n";
  return failures == 0 ? 0 : 1;
}
