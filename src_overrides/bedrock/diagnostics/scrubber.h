// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_DIAGNOSTICS_SCRUBBER_H_
#define BEDROCK_DIAGNOSTICS_SCRUBBER_H_

#include <string>
#include <vector>

// The scrubber (roadmap items 79, 80 and 81).
//
// Debug logs, error details and crash reports are three places where the
// browser writes down what it was doing at the moment something went wrong —
// which is exactly the moment when what it was doing is a URL the user visited,
// a cookie header, or the contents of a form. The three features share one
// scrubber rather than each rolling its own, because a redaction rule that
// exists in two of the three is the one that leaks.
//
// The rule is *deny by construction*: text is scrubbed on the way **in**, when
// it is recorded, not on the way out when it is shown or exported. A log line
// that was never stored in full cannot be exported in full by a later bug, and
// cannot be read out of a core dump by whoever gets hold of the machine.
//
// What is replaced, and why the placeholder is not empty: the reader has to be
// able to tell "there was a URL here" from "there was nothing here", otherwise
// a scrubbed log is unreadable and people turn scrubbing off.
//
//   http://site/path?q=1     -> <url>
//   user@example.com         -> <email>
//   Cookie: a=b; c=d         -> Cookie: <redacted>
//   Authorization: Bearer x  -> Authorization: <redacted>
//   password=hunter2         -> password=<redacted>
//   /home/anna/Downloads     -> <home>/Downloads
//   C:\Users\Anna\Desktop    -> <home>\Desktop
//   203.0.113.7              -> <ip>          (127.0.0.1 and ::1 are kept)
//   40+ chars of hex/base64  -> <blob>        (tokens, keys, session ids)
//
// Deliberately *not* scrubbed: loopback addresses, `chrome://` and `bedrock://`
// internal pages, and the Bedrock source paths in a stack frame. A crash report
// with no frames is not a crash report, and a diagnostician who cannot see that
// the failure happened on `bedrock://settings` has nothing to work with.

namespace bedrock {
namespace diagnostics {

struct ScrubResult {
  std::string text;
  int redactions = 0;
  // Which categories fired, in the order they are listed above. Used by the
  // tests and by the crash UI, which tells the user what was removed before
  // they decide whether to attach the file to a bug report.
  std::vector<std::string> categories;
};

class Scrubber {
 public:
  // Scrubs a single line or a whole document; the rules are line-independent.
  static ScrubResult Scrub(const std::string& text);

  // True when `text` still contains something that looks like user data. The
  // gate and the tests use it to assert that nothing reaches a sink unscrubbed;
  // it is the same detector Scrub() uses, so the two cannot drift.
  static bool LooksSensitive(const std::string& text);
};

}  // namespace diagnostics
}  // namespace bedrock

#endif  // BEDROCK_DIAGNOSTICS_SCRUBBER_H_
