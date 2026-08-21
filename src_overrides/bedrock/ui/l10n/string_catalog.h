// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_UI_L10N_STRING_CATALOG_H_
#define BEDROCK_UI_L10N_STRING_CATALOG_H_

#include <string>
#include <vector>

// Localization (roadmap item 61).
//
// i18n from day one, because retrofitting it is not a translation project, it
// is a rewrite of every surface at once. The rule is simple and absolute:
//
//   **No user-visible string is written at the point where it is displayed.**
//
// A string lives in this catalog under an id; the UI asks for the id. That
// single rule buys everything else — a translator sees the whole corpus, a gate
// can prove no locale is half-finished, and no developer has to remember that
// German is longer than English.
//
// Ship locales: English, Ukrainian, Russian, German. "Ship" is the important
// word: a locale is not listed until every id in it is translated, because a
// half-translated UI is worse than an English one — the user cannot tell which
// half they are reading, and privacy language is exactly where a guess hurts.
//
// Three things the architecture must support even though only four locales
// exist today, since they change the *shape* of the API rather than its
// contents:
//
//   1. **Placeholders are named, never positional.** `{site}` survives a word
//      order that differs from English; `%s` does not. Ukrainian, Russian and
//      German all reorder relative to English.
//   2. **Plurals are a category, not an `if (n == 1)`.** Russian and Ukrainian
//      have three plural categories; English and German have two. A string with
//      a count declares its categories and the catalog checks they are present.
//   3. **Sentences are never assembled from fragments.** Concatenating
//      "Blocked " + count + " trackers" is untranslatable — gender, case and
//      order all depend on the count. One id, one whole sentence.
//
// Bidirectional text (Arabic, Hebrew) is not in the ship set, but nothing here
// assumes left-to-right: a locale carries its direction, and layout mirrors
// from that flag rather than from a hardcoded assumption.
//
// `scripts/check_strings.py` fails the build if a locale is missing an id, if
// placeholders differ between locales of the same id, or if a plural string is
// missing a category its locale requires.

namespace bedrock {
namespace ui {
namespace l10n {

// A locale Bedrock ships. `kEnglish` is also the source language: ids are
// written in English first, then translated.
enum class Locale {
  kEnglish,
  kUkrainian,
  kRussian,
  kGerman,
  kMaxValue = kGerman,
};

enum class TextDirection {
  kLeftToRight,
  kRightToLeft,
};

// CLDR plural categories. Only the ones the ship locales use are listed; adding
// a locale that needs "few"/"many" adds them here rather than in call sites.
enum class PluralCategory {
  kOne,    // 1 book, 1 книга
  kFew,    // 2–4 книги (Russian, Ukrainian)
  kMany,   // 5+ книг (Russian, Ukrainian)
  kOther,  // English/German plural, and the fallback everywhere
};

struct LocaleInfo {
  Locale locale;
  const char* bcp47;  // "en-US", "uk-UA", "ru-RU", "de-DE"
  const char* endonym;  // the language's name in itself — never in English
  TextDirection direction;
  // Plural categories this locale needs. A counted string missing one of these
  // is a build failure, not a runtime surprise.
  std::vector<PluralCategory> plural_categories;
};

// Message ids. The name says where it is used and what it says, so a translator
// working without the running UI still has context.
enum class MessageId {
  kAppName,
  kPrivacyPanelTitle,
  kPrivacyPanelTrackersBlocked,   // counted
  kPrivacyPanelNothingBlocked,
  kPrivacyLevelBalanced,
  kPrivacyLevelStrict,
  kResetConfirmTitle,
  kResetConfirmBody,              // has {profile}
  kResetUntouchedHeading,
  kImportPreviewHeading,
  kImportRefusedReason,           // has {reason}
  kAdvancedGuardRejected,         // has {guard} and {reason}
  // Errors (item 80). Each error has a sentence and the step that follows it;
  // an error without an action is a dead end, so the pair is never split.
  kErrorNetworkUnreachable,
  kErrorNetworkUnreachableAction,
  kErrorCertificateInvalid,       // has {site}
  kErrorCertificateInvalidAction,
  kErrorProfileLocked,
  kErrorProfileLockedAction,
  kErrorDownloadRefused,          // has {reason}
  kErrorDownloadRefusedAction,
  kErrorExtensionBlocked,         // has {extension}
  kErrorExtensionBlockedAction,
  kErrorConfigInvalid,
  kErrorConfigInvalidAction,
  kMaxValue = kErrorConfigInvalidAction,
};

struct Message {
  MessageId id;
  const char* text;
  // Named placeholders this message uses, e.g. {"profile"}. Every locale must
  // use exactly this set — a translation that drops one drops information, and
  // one that invents one crashes at format time.
  std::vector<std::string> placeholders;
  // Empty unless the message is counted. When set, the catalog must hold one
  // variant per category the locale requires.
  std::vector<PluralCategory> plural_variants;
};

class StringCatalog {
 public:
  StringCatalog();

  const std::vector<LocaleInfo>& Locales() const;
  const LocaleInfo* Info(Locale locale) const;

  // Every message in a locale. The gate compares these across locales.
  std::vector<Message> Messages(Locale locale) const;

  // Writes the message into `out` and returns true when the locale has it.
  bool Lookup(Locale locale, MessageId id, Message* out) const;

  // The lookup order for a locale: itself, then English. Bedrock deliberately
  // does *not* fall back Ukrainian → Russian, however close the languages are;
  // an unrequested language appearing mid-interface is a political statement
  // the browser has no business making.
  std::vector<Locale> FallbackChain(Locale locale) const;

  // True when every id has a translation in this locale — the condition for
  // offering it in the language menu at all.
  bool IsComplete(Locale locale) const;

  // Ids missing from `locale`, for the gate's error message.
  std::vector<MessageId> MissingIds(Locale locale) const;

  // Resolve a locale request from the system or `--lang`. "uk", "uk-UA" and
  // "uk_UA" all resolve; an unknown tag resolves to English rather than
  // failing, because a browser that will not start in an unexpected locale is
  // a browser that will not start.
  Locale Resolve(const std::string& bcp47_tag) const;

  static const char* Name(MessageId id);
  static const char* Name(PluralCategory category);
};

}  // namespace l10n
}  // namespace ui
}  // namespace bedrock

#endif  // BEDROCK_UI_L10N_STRING_CATALOG_H_
