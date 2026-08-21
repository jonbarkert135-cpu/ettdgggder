# Memory protocol

How to restore context cheaply, and how to leave the memory correct for whoever
comes next. This protocol is enforced by `scripts/check_memory.py` in CI.

## Restore (do this, in this order, and stop when you have enough)

| Step | Read | Cost | Gives you |
| --- | --- | --- | --- |
| 1 | [`../MEMORY.md`](../MEMORY.md) | ~1.5k tokens | What the project is, the non-negotiables, the layout, the working agreement |
| 2 | [`STATE.md`](STATE.md) | ~0.8k | Where the work stands, what is real vs. documented, open threads |
| 3 | [`MAP.md`](MAP.md) *(only when touching code)* | ~2k | Which file to open — instead of grepping the tree |
| 4 | [`INVARIANTS.md`](INVARIANTS.md) *(before changing behaviour)* | ~1k | What must stay true and which gate checks it |
| 5 | [`DECISIONS.md`](DECISIONS.md) *(before re-arguing a design)* | ~1k | Why it is like this |
| 6 | [`HISTORY.md`](HISTORY.md) *(only when the past matters)* | ~1k | What already happened, newest first |

Steps 1–2 are the full restore for a conversation. **Do not** read `git log`,
walk `docs/`, or open source files to "get oriented" — that is the expensive
path this memory exists to replace. Read a source file when the map has told you
which one, and read the design doc only when you are about to change its subject.

## Update (same PR as the change — not later)

1. **`STATE.md`** — rewrite the parts that are no longer now: roadmap position,
   real-vs-documented, findings, open threads. It is a snapshot, not a log.
2. **`HISTORY.md`** — one new entry on top: what landed, plus anything that cost
   effort to discover.
3. **`MAP.md`** — never by hand: `python3 scripts/gen_memory.py`.
4. **`modules.json`** — a new directory under `src_overrides/bedrock/` needs its
   one-line summary here, or CI fails.
5. **`INVARIANTS.md`** — a new rule or a new gate goes in the table.
6. **`DECISIONS.md`** — only when a real choice was made between alternatives.
7. Verify: `python3 scripts/check_memory.py`.

Keep it small. Memory that grows without limit stops being cheaper than the
source it summarises: `MEMORY.md` ≤ ~250 lines, `STATE.md` ≤ ~80,
`HISTORY.md` entries ≤ ~8 lines each. Prune superseded lines rather than
appending to them — a wrong line in memory is worse than a missing one.

## Optional: local code index for deep queries

For questions the map cannot answer — call chains, impact analysis, dead code —
index the repository with an MCP code-intelligence server rather than reading
files in bulk. [`codebase-memory-mcp`](https://github.com/DeusData/codebase-memory-mcp)
(MIT, fully local, tree-sitter based) is the one this project was set up
against: install it, say "index this project", then query the graph. It is a
**query accelerator, not the memory** — it is rebuilt from the code and knows
nothing about intent, decisions or history, which is exactly what the files
above carry. Nothing in this repository depends on it being installed, and
nothing it produces is committed (`.ai/index/` and its caches stay untracked).
