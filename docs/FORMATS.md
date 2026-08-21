# Import and export formats

**Roadmap item 59.** Everything you make in Bedrock comes back out in a format that is written
down here. A browser you cannot leave is a browser you cannot audit.

The table is generated from `src_overrides/bedrock/settings/portability.cc`; if the two disagree,
`scripts/check_config_surface.py` fails the build.

| Payload | Format id | File format | Extension | Direction | Secrets |
| --- | --- | --- | --- | --- | --- |
| Bookmarks | `bedrock.bookmarks.v1` | Netscape bookmark HTML | `.html` | import + export | never |
| Settings | `bedrock.settings.v1` | JSON | `.json` | import + export | never |
| Privacy rules | `bedrock.privacy-rules.v1` | JSON | `.json` | import + export | never |
| Filter rules | `bedrock.filter-rules.v1` | text, one rule per line | `.txt` | import + export | never |
| Profile | `bedrock.profile.v1` | zip archive with a manifest | `.bedrockprofile` | import + export | opt-in, encrypted |

## Versioning

Every file names its format and version (`bedrock.settings.v1`). An older file is upgraded; the
current one is read; **a newer one is refused**. Reading a newer file "as far as we understand
it" is how a restore quietly drops the rules you cared about.

## What an export never contains

- **Passwords**, except inside a profile bundle, and only when you asked for them *and* set a
  passphrase. Two separate acts, because one is an accident.
- **Cookies, tokens and history** — a settings file that logs someone else into your accounts is
  not a settings file.
- **The contents of third-party filter lists.** Subscriptions are exported as URLs. Those lists
  are other people's work under other people's licences (see [privacy/FILTER_LISTS.md](privacy/FILTER_LISTS.md));
  redistributing them inside our export format is not ours to do.

## What an import can never do

An imported file is untrusted input that arrives wearing your trust. It cannot:

- switch on telemetry (there is none),
- set or forge enterprise policy, or change a key your organisation has locked,
- point updates at a plain-HTTP source,
- carry an advanced value the settings dialog would refuse — imported values go through the same
  guards as typed ones ([item 57](CONFIGURATION.md#advanced-settings)).

Every import is previewed first: a count of what will be applied, and one line per refused item
with the reason. Nothing changes until you confirm.

## Bookmarks

Netscape bookmark HTML, the format Chrome, Firefox and Safari all read and write. Bedrock does not
invent a bookmark format; a proprietary one would be a lock-in with extra steps.

## Settings

The JSON keys are the config-file keys from [CONFIGURATION.md](CONFIGURATION.md) — one vocabulary
for the file you edit by hand and the file you export.

```json
{
  "format": "bedrock.settings.v1",
  "version": 1,
  "settings": {
    "privacy.level": "balanced",
    "privacy.cookies": "block-third-party",
    "search.default_engine": "duckduckgo"
  }
}
```

## Profile bundles

A zip with a `manifest.json` naming the format, the version and each member. Settings, privacy
rules, filter rules and bookmarks; history and cookies are deliberately absent. Passwords, when
requested, live in one separately encrypted member so the rest of the bundle stays readable.
