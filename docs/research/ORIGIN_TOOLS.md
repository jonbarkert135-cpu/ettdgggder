# "Origin Tools" — searched for, not found

**Roadmap item 54.** The brief named a project called *Origin Tools* and asked, correctly, not
to guess: find the exact project, check its repository, licence, activity and purpose — and if
it does not exist, continue without it.

## Result

**No open-source privacy project called "Origin Tools" was found.** Nothing matching that name
exists as a browser, blocker, privacy library or extension with a repository we could pin, so
there is no licence to record, no activity to assess and no code to consider. Bedrock proceeds
without it, and no dependency was invented to fill the gap.

Searched on **2026-08-21** (web search over public repositories and documentation). What came
back, and why each is not it:

| Candidate | What it actually is | Why it is not "Origin Tools" |
| --- | --- | --- |
| `pgasawa/origin` | LLM browser extension that groups tabs into workspaces | Unrelated purpose; not a privacy tool; recommends URLs from other users |
| `NDevTK/OriginMarker` (MIT) | bookmark-bar marker showing the current origin, anti-phishing aid | Real and small, but a phishing-indicator extension, not a privacy toolkit |
| Orion browsers (`oneKn8/orion-browser`, `Orion-Intelligence/Orion-Browser`) | AI-agent Chromium fork; separately, an Android onion-routing browser | Name similarity only |
| OP Inspector / Originator Profile | content-attestation verification extension | Different problem domain (provenance of content, not user privacy) |

## The two things the name probably meant

Both are real, and both are worth an answer, so the item does not end in a shrug:

1. **uBlock Origin's tooling** — the logger, the dynamic-filtering pane, "my rules", the
   element picker. If that was the intent, it is covered by
   [`UBLOCK_ORIGIN.md`](UBLOCK_ORIGIN.md) (item 52), including the point that a blocking
   decision the user cannot interrogate is one they cannot trust.
2. **Chromium Origin Trials** — a real subsystem in our own base: token-gated experimental Web
   APIs that a *site* can switch on for its visitors. That is a privacy-relevant surface we
   have not yet ruled on, and it deserves a decision rather than a default:
   - an origin trial can expose an API that our fingerprinting policy has not seen, which means
     the surface list in `docs/privacy/fingerprinting/` can be bypassed by a site opting in;
   - trial tokens are per-origin and visible in requests;
   - Chromium's default is to accept valid tokens.
   **Recommendation, recorded for a future roadmap item:** treat origin trials as a
   fingerprinting surface — no trial may enable an API whose exposure the fingerprint policy
   restricts at the active level, and the set of accepted trials should be visible in the
   Privacy Center. This is not implemented and is not claimed.

## Rule this leaves behind

If a brief names a dependency that cannot be found, the answer is this document: what was
searched, when, what came back, and what was decided — never a plausible-looking library with
an invented URL. A fabricated dependency row would also fail `scripts/check_provenance.py`,
which is the mechanical half of the same rule.
