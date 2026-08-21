# Releases

**Roadmap item 71.** Three channels, one version scheme, and six things every release states.
Enforced by [`src_overrides/bedrock/updater/release_channels.h`](../src_overrides/bedrock/updater/release_channels.h)
and `scripts/check_releases.py`, so the document and the tooling cannot drift apart.

## Channels

| Channel | Cadence | Version suffix | Changes may land directly | Soak before promotion | Audience |
| --- | --- | --- | --- | --- | --- |
| nightly | 1 day | `-nightly.<date>` | yes | 7 days | developers, testers; expected to break |
| beta | 7 days | `-beta.<n>` | no | 14 days | users who accept regressions and report them |
| stable | 28 days | none | no | — | everyone else |

Stable's 28 days track upstream Chromium's cycle, not a feature calendar. A security roll publishes
sooner and ignores the cadence entirely — the deadlines in [`UPSTREAM_SYNC.md`](UPSTREAM_SYNC.md)
outrank everything here (item 69).

Promotion is one step at a time: nightly → beta → stable. Skipping a channel is refused
(`IsPromotionAllowed`), because the soak is the only thing the funnel actually provides.

## Version numbers

```
<bedrock-major>.<bedrock-minor>.<bedrock-patch>.<build>[-channel.<n>]
1.2.0.4412            stable
1.2.0.4413-beta.2     beta
1.3.0.4498-nightly.20260821
```

- The **Bedrock version is Bedrock's own** and never mirrors Chromium's. Copying Chromium's version
  string would tell the user the two are the same thing, and it makes "which Chromium is this?"
  unanswerable the moment a security roll changes the base under a patch release.
- The Chromium base is published as a separate, mandatory field (below) and must equal
  [`build/chromium.pin`](../build/chromium.pin) — `release_channels.cc` refuses a release where
  they disagree.
- Pre-release builds carry the channel **in the version string**, not only on the download page:
  the version is what ends up in a bug report.
- The build number is monotonic across all channels, so any two builds can be ordered. The updater
  refuses downgrades (item 40) and needs a total order to do it.

## What every release states

All six fields, on every channel, including nightly:

| Field | Content |
| --- | --- |
| version number | the full version string above |
| Chromium base version | version + commit from `build/chromium.pin` |
| security fixes | upstream CVEs picked up by this build, plus Bedrock-side fixes; "none" is a valid answer when it is true |
| privacy changes | anything that changes what leaves the machine, what is stored, or what a site can observe — including defaults |
| dependencies | third-party components added, removed or version-changed since the previous release on this channel |
| known issues | what is broken, with the workaround if there is one |

`MissingNoteFields()` names the absent field rather than counting, and the release is not
publishable without all six. **Known issues is the field that gets dropped first and matters most**:
a note without it reads as a build with no known problems, which is never true, and after a user
hits one, nothing else in the document is believed.

Template: [`releases/TEMPLATE.md`](releases/TEMPLATE.md). Published notes live in `docs/releases/`
as `<version>.md`.

## Publishing checklist

A build is publishable when `Evaluate()` returns `kPublishable`. It checks, in the order a human
should fix them:

1. built against the pinned Chromium;
2. no open blocking issues (beta and stable — a nightly exists to expose them);
3. the pipeline from `UPSTREAM_SYNC.md` completed (beta and stable);
4. the soak period elapsed (beta and stable);
5. the artifact is signed and its provenance published — on **every** channel, including nightly.
   An unsigned nightly is not a faster nightly, it is an unverifiable one
   ([`SUPPLY_CHAIN.md`](SUPPLY_CHAIN.md));
6. all six note fields present.

## Support window

- **stable**: the current release. Security fixes land in a new stable release, not as backports to
  an old one — a two-person project maintaining a security branch is a promise it cannot keep, and
  a stale branch that looks supported is worse than no branch.
- **beta**: the current beta only.
- **nightly**: no support window; yesterday's nightly is not maintained.

## What does not exist yet

No release has been published. There is no download page, no signing key, no artifact and no
`docs/releases/` entry other than the template. This section stays here, and CI keeps it accurate,
until the first stable build actually ships.
