// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/ui/l10n/string_catalog.h"

#include <algorithm>
#include <cctype>

namespace bedrock {
namespace ui {
namespace l10n {

namespace {

// One row per (locale, id, plural category). Non-counted messages use kOther.
// The English column is the source text; the others are translations of it, not
// of each other, so a fix in English is a fix everyone re-reads.
struct RawMessage {
  Locale locale;
  MessageId id;
  PluralCategory category;
  const char* text;
  bool counted;
};

// clang-format off
const RawMessage kMessages[] = {
    // --- English (source) ---
    {Locale::kEnglish, MessageId::kAppName, PluralCategory::kOther, "Bedrock", false},
    {Locale::kEnglish, MessageId::kPrivacyPanelTitle, PluralCategory::kOther, "Protections for {site}", false},
    {Locale::kEnglish, MessageId::kPrivacyPanelTrackersBlocked, PluralCategory::kOne, "{count} tracker blocked on this page", true},
    {Locale::kEnglish, MessageId::kPrivacyPanelTrackersBlocked, PluralCategory::kOther, "{count} trackers blocked on this page", true},
    {Locale::kEnglish, MessageId::kPrivacyPanelNothingBlocked, PluralCategory::kOther, "Nothing was blocked on this page", false},
    {Locale::kEnglish, MessageId::kPrivacyLevelBalanced, PluralCategory::kOther, "Balanced", false},
    {Locale::kEnglish, MessageId::kPrivacyLevelStrict, PluralCategory::kOther, "Strict", false},
    {Locale::kEnglish, MessageId::kResetConfirmTitle, PluralCategory::kOther, "Erase everything in this profile?", false},
    {Locale::kEnglish, MessageId::kResetConfirmBody, PluralCategory::kOther, "Type the profile name {profile} to confirm. This cannot be undone.", false},
    {Locale::kEnglish, MessageId::kResetUntouchedHeading, PluralCategory::kOther, "This will not touch", false},
    {Locale::kEnglish, MessageId::kImportPreviewHeading, PluralCategory::kOther, "Review before importing", false},
    {Locale::kEnglish, MessageId::kImportRefusedReason, PluralCategory::kOther, "Not imported: {reason}", false},
    {Locale::kEnglish, MessageId::kAdvancedGuardRejected, PluralCategory::kOther, "Refused by {guard}: {reason}", false},
    {Locale::kEnglish, MessageId::kErrorNetworkUnreachable, PluralCategory::kOther, "Bedrock could not reach the network", false},
    {Locale::kEnglish, MessageId::kErrorNetworkUnreachableAction, PluralCategory::kOther, "Check your connection, then reload the page. If you are using Tor mode, the circuit may still be building.", false},
    {Locale::kEnglish, MessageId::kErrorCertificateInvalid, PluralCategory::kOther, "The certificate for {site} could not be verified", false},
    {Locale::kEnglish, MessageId::kErrorCertificateInvalidAction, PluralCategory::kOther, "Do not enter passwords on this page. Leave the site, or continue only if you know why the certificate is wrong.", false},
    {Locale::kEnglish, MessageId::kErrorProfileLocked, PluralCategory::kOther, "This profile is already open in another window", false},
    {Locale::kEnglish, MessageId::kErrorProfileLockedAction, PluralCategory::kOther, "Switch to the running window, or close it and try again. Nothing was lost.", false},
    {Locale::kEnglish, MessageId::kErrorDownloadRefused, PluralCategory::kOther, "The download was refused: {reason}", false},
    {Locale::kEnglish, MessageId::kErrorDownloadRefusedAction, PluralCategory::kOther, "Save it to a different folder, or allow this file type in Settings, Downloads.", false},
    {Locale::kEnglish, MessageId::kErrorExtensionBlocked, PluralCategory::kOther, "{extension} was blocked", false},
    {Locale::kEnglish, MessageId::kErrorExtensionBlockedAction, PluralCategory::kOther, "Review the permissions it asked for in Settings, Extensions. You can allow it there if you trust it.", false},
    {Locale::kEnglish, MessageId::kErrorConfigInvalid, PluralCategory::kOther, "The configuration file could not be read", false},
    {Locale::kEnglish, MessageId::kErrorConfigInvalidAction, PluralCategory::kOther, "Fix the line shown in the details, or rename the file to start from defaults. Bedrock started with the previous settings.", false},

    // --- Ukrainian ---
    {Locale::kUkrainian, MessageId::kAppName, PluralCategory::kOther, "Bedrock", false},
    {Locale::kUkrainian, MessageId::kPrivacyPanelTitle, PluralCategory::kOther, "Захист для {site}", false},
    {Locale::kUkrainian, MessageId::kPrivacyPanelTrackersBlocked, PluralCategory::kOne, "Заблоковано {count} стежач на цій сторінці", true},
    {Locale::kUkrainian, MessageId::kPrivacyPanelTrackersBlocked, PluralCategory::kFew, "Заблоковано {count} стежачі на цій сторінці", true},
    {Locale::kUkrainian, MessageId::kPrivacyPanelTrackersBlocked, PluralCategory::kMany, "Заблоковано {count} стежачів на цій сторінці", true},
    {Locale::kUkrainian, MessageId::kPrivacyPanelTrackersBlocked, PluralCategory::kOther, "Заблоковано {count} стежача на цій сторінці", true},
    {Locale::kUkrainian, MessageId::kPrivacyPanelNothingBlocked, PluralCategory::kOther, "На цій сторінці нічого не заблоковано", false},
    {Locale::kUkrainian, MessageId::kPrivacyLevelBalanced, PluralCategory::kOther, "Збалансований", false},
    {Locale::kUkrainian, MessageId::kPrivacyLevelStrict, PluralCategory::kOther, "Суворий", false},
    {Locale::kUkrainian, MessageId::kResetConfirmTitle, PluralCategory::kOther, "Стерти все у цьому профілі?", false},
    {Locale::kUkrainian, MessageId::kResetConfirmBody, PluralCategory::kOther, "Введіть назву профілю {profile} для підтвердження. Цю дію не можна скасувати.", false},
    {Locale::kUkrainian, MessageId::kResetUntouchedHeading, PluralCategory::kOther, "Це не торкнеться", false},
    {Locale::kUkrainian, MessageId::kImportPreviewHeading, PluralCategory::kOther, "Перегляньте перед імпортом", false},
    {Locale::kUkrainian, MessageId::kImportRefusedReason, PluralCategory::kOther, "Не імпортовано: {reason}", false},
    {Locale::kUkrainian, MessageId::kAdvancedGuardRejected, PluralCategory::kOther, "Відхилено правилом {guard}: {reason}", false},
    {Locale::kUkrainian, MessageId::kErrorNetworkUnreachable, PluralCategory::kOther, "Bedrock не зміг зʼєднатися з мережею", false},
    {Locale::kUkrainian, MessageId::kErrorNetworkUnreachableAction, PluralCategory::kOther, "Перевірте зʼєднання та перезавантажте сторінку. Якщо ввімкнено режим Tor, ланцюжок ще може будуватися.", false},
    {Locale::kUkrainian, MessageId::kErrorCertificateInvalid, PluralCategory::kOther, "Не вдалося перевірити сертифікат для {site}", false},
    {Locale::kUkrainian, MessageId::kErrorCertificateInvalidAction, PluralCategory::kOther, "Не вводьте паролі на цій сторінці. Залиште сайт або продовжуйте, лише якщо знаєте, чому сертифікат помилковий.", false},
    {Locale::kUkrainian, MessageId::kErrorProfileLocked, PluralCategory::kOther, "Цей профіль уже відкрито в іншому вікні", false},
    {Locale::kUkrainian, MessageId::kErrorProfileLockedAction, PluralCategory::kOther, "Перейдіть до відкритого вікна або закрийте його і спробуйте ще раз. Нічого не втрачено.", false},
    {Locale::kUkrainian, MessageId::kErrorDownloadRefused, PluralCategory::kOther, "Завантаження відхилено: {reason}", false},
    {Locale::kUkrainian, MessageId::kErrorDownloadRefusedAction, PluralCategory::kOther, "Збережіть файл до іншої теки або дозвольте цей тип файлів у Налаштуваннях, розділ Завантаження.", false},
    {Locale::kUkrainian, MessageId::kErrorExtensionBlocked, PluralCategory::kOther, "{extension} заблоковано", false},
    {Locale::kUkrainian, MessageId::kErrorExtensionBlockedAction, PluralCategory::kOther, "Перегляньте запитані дозволи в Налаштуваннях, розділ Розширення. Там же можна дозволити його, якщо ви йому довіряєте.", false},
    {Locale::kUkrainian, MessageId::kErrorConfigInvalid, PluralCategory::kOther, "Не вдалося прочитати файл конфігурації", false},
    {Locale::kUkrainian, MessageId::kErrorConfigInvalidAction, PluralCategory::kOther, "Виправте рядок, показаний у подробицях, або перейменуйте файл, щоб почати з типових значень. Bedrock запустився з попередніми налаштуваннями.", false},

    // --- Russian ---
    {Locale::kRussian, MessageId::kAppName, PluralCategory::kOther, "Bedrock", false},
    {Locale::kRussian, MessageId::kPrivacyPanelTitle, PluralCategory::kOther, "Защита для {site}", false},
    {Locale::kRussian, MessageId::kPrivacyPanelTrackersBlocked, PluralCategory::kOne, "Заблокирован {count} трекер на этой странице", true},
    {Locale::kRussian, MessageId::kPrivacyPanelTrackersBlocked, PluralCategory::kFew, "Заблокировано {count} трекера на этой странице", true},
    {Locale::kRussian, MessageId::kPrivacyPanelTrackersBlocked, PluralCategory::kMany, "Заблокировано {count} трекеров на этой странице", true},
    {Locale::kRussian, MessageId::kPrivacyPanelTrackersBlocked, PluralCategory::kOther, "Заблокировано {count} трекера на этой странице", true},
    {Locale::kRussian, MessageId::kPrivacyPanelNothingBlocked, PluralCategory::kOther, "На этой странице ничего не заблокировано", false},
    {Locale::kRussian, MessageId::kPrivacyLevelBalanced, PluralCategory::kOther, "Сбалансированный", false},
    {Locale::kRussian, MessageId::kPrivacyLevelStrict, PluralCategory::kOther, "Строгий", false},
    {Locale::kRussian, MessageId::kResetConfirmTitle, PluralCategory::kOther, "Стереть всё в этом профиле?", false},
    {Locale::kRussian, MessageId::kResetConfirmBody, PluralCategory::kOther, "Введите имя профиля {profile} для подтверждения. Отменить это будет нельзя.", false},
    {Locale::kRussian, MessageId::kResetUntouchedHeading, PluralCategory::kOther, "Это не затронет", false},
    {Locale::kRussian, MessageId::kImportPreviewHeading, PluralCategory::kOther, "Проверьте перед импортом", false},
    {Locale::kRussian, MessageId::kImportRefusedReason, PluralCategory::kOther, "Не импортировано: {reason}", false},
    {Locale::kRussian, MessageId::kAdvancedGuardRejected, PluralCategory::kOther, "Отклонено правилом {guard}: {reason}", false},
    {Locale::kRussian, MessageId::kErrorNetworkUnreachable, PluralCategory::kOther, "Bedrock не смог подключиться к сети", false},
    {Locale::kRussian, MessageId::kErrorNetworkUnreachableAction, PluralCategory::kOther, "Проверьте соединение и перезагрузите страницу. Если включён режим Tor, цепочка может ещё строиться.", false},
    {Locale::kRussian, MessageId::kErrorCertificateInvalid, PluralCategory::kOther, "Не удалось проверить сертификат для {site}", false},
    {Locale::kRussian, MessageId::kErrorCertificateInvalidAction, PluralCategory::kOther, "Не вводите пароли на этой странице. Покиньте сайт или продолжайте, только если знаете, почему сертификат неверен.", false},
    {Locale::kRussian, MessageId::kErrorProfileLocked, PluralCategory::kOther, "Этот профиль уже открыт в другом окне", false},
    {Locale::kRussian, MessageId::kErrorProfileLockedAction, PluralCategory::kOther, "Перейдите в открытое окно или закройте его и попробуйте снова. Ничего не потеряно.", false},
    {Locale::kRussian, MessageId::kErrorDownloadRefused, PluralCategory::kOther, "Загрузка отклонена: {reason}", false},
    {Locale::kRussian, MessageId::kErrorDownloadRefusedAction, PluralCategory::kOther, "Сохраните файл в другую папку или разрешите этот тип файлов в Настройках, раздел Загрузки.", false},
    {Locale::kRussian, MessageId::kErrorExtensionBlocked, PluralCategory::kOther, "{extension} заблокировано", false},
    {Locale::kRussian, MessageId::kErrorExtensionBlockedAction, PluralCategory::kOther, "Посмотрите запрошенные разрешения в Настройках, раздел Расширения. Там же можно разрешить его, если вы ему доверяете.", false},
    {Locale::kRussian, MessageId::kErrorConfigInvalid, PluralCategory::kOther, "Не удалось прочитать файл конфигурации", false},
    {Locale::kRussian, MessageId::kErrorConfigInvalidAction, PluralCategory::kOther, "Исправьте строку, показанную в подробностях, или переименуйте файл, чтобы начать со значений по умолчанию. Bedrock запустился с прежними настройками.", false},

    // --- German ---
    {Locale::kGerman, MessageId::kAppName, PluralCategory::kOther, "Bedrock", false},
    {Locale::kGerman, MessageId::kPrivacyPanelTitle, PluralCategory::kOther, "Schutz für {site}", false},
    {Locale::kGerman, MessageId::kPrivacyPanelTrackersBlocked, PluralCategory::kOne, "{count} Tracker auf dieser Seite blockiert", true},
    {Locale::kGerman, MessageId::kPrivacyPanelTrackersBlocked, PluralCategory::kOther, "{count} Tracker auf dieser Seite blockiert", true},
    {Locale::kGerman, MessageId::kPrivacyPanelNothingBlocked, PluralCategory::kOther, "Auf dieser Seite wurde nichts blockiert", false},
    {Locale::kGerman, MessageId::kPrivacyLevelBalanced, PluralCategory::kOther, "Ausgewogen", false},
    {Locale::kGerman, MessageId::kPrivacyLevelStrict, PluralCategory::kOther, "Streng", false},
    {Locale::kGerman, MessageId::kResetConfirmTitle, PluralCategory::kOther, "Alles in diesem Profil löschen?", false},
    {Locale::kGerman, MessageId::kResetConfirmBody, PluralCategory::kOther, "Geben Sie den Profilnamen {profile} ein, um zu bestätigen. Dies kann nicht rückgängig gemacht werden.", false},
    {Locale::kGerman, MessageId::kResetUntouchedHeading, PluralCategory::kOther, "Davon unberührt bleibt", false},
    {Locale::kGerman, MessageId::kImportPreviewHeading, PluralCategory::kOther, "Vor dem Import prüfen", false},
    {Locale::kGerman, MessageId::kImportRefusedReason, PluralCategory::kOther, "Nicht importiert: {reason}", false},
    {Locale::kGerman, MessageId::kAdvancedGuardRejected, PluralCategory::kOther, "Abgelehnt durch {guard}: {reason}", false},
    {Locale::kGerman, MessageId::kErrorNetworkUnreachable, PluralCategory::kOther, "Bedrock konnte das Netzwerk nicht erreichen", false},
    {Locale::kGerman, MessageId::kErrorNetworkUnreachableAction, PluralCategory::kOther, "Prüfen Sie Ihre Verbindung und laden Sie die Seite neu. Im Tor-Modus wird der Kanal möglicherweise noch aufgebaut.", false},
    {Locale::kGerman, MessageId::kErrorCertificateInvalid, PluralCategory::kOther, "Das Zertifikat für {site} konnte nicht geprüft werden", false},
    {Locale::kGerman, MessageId::kErrorCertificateInvalidAction, PluralCategory::kOther, "Geben Sie auf dieser Seite keine Passwörter ein. Verlassen Sie die Website, oder fahren Sie nur fort, wenn Sie wissen, warum das Zertifikat falsch ist.", false},
    {Locale::kGerman, MessageId::kErrorProfileLocked, PluralCategory::kOther, "Dieses Profil ist bereits in einem anderen Fenster geöffnet", false},
    {Locale::kGerman, MessageId::kErrorProfileLockedAction, PluralCategory::kOther, "Wechseln Sie zum offenen Fenster, oder schließen Sie es und versuchen Sie es erneut. Es ging nichts verloren.", false},
    {Locale::kGerman, MessageId::kErrorDownloadRefused, PluralCategory::kOther, "Der Download wurde abgelehnt: {reason}", false},
    {Locale::kGerman, MessageId::kErrorDownloadRefusedAction, PluralCategory::kOther, "Speichern Sie die Datei in einem anderen Ordner, oder erlauben Sie diesen Dateityp in den Einstellungen unter Downloads.", false},
    {Locale::kGerman, MessageId::kErrorExtensionBlocked, PluralCategory::kOther, "{extension} wurde blockiert", false},
    {Locale::kGerman, MessageId::kErrorExtensionBlockedAction, PluralCategory::kOther, "Prüfen Sie die angeforderten Berechtigungen in den Einstellungen unter Erweiterungen. Dort können Sie sie erlauben, wenn Sie ihr vertrauen.", false},
    {Locale::kGerman, MessageId::kErrorConfigInvalid, PluralCategory::kOther, "Die Konfigurationsdatei konnte nicht gelesen werden", false},
    {Locale::kGerman, MessageId::kErrorConfigInvalidAction, PluralCategory::kOther, "Korrigieren Sie die in den Details angezeigte Zeile, oder benennen Sie die Datei um, um mit Standardwerten zu starten. Bedrock wurde mit den vorherigen Einstellungen gestartet.", false},
};
// clang-format on

// Placeholders are parsed out of the text rather than declared twice: two
// declarations of the same fact drift, and the one that drifts is the one the
// translator never sees.
std::vector<std::string> ParsePlaceholders(const std::string& text) {
  std::vector<std::string> names;
  for (size_t open = text.find('{'); open != std::string::npos;
       open = text.find('{', open + 1)) {
    const size_t close = text.find('}', open);
    if (close == std::string::npos)
      break;
    std::string name = text.substr(open + 1, close - open - 1);
    if (!name.empty() &&
        std::find(names.begin(), names.end(), name) == names.end()) {
      names.push_back(name);
    }
  }
  std::sort(names.begin(), names.end());
  return names;
}

std::string Lowercase(const std::string& value) {
  std::string out = value;
  for (char& character : out)
    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  return out;
}

}  // namespace

StringCatalog::StringCatalog() = default;

const std::vector<LocaleInfo>& StringCatalog::Locales() const {
  static const std::vector<LocaleInfo> kLocales = {
      {Locale::kEnglish, "en-US", "English", TextDirection::kLeftToRight,
       {PluralCategory::kOne, PluralCategory::kOther}},
      {Locale::kUkrainian, "uk-UA", "Українська", TextDirection::kLeftToRight,
       {PluralCategory::kOne, PluralCategory::kFew, PluralCategory::kMany,
        PluralCategory::kOther}},
      {Locale::kRussian, "ru-RU", "Русский", TextDirection::kLeftToRight,
       {PluralCategory::kOne, PluralCategory::kFew, PluralCategory::kMany,
        PluralCategory::kOther}},
      {Locale::kGerman, "de-DE", "Deutsch", TextDirection::kLeftToRight,
       {PluralCategory::kOne, PluralCategory::kOther}},
  };
  return kLocales;
}

const LocaleInfo* StringCatalog::Info(Locale locale) const {
  for (const LocaleInfo& info : Locales()) {
    if (info.locale == locale)
      return &info;
  }
  return nullptr;
}

std::vector<Message> StringCatalog::Messages(Locale locale) const {
  std::vector<Message> messages;
  for (const RawMessage& raw : kMessages) {
    if (raw.locale != locale)
      continue;
    Message* existing = nullptr;
    for (Message& candidate : messages) {
      if (candidate.id == raw.id)
        existing = &candidate;
    }
    if (existing) {
      existing->plural_variants.push_back(raw.category);
      continue;
    }
    Message message;
    message.id = raw.id;
    message.text = raw.text;
    message.placeholders = ParsePlaceholders(raw.text);
    if (raw.counted)
      message.plural_variants.push_back(raw.category);
    messages.push_back(message);
  }
  return messages;
}

bool StringCatalog::Lookup(Locale locale, MessageId id, Message* out) const {
  for (const Message& message : Messages(locale)) {
    if (message.id != id)
      continue;
    if (out)
      *out = message;
    return true;
  }
  return false;
}

std::vector<Locale> StringCatalog::FallbackChain(Locale locale) const {
  if (locale == Locale::kEnglish)
    return {Locale::kEnglish};
  return {locale, Locale::kEnglish};
}

std::vector<MessageId> StringCatalog::MissingIds(Locale locale) const {
  const std::vector<Message> translated = Messages(locale);
  std::vector<MessageId> missing;
  for (const Message& source : Messages(Locale::kEnglish)) {
    bool found = false;
    for (const Message& candidate : translated) {
      if (candidate.id == source.id)
        found = true;
    }
    if (!found)
      missing.push_back(source.id);
  }
  return missing;
}

bool StringCatalog::IsComplete(Locale locale) const {
  return MissingIds(locale).empty();
}

Locale StringCatalog::Resolve(const std::string& bcp47_tag) const {
  std::string tag = Lowercase(bcp47_tag);
  for (char& character : tag) {
    if (character == '_')
      character = '-';
  }
  const std::string language = tag.substr(0, tag.find('-'));
  for (const LocaleInfo& info : Locales()) {
    const std::string candidate = Lowercase(info.bcp47);
    if (tag == candidate || language == candidate.substr(0, candidate.find('-')))
      return info.locale;
  }
  // An unknown tag is not an error: a browser that refuses to start in an
  // unexpected locale is a browser that refuses to start.
  return Locale::kEnglish;
}

// static
const char* StringCatalog::Name(MessageId id) {
  switch (id) {
    case MessageId::kAppName:
      return "IDS_APP_NAME";
    case MessageId::kPrivacyPanelTitle:
      return "IDS_PRIVACY_PANEL_TITLE";
    case MessageId::kPrivacyPanelTrackersBlocked:
      return "IDS_PRIVACY_PANEL_TRACKERS_BLOCKED";
    case MessageId::kPrivacyPanelNothingBlocked:
      return "IDS_PRIVACY_PANEL_NOTHING_BLOCKED";
    case MessageId::kPrivacyLevelBalanced:
      return "IDS_PRIVACY_LEVEL_BALANCED";
    case MessageId::kPrivacyLevelStrict:
      return "IDS_PRIVACY_LEVEL_STRICT";
    case MessageId::kResetConfirmTitle:
      return "IDS_RESET_CONFIRM_TITLE";
    case MessageId::kResetConfirmBody:
      return "IDS_RESET_CONFIRM_BODY";
    case MessageId::kResetUntouchedHeading:
      return "IDS_RESET_UNTOUCHED_HEADING";
    case MessageId::kImportPreviewHeading:
      return "IDS_IMPORT_PREVIEW_HEADING";
    case MessageId::kImportRefusedReason:
      return "IDS_IMPORT_REFUSED_REASON";
    case MessageId::kAdvancedGuardRejected:
      return "IDS_ADVANCED_GUARD_REJECTED";
    case MessageId::kErrorNetworkUnreachable:
      return "IDS_ERROR_NETWORK_UNREACHABLE";
    case MessageId::kErrorNetworkUnreachableAction:
      return "IDS_ERROR_NETWORK_UNREACHABLE_ACTION";
    case MessageId::kErrorCertificateInvalid:
      return "IDS_ERROR_CERTIFICATE_INVALID";
    case MessageId::kErrorCertificateInvalidAction:
      return "IDS_ERROR_CERTIFICATE_INVALID_ACTION";
    case MessageId::kErrorProfileLocked:
      return "IDS_ERROR_PROFILE_LOCKED";
    case MessageId::kErrorProfileLockedAction:
      return "IDS_ERROR_PROFILE_LOCKED_ACTION";
    case MessageId::kErrorDownloadRefused:
      return "IDS_ERROR_DOWNLOAD_REFUSED";
    case MessageId::kErrorDownloadRefusedAction:
      return "IDS_ERROR_DOWNLOAD_REFUSED_ACTION";
    case MessageId::kErrorExtensionBlocked:
      return "IDS_ERROR_EXTENSION_BLOCKED";
    case MessageId::kErrorExtensionBlockedAction:
      return "IDS_ERROR_EXTENSION_BLOCKED_ACTION";
    case MessageId::kErrorConfigInvalid:
      return "IDS_ERROR_CONFIG_INVALID";
    case MessageId::kErrorConfigInvalidAction:
      return "IDS_ERROR_CONFIG_INVALID_ACTION";
  }
  return "IDS_UNKNOWN";
}

// static
const char* StringCatalog::Name(PluralCategory category) {
  switch (category) {
    case PluralCategory::kOne:
      return "one";
    case PluralCategory::kFew:
      return "few";
    case PluralCategory::kMany:
      return "many";
    case PluralCategory::kOther:
      return "other";
  }
  return "unknown";
}

}  // namespace l10n
}  // namespace ui
}  // namespace bedrock
