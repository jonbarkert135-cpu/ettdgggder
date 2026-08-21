# 038 — Per-site privacy panel

**Roadmap item 38.** Status: landed and host-tested
(`src_overrides/bedrock/ui/site_privacy_panel.{h,cc}`).

Address-bar button → **Privacy Shield** →

```
example.com

Connection            HTTPS
Trackers              12 blocked
Ads                   27 blocked
Fingerprinting        Protected (standard)
Third-party cookies   Blocked
Scripts               Allowed
Site storage          Partitioned
```

## No fake counters

Each row comes from exactly one of two sources:

- a **count** from the privacy event log — actions the engine actually performed on this page, or
- a **state** from the protection controller — the policy in force for this host, including
  per-site overrides (item 11 precedence: site → domain → global → default).

There is no third category: no estimates, no "typical" numbers, and no counting a request twice
because two subsystems both wanted credit for it (item 13 already guarantees one decision point).

**"Not measured" is a real value.** A page that has not been observed shows `Not measured`, never
`0 blocked`. Zero claims we looked and found nothing; that is a different statement, and users
make trust decisions on the difference. Once the page has produced any privacy event, the counters
turn into real numbers — including honest zeros.

The connection row states facts about the load: `HTTPS`, `HTTPS (upgraded)` (an upgrade is
disclosed, not hidden), `Certificate problem` (never softened into a green tick — item 16),
`HTTPS with blocked mixed content`, or `Not secure (HTTP)`.

A test scans every rendered row for the banned absolutes (*anonymous*, *untraceable*, *100%*,
*invisible*, …). The panel describes protection; it never promises anonymity.
