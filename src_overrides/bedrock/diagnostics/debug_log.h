// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_DIAGNOSTICS_DEBUG_LOG_H_
#define BEDROCK_DIAGNOSTICS_DEBUG_LOG_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "bedrock/diagnostics/scrubber.h"

// Local debug logging (roadmap item 79).
//
//   DEBUG LOG != TELEMETRY.
//
// The difference is not the content, it is who decides and where it goes. A
// telemetry system decides for the user and sends the result to a vendor; a
// debug log is written on the user's disk, at a verbosity the user chose, and
// moves only when the user themselves attaches it to a bug report. Item 39
// removed the first thing from Bedrock entirely. This class provides the
// second, with the property that makes it safe to keep: **there is no sink
// that can reach the network.**
//
// That is enforced three ways, deliberately overlapping, because "we would
// never add an upload" is not an architecture:
//
//   1. `Sink` has two values — an in-memory ring and a file inside the profile.
//      Adding a third means editing this enum, which is a reviewable event.
//   2. Nothing here takes a URL, a socket, or a callback that could carry one.
//   3. `scripts/check_diagnostics.py` fails the build if this directory grows a
//      network symbol, and `check_no_telemetry.py` already fails it if any
//      Bedrock file gains reporting machinery.
//
// Defaults, which are the part users actually live with:
//
//   * verbosity `kOff` — nothing is recorded until asked for. A browser that
//     logs by default writes the user's browsing history to disk in a second
//     place, under a name nobody thinks to clear.
//   * the file sink is disabled; turning it on requires a path inside the
//     profile, so a log cannot be aimed at a synced folder by accident.
//   * every line is scrubbed *before* it is stored (see scrubber.h), so the
//     worst a careless `Log("navigating to " + url)` can do is write `<url>`.
//
// The ring buffer is bounded and overwrites oldest-first: a crash-time log that
// filled the disk would be a denial of service the user paid for in disk space.

namespace bedrock {
namespace diagnostics {

enum class Level {
  kOff,      // default
  kError,
  kWarning,
  kInfo,
  kVerbose,  // what a developer asks a reporter to turn on for one repro
};

// Where a recorded line may go. Both are on this machine. There is no third.
enum class Sink {
  kMemoryRing,
  kProfileFile,
};

struct LogEntry {
  Level level = Level::kError;
  std::string area;     // "net", "privacy.filter", "ui.tabs" — a subsystem
  std::string message;  // already scrubbed
  int64_t at = 0;       // unix seconds
  int redactions = 0;   // how much the scrubber removed from this line
};

class DebugLog {
 public:
  explicit DebugLog(std::size_t capacity = 2000);

  // Verbosity. Off by default; the user sets it in Settings or with
  // --debug-log-level, and it resets to kOff on restart unless persisted.
  Level level() const { return level_; }
  void SetLevel(Level level) { level_ = level; }

  // Records a line if the level allows it. Returns true when something was
  // stored. `message` is scrubbed here, not at read time.
  bool Log(Level level, const std::string& area, const std::string& message,
           int64_t at = 0);

  const std::vector<LogEntry>& Entries() const { return entries_; }
  std::size_t capacity() const { return capacity_; }
  void Clear();

  // Turns on the file sink. `path` must be inside `profile_dir` — an absolute
  // path elsewhere is refused rather than silently redirected, because a log
  // the user cannot find is a log the user cannot delete. Returns false and
  // changes nothing when refused.
  bool EnableFileSink(const std::string& profile_dir, const std::string& path);
  void DisableFileSink();
  bool file_sink_enabled() const { return file_sink_enabled_; }
  const std::string& file_sink_path() const { return file_sink_path_; }

  // The text the user gets when they click "Save diagnostics" — the stored
  // (already scrubbed) lines, with a header stating what was removed. Nothing
  // sends this anywhere; it is handed to the user as a file.
  std::string ExportBundle() const;

  // Stated as code so that a reader of this header, and the gate, both get the
  // same answer: uploading is not a feature that is switched off, it is a
  // feature that does not exist.
  static constexpr bool kUploadSupported = false;

 private:
  std::size_t capacity_;
  Level level_ = Level::kOff;
  std::vector<LogEntry> entries_;
  bool file_sink_enabled_ = false;
  std::string file_sink_path_;
};

}  // namespace diagnostics
}  // namespace bedrock

#endif  // BEDROCK_DIAGNOSTICS_DEBUG_LOG_H_
