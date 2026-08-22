// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/diagnostics/debug_log.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace bedrock {
namespace diagnostics {

namespace {

int Rank(Level level) {
  switch (level) {
    case Level::kOff:
      return 0;
    case Level::kError:
      return 1;
    case Level::kWarning:
      return 2;
    case Level::kInfo:
      return 3;
    case Level::kVerbose:
      return 4;
  }
  return 0;
}

const char* Name(Level level) {
  switch (level) {
    case Level::kOff:
      return "off";
    case Level::kError:
      return "error";
    case Level::kWarning:
      return "warning";
    case Level::kInfo:
      return "info";
    case Level::kVerbose:
      return "verbose";
  }
  return "off";
}

// A path is inside the profile when it starts with the profile directory and
// does not climb out of it again. No filesystem call: this must give the same
// answer in a host test as on a machine where the directory does not exist yet.
bool IsInside(const std::string& profile_dir, const std::string& path) {
  if (profile_dir.empty() || path.size() <= profile_dir.size()) {
    return false;
  }
  if (path.compare(0, profile_dir.size(), profile_dir) != 0) {
    return false;
  }
  const char separator = path[profile_dir.size()];
  if (separator != '/' && separator != '\\') {
    return false;
  }
  return path.find("..") == std::string::npos;
}

}  // namespace

DebugLog::DebugLog(std::size_t capacity) : capacity_(capacity ? capacity : 1) {}

bool DebugLog::Log(Level level,
                   const std::string& area,
                   const std::string& message,
                   int64_t at) {
  if (level == Level::kOff || Rank(level) > Rank(level_)) {
    return false;
  }
  const ScrubResult scrubbed = Scrubber::Scrub(message);
  LogEntry entry;
  entry.level = level;
  entry.area = area;
  entry.message = scrubbed.text;
  entry.at = at;
  entry.redactions = scrubbed.redactions;
  if (entries_.size() == capacity_) {
    entries_.erase(entries_.begin());
  }
  entries_.push_back(entry);
  return true;
}

void DebugLog::Clear() {
  entries_.clear();
}

bool DebugLog::EnableFileSink(const std::string& profile_dir,
                              const std::string& path) {
  if (!IsInside(profile_dir, path)) {
    return false;
  }
  file_sink_enabled_ = true;
  file_sink_path_ = path;
  return true;
}

void DebugLog::DisableFileSink() {
  file_sink_enabled_ = false;
  file_sink_path_.clear();
}

std::string DebugLog::ExportBundle() const {
  int redactions = 0;
  for (const LogEntry& entry : entries_) {
    redactions += entry.redactions;
  }
  std::string out = "Bedrock debug log\n";
  out += "level: ";
  out += Name(level_);
  out += "\nentries: " + std::to_string(entries_.size()) + "\n";
  out += "redacted values: " + std::to_string(redactions) +
         " (URLs, cookies, credentials and paths were removed before this file "
         "was written)\n";
  out += "this file stays on your computer unless you attach it yourself\n\n";
  for (const LogEntry& entry : entries_) {
    out += std::to_string(entry.at);
    out += " [";
    out += Name(entry.level);
    out += "] ";
    out += entry.area;
    out += ": ";
    out += entry.message;
    out += "\n";
  }
  return out;
}

}  // namespace diagnostics
}  // namespace bedrock
