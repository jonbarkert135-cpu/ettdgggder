// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_DATA_BOOKMARK_STORE_H_
#define BEDROCK_DATA_BOOKMARK_STORE_H_

#include <cstdint>
#include <string>
#include <vector>

// Bookmarks (roadmap item 35).
//
// Folders and tags both, because they answer different questions: a folder is
// where a bookmark lives, a tag is what it is about. Search covers title, URL
// and tags at once — a bookmark the user cannot find again is a bookmark they
// did not make.
//
// Import/export uses the Netscape bookmark file format, the one every browser
// reads and writes. Not a Bedrock-specific JSON: an export nobody else can
// open is a lock-in, and this project has no business locking anyone in.

namespace bedrock {
namespace data {

struct Bookmark {
  int id = 0;
  std::string url;
  std::string title;
  int folder_id = 0;  // 0 = the root folder
  std::vector<std::string> tags;
  int64_t added_at = 0;  // unix seconds
};

struct BookmarkFolder {
  int id = 0;
  std::string name;
  int parent_id = 0;
};

class BookmarkStore {
 public:
  BookmarkStore();
  ~BookmarkStore();

  int AddFolder(const std::string& name, int parent_id);
  // Removing a folder moves its contents up to the parent instead of deleting
  // them: "delete folder" losing fifty bookmarks is not a feature.
  bool RemoveFolder(int folder_id);
  const std::vector<BookmarkFolder>& folders() const { return folders_; }
  std::string PathOf(int folder_id) const;  // "Root/Reading/Papers"

  int Add(const std::string& url, const std::string& title, int folder_id);
  bool Remove(int bookmark_id);
  bool Move(int bookmark_id, int folder_id);
  bool AddTag(int bookmark_id, const std::string& tag);
  bool RemoveTag(int bookmark_id, const std::string& tag);
  std::vector<std::string> AllTags() const;  // sorted, de-duplicated

  const Bookmark* Get(int bookmark_id) const;
  const std::vector<Bookmark>& All() const { return bookmarks_; }
  int count() const { return static_cast<int>(bookmarks_.size()); }

  // Case-insensitive substring over title, URL and tags. A leading "#" limits
  // the query to tags.
  std::vector<Bookmark> Search(const std::string& query) const;
  std::vector<Bookmark> InFolder(int folder_id) const;
  std::vector<Bookmark> WithTag(const std::string& tag) const;

  // Netscape bookmark file. Tags travel in the TAGS attribute, which Firefox
  // also writes, so they survive a round trip through other browsers.
  std::string ExportHtml() const;
  // Returns the number of bookmarks imported. Existing bookmarks are kept;
  // an import that silently replaces the collection is data loss.
  int ImportHtml(const std::string& html);

 private:
  Bookmark* Find(int bookmark_id);

  std::vector<Bookmark> bookmarks_;
  std::vector<BookmarkFolder> folders_;
  int next_bookmark_id_ = 1;
  int next_folder_id_ = 1;
};

}  // namespace data
}  // namespace bedrock

#endif  // BEDROCK_DATA_BOOKMARK_STORE_H_
