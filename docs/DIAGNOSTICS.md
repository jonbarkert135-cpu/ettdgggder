# Diagnostics: debug logs and crash reports

**Roadmap items 79 and 81.** Gate: `scripts/check_diagnostics.py`.
Code: [`src_overrides/bedrock/diagnostics/`](../src_overrides/bedrock/diagnostics).

Bedrock collects no telemetry (item 39, [`PRIVACY.md`](PRIVACY.md)). It still has to be
debuggable, because "we cannot diagnose your problem" is its own kind of user harm. These are
the two local mechanisms that make that possible, and the rules that keep them from becoming
the thing item 39 removed.

## DEBUG LOG ≠ TELEMETRY

| | Debug log | Telemetry |
| --- | --- | --- |
| Who turns it on | the user, for a specific problem | the vendor, at install |
| Where it goes | a file in the profile | a server |
| Who reads it | the user, and whoever they send it to | the vendor, always |
| Default | **off by default** | on |
| How it stops | it was never running | a setting, if you find it |

Both can contain the same sentences. What differs is consent and direction — which is the whole
of the distinction, and why Bedrock has one and not the other.

## Debug log

* **Off by default.** Verbosity starts at `Level::kOff`; nothing is recorded until the user
  raises it in Settings → Advanced or with `--debug-log-level=`. It returns to off on restart
  unless explicitly persisted.
* **Two sinks, both local:** a bounded in-memory ring (2000 lines, oldest dropped first) and a
  file *inside the profile directory*. A path outside the profile is refused rather than
  redirected — a log the user cannot find is a log the user cannot delete.
* **Never uploaded.** `DebugLog::kUploadSupported` is `false` in the header, the `Sink` enum has
  no network member, and the gate fails the build if the diagnostics tree gains a network
  symbol. Sending a log means the user attaching the file to a bug report themselves.
* **Scrubbed on the way in.** Lines pass the [scrubber](#the-scrubber) *before* they are stored,
  so a careless `Log("navigating to " + url)` stores `<url>`. Nothing can later export what was
  never written.
* **Export states what was removed.** "Save diagnostics" writes a header with the line count and
  the number of redacted values, so the user knows what they are handing over.

## Crash reports

* **Local first.** A crash writes a report into the profile, listed in a UI the user can read
  line by line. Reports older than 30 days are deleted on startup; the user can delete any or
  all of them at any time.
* **Upload is off by default and there is no build where it is not.** `UploadConsent::kNever` is
  the first enum value and the initial value. Enterprise policy can lock it there; no policy can
  loosen it ([`CONFIGURATION.md`](CONFIGURATION.md)).
* **Consent is per report.** Even with `kAskEachTime`, `MayUpload()` returns false unless the
  user opened *that* report and confirmed it. Agreeing to "send crash reports" is not agreeing
  to send a specific file nobody has looked at.
* **Whitelist, not blacklist.** A report may carry only these fields:

  | Field | What it is |
  | --- | --- |
  | `build_id` | which Bedrock build crashed |
  | `channel` | nightly / beta / stable |
  | `chromium_base` | the engine version underneath |
  | `os` | e.g. "Linux 6.8" — never a machine name |
  | `cpu_arch` | x86_64, arm64 |
  | `gpu_enabled` | whether hardware acceleration was on |
  | `module` | browser / renderer / gpu / utility |
  | `signal` | what killed the process |
  | `uptime_seconds` | how long it had been running |
  | `tab_count` | how many tabs — never which |
  | `locale` | the UI language |

  Anything else is dropped and counted, including keys nobody has thought of yet. Values of
  whitelisted keys are scrubbed too, so `os` cannot smuggle a path.
* **Never collected, at any consent level:** page URLs, referrers, page titles, cookies,
  passwords, form and POST data, session tokens, clipboard, history, bookmarks, the profile
  path, search queries. The test asserts each name is refused one by one.
* **Stack frames are kept.** Bedrock source paths and line numbers survive scrubbing — a report
  without frames is not a report.

## The scrubber

One implementation shared by the debug log, the error catalog and crash reports, because a
redaction rule present in two of the three is the one that leaks.

| Input | Stored |
| --- | --- |
| `https://mail.example.com/inbox?id=7` | `<url>` |
| `anna@example.org` | `<email>` |
| `Cookie: sid=abc; theme=dark` | `<header>: <redacted>` |
| `password=hunter2` | `<secret>=<redacted>` |
| `/home/anna/Downloads/f.txt` | `<home>/Downloads/f.txt` |
| `C:\Users\Anna\Desktop` | `<home>\Desktop` |
| `203.0.113.7` | `<ip>` |
| a 40+ character opaque token | `<blob>` |

Deliberately kept: `chrome://` and `bedrock://` pages, loopback addresses (`127.0.0.1`,
`localhost`), and source paths in stack frames. Placeholders are visible rather than empty so a
reader can tell "a URL was here" from "nothing was here" — invisible redaction is how people end
up turning redaction off.

## What is not here yet

Crashpad integration. Catching a real segfault needs the Chromium build; what exists today is
the policy layer that decides what a report may contain and whether it may ever move, tested on
its own. When the build lands, the handler feeds `BuildReport()` and the whitelist applies to
real minidump metadata — a minidump itself is memory, so it stays local and is never a candidate
for upload.

Related: [`PRIVACY.md`](PRIVACY.md) (what connects out — nothing),
[`ERRORS.md`](ERRORS.md) (what the user sees when something fails),
[`THREAT_MODEL.md`](THREAT_MODEL.md) (who reads a stolen profile directory).
