# Localization

**Roadmap item 61.** i18n from the first day, because retrofitting it is not a translation
project — it is a rewrite of every surface at once.

## The rule

**No user-visible string is written where it is displayed.** A string lives in the catalog
(`src_overrides/bedrock/ui/l10n/string_catalog.cc`) under an id; the UI asks for the id.

That one rule buys the rest: a translator sees the whole corpus, a gate can prove no locale is
half-finished, and nobody has to remember that German runs 30% longer than English.

## Ship locales

| Locale | Tag | Endonym | Plural categories |
| --- | --- | --- | --- |
| English (source) | `en-US` | English | one, other |
| Ukrainian | `uk-UA` | Українська | one, few, many, other |
| Russian | `ru-RU` | Русский | one, few, many, other |
| German | `de-DE` | Deutsch | one, other |

A locale is offered in the language menu **only when every id is translated**. A half-translated
interface is worse than an English one: the user cannot tell which half they are reading, and
privacy wording is exactly where a guess causes harm. `scripts/check_strings.py` fails the build if
a locale is incomplete.

Languages are listed in themselves, never in English — a language menu the reader cannot read is
not a language menu.

## Three architectural consequences

1. **Named placeholders, never positional.** `{site}` survives a word order that differs from
   English; `%s` does not. All three non-English ship locales reorder relative to English. Every
   locale of an id must use exactly the same placeholder set — dropping one loses information,
   inventing one fails at format time. Enforced by test and by gate.
2. **Plurals are a category, not `if (n == 1)`.** Russian and Ukrainian need *one / few / many /
   other*; English and German need *one / other*. A counted message must carry every category its
   locale requires.
3. **No sentence is assembled from fragments.** `"Blocked " + count + " trackers"` is
   untranslatable: case and agreement depend on the number. One id, one whole sentence.

Text direction is a property of the locale, not an assumption. No RTL locale ships yet, but no
layout code hardcodes left-to-right either — the flag is there, so adding Arabic or Hebrew is a
translation job rather than a UI rewrite.

## Fallback

The chain is **the requested locale, then English**. Deliberately *not* Ukrainian → Russian, however
close the languages are: an unrequested language appearing mid-interface is a political statement a
browser has no business making.

An unknown or malformed tag resolves to English rather than failing. `uk`, `uk-UA` and `uk_UA` all
resolve; a browser that will not start in an unexpected locale is a browser that will not start.

## Choosing a language

The locale comes from the system by default, and can be set in Settings or with `--lang=uk`
([CONFIGURATION.md](CONFIGURATION.md)). The choice is local: **the browser does not send the user's
language anywhere.** `Accept-Language` and `navigator.language` are a fingerprinting surface, and
they are handled by the privacy engine's language-fingerprint mitigation — the UI language and the
language a website is told about are two different settings on purpose.

## Adding a locale

1. Add the `Locale` value and its `LocaleInfo` (tag, endonym, direction, plural categories).
2. Translate every id. There is no partial state — the gate rejects one.
3. Placeholders must match the English source exactly; word order may differ freely.
4. `python3 scripts/check_strings.py` and the host tests must pass.
