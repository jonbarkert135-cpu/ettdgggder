// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_DOWNLOADS_DOWNLOAD_MANAGER_H_
#define BEDROCK_DOWNLOADS_DOWNLOAD_MANAGER_H_

#include <cstdint>
#include <string>
#include <vector>

// Download manager (roadmap item 33).
//
// Two things make download managers bad. The first is lying about pause: the
// UI offers Pause, the server does not support range requests, and Resume
// silently restarts from zero on a 3 GB file. Here `can_resume` is derived
// from the response (Accept-Ranges / strong validator) and Pause on a
// non-resumable transfer is *refused with a reason* instead of accepted and
// betrayed later.
//
// The second is safety theatre. Bedrock has no server, so there is no cloud
// reputation lookup and no telling the user a file is "verified safe". What
// it can do locally: recognise executable and script types, spot the
// extension tricks (invoice.pdf.exe, right-to-left override), notice a
// mismatch between the promised type and the served one, and require an
// explicit action before such a file is kept. It never auto-opens anything.

namespace bedrock {
namespace downloads {

enum class State {
  kQueued,
  kActive,
  kPaused,
  kInterrupted,  // network died, disk full, server hung up
  kComplete,
  kCancelled,
};

enum class Risk {
  kSafe,        // ordinary document, image, archive
  kUncommon,    // rare type; we say so, we do not claim it is malicious
  kExecutable,  // runs code if opened: .exe, .msi, .dmg, .sh, .ps1, .apk
  kDeceptive,   // the name is engineered to look like something else
};

struct RiskAssessment {
  Risk risk = Risk::kSafe;
  std::string reason;   // shown to the user, plain language
  bool requires_confirmation = false;
};

// What the response told us about resuming. Filled in by the network layer.
struct TransferCapabilities {
  bool accept_ranges = false;
  std::string validator;  // ETag or Last-Modified; empty = nothing to pin to
  int64_t total_bytes = -1;  // -1 = unknown length
};

struct DownloadItem {
  int id = 0;
  std::string url;
  std::string filename;
  std::string directory;
  std::string mime_type;
  State state = State::kQueued;
  int64_t received_bytes = 0;
  int64_t total_bytes = -1;
  int retry_count = 0;
  bool can_resume = false;
  bool from_private_window = false;
  RiskAssessment risk;
  std::string interrupt_reason;
};

class DownloadManager {
 public:
  DownloadManager();
  ~DownloadManager();

  static constexpr int kMaxAutomaticRetries = 3;

  // Local, offline risk assessment of a proposed download.
  static RiskAssessment Assess(const std::string& filename,
                               const std::string& served_mime_type);

  int Start(const std::string& url,
            const std::string& filename,
            const std::string& directory,
            const std::string& mime_type,
            bool from_private_window);

  // Called once headers arrive.
  bool OnResponse(int id, const TransferCapabilities& capabilities);
  bool OnProgress(int id, int64_t received_bytes);
  bool OnInterrupted(int id, const std::string& reason);
  bool OnFinished(int id);

  bool Pause(int id);
  bool Resume(int id);
  bool Cancel(int id);
  // Manual retry works after failure *and* after cancel; the automatic one
  // (Retry called by the network layer) stops at kMaxAutomaticRetries so a
  // dead link cannot spin forever.
  bool Retry(int id, bool automatic);

  // Reveal in folder. Refused while the file does not exist yet, because
  // opening a file manager on a .crdownload part file is a bug report.
  bool CanRevealInFolder(int id) const;
  std::string RevealPath(int id) const;

  // A completed dangerous file is kept only after the user says so.
  bool Confirm(int id);
  bool NeedsConfirmation(int id) const;

  const DownloadItem* Get(int id) const;
  // History. Private-window downloads are not written to it: the file stays
  // on disk (the user asked for it), the record does not (they did not).
  std::vector<DownloadItem> History() const;
  bool RemoveFromHistory(int id);
  void ClearHistory();

  const std::string& last_error() const { return last_error_; }

 private:
  DownloadItem* Find(int id);
  const DownloadItem* Find(int id) const;

  std::vector<DownloadItem> items_;
  int next_id_ = 1;
  std::string last_error_;
};

}  // namespace downloads
}  // namespace bedrock

#endif  // BEDROCK_DOWNLOADS_DOWNLOAD_MANAGER_H_
