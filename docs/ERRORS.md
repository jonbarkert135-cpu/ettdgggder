# Errors: what the user sees when something fails

**Roadmap item 80.** Gate: `scripts/check_diagnostics.py`.
Code: [`src_overrides/bedrock/errors/error_catalog.cc`](../src_overrides/bedrock/errors/error_catalog.cc).

An error message is the only part of a browser a user reads word for word. It is also the part
written last, in English, by the person who caused the failure. Item 80 gives four properties;
each one rules out a specific habit.

## Meaningful

The message says what failed, in the user's terms, in a whole sentence. Not `ERR_SSL_PROTOCOL_ERROR`,
which tells the user nothing and a developer almost nothing. The stable code is still there —
`BR-SEC-002` — as something to quote in a bug report, but it is never the message.

## Actionable

Every error carries a next step, and the step is a different string from the description, so it
cannot be dropped while "improving the wording". If the user genuinely cannot fix it, the step
says the honest thing and offers what is available: go back, retry, or copy the code. An error
without an exit is a dead end the user has to guess their way out of.

## Localized

Both strings live in the string catalog under ids, like every other user-visible string
([`LOCALIZATION.md`](LOCALIZATION.md)). The gate checks each error's title and action exist in
all four ship locales; error paths are exactly where hardcoded English survives longest, because
they are the paths nobody demos.

## Security-conscious

The internal detail — a certificate chain, a file path, an exception string, the URL that failed
— is *not* what the user is shown. It goes to the local debug log, scrubbed. The API makes the
split hard to get wrong: `Present()` returns `user_text` and `log_detail` as separate fields, and
a surface renders only the first. A single `ShowError(message)` call is how internal detail ends
up in screenshots on support forums.

Two classifications:

| Sensitivity | Meaning | Count today |
| --- | --- | --- |
| `kDiagnosticOnly` | detail never reaches the UI — it can carry a URL, path or chain | 5 |
| `kShowOnRequest` | detail is about the user's *own* input, safe behind a "Details" disclosure | 1 |

The one showable case is an invalid configuration file, where the line number *is* the message.
Even there the detail is scrubbed first: a bad `proxy = https://internal.example/...` line shows
the line number, not the URL. The test asserts at most one class is showable, so a future drift
towards "just show them the exception" is a review conversation rather than a quiet change.

## The catalog today

| Code | Situation | Sensitivity |
| --- | --- | --- |
| `BR-NET-001` | the network could not be reached | diagnostic only |
| `BR-SEC-002` | a certificate could not be verified | diagnostic only |
| `BR-PRF-003` | the profile is open in another window | diagnostic only |
| `BR-DL-004` | a download was refused | diagnostic only |
| `BR-EXT-005` | an extension was blocked | diagnostic only |
| `BR-CFG-006` | the configuration file could not be read | show on request |

Codes are namespaced `BR-<AREA>-<NNN>`, unique, and never reused after removal — a code in a
three-year-old forum post should not come to mean something else.

## Adding an error

1. Add the `ErrorCode` and its entry in `error_catalog.cc` with a new `BR-` code.
2. Add two message ids (title and action) to `string_catalog.h`, then the text for **all four**
   locales in `string_catalog.cc`. Write the English first; translate from English, never from
   another translation.
3. Classify the sensitivity. Default to `kDiagnosticOnly`; the burden is on showing detail.
4. Run `python3 scripts/check_diagnostics.py` and the host tests.

Related: [`DIAGNOSTICS.md`](DIAGNOSTICS.md) (where the detail goes),
[`LOCALIZATION.md`](LOCALIZATION.md), [`ACCESSIBILITY.md`](ACCESSIBILITY.md) (an error must also
be announced, not only drawn).
