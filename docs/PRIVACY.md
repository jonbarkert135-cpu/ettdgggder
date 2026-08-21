# Privacy

**Roadmap item 72.** What Bedrock does with data, in the terms a user actually cares about. This is
not a privacy policy for a service, because there is no service: the project operates no server of
any kind.

## The short version

- **Nothing is collected.** No analytics, no usage counts, no crash reports by default, no
  "anonymous" statistics. Five of the six telemetry categories cannot be switched on at all — the
  code refuses (`privacy/core/telemetry_policy`), and `check_no_telemetry.py` scans the sources on
  every commit for a histogram macro or an analytics host that slipped in.
- **No account, no sync, no vendor backend.** Nothing to sign into, nothing that leaves the device
  because of us.
- **Your data stays on your machine**: history, bookmarks, passwords, profiles, sessions, settings,
  filter lists. Passwords use the platform keystore.
- **The browser still talks to the internet you asked for** — the sites you open, the search engine
  you chose, your DNS resolver. Bedrock never inserts its own infrastructure between you and them.

## What connects out, and when

Honesty here matters more than the list being short. A browser that claims "no connections" is
lying; this is every category of outbound traffic Bedrock can produce:

| Connection | When | Who it goes to | Can it be turned off |
| --- | --- | --- | --- |
| The page you opened | you navigate | the site and its resources (minus what the blocker stops) | that is the browser |
| Search | you search | the engine you selected, per context (normal / private / Tor) | choose another engine |
| DNS | every navigation | the resolver you configured, or the system one | configurable, including DoH; strict mode fails closed |
| Filter list updates | on the schedule you set, if you enabled a list | the list's own host | lists are off by default until their licences are verified ([`privacy/FILTER_LISTS.md`](privacy/FILTER_LISTS.md)) |
| Update check | if enabled and configured | the provider you chose — no hostname is compiled in (item 40) | yes, including fully manual |
| Crash report | only after explicit opt-in to the current disclosure, and only if you configured a service | that service; the project runs none | off by default, permanently off unless configured |

There is no fifth thing. If a future change adds one, it belongs in this table in the same commit,
and in the release notes' privacy section ([`RELEASES.md`](RELEASES.md)).

## What is stored locally

| Data | Where | Removed by |
| --- | --- | --- |
| history, bookmarks, downloads | profile directory | per-item delete, which also removes derived data; "clear all local data" |
| passwords | profile directory, encrypted with the platform keystore | password manager; profile deletion |
| cookies and site storage | partitioned by `StorageKey` — third-party state is separated, not merely deletable | per-site "forget", clear-data, private windows |
| filter rules, settings, themes | profile directory, plain files you can read and export | reset controls, export/import ([`FORMATS.md`](FORMATS.md)) |
| privacy event log | memory and profile, feeds the panels; counts real events only (item 55) | clear-data |

Private windows keep nothing after the last one closes, and — deliberately — the tracker-blocking
heuristics learn nothing there either.

## What we will not do

- No "anonymous" telemetry, no A/B experiments, no variations service, no engagement metrics.
- No claim that Bedrock makes you anonymous. It reduces what sites can observe and correlate;
  anonymity is a different, much stronger property that no browser alone can provide
  ([`THREAT_MODEL.md`](THREAT_MODEL.md) lists what is out of scope).
- No counter or badge for an event the engine does not actually emit (item 55, gated by
  `check_no_fake_features.py`).
- No default that is more private on paper than in use: a protection that breaks the web is turned
  off by users, and a protection nobody keeps enabled protects nobody.

## Verifying rather than believing

Every statement above is checkable in this repository: the telemetry gate, the fake-feature gate,
the honest-status field on each privacy feature (`kDesigned` / `kImplemented` / `kEnforced`), and
the fingerprinting notes under [`privacy/fingerprinting/`](privacy/fingerprinting/README.md) that
say what each mitigation costs. When a claim here cannot yet be verified by a real build, the
status field says so.
