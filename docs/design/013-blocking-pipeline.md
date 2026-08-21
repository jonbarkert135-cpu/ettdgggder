# 013 — One blocking pipeline

**Roadmap item 13.** Status: landed and host-tested
(`src_overrides/bedrock/privacy/tracker_blocker/blocking_pipeline.{h,cc}`).

## The rule

Exactly one component decides whether a request happens: `BlockingPipeline::Evaluate()`.
Filter lists, the behavioral heuristic, the shields settings, the script policy and the cookie
policy are **stages** of that function, not blockers that each get a veto.

Why this is a hard rule and not a preference: four independent blockers give four sources of
truth. The shields panel cannot say why something was blocked, an allow rule in one system is
silently overruled by another, and every new protection multiplies the interactions someone has
to reason about. One pipeline means one answer, one `Reason`, one place to fix a broken site.

## Stages

```
request
  → 0. shields          (protection off for this site? → allow, stop)
  → 1. filter lists     (block / redirect / @@ allow, stop)
  → 2. party analysis   (first party → allow, subject to script policy)
  → 3. script policy    (per-site scripts=reduce/block)
  → 4. heuristic        (locally learned tracker → block / partition)
  → 5. cookie policy    (unknown third party → partition)
  → allow
```

This is roadmap item 14's diagram with one change: the party check runs **before** the
heuristic, because the heuristic only ever applies to third parties and must never judge the
site the user chose to visit.

Two consequences the tests pin down:

- **Shields down means down.** If the user turned protection off for a site, no later stage
  re-blocks anything — including a domain the heuristic learned to hate.
- **The learned verdict is not a second blocker.** It is read at stage 4 and gated by the same
  per-site tracker setting as the lists.

## Everything integrates through it

- The **Protection Controller** (011) supplies the settings each stage reads; it never blocks
  anything itself.
- The **fingerprinting shims** (010) report state-storing behaviour *into* the pipeline via
  `NoteStoredState(kHighEntropyFingerprint)` instead of maintaining their own blocklist.
- The **heuristic** (014) learns only through `NoteStoredState()`, which is called by the
  pipeline — so learning can never disagree with what the pipeline actually allowed.
- Every decision carries a `Reason` with a human-readable string, and a test asserts every enum
  value has one. A decision the panel cannot explain is a bug.

## Result

```cpp
struct Decision {
  Action action;             // Allow | Partition | Redirect | Block
  Reason reason;             // for the panel and the local log
  std::string detail;        // the deciding rule or domain
  std::string redirect_url;  // inert resource, when redirecting
};
```

`Partition` matters: the honest default for an unknown third party is not "block" (breaks sites)
and not "allow" (tracks the user), but "load with no access to cross-site state".
