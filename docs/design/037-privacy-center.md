# 037 — Privacy Center

**Roadmap item 37.** Status: landed and host-tested
(`src_overrides/bedrock/stats/{privacy_event_log,privacy_center}.{h,cc}`).

```
PRIVACY CENTER

Trackers blocked       12,481
Ads blocked             7,294
Fingerprint attempts    1,182
HTTPS upgrades             213
Cookies partitioned      5,912

Protection Level:
BALANCED
```

## One log, three screens

The DevTools panels (036), this dashboard and the Shield popup (038) all read the same
`PrivacyEventLog`. If each counted for itself they would disagree within a week, and a dashboard
that contradicts the popup is worse than no dashboard.

Two rules keep the numbers honest:

1. **Only performed actions are recorded.** The subsystem that carried out the action reports it —
   the blocking pipeline for a blocked request, the fingerprint policy for a blocked probe, the
   HTTPS policy for an upgrade. No estimator, no extrapolation, no "a typical page has ~30
   trackers" filler.
2. **Nothing leaves the machine.** The log lives in memory and the profile. This class has no
   upload path and no consumer that takes one. Item 37 asks for a dashboard, not telemetry. The
   only export is a JSON file the user asks for, and the dashboard says so underneath:
   *"These counts come from this browser and stay on this device. Bedrock has no server to send
   them to."*

## Details worth naming

- **Private-window events are counted for the live popup but never persisted and never added to
  the lifetime totals.** The popup has to work in a private window; the lifetime total must not
  remember it.
- **Clearing is real**: `ClearSite()` also subtracts that site's contribution from the totals, so
  the dashboard cannot keep a number whose evidence is gone.
- The detailed event list is a 5,000-entry ring buffer; totals are integers and are unaffected
  when old events fall off the end.
- **The protection level is derived from the current settings, never stored.** A stored label
  drifts: the user changes one control and the badge still claims BALANCED. Settings that match no
  preset produce **CUSTOM**, which is a real answer, not a failure. BALANCED is the shipped default
  from item 11.
