// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/bookmarks/bookmark_store.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace bedrock {
namespace data {
namespace {

std::string Lower(const std::string& text) {
  std::string out = text;
  for (char& c : out)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

bool Contains(const std::string& haystack, const std::string& needle) {
  return Lower(haystack).find(Lower(needle)) != std::string::npos;
}

std::string EscapeHtml(const std::string& text) {
  std::string out;
  for (char c : text) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += c;
    }
  }
  return out;
}

std::string UnescapeHtml(const std::string& text) {
  std::string out = text;
  const std::pair<const char*, const char*> kEntities[] = {
      {"&quot;", "\""}, {"&lt;", "<"}, {"&gt;", ">"}, {"&amp;", "&"}};
  for (const auto& entity : kEntities) {
    size_t pos = 0;
    const std::string from = entity.first;
    const std::string to = entity.second;
    while ((pos = out.find(from, pos)) != std::string::npos) {
      out.replace(pos, from.size(), to);
      pos += to.size();
    }
  }
  return out;
}

// Reads attribute="value" out of an <A ...> tag, case-insensitively.
std::string Attribute(const std::string& tag, const std::string& name) {
  const std::string lower = Lower(tag);
  const std::string key = Lower(name) + "=\"";
  const size_t start = lower.find(key);
  if (start == std::string::npos)
    return std::string();
  const size_t value_start = start + key.size();
  const size_t end = tag.find('"', value_start);
  if (end == std::string::npos)
    return std::string();
  return UnescapeHtml(tag.substr(value_start, end - value_start));
}

std::vector<std::string> SplitTags(const std::string& value) {
  std::vector<std::string> tags;
  std::string current;
  for (char c : value) {
    if (c == ',') {
      if (!current.empty())
        tags.push_back(current);
      current.clear();
    } else if (c != ' ' || !current.empty()) {
      current += c;
    }
  }
  if (!current.empty())
    tags.push_back(current);
  return tags;
}

}  // namespace

BookmarkStore::BookmarkStore() {
  folders_.push_back({0, "Bookmarks", 0});  // the root always exists
}

BookmarkStore::~BookmarkStore() = default;

int BookmarkStore::AddFolder(const std::string& name, int parent_id) {
  BookmarkFolder folder;
  folder.id = next_folder_id_++;
  folder.name = name;
  folder.parent_id = parent_id;
  folders_.push_back(folder);
  return folder.id;
}

bool BookmarkStore::RemoveFolder(int folder_id) {
  if (folder_id == 0)
    return false;  // the root stays
  for (auto it = folders_.begin(); it != folders_.end(); ++it) {
    if (it->id != folder_id)
      continue;
    const int parent = it->parent_id;
    folders_.erase(it);
    for (BookmarkFolder& child : folders_) {
      if (child.parent_id == folder_id)
        child.parent_id = parent;
    }
    for (Bookmark& bookmark : bookmarks_) {
      if (bookmark.folder_id == folder_id)
        bookmark.folder_id = parent;  // contents move up, nothing vanishes
    }
    return true;
  }
  return false;
}

std::string BookmarkStore::PathOf(int folder_id) const {
  std::vector<std::string> parts;
  int current = folder_id;
  for (int guard = 0; guard < 64; ++guard) {
    const BookmarkFolder* found = nullptr;
    for (const BookmarkFolder& folder : folders_) {
      if (folder.id == current) {
        found = &folder;
        break;
      }
    }
    if (!found)
      break;
    parts.push_back(found->name);
    if (found->id == 0)
      break;
    current = found->parent_id;
  }
  std::string path;
  for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
    if (!path.empty())
      path += "/";
    path += *it;
  }
  return path;
}

Bookmark* BookmarkStore::Find(int bookmark_id) {
  for (Bookmark& bookmark : bookmarks_) {
    if (bookmark.id == bookmark_id)
      return &bookmark;
  }
  return nullptr;
}

const Bookmark* BookmarkStore::Get(int bookmark_id) const {
  return const_cast<BookmarkStore*>(this)->Find(bookmark_id);
}

int BookmarkStore::Add(const std::string& url,
                       const std::string& title,
                       int folder_id) {
  Bookmark bookmark;
  bookmark.id = next_bookmark_id_++;
  bookmark.url = url;
  bookmark.title = title;
  bookmark.folder_id = folder_id;
  bookmarks_.push_back(bookmark);
  return bookmark.id;
}

bool BookmarkStore::Remove(int bookmark_id) {
  for (auto it = bookmarks_.begin(); it != bookmarks_.end(); ++it) {
    if (it->id == bookmark_id) {
      bookmarks_.erase(it);
      return true;
    }
  }
  return false;
}

bool BookmarkStore::Move(int bookmark_id, int folder_id) {
  Bookmark* bookmark = Find(bookmark_id);
  if (!bookmark)
    return false;
  const bool known = std::any_of(
      folders_.begin(), folders_.end(),
      [folder_id](const BookmarkFolder& f) { return f.id == folder_id; });
  if (!known)
    return false;
  bookmark->folder_id = folder_id;
  return true;
}

