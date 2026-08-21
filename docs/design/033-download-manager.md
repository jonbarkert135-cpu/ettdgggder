# 033 — Download manager

**Roadmap item 33.** Status: landed and host-tested
(`src_overrides/bedrock/downloads/download_manager.{h,cc}`).

Pause · resume · cancel · retry · reveal in folder · history · file-type warnings · dangerous
download protection.

## Pause that means pause

The common bug: the UI offers Pause, the server does not support range requests, and Resume
silently restarts a 3 GB file from zero. `can_resume` is derived from the response — `Accept-Ranges`
**and** a validator (ETag / Last-Modified) to prove the bytes still belong to the same file. Pause
on a non-resumable transfer is **refused with a reason** instead of accepted and betrayed later:

> this server cannot continue an interrupted download, so pausing would restart it from the
> beginning

A retry of a non-resumable download resets `received_bytes` to 0, so the progress bar tells the
truth about what is happening. Automatic retries stop at `kMaxAutomaticRetries = 3`; the user can
always retry by hand.

## Safety without a server

Bedrock has no backend, so there is no cloud reputation service and no "verified safe" badge.
What is decidable locally:

| Risk | Meaning | Blocks? |
| --- | --- | --- |
| `kSafe` | ordinary document/image/archive | no |
| `kUncommon` | rare extension — information, not an accusation | no |
| `kExecutable` | runs code if opened (.exe .msi .dmg .sh .ps1 .apk …) | needs confirmation |
| `kDeceptive` | `invoice.pdf.exe`, U+202E right-to-left override, promised PDF served as a program | needs confirmation |

The test asserts no assessment reason ever contains *verified* or *is safe*: absence of a local
warning is not a clean bill of health. Nothing is ever auto-opened.

## Smaller decisions

- **Reveal in folder** is refused until the download completes; opening a file manager on a part
  file is a bug report.
- **Private-window downloads leave no history record.** The file stays on disk — the user asked
  for it — the record does not, because they did not.
- History supports deleting one entry or clearing everything.
