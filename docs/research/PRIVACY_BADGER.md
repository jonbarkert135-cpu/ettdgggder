# Privacy Badger research

**Roadmap item 53.** EFF's Privacy Badger is the source for behavioral tracker detection, GPC,
DNT and link cleaning. It is **GPL-3.0-or-later**, so this is an ideas-only relationship, and
the ideas are published in EFF's own write-ups — which is exactly why they are usable.

**Sourcing, honestly.** Compiled from EFF's public documentation of Privacy Badger's heuristic,
its GPC/DNT work and its FAQ — not from a code read. With a GPL-3.0 project whose behaviour we
reimplement under MPL-2.0, "we did not read the code" is not modesty, it is the compliance
position, and every blocking file in this tree carries a header saying so.

## Licence position

- **Privacy Badger's code, filter data and "yellow list" are off-limits**: no vendoring, no
  linking, no porting, no translation. Recorded in [`docs/LICENSING.md` §3](../LICENSING.md).
- **The heuristic is a published algorithm.** Reimplementing a described algorithm from public
  documentation is allowed and is what `privacy/tracker_blocker/tracker_heuristic.{h,cc}` does
  (item 14), with a header marking it "inspired by, not derived from".
- **Provenance tracking is the obligation**, and it is mechanical here: the inventory row, the
  notice file, and the per-file header. `scripts/check_provenance.py` fails the build if the
  row or notice goes missing.

## Mechanism by mechanism

| Privacy Badger mechanism | What it is | Verdict for Bedrock | Notes |
| --- | --- | --- | --- |
| **Tracker learning** | a third-party origin seen storing identifying state across ≥3 unrelated first parties is classified as a tracker — learned locally, per user, not from a list | **Reimplemented** (item 14). The value is that it catches trackers no list knows; the cost is that it is a *classifier*, and classifiers are wrong sometimes. | ours, independently written |
| **Local-only learning** | the learned table never leaves the machine | **Adopted and enforced**: no upload path exists, and the zero-telemetry gate would fail if one appeared. | invariant 1 + `check_no_telemetry.py` |
| **Three-state classification** (allow / cookie-block / block) | a middle state where the request is allowed but its cookies are stripped, because blocking outright breaks the page | **Adopt fully.** Our `Value` ladder already has `kAllow / kReduce / kBlock / kBlockStrict`; the *cookie-block* middle state is exactly `kReduce` for a learned tracker and should be the default outcome of learning, not `kBlock`. This is the single most valuable detail in Privacy Badger's design and the easiest to lose. | already expressible |
| **Heuristic thresholds** (the "three sites" rule) | the number of distinct first parties before a domain is flagged | **Adopted as a number we own.** Three is EFF's tuned value; ours must be tunable, documented and testable, not folded into the code as a magic constant. | test asserts the boundary |
| **Yellow list** (allow-with-cookie-blocking for domains that break when blocked) | a curated compatibility list | **Refused as data** (GPL-3.0, and it is EFF's curation). We need the *mechanism* — an override that keeps a learned tracker in the `kReduce` state — with our own entries, or better, derived from user-visible breakage signals. | licence trap |
| **Learning disabled in private/incognito windows** | learning from a private session would leak that session into the persistent profile | **Adopt, and it is close to mandatory for us**: our private and Tor windows are ephemeral by contract (items 19–20). A learned row surviving a private window would be a real leak, not a nuance. Test-worthy. | must-have |
| **Global Privacy Control (GPC)** | a machine-readable "do not sell/share" signal with legal weight in some jurisdictions | **Already implemented** (item 14) and correctly framed: GPC is a *legal* signal, not a technical protection. Sending it must never relax an actual block. | keep the framing honest |
| **DNT + the DNT policy allowlist** | the older signal, plus EFF's compliance-policy mechanism that unblocks domains promising to honour it | **Send DNT: yes. Adopt the policy allowlist: no.** Unblocking a tracker because it hosts a JSON file claiming good behaviour is trust without verification, and it needs a fetch to a third party to check. That trade is wrong for us. | deliberate divergence, documented |
| **Link cleaning / unshimming** | strips tracking wrappers and redirect shims from outbound links (`l.facebook.com/l.php?u=…`) | **Already implemented** (item 14) and it overlaps Brave's query stripping and debouncing (item 50) — these must be **one navigation-cleaning stage**, not three features that each rewrite URLs. | consolidation item |
| **Widget replacement** (click-to-activate social buttons) | replaces tracking widgets with a placeholder the user can click to load | **Adopt the idea, write our own replacements.** Same shape as uBO's `$redirect` resources and Brave's stand-ins; three sources, one mechanism, our own catalogue. | consolidation item |
| **User overrides per domain** | the user can force any learned classification | **Present** (Protection Controller, item 11), and the learned table must show *why* something was flagged — a classifier the user cannot interrogate is a classifier they cannot correct. | needs the "why" surface |

## What this changes now

Nothing in the code. Recorded follow-ups:

1. **Cookie-blocking as the default learned outcome** (`kReduce`), with `kBlock` reserved for
   repeat offenders or explicit user choice.
2. **No learning in private/Tor windows** — write the test first; this is a leak, not a policy.
3. **One navigation-cleaning stage** absorbing link cleaning, query stripping and debouncing
   (items 50 and 53 arrive at the same place).
4. **A "why was this flagged" view** for the learned table, alongside the "why was this blocked"
   answer that dynamic filtering needs (item 52).

## What we will not take

- Privacy Badger's code, lists or yellow list.
- The DNT compliance-policy allowlist, and any other "unblock on a promise" mechanism.
- The word "anonymous", here as everywhere: a heuristic classifier reduces tracking; it does
  not make a user untrackable, and it will both miss trackers and flag innocents.
