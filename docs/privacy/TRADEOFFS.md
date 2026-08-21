# Privacy versus usability: the scoring table

**Roadmap item 85.** Generated from the same table as
[`FEATURES.md`](FEATURES.md) by `scripts/check_transparency.py --write`.

Never optimise privacy blindly. Before a protection is switched on by default it is
scored on five axes — 0 none, 1 low, 2 medium, 3 high — and the score lives in the
source next to the feature, not in a review thread that scrolls away.

*Default-able* means the gain is at least medium and does not cost more than it buys;
a compatibility loss of 3 is never a default, whatever it protects. The column is
computed, not typed: `IsDefaultable()` in the same file, tested by
`feature_disclosure_test.cc`.

| Feature | Privacy | Security | Compat. loss | Perf. cost | Complexity | Default-able |
| --- | --- | --- | --- | --- | --- | --- |
| `tracker_protection` | 3 | 1 | 1 | 0 | 2 | yes |
| `ad_blocking` | 2 | 2 | 1 | 0 | 1 | yes |
| `cosmetic_filtering` | 1 | 0 | 2 | 1 | 1 | **no — opt-in** |
| `cross_site_tracking_protection` | 3 | 1 | 2 | 1 | 2 | yes |
| `referrer_control` | 2 | 1 | 1 | 0 | 1 | yes |
| `query_param_stripping` | 2 | 0 | 2 | 0 | 1 | yes |
| `https_only` | 2 | 3 | 2 | 0 | 1 | yes |
| `secure_dns` | 2 | 2 | 2 | 1 | 2 | **no — opt-in** |
| `third_party_requests` | 3 | 2 | 3 | 0 | 1 | **no — opt-in** |
| `cookie_isolation` | 3 | 1 | 2 | 0 | 2 | yes |
| `storage_partitioning` | 3 | 1 | 1 | 1 | 3 | yes |
| `ephemeral_third_party_storage` | 2 | 0 | 2 | 1 | 2 | **no — opt-in** |
| `canvas_protection` | 3 | 0 | 2 | 1 | 3 | yes |
| `webgl_controls` | 2 | 0 | 2 | 1 | 2 | **no — opt-in** |
| `font_exposure` | 2 | 0 | 2 | 0 | 2 | yes |
| `client_hints` | 2 | 0 | 1 | 0 | 2 | yes |
| `language_normalization` | 2 | 0 | 2 | 0 | 1 | yes |
| `timezone_normalization` | 1 | 0 | 2 | 0 | 1 | **no — opt-in** |
| `screen_metrics` | 2 | 0 | 2 | 0 | 2 | yes |
| `hardware_info` | 2 | 0 | 1 | 1 | 1 | yes |
| `battery_api` | 1 | 0 | 0 | 0 | 0 | yes |
| `webrtc_policy` | 3 | 2 | 2 | 0 | 2 | yes |
| `timer_coarsening` | 2 | 3 | 2 | 1 | 2 | yes |
| `media_devices` | 2 | 1 | 2 | 0 | 1 | yes |
| `gamepad_and_sensors` | 1 | 1 | 2 | 0 | 1 | **no — opt-in** |
| `clipboard_permission` | 2 | 3 | 1 | 0 | 1 | yes |
| `geolocation_permission` | 2 | 1 | 1 | 0 | 1 | yes |
| `notification_permission` | 1 | 1 | 1 | 0 | 1 | **no — opt-in** |
| `autoplay_control` | 1 | 0 | 1 | 0 | 1 | **no — opt-in** |
| `permission_isolation` | 2 | 2 | 2 | 0 | 2 | yes |

## Shipped on against the score

Where the arithmetic says opt-in and the feature is on anyway, the argument is
recorded in the source and reproduced here. The host test fails if one is missing.

* **`cosmetic_filtering`** — On by default despite scoring as opt-in: blocking a request without hiding the hole it leaves produces a page the user reads as broken, and they blame the browser rather than the ad. The privacy gain really is low; the cost of shipping it off is that blocking looks broken.
* **`webgl_controls`** — On by default although the cost edges past the gain: the GPU string is the second most distinguishing value a page can read, and the sites that branch on it degrade rather than fail.
* **`gamepad_and_sensors`** — On by default although it scores as opt-in: sensor calibration is a durable device identifier, and the cost is one extra click in the small number of pages that use a gamepad or a motion sensor at all.
* **`notification_permission`** — On by default despite scoring as opt-in: the benefit is not to the one site being gated but to every later prompt, and that benefit only exists if gating is on for everyone. The cost is one click on the rare site that genuinely wants notifications.
* **`autoplay_control`** — On by default although it scores as opt-in: autoplay with sound is a presence signal sent to a media host before the user has decided to watch anything, and the cost is a single click to start playback.

## Where a score of 3 is explained

* **`tracker_protection`** — The filter engine is the most complex privacy component (ADR 0002); it earns that with the largest single privacy gain.
* **`cross_site_tracking_protection`** — The privacy gain is the highest available: breaking the cross-site join is the single change trackers cannot work around client-side.
* **`https_only`** — The largest security gain in the list: it removes the whole class of on-path attacks.
* **`third_party_requests`** — Highest compatibility loss in the table, which is exactly why the standard default is off and only Strict enables it.
* **`cookie_isolation`** — Cookie isolation is the protection trackers most actively work around, so its privacy gain is scored at the top of the scale.
* **`storage_partitioning`** — Partitioning touches every storage backend in the engine; the complexity is real and is why it is one subsystem, not many.
* **`canvas_protection`** — Deterministic per-site derivation is subtle: get it wrong in either direction and you either break sites or create a new identifier.
* **`webrtc_policy`** — A local-IP leak identifies the machine itself, not the session, which is why the privacy gain is scored at the top.
* **`timer_coarsening`** — The security gain is the reason this is on by default: it is a mitigation for a family of side-channel attacks, not only tracking.
* **`clipboard_permission`** — A silent clipboard read is a credential-theft primitive, not only a privacy problem.

## How to read a 'no'

A `no` is not a rejection of the feature. It means the protection ships available but
off, reachable from the privacy axis (item 83) by a user who has decided the breakage
is worth it. Blocking every third-party request is the clearest case: the privacy gain
is the highest in the table, and so is the compatibility loss.
