# How a part of Bedrock gets built

**Roadmap items 87 and 88.** Companion: [`PHASES.md`](PHASES.md) for the order of the work.

Item 87 asks for research-first development. Item 88 asks, in the same breath, for that research
not to become the project. Both are right, and the tension between them is the useful part: the
question is never "research or build", it is "how much understanding is enough to start".

## The three principles that outrank the rest (item 104)

When two rules in this repository conflict, these decide:

1. **Do not build the browser that looks private. Build the browser that can prove what
   protections it is actually applying.** The consequence is unglamorous and everywhere: a feature
   carries `Status::kDesigned`, `kImplemented` or `kEnforced`, only a build may promote it to the
   last one (`build/ENFORCEMENT.md`), the shield counts events that happened rather than events a
   feature would cause, and `check_no_fake_features.py` fails the build over the difference.
   A protection nobody can verify is a claim, and this project treats a claim as a defect.
2. **Do not blindly merge open-source projects.** Study them, respect their licences, isolate the
   *concept*, and integrate only what is technically and legally appropriate. The mechanism is
   [`LICENSING.md`](LICENSING.md)'s reuse ladder plus [`PROVENANCE.md`](PROVENANCE.md): a project
   may only be marked `port` or `vendored` when a specific file in this tree is recorded with its
   version, commit, licence and modification status. Today the honest answer is one data snapshot
   and no borrowed code at all.
3. **Privacy is local-by-default, transparent-by-design, configurable-by-user and verifiable by
   tests.** Local: [`privacy/REMOTE.md`](privacy/REMOTE.md) lists every possible outbound
   interaction and `check_remote_features.py` rejects an undeclared one. Transparent: every
   protection states what it *cannot* do ([`privacy/FEATURES.md`](privacy/FEATURES.md)).
   Configurable: one setting is expressible in GUI, config file, policy and CLI
   ([`CONFIGURATION.md`](CONFIGURATION.md)). Verifiable: a protection with no test is not a
   protection, which is why the host suite runs on every commit.

Item 103 adds the delivery rule: a milestone ends with a product that builds and runs, not with a
list of TODOs. Where a feature is not finished, it is marked incomplete in the documentation
(item 90) instead of being described as working — and the phase table in
[`PHASES.md`](PHASES.md) says `policy` eleven times for exactly that reason.

## The ten steps

For each **major** part — a subsystem, not a bug fix:

1. **Study upstream.** What does Chromium already do here? Most of the time the answer changes
   the design, and occasionally it makes the work unnecessary.
2. **Find the Chromium APIs.** Name them. A design that does not name the seam it attaches to is
   a wish.
3. **Study comparable implementations.** Brave, Firefox, Tor Browser, uBlock Origin, Privacy
   Badger — what they did, and more importantly what went wrong for them.
4. **Check the licence** before reading closely enough to be influenced. GPL code is studied for
   *behaviour*, never copied (ADR 0014).
5. **Write the design note** in `docs/design/`. One document, the decision and the rejected
   options. If it is an architectural decision, an ADR instead.
6. **Name the risks.** Security, compatibility, performance, maintenance. Scored where item 85
   applies.
7. **Implement.**
8. **Write the tests** — host tests next to the code, and a row in `tests/matrix.json` if it is
   observable in a running browser.
9. **Security review**: threat-model delta, fuzz target for any new parser of untrusted input,
   sanitizer coverage.
10. **Document** — the user-facing document, the memory update, and the gate that keeps them true.

Steps 1–6 before step 7. Not because process is virtuous, but because the alternative is
discovering at step 7 that Chromium already partitions the thing you just partitioned.

## Where the timebox is

Item 88's rule, made concrete:

* Research produces **a design note and a list of risks**, not a survey. If a study is longer than
  the design it produced, it was for the author, not the project.
* A comparison stops when it changes a decision. Reading a fifth blocker after four agreed on the
  same architecture buys nothing.
* Anything that cannot be settled by reading is settled by **building the smallest thing that
  answers it**. The filter index redesign came from a benchmark, not from an essay: 70 µs against
  a 20 µs budget, fixed to 0.21 µs by changing the index key.
* When the same question comes back twice, it becomes an ADR and stops being a question.

The research in `docs/research/` follows that shape: each study ends with "what we take, what we
leave, what we may not use", and the licence conclusion. That last section is the deliverable;
the rest is working notes.

## What counts as done

A part is finished when all five exist: the code, the host test, the document, the gate that
fails when code and document disagree, and the memory update in the same change. Four out of five
is how a project acquires documentation that lies.

## Where it is deliberately lighter

Bug fixes, filter-list updates, translations and refactors inside one module skip steps 1–6. The
ten steps are for new subsystems; applying them to everything is exactly the paralysis item 88
warns about.
