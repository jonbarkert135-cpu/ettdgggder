# Brand identity

**Roadmap item 65.** Bedrock's identity is its own: name, mark, colours, type and UI language are
authored here and owe nothing to another browser. `scripts/check_branding.py` enforces the parts
of that a script can check. The mirror image — other vendors' names, marks and interface
vocabulary, and what Bedrock refuses to borrow from them — is [`IDENTITY.md`](IDENTITY.md)
(items 92, 96, 97), enforced by `scripts/check_trademarks.py`.

## Name

Full name **Bedrock Browser**; short form **Bedrock** inside the product, where the window title
and the toolbar have no room for two words.

**The name is written in Latin script in every language — always.** Not translated, not
transliterated into Cyrillic, no localised variant in Russian, Ukrainian or German copy. This is what Firefox, Brave and Tor do, and the reason is practical rather than
stylistic: a user searching for the browser, typing its name into a package manager, or checking a
signature must find one spelling. A transliterated name splits the search results and the trust.
Written **Bedrock**, one word, capital B — never "BedRock", never lowercase as a proper noun.

The meaning survives translation without help: bedrock is the layer under everything else that does
not move. It says what the project is (a browser on a solid base) without claiming what it cannot
prove — nothing in the name promises anonymity, and no browser vendor's word appears in it.

The engine is Chromium and the project says so in prose — "Bedrock Browser, a Chromium-derived
browser" — but never as part of the name (no "Bedrock Chrome", no "Chromium Bedrock").

## Mark

| Asset | Use |
| --- | --- |
| [`branding/bedrock-logo.png`](../branding/bedrock-logo.png) | **The logo.** Opaque original, 1254 px. About box, installer, site, press. |
| [`branding/bedrock-logo-transparent.png`](../branding/bedrock-logo-transparent.png) | The same mark without a background. Source for every application icon, and for any surface whose background is not black — README, packaging, avatars. |
| [`branding/bedrock-mark-small.svg`](../branding/bedrock-mark-small.svg) | **Only** 16–32 px: tab strip, tray, favicon, taskbar. Three bands, derived from the logo. |

The mark is a sphere of fractured strata — rock seen in section, layers offset along their faults.
It is the project's own artwork and it does not change: no recolouring, no gradient overlay, no
outline variant, no rotation, no drop shadow, no placing it inside another shape.

Two rules that exist because icons are usually reviewed at the wrong size:

- **Below 32 px the mark changes, not shrinks.** Downscaled to 16 px the strata average out into a
  grey circle — verified, not assumed. The small variant keeps three bands so the silhouette still
  reads in a tab strip; everywhere at 32 px and up, the logo itself is used.
- **The mark is never recoloured to signal state.** State lives in the UI, not in the app icon;
  a browser whose icon changes colour has taught the user nothing they can act on.

Application icons are generated, not hand-cut and not committed:

```
python3 scripts/gen_icons.py --out out/branding      # 16…512 px PNG, Windows .ico, Linux hicolor
```

The generator trims the transparent margin before resizing, so the mark fills the icon box instead
of floating in it — the source art has ~15 % padding on each side, which at 32 px costs five pixels
of the only thing anyone can see.

Not permitted, anywhere: another browser's mark, a mark derived from one, a "shield" borrowed from
a privacy product, a lion, a fox, a globe-with-orbit, or a rounded-square gradient that reads as
someone else's platform icon at a glance.

## Colour

Values live in [`branding/design-tokens.json`](../branding/design-tokens.json) and nowhere else;
this document names them, it does not redefine them. Graphite surfaces, one copper accent
(`#B4622A` light, `#E08A4C` dark).

The palette is deliberately **not blue** (Chrome, Edge, Safari and most of the industry) and not
orange-red (Brave, Firefox). Copper on graphite is close to nothing else in the category, which is
the point of a brand colour, and it reads as warm rather than corporate.

Contrast is a brand rule, not only an accessibility one: every text and control pairing meets the
floors from [ACCESSIBILITY.md](ACCESSIBILITY.md) (4.5:1 text, 3.0:1 controls) in both themes.
A brand colour that has to be used at an unreadable contrast is the wrong brand colour.

## Typography

System-stack UI type — Inter, SF Pro Text, Segoe UI Variable, then `system-ui` — six sizes
(11–24 px), three weights, tabular numerals wherever a count appears. No licensed display face
ships with the browser: a font file is a dependency with a licence, a download and a fingerprinting
surface, and the browser's own chrome is not where a project should spend that.

The wordmark — "Bedrock Browser", Latin script in every locale — is set in the UI face at semibold
with tight tracking. There is no separate logotype file, because a wordmark that cannot be rebuilt
from the token set is a wordmark that will drift.

## UI identity

- **Content is the only saturated thing on screen.** Chrome is neutral so the page is not.
- **Protection is a colour, not a badge.** Copper in the toolbar means something is being
  protected right now, and the number beside it is a real count (item 55).
- **Words over icons.** Every control has a label; icon-only controls carry an accessible name.
- **Motion you notice only when it is missing.** 90 ms for state, 160 ms for surfaces, zero under
  `prefers-reduced-motion`.
- **The honest voice.** No "anonymous", no "100%", no counter for an event the engine does not
  emit. The brand's promise is the same as the engine's behaviour, which is why item 55 has a gate.

## Forks and redistribution

The code is MPL-2.0; the name and the mark are not a licence to imply endorsement. A fork may use
the code freely, must change the product name and the mark if it changes behaviour, and must not
present itself as Bedrock. This is the same courtesy other browsers ask for, and it is stated here
so a downstream packager does not have to guess.