bool BookmarkStore::AddTag(int bookmark_id, const std::string& tag) {
  Bookmark* bookmark = Find(bookmark_id);
  if (!bookmark || tag.empty())
    return false;
  const std::string normalized = Lower(tag);
  if (std::find(bookmark->tags.begin(), bookmark->tags.end(), normalized) !=
      bookmark->tags.end()) {
    return false;
  }
  bookmark->tags.push_back(normalized);
  return true;
}

bool BookmarkStore::RemoveTag(int bookmark_id, const std::string& tag) {
  Bookmark* bookmark = Find(bookmark_id);
  if (!bookmark)
    return false;
  const std::string normalized = Lower(tag);
  auto it = std::find(bookmark->tags.begin(), bookmark->tags.end(), normalized);
  if (it == bookmark->tags.end())
    return false;
  bookmark->tags.erase(it);
  return true;
}

std::vector<std::string> BookmarkStore::AllTags() const {
  std::vector<std::string> tags;
  for (const Bookmark& bookmark : bookmarks_)
    tags.insert(tags.end(), bookmark.tags.begin(), bookmark.tags.end());
  std::sort(tags.begin(), tags.end());
  tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
  return tags;
}

std::vector<Bookmark> BookmarkStore::Search(const std::string& query) const {
  std::vector<Bookmark> results;
  if (query.empty())
    return results;
  if (query[0] == '#') {
    const std::string tag = Lower(query.substr(1));
    if (tag.empty())
      return results;
    for (const Bookmark& bookmark : bookmarks_) {
      for (const std::string& candidate : bookmark.tags) {
        if (candidate.find(tag) != std::string::npos) {
          results.push_back(bookmark);
          break;
        }
      }
    }
    return results;
  }
  for (const Bookmark& bookmark : bookmarks_) {
    bool hit = Contains(bookmark.title, query) || Contains(bookmark.url, query);
    for (const std::string& tag : bookmark.tags)
      hit = hit || Contains(tag, query);
    if (hit)
      results.push_back(bookmark);
  }
  return results;
}

std::vector<Bookmark> BookmarkStore::InFolder(int folder_id) const {
  std::vector<Bookmark> results;
  for (const Bookmark& bookmark : bookmarks_) {
    if (bookmark.folder_id == folder_id)
      results.push_back(bookmark);
  }
  return results;
}

std::vector<Bookmark> BookmarkStore::WithTag(const std::string& tag) const {
  const std::string normalized = Lower(tag);
  std::vector<Bookmark> results;
  for (const Bookmark& bookmark : bookmarks_) {
    if (std::find(bookmark.tags.begin(), bookmark.tags.end(), normalized) !=
        bookmark.tags.end()) {
      results.push_back(bookmark);
    }
  }
  return results;
}

std::string BookmarkStore::ExportHtml() const {
  std::string out =
      "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n"
      "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; "
      "charset=UTF-8\">\n"
      "<TITLE>Bookmarks</TITLE>\n"
      "<H1>Bookmarks</H1>\n<DL><p>\n";
  for (const BookmarkFolder& folder : folders_) {
    const std::vector<Bookmark> in_folder = InFolder(folder.id);
    if (in_folder.empty())
      continue;
    if (folder.id != 0)
      out += "    <DT><H3>" + EscapeHtml(PathOf(folder.id)) + "</H3>\n";
    for (const Bookmark& bookmark : in_folder) {
      std::string tags;
      for (const std::string& tag : bookmark.tags) {
        if (!tags.empty())
          tags += ",";
        tags += tag;
      }
      out += "    <DT><A HREF=\"" + EscapeHtml(bookmark.url) + "\"";
      if (!tags.empty())
        out += " TAGS=\"" + EscapeHtml(tags) + "\"";
      out += ">" + EscapeHtml(bookmark.title) + "</A>\n";
    }
  }
  out += "</DL><p>\n";
  return out;
}

int BookmarkStore::ImportHtml(const std::string& html) {
  int imported = 0;
  const std::string lower = Lower(html);
  size_t pos = 0;
  while ((pos = lower.find("<a ", pos)) != std::string::npos) {
    const size_t tag_end = html.find('>', pos);
    if (tag_end == std::string::npos)
      break;
    const std::string tag = html.substr(pos, tag_end - pos + 1);
    const size_t close = lower.find("</a>", tag_end);
    const std::string title =
        close == std::string::npos
            ? std::string()
            : UnescapeHtml(html.substr(tag_end + 1, close - tag_end - 1));
    const std::string url = Attribute(tag, "href");
    pos = tag_end + 1;
    if (url.empty())
      continue;
    const int id = Add(url, title, 0);
    for (const std::string& tag_value : SplitTags(Attribute(tag, "tags")))
      AddTag(id, tag_value);
    ++imported;
  }
  return imported;
}

}  // namespace data
}  // namespace bedrock
