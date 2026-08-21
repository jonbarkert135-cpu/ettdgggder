// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/downloads/download_manager.h"

#include <iostream>
#include <string>

namespace {

using bedrock::downloads::DownloadManager;
using bedrock::downloads::Risk;
using bedrock::downloads::RiskAssessment;
using bedrock::downloads::State;
using bedrock::downloads::TransferCapabilities;

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

TransferCapabilities Resumable(int64_t total) {
  TransferCapabilities caps;
  caps.accept_ranges = true;
  caps.validator = "\"abc123\"";
  caps.total_bytes = total;
  return caps;
}

TransferCapabilities NotResumable() {
  TransferCapabilities caps;
  caps.accept_ranges = false;
  caps.total_bytes = -1;
  return caps;
}

}  // namespace

int main() {
  // Risk assessment is pure and local.
  Check(DownloadManager::Assess("report.pdf", "application/pdf").risk ==
            Risk::kSafe,
        "an ordinary document is not flagged");
  const RiskAssessment exe =
      DownloadManager::Assess("setup.exe", "application/octet-stream");
  Check(exe.risk == Risk::kExecutable && exe.requires_confirmation,
        "an executable needs confirmation");
  const RiskAssessment deceptive =
      DownloadManager::Assess("invoice.pdf.exe", "application/octet-stream");
  Check(deceptive.risk == Risk::kDeceptive && deceptive.requires_confirmation,
        "the double-extension trick is caught");
  Check(DownloadManager::Assess("\u202Egpj.exe", "application/octet-stream")
                .risk == Risk::kDeceptive,
        "a right-to-left override in the name is caught");
  Check(DownloadManager::Assess("model.q4_k_m", "application/octet-stream")
                .risk == Risk::kUncommon,
        "an unknown type is called uncommon");
  Check(!DownloadManager::Assess("model.q4_k_m", "application/octet-stream")
             .requires_confirmation,
        "and uncommon alone does not block the download");

  // We never claim a file is safe — only that we have no local warning.
  for (const char* name : {"report.pdf", "setup.exe", "photo.jpg"}) {
    const std::string reason = DownloadManager::Assess(name, "").reason;
    Check(reason.find("verified") == std::string::npos &&
              reason.find("is safe") == std::string::npos,
          std::string("no safety guarantee is made for ") + name);
  }

  DownloadManager manager;

  // Resumable transfer: pause and resume behave as advertised.
  const int big = manager.Start("https://example.org/iso", "linux.iso", "/tmp",
                                "application/octet-stream", false);
  Check(manager.OnResponse(big, Resumable(3'000'000'000LL)), "headers arrive");
  Check(manager.Get(big)->can_resume, "ranges plus a validator means resumable");
  manager.OnProgress(big, 1'000'000);
  Check(manager.Pause(big), "pause works");
  Check(manager.Get(big)->state == State::kPaused, "and is recorded");
  Check(manager.Resume(big), "resume works");
  Check(manager.Get(big)->received_bytes == 1'000'000,
        "resuming keeps the bytes already fetched");

  // Non-resumable transfer: pause is refused with a reason, not faked.
  const int stream = manager.Start("https://example.org/stream", "video.mp4",
                                   "/tmp", "video/mp4", false);
  manager.OnResponse(stream, NotResumable());
  manager.OnProgress(stream, 500);
  Check(!manager.Pause(stream), "pause is refused when the server cannot resume");
  Check(manager.last_error().find("restart") != std::string::npos,
        "and the refusal explains what would happen");
  Check(manager.Get(stream)->state == State::kActive,
        "the download keeps running instead of being silently broken");

  // Interruption and retry.
  manager.OnInterrupted(stream, "connection reset");
  Check(!manager.Resume(stream), "a non-resumable interrupted download cannot resume");
  Check(manager.Retry(stream, false), "it can be retried");
  Check(manager.Get(stream)->received_bytes == 0,
        "and the progress bar restarts honestly at zero");

  const int dead = manager.Start("https://example.org/gone", "a.zip", "/tmp",
                                 "application/zip", false);
  manager.OnResponse(dead, NotResumable());
  for (int i = 0; i < DownloadManager::kMaxAutomaticRetries; ++i) {
    manager.OnInterrupted(dead, "timeout");
    Check(manager.Retry(dead, true), "automatic retry within the budget");
  }
  manager.OnInterrupted(dead, "timeout");
  Check(!manager.Retry(dead, true), "automatic retries stop at the cap");
  Check(manager.Retry(dead, false), "but the user can still retry by hand");

  // Reveal in folder only once there is a file to reveal.
  Check(!manager.CanRevealInFolder(big), "cannot reveal a partial file");
  manager.OnFinished(big);
  Check(manager.CanRevealInFolder(big), "can reveal a finished file");
  Check(manager.RevealPath(big) == "/tmp/linux.iso", "with the real path");

  // Dangerous file: kept only after an explicit confirmation.
  const int installer = manager.Start("https://example.org/s", "setup.exe",
                                      "/tmp", "application/octet-stream", false);
  Check(manager.NeedsConfirmation(installer), "the executable waits for a yes");
  Check(manager.Confirm(installer), "the user can say yes");
  Check(!manager.NeedsConfirmation(installer), "and is not asked again");

  // Private-window downloads leave no history record.
  const int secret = manager.Start("https://example.org/x", "notes.pdf", "/tmp",
                                   "application/pdf", true);
  manager.OnResponse(secret, Resumable(10));
  manager.OnFinished(secret);
  bool found = false;
  for (const auto& item : manager.History())
    found |= item.id == secret;
  Check(!found, "a private-window download is not in the history list");
  Check(manager.Get(secret) != nullptr,
        "though it is still manageable while its window is open");

  Check(manager.RemoveFromHistory(big), "a single entry can be deleted");
  manager.ClearHistory();
  Check(manager.History().empty(), "and the whole list can be cleared");

  if (failures == 0)
    std::cout << "download_manager_test: all assertions passed\n";
  return failures == 0 ? 0 : 1;
}
