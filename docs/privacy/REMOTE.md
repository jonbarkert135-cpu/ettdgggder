# Everything that leaves your machine

**Roadmap items 94 and 95.** Generated from
[`src_overrides/bedrock/privacy/network/remote_features.cc`](../../src_overrides/bedrock/privacy/network/remote_features.cc)
by `scripts/check_remote_features.py --write`; the gate fails if this file drifts
from the table, and fails if any module gains networking code without a row.

Two claims this page exists to make checkable:

1. **Bedrock operates no servers.** No configuration service, no personalisation, no
   cloud profile, no fingerprint database, no rules service, no assistant, no sync.
   Every row below contacts either the site you asked for or a third party you chose.
2. **The browser is complete with all of them off.** Nothing below is required for
   Bedrock to start, browse, block, isolate or protect you.

`policy` in the status column means the interaction is *permitted by the design and
not built*: this overlay contains no network stack code today. Saying otherwise would
be the kind of claim item 90 bans.

<!-- BEGIN REMOTE FEATURES -->
| Feature | Contacts | Operator | Default | Status | How to turn it off | Replaceable with |
| --- | --- | --- | --- | --- | --- | --- |
| `search_query` | The search engine you picked during first run, directly. Bedrock runs no search proxy and no server of its own (item 93). | the site or engine you chose | on (your own request) | policy | Type a URL instead of a query, or choose a different engine in Settings > Search. Private windows use the same engine and send no extra identifier. | Any engine in the list, or a custom search URL you enter yourself. |
| `search_suggestions` | The engine you chose receives what you type, keystroke by keystroke, before you press Enter. | the site or engine you chose | off | policy | Off unless you switch it on. Settings > Search > Suggestions, and it stays off in private windows whatever the setting says. | Follows the search engine choice; there is no separate suggestion host. |
| `doh_resolver` | A public DNS resolver you selected, over HTTPS or TLS. The default is your system resolver, so out of the box Bedrock contacts nobody new. | a third party you picked | off | policy | It is off by default (DnsMode::kSystem). Settings > Privacy > DNS. | Any RFC 8484 endpoint, including one you host: the presets are a convenience, not a fixed list. |
| `filter_list_subscriptions` | The list author's own URL, fetched by your machine. No Bedrock mirror, no Bedrock CDN, no rules service (invariant 1). | a third party you picked | off | policy | Bedrock ships with no default subscription, so nothing is fetched until you add one. Settings > Shield > Filter lists. | Any list URL you enter; a list can be removed without a browser update. |
| `extension_updates` | The store an extension came from, for that extension's own updates. The privacy catalogue is a description of extensions, not a host for them. | a third party you picked | off | policy | No extension is installed by default, so nothing is contacted. Remove the extension, or turn its updates off in Settings > Extensions. | Whichever store the extension declares; Bedrock adds none of its own. |
| `tor_mode` | The Tor network, through its own entry nodes, when you deliberately open a Tor window. | a third party you picked | off | policy | Off until you open a Tor window; normal browsing never touches it. | The Tor network only. Bedrock operates no relay, bridge or proxy. |
| `update_check` | Nothing yet: this build never checks for updates. When it does, it will ask the release host for a signed manifest and send no profile data. | a third party you picked | off | policy | Not implemented, so nothing to disable; when it lands it is a setting, and distribution packages can point it elsewhere or remove it. | The release host is a build argument, so a distribution can host its own. |
<!-- END REMOTE FEATURES -->

## What is not here, and will not be

Item 94 names the things a privacy browser is most often caught doing quietly. None
of them exists in this tree, and the gate keeps it that way: cloud configuration,
cloud personalisation, a cloud profile, a remote fingerprint database, a remote rules
service, a server-side assistant. If any of them is ever built it is opt-in, modular,
off by default, replaceable and documented here (item 95) — or it is not built.

Telemetry is a separate promise with a separate gate: `scripts/check_no_telemetry.py`.
