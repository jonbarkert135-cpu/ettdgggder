# Agents

This repository keeps a maintained project memory so you do not have to rebuild
context by reading the tree.

**Start here: [`.ai/MEMORY.md`](.ai/MEMORY.md), then
[`.ai/memory/STATE.md`](.ai/memory/STATE.md).** Two files, ~2.5k tokens, and you
know what Bedrock is, what may never break, where the code lives and what
landed last. Use [`.ai/memory/MAP.md`](.ai/memory/MAP.md) to pick a file instead
of grepping.

**Before you finish a change:** update the memory in the same PR — the procedure
is [`.ai/memory/PROTOCOL.md`](.ai/memory/PROTOCOL.md), and
`scripts/check_memory.py` enforces it in CI (run by `./scripts/run_host_tests.sh`).

Quick loop: `./scripts/run_host_tests.sh` (tests + fuzz smoke),
`python3 scripts/check_memory.py` (memory), and the gates listed in
[`.ai/memory/INVARIANTS.md`](.ai/memory/INVARIANTS.md).

The same applies to human contributors — see [CONTRIBUTING.md](CONTRIBUTING.md).
