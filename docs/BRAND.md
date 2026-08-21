# Brand identity

**Roadmap item 65.** Bedrock's identity is its own: name, mark, colours, type and UI language are
authored here and owe nothing to another browser. `scripts/check_branding.py` enforces the parts
of that a script can check.

## Name

**Bedrock** — the layer under everything else that does not move. It says what the project is (a
browser built on a solid base) without claiming what it cannot prove; nothing in the name promises
anonymity, and no browser vendor's word appears in it.

The engine is Chromium and the project says so in prose — "Bedrock, a Chromium-derived browser" —
but never as a name (no "Bedrock Chrome", no "Chromium Bedrock"). Written **Bedrock**, one word,
capital B, never "BedRock" or "bedrock browser" as a proper noun. The Russian and Ukrainian
materials transliterate it as *Бедрок* and keep the Latin form for anything a user types.

## Mark

| Asset | Use |
| --- | --- |
| [`branding/bedrock-mark.svg`](../branding/bedrock-mark.svg) | The mark. Anything 32 px and up: about box, installer, site, README. |
| [`branding/bedrock-mark-small.svg`](../branding/bedrock-mark-small.svg) | 16–32 px: tab strip, tray, favicon, taskbar. Three bands instead of six. |
| [`branding/bedrock-logo.png`](../branding/bedrock-logo.png) | Raster fallback for contexts that cannot take SVG. |

Strata of stone seen in section, clipped to a circle, with one copper seam. The seam is the same
accent the UI uses for protection state, so the brand and the product agree on what the colour
means — an identity that contradicts the interface teaches the user to ignore both.

Two rules that exist because icons are usually reviewed at the wrong size:

- **Below 32 px the mark changes, not shrinks.** Six seams under 32 px fall beneath a pixel and
  the icon becomes a grey dot. The small variant is a separate file for that reason.
- **The mark is never recoloured to signal state.** State lives in the UI, not in the app icon;
  a browser whose icon changes colour has taught the user nothing they can act on.

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

The wordmark is set in the UI face at semibold with tight tracking. There is no separate logotype
file, because a wordmark that cannot be rebuilt from the token set is a wordmark that will drift.

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
