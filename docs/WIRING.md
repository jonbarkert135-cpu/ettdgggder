# What of Bedrock actually runs

The overlay is host-tested to a level most browser forks never reach, and that is
exactly why this page exists: a passing test proves the logic is right, not that
anything calls it. This is the honest split, measured from the linked binaries of
a local build with `scripts/report_wiring.py` — a class counts as *reached* only
when its own name is a `::` component of a symbol in `chrome` or
`libservices_network_network_service.so`.

Numbers move with every wiring PR; regenerate rather than edit by hand.

**Build 6, 2026-08-27:** modules: 33  live: 5  dormant: 28

## Reached from the running browser

| Module | Lines | Classes the binaries reference |
| --- | --- | --- |
| `integration` | 775 | NetworkRequest, OutgoingHeaderRequest, PrefAssignment, StartupPlan, UnenforcedDefault |
| `privacy/core` | 1708 | ProtectionController |
| `privacy/network` | 1845 | OutgoingRequest, RequestHeaderPolicy |
| `privacy/tracker_blocker` | 2155 | BlockingPipeline, CnameUncloaker, CosmeticFilter, FilterEngine, MatchResult, NetworkFilter, Request, TrackerHeuristic, UncloakResult |
| `settings` | 2216 | DefaultSetting |

The call sites are `patches/bedrock/integration/*`: the startup plan (prefs),
the network blocking hook and the outgoing-header floor. Nothing else in the
overlay has a call site yet.

## Compiled, not yet called

| Module | Lines | Classes |
| --- | --- | --- |
| `bookmarks` | 433 | 3 |
| `crypto` | 514 | 1 |
| `devtools` | 256 | 3 |
| `diagnostics` | 703 | 6 |
| `downloads` | 464 | 4 |
| `errors` | 197 | 3 |
| `extensions` | 510 | 3 |
| `extensions/catalog` | 669 | 8 |
| `fuzz` | 611 | 0 |
| `history` | 353 | 4 |
| `omnibox` | 208 | 1 |
| `onboarding` | 573 | 4 |
| `passwords` | 666 | 6 |
| `perf` | 163 | 2 |
| `platform` | 455 | 4 |
| `privacy/fingerprinting` | 568 | 2 |
| `privacy/security` | 250 | 3 |
| `privacy/stats` | 264 | 3 |
| `privacy/storage` | 294 | 2 |
| `profiles` | 516 | 5 |
| `search` | 120 | 1 |
| `session` | 262 | 3 |
| `settings/knowledge` | 884 | 6 |
| `themes` | 745 | 4 |
| `ui` | 1551 | 15 |
| `ui/l10n` | 525 | 3 |
| `updater` | 883 | 12 |
| `workspaces` | 334 | 5 |

Being on this list is not a defect — the roadmap builds the logic first and wires
it once it can be proven in a running browser — but it *is* the honest answer to
"does the browser do this yet?": no, not until a patch calls it and
`build/ENFORCEMENT.md` records the measurement.

Wiring order that follows from the phases: storage partitioning and the cookie
policy (phase 9) unlock `privacy/storage` and the `kPartition` verdict the
blocking pipeline already returns; the Blink hooks (phase 8) unlock
`privacy/fingerprinting`; the WebUI host (phase 5) unlocks `ui`, `themes`,
`settings/knowledge` and `ui/l10n`; the profile layer unlocks `bookmarks`,
`history`, `passwords`, `session` and `workspaces`.

