# Configuration

**Roadmap item 56.** Every major privacy control is reachable four ways: the GUI, a config
file, an enterprise policy, and — where it is useful — the command line. One table defines all
four (`src_overrides/bedrock/settings/config_surface.{h,cc}`), so the surfaces cannot disagree.

## Precedence

```
policy  >  command line  >  config file  >  GUI / prefs  >  built-in default
```

An administrator must be able to bind a setting a user cannot undo, so policy wins and the
value is reported as **locked** — the GUI shows it as managed rather than as a control that
mysteriously snaps back. Every resolved value carries its origin, so "why is this off?" always
has an answer.

## Strictness — the rule that matters

**An unknown switch, a missing value or a value outside the allowed set is an error.** Bedrock
prints it and refuses to start rather than continuing with the setting silently dropped. A
browser that accepts `--disable-telemetry` and ignores it teaches the user something false;
that is the same failure as a fake toggle (item 55), typed on a command line.

## Settings

| Config key | CLI | Policy | Values (default) |
| --- | --- | --- | --- |
| `privacy.level` | `--privacy-level=` | `PrivacyLevel` | standard, balanced, strict, maximum (**balanced**) |
| `privacy.fingerprinting` | `--fingerprinting=` | `FingerprintProtection` | off, standard, strict, maximum (**standard**) |
| `privacy.cookies` | `--cookies=` | `CookiePolicy` | allow, block-third-party, block (**block-third-party**) |
| `privacy.https` | `--https=` | `HttpsPolicy` | upgrade, only (**upgrade**) |
| `privacy.referrer` | `--referrer=` | `ReferrerPolicy` | full, origin, none (**origin**) |
| `network.dns.mode` | `--dns-mode=` | `DnsMode` | system, doh, doh-strict (**system**) |
| `network.dns.provider` | `--dns-provider=` | `DnsProvider` | provider id (**empty**) |
| `network.webrtc` | `--webrtc-policy=` | `WebRtcPolicy` | default, privacy, strict (**privacy**) |
| `blocking.lists` | `--filter-lists=` | `FilterLists` | comma-separated list ids (**empty**, see [FILTER_LISTS.md](privacy/FILTER_LISTS.md)) |
| `search.default_engine` | `--search-engine=` | `DefaultSearchEngine` | engine id (**duckduckgo**) |
| `profile.name` | `--profile=` | — | profile name (**empty**) |
| `session.tor_window` | `--tor-window` | — | flag |
| `telemetry.enabled` | `--disable-telemetry` | `TelemetryEnabled` | false (**false**, cannot be set to true) |
| `updates.channel` | `--update-channel=` | `UpdateChannel` | stable, beta, none (**stable**) |

Two settings are deliberately not on every surface, and the table records the reason in code:

- **`profile.name` has no policy.** Pinning which profile a person opens is user management,
  not configuration, and profiles are a local privacy boundary.
- **`session.tor_window` is GUI + CLI only.** A Tor window is an action, not a stored state; a
  config file that silently opened one would surprise the user. And, as everywhere: a Tor
  window is a transport, not an anonymity guarantee.

**`--disable-telemetry` is accepted and does nothing, on purpose.** Bedrock has no telemetry to
switch off (item 39), and scripts that pass the switch should not be told it is unknown. Trying
to *enable* telemetry (`--disable-telemetry=true`) is rejected rather than accepted and ignored.

## Examples

```bash
bedrock --privacy-level=balanced --disable-telemetry --search-engine=duckduckgo --profile=research
bedrock --dns-mode=doh-strict --dns-provider=quad9 --https=only
bedrock --tor-window
bedrock --help        # generated from the same table
```

## Config file

Plain keys as above, one per line or as nested objects, in the profile directory. The file is
local, human-readable and never uploaded; there is no sync service to upload it to.

## Enterprise policy

Policy names are listed in the table. Bedrock reads policies through Chromium's existing policy
mechanism — no Bedrock-run management service exists, and none is planned (invariant 1).

## Keeping this file true

`scripts/check_config_surface.py` fails the build when a switch exists in code but not in this
document, when this document names a switch that no longer exists, or when a documented value
is not in the allowed set. The CLI is documented because it is checked, not because we
remembered.

## Advanced settings

**Roadmap item 57.** Power users and administrators get their own filter lists, resolver, proxy,
user-agent policy, per-site permissions, per-site policies and CSP-like rules. Every one of those
inputs goes through `AdvancedSettings::Evaluate`, which answers **accepted**, **accepted with a
warning**, or **rejected with a reason** — never a silent shrug.

The warning verdict exists because some legitimate choices cost the user something they would not
guess: a custom user agent makes them *more* identifiable, a plain-DNS resolver hands every
hostname to their network, an HTTP proxy sees every unencrypted request.

Guards — rules no advanced setting can break, from the GUI **or** from enterprise policy:

| Guard | Rule |
| --- | --- |
| G1 | No advanced setting can enable reporting or a management server |
| G2 | Remote configuration (filter lists, rules) must be fetched over HTTPS |
| G3 | A DoH endpoint must be HTTPS |
| G4 | Proxies are limited to http, https and socks5 |
| G5 | Credentials never live in a URL |
| G6 | No global custom user agent |
| G7 | No wildcard permission grant |
| G8 | User content policy may only tighten, never relax, a site's CSP |
| G9 | Nothing can disable certificate validation, the sandbox or site isolation |

Rejection messages name the guard, so a refusal can be looked up rather than argued with.

## Reset and recovery

**Roadmap item 58.** Five actions, each stating both what it changes *and* what it leaves alone —
the second list is why people are willing to press the first button.

| Action | Confirmation | Leaves alone |
| --- | --- | --- |
| Reset privacy settings | dialog | bookmarks, history, passwords, tabs, extensions, site data |
| Reset browser settings | dialog | bookmarks, history, passwords, downloads, other profiles |
| Create new profile | none — nothing is lost | everything in the current profile |
| Clear all local data | type the profile name | bookmarks, passwords, downloaded files, settings |
| Restore defaults | type the profile name | bookmarks, passwords, other profiles, downloaded files |

Anything irreversible requires the profile name typed in, so the confirmation cannot be given for
the wrong profile, and offers an [export](FORMATS.md) first. The dialogs also say what a reset
cannot do: Bedrock erases what is on this computer — sites keep their own logs, and so does the
network.
