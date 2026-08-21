# 016 — HTTPS

**Roadmap item 16.** Status: landed and host-tested
(`src_overrides/bedrock/privacy/network/https_policy.{h,cc}`).

## Two rules

1. **A certificate problem is never hidden and never global.** There is no "ignore certificate
   errors" switch — not in settings, not behind a flag, not for enterprise. Exceptions are per
   host, for the exact error that was shown.
2. **Upgrading is silent, downgrading is loud.** Rewriting `http://` to `https://` needs no
   interruption. Falling back to plaintext always does.

## Modes

| Mode | Plaintext navigation |
|---|---|
| Upgrade (default) | rewritten to HTTPS; falls back with a warning |
| HTTPS-Only | interstitial before any plaintext load |

The per-site shields setting can make a site **stricter** than the profile, and a site cannot
opt itself out of a profile-wide HTTPS-Only — tested in both directions.

Local and `.onion` hosts (`localhost`, RFC1918, `.local`, `.onion`) load over HTTP without a
warning. They have no public certificate by design, and warning there would teach the user that
the warning means nothing — which is what actually gets people phished.

## Mixed content

| Subresource | Action |
|---|---|
| active (script, iframe, XHR, WebSocket) | **blocked, always** — no exception can enable it |
| passive (image, media, font) | upgraded; blocked if the upgrade fails; allowed only by an explicit per-host exception |

Nothing to weigh for active content: an attacker who can rewrite one script owns the page.

## Certificate errors

| Error | Click-through? |
|---|---|
| expired, not-yet-valid, unknown authority, wrong host | yes — common on honest but misconfigured servers |
| **revoked, pinned-key mismatch, weak signature** | **no button at all** |

The second group does not happen by accident. Revocation means the key is known to be
compromised; a pin mismatch is what interception looks like. `AddCertException()` refuses to
store an exception for them, so no UI can accidentally offer one.

An exception covers the **exact error** it was granted for: a host excepted for an expired
certificate still warns if the authority changes. Every error has a plain-language explanation
that says what is wrong and what it might mean, and a test asserts none is a stub.
