// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/downloads/download_manager.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bedrock {
namespace downloads {
namespace {

std::string Lower(const std::string& text) {
  std::string out = text;
  for (char& c : out)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

std::string Extension(const std::string& filename) {
  const size_t dot = filename.find_last_of('.');
  if (dot == std::string::npos || dot + 1 >= filename.size())
    return std::string();
  return Lower(filename.substr(dot + 1));
}

bool IsExecutableExtension(const std::string& ext) {
  static const char* kExecutable[] = {
      "exe", "msi", "bat", "cmd", "com", "scr", "pif", "vbs", "js",  "jse",
      "ps1", "psm1", "sh", "bash", "zsh", "dmg", "pkg", "app", "apk", "deb",
      "rpm", "jar", "reg", "dll", "so",  "dylib", "lnk", "desktop", "hta",
      "wsf", "scpt", "run", "appimage", "gadget", "cpl"};
  for (const char* candidate : kExecutable) {
    if (ext == candidate)
      return true;
  }
  return false;
}

bool IsCommonExtension(const std::string& ext) {
  static const char* kCommon[] = {
      "pdf", "txt", "md",  "csv",  "json", "xml", "zip",  "gz",  "tar",
      "7z",  "rar", "png", "jpg",  "jpeg", "gif", "webp", "svg", "mp3",
      "mp4", "mov", "wav", "doc",  "docx", "xls", "xlsx", "ppt", "pptx",
      "odt", "ods", "epub", "iso", "torrent"};
  for (const char* candidate : kCommon) {
    if (ext == candidate)
      return true;
  }
  return false;
}

// The classic trick: invoice.pdf.exe, holiday.jpg.scr. Also catches the
// right-to-left override character used to reverse a name on screen.
bool LooksDeceptive(const std::string& filename, const std::string& ext) {
  if (filename.find("\u202E") != std::string::npos)
    return true;
  const size_t last_dot = filename.find_last_of('.');
  if (last_dot == std::string::npos || last_dot == 0)
    return false;
  const std::string rest = filename.substr(0, last_dot);
  const std::string inner = Extension(rest);
  if (inner.empty())
    return false;
  // Two extensions where the visible-looking one is a document and the real
  // one runs code.
  return IsCommonExtension(inner) && IsExecutableExtension(ext);
}

std::string BaseMime(const std::string& mime_type) {
  const size_t semicolon = mime_type.find(';');
  return Lower(semicolon == std::string::npos ? mime_type
                                              : mime_type.substr(0, semicolon));
}

}  // namespace

DownloadManager::DownloadManager() = default;
DownloadManager::~DownloadManager() = default;

RiskAssessment DownloadManager::Assess(const std::string& filename,
                                       const std::string& served_mime_type) {
  RiskAssessment assessment;
  const std::string ext = Extension(filename);
  const std::string mime = BaseMime(served_mime_type);

  if (LooksDeceptive(filename, ext)) {
    assessment.risk = Risk::kDeceptive;
    assessment.reason =
        "This file name is arranged to look like a document, but it is a "
        "program.";
    assessment.requires_confirmation = true;
    return assessment;
  }
  if (IsExecutableExtension(ext)) {
    assessment.risk = Risk::kExecutable;
    assessment.reason =
        "This file can run programs on your computer. Keep it only if you "
        "trust the source.";
    assessment.requires_confirmation = true;
    return assessment;
  }
  if (!ext.empty() && !IsCommonExtension(ext)) {
    assessment.risk = Risk::kUncommon;
    assessment.reason = "This file type is uncommon.";
    // Uncommon is information, not an accusation, and does not block.
    return assessment;
  }
  // A PDF served as an executable stream is worth mentioning without
  // pretending we know what it is.
  if (ext == "pdf" && mime == "application/x-msdownload") {
    assessment.risk = Risk::kDeceptive;
    assessment.reason =
        "The site sent a program while offering a PDF.";
    assessment.requires_confirmation = true;
    return assessment;
  }
  assessment.reason = "No local warnings for this file type.";
  return assessment;
}

DownloadItem* DownloadManager::Find(int id) {
  for (DownloadItem& item : items_) {
    if (item.id == id)
      return &item;
  }
  return nullptr;
}

const DownloadItem* DownloadManager::Find(int id) const {
  return const_cast<DownloadManager*>(this)->Find(id);
}

const DownloadItem* DownloadManager::Get(int id) const {
  return Find(id);
}

int DownloadManager::Start(const std::string& url,
                           const std::string& filename,
                           const std::string& directory,
                           const std::string& mime_type,
                           bool from_private_window) {
  DownloadItem item;
  item.id = next_id_++;
  item.url = url;
  item.filename = filename;
  item.directory = directory;
  item.mime_type = mime_type;
  item.from_private_window = from_private_window;
  item.risk = Assess(filename, mime_type);
  item.state = State::kQueued;
  items_.push_back(item);
  return item.id;
}

bool DownloadManager::OnResponse(int id,
                                 const TransferCapabilities& capabilities) {
  DownloadItem* item = Find(id);
  if (!item)
    return false;
  // Resuming needs both a server that accepts ranges and something to prove
  // the bytes still belong to the same file.
  item->can_resume =
      capabilities.accept_ranges && !capabilities.validator.empty();
  item->total_bytes = capabilities.total_bytes;
  item->state = State::kActive;
  return true;
}

bool DownloadManager::OnProgress(int id, int64_t received_bytes) {
  DownloadItem* item = Find(id);
  if (!item || item->state != State::kActive)
    return false;
  item->received_bytes = std::max(item->received_bytes, received_bytes);
  return true;
}

bool DownloadManager::OnInterrupted(int id, const std::string& reason) {
  DownloadItem* item = Find(id);
  if (!item)
    return false;
  item->state = State::kInterrupted;
  item->interrupt_reason = reason;
  return true;
}

bool DownloadManager::OnFinished(int id) {
  DownloadItem* item = Find(id);
  if (!item)
    return false;
  item->state = State::kComplete;
  if (item->total_bytes < 0)
    item->total_bytes = item->received_bytes;
  return true;
}

bool DownloadManager::Pause(int id) {
  last_error_.clear();
  DownloadItem* item = Find(id);
  if (!item || item->state != State::kActive) {
    last_error_ = "this download is not running";
    return false;
  }
  if (!item->can_resume) {
    // Offering Pause here would be a promise we cannot keep.
    last_error_ =
        "this server cannot continue an interrupted download, so pausing "
        "would restart it from the beginning";
    return false;
  }
  item->state = State::kPaused;
  return true;
}

bool DownloadManager::Resume(int id) {
  last_error_.clear();
  DownloadItem* item = Find(id);
  if (!item) {
    last_error_ = "no such download";
    return false;
  }
  if (item->state != State::kPaused && item->state != State::kInterrupted) {
    last_error_ = "this download is not paused";
    return false;
  }
  if (!item->can_resume) {
    // An interrupted non-resumable transfer can only be retried, and the
    // user is told the bytes so far are lost.
    last_error_ = "this download must be restarted from the beginning";
    return false;
  }
  item->state = State::kActive;
  return true;
}

bool DownloadManager::Cancel(int id) {
  last_error_.clear();
  DownloadItem* item = Find(id);
  if (!item || item->state == State::kComplete) {
    last_error_ = "this download has already finished";
    return false;
  }
  item->state = State::kCancelled;
  return true;
}

bool DownloadManager::Retry(int id, bool automatic) {
  last_error_.clear();
  DownloadItem* item = Find(id);
  if (!item) {
    last_error_ = "no such download";
    return false;
  }
  if (item->state == State::kComplete || item->state == State::kActive) {
    last_error_ = "nothing to retry";
    return false;
  }
  if (automatic && item->retry_count >= kMaxAutomaticRetries) {
    last_error_ = "automatic retries exhausted";
    return false;
  }
  ++item->retry_count;
  item->interrupt_reason.clear();
  if (!item->can_resume)
    item->received_bytes = 0;  // honest: we are starting over
  item->state = State::kQueued;
  return true;
}

bool DownloadManager::CanRevealInFolder(int id) const {
  const DownloadItem* item = Find(id);
  return item && item->state == State::kComplete;
}

std::string DownloadManager::RevealPath(int id) const {
  const DownloadItem* item = Find(id);
  if (!item || item->state != State::kComplete)
    return std::string();
  return item->directory + "/" + item->filename;
}

bool DownloadManager::NeedsConfirmation(int id) const {
  const DownloadItem* item = Find(id);
  return item && item->risk.requires_confirmation;
}

bool DownloadManager::Confirm(int id) {
  DownloadItem* item = Find(id);
  if (!item)
    return false;
  item->risk.requires_confirmation = false;
  return true;
}

std::vector<DownloadItem> DownloadManager::History() const {
  std::vector<DownloadItem> history;
  for (const DownloadItem& item : items_) {
    if (item.from_private_window)
      continue;
    history.push_back(item);
  }
  return history;
}

bool DownloadManager::RemoveFromHistory(int id) {
  for (auto it = items_.begin(); it != items_.end(); ++it) {
    if (it->id == id) {
      items_.erase(it);
      return true;
    }
  }
  return false;
}

void DownloadManager::ClearHistory() {
  items_.clear();
}

}  // namespace downloads
}  // namespace bedrock
