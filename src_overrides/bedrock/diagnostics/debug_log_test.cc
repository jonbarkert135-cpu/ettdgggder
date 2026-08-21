// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.
// Covers the scrubber (items 79/80/81) and the debug log (item 79).

#include "bedrock/diagnostics/debug_log.h"

#include <iostream>
#include <string>

#include "bedrock/diagnostics/scrubber.h"

namespace {

using bedrock::diagnostics::DebugLog;
using bedrock::diagnostics::Level;
using bedrock::diagnostics::ScrubResult;
using bedrock::diagnostics::Scrubber;

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

void ScrubberRemovesEveryCategory() {
  const ScrubResult url = Scrubber::Scrub("navigating to https://mail.example.com/inbox?id=7");
  Check(Has(url.text, "<url>"), "url replaced");
  Check(!Has(url.text, "mail.example.com"), "host does not survive scrubbing");

  const ScrubResult header = Scrubber::Scrub("Cookie: sid=abc; theme=dark");
  Check(Has(header.text, "<redacted>"), "cookie header redacted");
  Check(!Has(header.text, "sid=abc"), "cookie value does not survive");

  const ScrubResult secret = Scrubber::Scrub("posting password=hunter2 to form");
  Check(!Has(secret.text, "hunter2"), "password value does not survive");

  const ScrubResult mail = Scrubber::Scrub("signed in as anna@example.org");
  Check(Has(mail.text, "<email>"), "email replaced");

  const ScrubResult home = Scrubber::Scrub("writing /home/anna/Downloads/file.txt");
  Check(Has(home.text, "<home>/Downloads/file.txt"), "home directory replaced, rest kept");
  Check(!Has(home.text, "anna"), "username does not survive");

  const ScrubResult win = Scrubber::Scrub("C:\\Users\\Anna\\Desktop\\x.log");
  Check(!Has(win.text, "Anna"), "windows username does not survive");

  const ScrubResult ip = Scrubber::Scrub("connected to 203.0.113.7");
  Check(Has(ip.text, "<ip>"), "public address replaced");

  const ScrubResult token = Scrubber::Scrub(
      "bearer AAAABBBBCCCCDDDDEEEEFFFFGGGGHHHHIIIIJJJJKKKK");
  Check(Has(token.text, "<blob>"), "long opaque value replaced");
}

void ScrubberKeepsWhatDiagnosisNeeds() {
  const ScrubResult internal = Scrubber::Scrub("crash on bedrock://settings/privacy");
  Check(Has(internal.text, "bedrock://settings/privacy"), "internal page kept");
  Check(internal.redactions == 0, "internal page is not a redaction");

  const ScrubResult loopback = Scrubber::Scrub("test server http://127.0.0.1:8931/page.html");
  Check(Has(loopback.text, "127.0.0.1:8931"), "loopback kept for local debugging");

  const ScrubResult frame = Scrubber::Scrub(
      "#3 bedrock::privacy::FilterEngine::Match(src_overrides/bedrock/privacy/"
      "tracker_blocker/filter_engine.cc:212)");
  Check(Has(frame.text, "filter_engine.cc:212"), "source location kept in frames");
}

void DefaultsAreOff() {
  DebugLog log;
  Check(log.level() == Level::kOff, "logging is off by default");
  Check(!log.Log(Level::kError, "net", "boom"), "nothing recorded while off");
  Check(log.Entries().empty(), "no entries while off");
  Check(!log.file_sink_enabled(), "file sink off by default");
  Check(DebugLog::kUploadSupported == false, "there is no upload path");
}

void LevelFiltersAndScrubsOnTheWayIn() {
  DebugLog log;
  log.SetLevel(Level::kWarning);
  Check(log.Log(Level::kError, "net", "failed"), "error passes at warning level");
  Check(!log.Log(Level::kInfo, "net", "chatter"), "info filtered at warning level");

  log.SetLevel(Level::kVerbose);
  Check(log.Log(Level::kInfo, "net", "GET https://example.com/secret?token=abc"),
        "info recorded at verbose level");
  const auto& entry = log.Entries().back();
  Check(!Has(entry.message, "example.com"), "url scrubbed before storage");
  Check(entry.redactions > 0, "redaction counted on the entry");
}

void RingBufferIsBounded() {
  DebugLog log(3);
  log.SetLevel(Level::kVerbose);
  for (int i = 0; i < 10; ++i) {
    log.Log(Level::kInfo, "loop", "line " + std::to_string(i), i);
  }
  Check(log.Entries().size() == 3, "ring buffer holds its capacity");
  Check(log.Entries().front().message == "line 7", "oldest lines dropped first");
}

void FileSinkStaysInsideTheProfile() {
  DebugLog log;
  Check(!log.EnableFileSink("/home/u/.config/bedrock/Default",
                            "/tmp/bedrock.log"),
        "sink outside the profile refused");
  Check(!log.file_sink_enabled(), "refusal changes nothing");
  Check(!log.EnableFileSink("/home/u/.config/bedrock/Default",
                            "/home/u/.config/bedrock/Default/../../x.log"),
        "path climbing out of the profile refused");
  Check(log.EnableFileSink("/home/u/.config/bedrock/Default",
                           "/home/u/.config/bedrock/Default/debug.log"),
        "sink inside the profile accepted");
  log.DisableFileSink();
  Check(!log.file_sink_enabled() && log.file_sink_path().empty(),
        "disabling clears the path");
}

void ExportStatesWhatWasRemoved() {
  DebugLog log;
  log.SetLevel(Level::kVerbose);
  log.Log(Level::kWarning, "net", "blocked https://tracker.example/pixel", 1787000000);
  const std::string bundle = log.ExportBundle();
  Check(Has(bundle, "redacted values: 1"), "export counts redactions");
  Check(Has(bundle, "stays on your computer"), "export states where the file goes");
  Check(!Has(bundle, "tracker.example"), "export cannot leak what was never stored");
}

}  // namespace

int main() {
  ScrubberRemovesEveryCategory();
  ScrubberKeepsWhatDiagnosisNeeds();
  DefaultsAreOff();
  LevelFiltersAndScrubsOnTheWayIn();
  RingBufferIsBounded();
  FileSinkStaysInsideTheProfile();
  ExportStatesWhatWasRemoved();
  if (failures == 0) {
    std::cout << "debug_log: ok\n";
  }
  return failures == 0 ? 0 : 1;
}
