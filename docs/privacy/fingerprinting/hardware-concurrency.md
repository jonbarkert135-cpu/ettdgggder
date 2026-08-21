# navigator.hardwareConcurrency

**Surface id:** `hardware-concurrency` · **Levels:** 0 allow · 1–3 normalize

## Attack vector
Core count splits the population into a handful of buckets and is stable forever — cheap, reliable entropy that costs an attacker one property read.

## Mitigation
Reported as `min(actual, 8)` at level 1, `min(actual, 4)` at level 2 and a fixed `2` at level 3. We never report **more** cores than exist: over-reporting makes worker pools oversubscribe and actually degrades the page.

## Compatibility impact
Sites sizing worker pools create fewer workers. Level 3 halves parallelism for heavy apps (video editors, WASM builds) — flagged.

## Performance impact
Level 3 can reduce throughput in genuinely parallel workloads; that is the trade the user chose.

## Test cases
- 16-core machine reports 8 at level 1.
- 2-core machine still reports 2 (never inflated).
- Level 0 reports the truth.
