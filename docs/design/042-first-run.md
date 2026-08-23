# 042 — First run, search disclosure, honest onboarding

**Roadmap items 93, 98 and 99.** Status: landed and host-tested
(`src_overrides/bedrock/onboarding/first_run.*`). The flow logic is complete and
covered by tests; **the WebUI that renders it does not exist yet** — no Settings
or onboarding page has been written (see `.ai/memory/STATE.md`). Do not describe
first run as something a user has seen.

## 98 — The six steps

Welcome → Privacy level → Search engine → Theme → Import → Finish.

Two properties matter more than the screens:

- **Every step starts answered.** The flow is initialised from the shipped
  Balanced Privacy profile (item 84), the system theme, suggestions off and
  "skip import". A user who presses Next six times without reading ends exactly
  where a user with no onboarding at all would end. Onboarding is allowed to
  inform; it is not allowed to be the only place a safe default is chosen.
- **A choice that is not on offer is refused, not absorbed.** `ChooseEngine`
  rejects an id that is not in the offered list, and the theme step accepts only
  light, dark and system — high-contrast and custom live in Settings. A UI
  cannot leave the flow with "no search engine" or a half-applied theme.

Import sources: Chrome, Firefox, Edge, Chromium, an HTML bookmark file, or skip.
The import itself runs through `settings/portability` and its rule that an
import can only lower privilege (item 59) — first run adds no separate path.

## 93 — What the search step must say

Choosing a provider shows four lines, built from the facts the search layer
already holds (`bedrock_search_engines.json`), never from a second table:

| Row | Source |
| --- | --- |
| Search provider | the engine's display name |
| Suggestions | the pref, plus whether the engine has a suggest endpoint |
| Safe browsing | fixed: Bedrock ships with `safe_browsing_mode=0` |
| Privacy | "Searches are sent directly to <provider>." |

Three deliberate choices in that table:

1. **No search proxy.** The privacy line states that queries go straight from
   this browser to that provider and that Bedrock runs no server of any kind. A
   privacy-preserving intermediary would be a server we operate, which
   non-negotiable 1 forbids and item 93 explicitly rules out.
2. **The suggestions row describes the leak, not the feature.** On means what
   you type is sent before you press Enter, and it names who receives it.
3. **Safe browsing says "off" rather than implying cover.** Google Safe Browsing
   is compiled out (ADR 0001) because it would send browsing data to Google. The
   row says that, and says what still applies: HTTPS enforcement and content
   blocking. A row that claimed protection nobody ships would be exactly the
   fake feature item 55 gates against.

## 99 — What onboarding admits

Headline: **"Privacy protection is not invisibility."** Then five points, held
by a test at exactly five, each phrased so it cannot be read as a promise:

- websites still know what you give them;
- signing in identifies you to that service, whatever the browser does;
- fingerprinting protection raises the cost of identifying you — no browser can
  guarantee it is impossible;
- Tor Mode and normal browsing answer different threats, and Tor Mode is slower;
- extensions can read the pages you open, so each one is a risk you take on.

The test also refuses the words this project does not use about itself
("anonymous", "untraceable"), the same vocabulary `check_no_fake_features.py`
bans in the UI sources — that gate now covers `onboarding/` too.
