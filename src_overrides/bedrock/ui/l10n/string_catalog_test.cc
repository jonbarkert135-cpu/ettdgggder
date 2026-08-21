// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/ui/l10n/string_catalog.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <set>
#include <string>

namespace bedrock {
namespace ui {
namespace l10n {
namespace {

bool Has(const std::vector<PluralCategory>& categories, PluralCategory wanted) {
  return std::find(categories.begin(), categories.end(), wanted) !=
         categories.end();
}

// Item 61: a locale is offered only when it is finished. Half a translation is
// worse than none — the user cannot tell which half they are reading.
void TestEveryShipLocaleIsComplete() {
  const StringCatalog catalog;
  for (const LocaleInfo& info : catalog.Locales()) {
    if (!catalog.IsComplete(info.locale)) {
      for (MessageId id : catalog.MissingIds(info.locale))
        std::printf("  %s missing %s\n", info.bcp47, StringCatalog::Name(id));
    }
    assert(catalog.IsComplete(info.locale));
  }
  assert(catalog.Locales().size() == 4);
}

// The whole point of named placeholders: a translation may reorder them, but it
// may not drop one (information lost) or invent one (crash at format time).
void TestPlaceholdersMatchAcrossLocales() {
  const StringCatalog catalog;
  for (const Message& source : catalog.Messages(Locale::kEnglish)) {
    for (const LocaleInfo& info : catalog.Locales()) {
      Message translated;
      assert(catalog.Lookup(info.locale, source.id, &translated));
      assert(translated.placeholders == source.placeholders);
    }
  }
}

// Russian and Ukrainian need three plural forms plus "other"; a counted string
// that only has singular/plural is a grammatical error in half the ship set.
void TestCountedStringsCoverTheirLocalePlurals() {
  const StringCatalog catalog;
  for (const LocaleInfo& info : catalog.Locales()) {
    for (const Message& message : catalog.Messages(info.locale)) {
      if (message.plural_variants.empty())
        continue;
      for (PluralCategory category : info.plural_categories)
        assert(Has(message.plural_variants, category));
    }
  }

  Message russian;
  assert(catalog.Lookup(Locale::kRussian,
                        MessageId::kPrivacyPanelTrackersBlocked, &russian));
  assert(russian.plural_variants.size() == 4);
  Message english;
  assert(catalog.Lookup(Locale::kEnglish,
                        MessageId::kPrivacyPanelTrackersBlocked, &english));
  assert(english.plural_variants.size() == 2);
}

// No string is assembled from fragments: a counted message carries the whole
// sentence, so the translator controls word order, case and agreement.
void TestCountedMessagesAreWholeSentences() {
  const StringCatalog catalog;
  for (const LocaleInfo& info : catalog.Locales()) {
    for (const Message& message : catalog.Messages(info.locale)) {
      if (message.plural_variants.empty())
        continue;
      const std::string text = message.text;
      assert(text.find("{count}") != std::string::npos);
      assert(text.find(' ') != std::string::npos);
    }
  }
}

// Ukrainian must never fall back to Russian, however similar the languages
// are. A browser does not get to make that substitution on a user's behalf.
void TestFallbackChainIsEnglishOnly() {
  const StringCatalog catalog;
  for (const LocaleInfo& info : catalog.Locales()) {
    for (Locale fallback : catalog.FallbackChain(info.locale))
      assert(fallback == info.locale || fallback == Locale::kEnglish);
  }
  assert(catalog.FallbackChain(Locale::kUkrainian).size() == 2);
  assert(catalog.FallbackChain(Locale::kEnglish).size() == 1);
}

void TestResolveAcceptsTheTagsSystemsActuallySend() {
  const StringCatalog catalog;
  assert(catalog.Resolve("uk") == Locale::kUkrainian);
  assert(catalog.Resolve("uk-UA") == Locale::kUkrainian);
  assert(catalog.Resolve("uk_UA") == Locale::kUkrainian);
  assert(catalog.Resolve("de-AT") == Locale::kGerman);
  assert(catalog.Resolve("RU-ru") == Locale::kRussian);
  // Unknown or malformed tags start the browser in English rather than not at
  // all.
  assert(catalog.Resolve("qq-ZZ") == Locale::kEnglish);
  assert(catalog.Resolve("") == Locale::kEnglish);
}

// Endonyms: a language menu written in English is unusable by the people who
// need it. Also catches a copy-paste of the English name.
void TestLanguageMenuNamesLanguagesInThemselves() {
  const StringCatalog catalog;
  std::set<std::string> endonyms;
  for (const LocaleInfo& info : catalog.Locales()) {
    const std::string endonym = info.endonym;
    assert(!endonym.empty());
    assert(endonyms.insert(endonym).second);
  }
  const LocaleInfo* ukrainian = catalog.Info(Locale::kUkrainian);
  assert(ukrainian != nullptr);
  assert(std::string(ukrainian->endonym) != "Ukrainian");
}

// Message ids are stable, unique names — that is what a translation memory is
// keyed on.
void TestMessageIdNamesAreUniqueAndStable() {
  const StringCatalog catalog;
  std::set<std::string> names;
  for (const Message& message : catalog.Messages(Locale::kEnglish)) {
    const std::string name = StringCatalog::Name(message.id);
    assert(name.rfind("IDS_", 0) == 0);
    assert(name != "IDS_UNKNOWN");
    assert(names.insert(name).second);
  }
}

}  // namespace
}  // namespace l10n
}  // namespace ui
}  // namespace bedrock

int main() {
  bedrock::ui::l10n::TestEveryShipLocaleIsComplete();
  bedrock::ui::l10n::TestPlaceholdersMatchAcrossLocales();
  bedrock::ui::l10n::TestCountedStringsCoverTheirLocalePlurals();
  bedrock::ui::l10n::TestCountedMessagesAreWholeSentences();
  bedrock::ui::l10n::TestFallbackChainIsEnglishOnly();
  bedrock::ui::l10n::TestResolveAcceptsTheTagsSystemsActuallySend();
  bedrock::ui::l10n::TestLanguageMenuNamesLanguagesInThemselves();
  bedrock::ui::l10n::TestMessageIdNamesAreUniqueAndStable();
  std::printf("string_catalog_test: all assertions passed\n");
  return 0;
}
