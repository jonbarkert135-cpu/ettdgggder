# Patch management

**Roadmap item 67.** Every difference between Bedrock and upstream Chromium must be isolated,
documented, reviewable and reproducible. In practice that means: a stranger can list what this
project changes in the engine, and for each change find out why, who decided it, and what happens
if it is dropped.

The alternative — a fork carrying a thousand undocumented edits — is not a codebase, it is a
sediment. Nobody can tell an intentional privacy change from a merge accident from 2024, so nobody
dares delete anything, so the patch set only grows and the rolls only get harder.

## Two ways to change the engine, in order of preference

1. **`src_overrides/`** — new files, symlinked into the Chromium tree at build time. New files do
   not conflict on a roll. Almost all Bedrock code lives here for that reason alone.
2. **`patches/`** — real diffs against upstream files, applied by `build/sync.py` in sorted path
   order. Only when an existing upstream file has to change.

If a change can be expressed as a new file plus a one-line hook, that is cheaper for the next ten
rolls than a clever twenty-line diff.

## Layout

```
patches/
  bedrock/<area>/NNNN-short-name.patch    authored here, MPL-2.0
  upstream/<project>/NNNN-short-name.patch  adopted from another project, original licence
```

`NNNN` orders application within a directory. `<area>` matches the subsystem it belongs to
(`privacy`, `network`, `ui`, `security`, `build`), so `ls` answers "what do we change about the
network stack".

## Required header

Every patch file begins with a block that `scripts/check_upstream.py` parses and enforces:

```
# Bedrock-Patch: 0001-strip-tracking-params
# Area: privacy
# Upstream-Paths: components/url_formatter/url_formatter.cc
# Reason: item 12 — query parameter stripping needs a hook upstream does not expose
# Owner: privacy
# Chromium-Version: 151.0.7922.173
# Upstream-Bug: none
# Drop-When: upstream lands a public API for this (crbug reference if one exists)
```

- **Reason** cites the roadmap item or the threat it answers. "Needed for the UI" is not a reason.
- **Chromium-Version** records the tree the patch was last verified against, so a stale patch is
  visible rather than assumed fine.
- **Drop-When** is the field people skip and the one that keeps the set small: every patch states
  the condition under which it should be deleted. A patch with no exit condition is a permanent
  maintenance cost, and it should have to justify itself in review.
- Patches under `patches/upstream/<project>/` additionally record upstream project, path, revision
  and licence, and the project must have a `port` or `patched-base` row in
  [THIRD_PARTY.md](THIRD_PARTY.md) — enforced by `scripts/check_provenance.py`.

## Reviewable and reproducible

- One patch, one purpose. A diff that fixes two things is split, because one of them will need to
  be reverted alone during a roll.
- No reformatting inside a patch. A whitespace-only hunk turns a reviewable change into a diff
  nobody reads, and it conflicts on every roll for free.
- The patch set applies cleanly to the pinned tree in `build/chromium.pin`, and
  `scripts/upstream_sync.py --dry-run` proves it before a roll starts.
- `git log patches/` is the history of the fork's relationship with upstream, so patch changes go
  in their own commits with the reason in the message — not folded into a feature commit.

## Auditing the set

```
python3 scripts/upstream_sync.py --check-patches   # headers, fields, stale versions
python3 scripts/check_provenance.py                # licence rows for adopted patches
```

There are currently **no patch files** in the tree: everything so far is `src_overrides/` and
documentation. The rules and the gate exist before the first patch on purpose — a patch discipline
introduced after the fiftieth patch is archaeology, not policy.
