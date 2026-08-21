# Upstream sync

**Roadmap items 66, 68 and 69.** How Bedrock stays on a current Chromium instead of becoming a
fork with a known-vulnerable engine.

The failure mode this document exists to prevent is not dramatic. Nobody decides to fall behind.
A roll is skipped because a patch conflicts, then the next roll has two conflicts, and a year later
the project is shipping an engine with hundreds of public, unfixed bugs while its website talks
about privacy. **A browser that is behind on Chromium security updates is not a privacy browser**,
whatever its feature list says.

## The pipeline

```
Chromium upstream
        ↓
Security review        what did upstream fix, does any of it touch our patches
        ↓
Privacy patches        re-apply, then verify behaviour, not just that it applied
        ↓
Browser patches        the rest of the overlay
        ↓
Automated tests        host tests, gates, browser tests, privacy regression suite
        ↓
Release candidate      evaluated by updater/release_policy.{h,cc}
```

Every stage is recorded per release candidate, and the decision at the end is made by code
(`bedrock::update::Evaluate`), not by whoever is in the room. The stage names in that header and
the ones above are kept in sync by `scripts/check_upstream.py`.

## Cadence

| Trigger | Response |
| --- | --- |
| Chromium stable refresh | roll the pin, run the pipeline, release |
| Chromium security release | roll immediately; deadlines below |
| Chromium milestone (new major) | roll on a branch first — milestones move code our patches touch |
| Nothing from upstream for two weeks | still roll: a large gap is what makes conflicts unmanageable |

Small, boring, frequent rolls. A roll that is three months of upstream at once is a research
project; a roll that is one week of upstream is an afternoon.

## Security deadlines (item 69)

Measured from the moment the fix is **public upstream**, because that is when attackers have it —
not when we noticed, not when the branch is convenient. Encoded in `DeadlineHours()`:

| Severity | Ship within |
| --- | --- |
| Critical | 72 hours |
| High | 7 days |
| Medium | 14 days |
| Low | 30 days |

**The security update always wins.** When a feature is not ready and a deadline is running, the
feature leaves the branch (`Action::kDropFeatures`) — the release never waits for it. Reverting a
feature costs an afternoon; an unpatched renderer costs users. There is no severity at which "we
will take it in the next release, this one has a nice feature in it" is an acceptable answer.

Two things a deadline never buys:

- **Skipping the security review when a fix touches code we patch.** That is not a rebuild; a
  human re-reads the patch against the new upstream code, or the release is blocked. Re-applying a
  patch cleanly onto changed code is exactly how a fork silently re-opens the hole upstream just
  closed.
- **Skipping the privacy regression tests.** A Chromium roll is the most common way a privacy
  behaviour stops working, because upstream re-enables things by default and our patch no longer
  covers the new path.

`Action::kEmergencyRelease` exists for the case where the deadline has passed and the full pipeline
cannot finish: security content only, no feature commits, a written justification, and still the
security review and the privacy regression suite. Everything else can wait; those two are what
separates an emergency release from an untested binary.

## Tooling (item 68)

`scripts/upstream_sync.py` — offline, no network unless asked:

| Command | Does |
| --- | --- |
| `--status` | reads `build/chromium.pin`, prints the pinned version, pin age and whether a roll is due |
| `--check-patches` | parses every patch header, verifies the required fields and reports which patches touch paths a roll is likely to move |
| `--dry-run --workspace DIR` | `git apply --check` for every patch against a real Chromium tree: conflict detection before anything is modified |
| `--plan` | prints the pipeline stages with the commands for each, in order |
| `--selftest` | runs the parser and the ordering logic against fixtures |

Build verification and the privacy regression suite are `scripts/run_host_tests.sh` plus the
Chromium build from [BUILD.md](BUILD.md); the sync tool prints them rather than reimplementing them.

## When a patch conflicts

1. Read what upstream changed. Not the conflict markers — the upstream commit.
2. Ask whether the patch is still needed. Upstream sometimes lands the thing we were patching
   around, and the laziest correct resolution is deleting our patch (record it in the patch log).
3. If it is still needed, rewrite it against the new code and re-verify the *behaviour*, with a
   test where one is possible.
4. If it cannot be resolved before the security deadline, the patch is dropped for this release and
   the feature it supports is announced as temporarily unavailable. **The roll is not delayed.**

That last line is the whole policy. Everything above is detail.
