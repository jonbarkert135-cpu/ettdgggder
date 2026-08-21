# Dependency policy

**Roadmap item 77.** Every third-party dependency must be identified, version-pinned, licensed,
reviewed and justified. Five properties, all checked by `scripts/check_provenance.py` against the
inventory in [`THIRD_PARTY.md`](THIRD_PARTY.md).

The reason this is a policy rather than a habit: in a browser, a dependency is not a convenience,
it is code running with the user's privileges next to their passwords and session cookies. It also
never arrives alone — it arrives with its own dependencies, its own release cadence, its own
maintainer's account, and its own supply-chain risk ([`SUPPLY_CHAIN.md`](SUPPLY_CHAIN.md)).

## The five properties

| Property | What it means | Where |
| --- | --- | --- |
| identified | exact project, exact upstream repository — not "a JSON library" | inventory row |
| version-pinned | an exact version or commit; `main`, `latest` and empty are refused | inventory row |
| licensed | licence recorded, notice file present, compatibility decided | row + `THIRD_PARTY_NOTICES/` |
| reviewed | a human checked version, licence and continued need, with a date | `Reviewed` column, max 365 days old |
| justified | what breaks without it, in one sentence | `Justification` column |

## What is not a justification

The gate rejects these words in the justification column, because each one has been the reason
behind a dependency that later had to be removed the hard way:

- "it makes the UI nicer / prettier"
- "it is popular" / "everyone uses it"
- "it is modern" / "industry standard"
- "it is convenient" / "it saves time" / "it is easier"

None of them says what the browser cannot do without the dependency. If the answer is "nothing",
the dependency does not belong in a browser — write the twenty lines instead. A twenty-line
function is read once and understood; a dependency is a permanent relationship with someone else's
release process.

## Before adding one

1. **Can the standard library or Chromium do it?** Chromium already contains a JSON parser, a URL
   parser, string utilities, a task scheduler, crypto primitives and an HTTP stack. A second copy
   of any of these is a second attack surface with different bugs.
2. **What is the smallest thing that solves the problem?** Usually a function, sometimes a file,
   rarely a library.
3. **What happens if the project is abandoned?** Answer before adding, not after.
4. **Does it pull transitive dependencies?** Count them; they are part of the decision. A "small"
   library with forty transitive dependencies is forty relationships.
5. **Does the licence allow what Bedrock needs?** GPL-family code cannot be linked into the browser
   (`LICENSING.md`) — it can only ever be a separate artifact.
6. **Is it maintained?** Last release, open security issues, how CVEs are handled.

Then, and only then: inventory row with all eight columns, notice file, and — if code actually
enters the tree — a digest in `build/dependency-hashes.txt`.

## Review cadence

A row must be re-reviewed within twelve months or CI fails. A review means:

- the pinned version still exists upstream and has not been retagged;
- the licence has not changed (they do change, and the change is rarely announced);
- the justification still holds — the feature that needed it may be gone;
- the project is still maintained.

The date, not a checkbox, goes in the `Reviewed` column. An inventory nobody re-reads is a list of
what the project used to depend on.

## Removing a dependency

Removal is a normal, expected outcome of a review — the inventory is not a ratchet. Delete the row,
delete the notice file, delete the hash, and say so in the release notes' dependencies section
([`RELEASES.md`](RELEASES.md)). Anything else leaves a notice for code that no longer ships, which
is its own kind of lie about what the binary contains.
