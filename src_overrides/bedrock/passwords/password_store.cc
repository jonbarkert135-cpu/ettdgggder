// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/passwords/password_store.h"

#include "bedrock/crypto/hash.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace bedrock {
namespace passwords {
namespace {

std::string HostOf(const std::string& origin) {
  const size_t scheme_end = origin.find("://");
  if (scheme_end == std::string::npos)
    return origin;
  const std::string rest = origin.substr(scheme_end + 3);
  const size_t slash = rest.find('/');
  return slash == std::string::npos ? rest : rest.substr(0, slash);
}

bool IsHttps(const std::string& origin) {
  return origin.rfind("https://", 0) == 0;
}

// Domain separation labels. Each envelope is bound to its purpose, so a blob
// from one context cannot be replayed into another.
constexpr char kMasterAad[] = "bedrock/pw/v1/master-key";
constexpr char kKdfInfo[] = "bedrock/pw/v1/kek";

std::string EntryAad(const std::string& origin, const std::string& username) {
  // Binds a sealed password to its own row: swapping two blobs in the file, or
  // moving one to a different site, fails the tag check.
  return "bedrock/pw/v1/entry\n" + origin + "\n" + username;
}

}  // namespace

// Uppercase hex of SHA-1, used only to build the five-character k-anonymity
// prefix for breach lookups, because that is what the range APIs in the world
// are defined on. It protects nothing — the AEAD and the KDF do that. The
// digest now comes from bedrock/crypto (BoringSSL in the real build); this file
// no longer carries a hand-rolled one.
std::string PasswordHashHex(const std::string& input) {
  std::string hex = crypto::ToHex(crypto::Sha1(input));
  for (char& c : hex) {
    if (c >= 'a' && c <= 'f')
      c = static_cast<char>(c - 'a' + 'A');
  }
  return hex;
}

PasswordStore::PasswordStore(std::unique_ptr<Cipher> cipher,
                             std::unique_ptr<crypto::RandomSource> random)
    : cipher_(std::move(cipher)), random_(std::move(random)) {}

PasswordStore::~PasswordStore() {
  // Key material does not outlive the object, even if the allocator reuses the
  // page. (The real build uses a scrubbing container; this is the honest
  // minimum without one.)
  for (char& c : data_key_)
    c = '\0';
}

bool PasswordStore::SetMasterProtection(MasterProtection protection) {
  last_error_.clear();
  if (protection == MasterProtection::kMasterPassword &&
      master_envelope_.empty()) {
    // Otherwise the store would be locked behind a password that does not
    // exist: unopenable, with no error the user could act on.
    last_error_ = "set a master password before requiring one";
    return false;
  }
  protection_ = protection;
  locked_ = true;  // changing the lock relocks; anything else is a hole
  for (char& c : data_key_)
    c = '\0';
  data_key_.clear();
  return true;
}

bool PasswordStore::SetMasterPassword(const std::string& master_password) {
  last_error_.clear();
  if (master_password.empty()) {
    last_error_ = "a master password is required";
    return false;
  }
  if (!master_envelope_.empty()) {
    last_error_ = "a master password is already set";
    return false;
  }
  // The data key must exist before it can be wrapped, and creating it requires
  // an unlocked store — otherwise this call would be a way to replace the key
  // (and orphan every stored entry) from a locked store.
  if (locked_ && !Unlock()) {
    last_error_ = "unlock the store before setting a master password";
    return false;
  }
  if (data_key_.size() != crypto::kKeySize) {
    last_error_ = "no data key";
    return false;
  }
  kdf_salt_ = random_->Bytes(16);
  const std::string kek = crypto::HkdfSha256(
      crypto::Pbkdf2HmacSha256(master_password, kdf_salt_, kdf_iterations_,
                               crypto::kKeySize),
      kdf_salt_, kKdfInfo, crypto::kKeySize);
  const std::string sealed = crypto::Seal(
      kek, random_->Bytes(crypto::kNonceSize), kMasterAad, data_key_);
  // Then the keystore on top: an attacker with the profile directory but no OS
  // session cannot even start guessing the master password offline.
  master_envelope_ = sealed.empty() ? std::string() : cipher_->Encrypt(sealed);
  if (master_envelope_.empty()) {
    last_error_ = "could not wrap the data key";
    return false;
  }
  protection_ = MasterProtection::kMasterPassword;
  locked_ = true;  // prove the new password works before trusting it
  for (char& c : data_key_)
    c = '\0';
  data_key_.clear();
  return true;
}

bool PasswordStore::ChangeMasterPassword(const std::string& current,
                                         const std::string& next) {
  last_error_.clear();
  if (master_envelope_.empty()) {
    last_error_ = "no master password is set";
    return false;
  }
  if (next.empty()) {
    last_error_ = "a master password is required";
    return false;
  }
  std::string data_key;
  if (!UnwrapWithPassword(current, &data_key)) {
    last_error_ = "incorrect master password";
    return false;
  }
  // The SAME data key is re-wrapped: changing the password must not orphan
  // stored entries, and must not require re-encrypting them one by one.
  const std::string salt = random_->Bytes(16);
  const std::string kek = crypto::HkdfSha256(
      crypto::Pbkdf2HmacSha256(next, salt, kdf_iterations_, crypto::kKeySize),
      salt, kKdfInfo, crypto::kKeySize);
  const std::string sealed = crypto::Seal(
      kek, random_->Bytes(crypto::kNonceSize), kMasterAad, data_key);
  const std::string envelope =
      sealed.empty() ? std::string() : cipher_->Encrypt(sealed);
  if (envelope.empty()) {
    last_error_ = "could not wrap the data key";
    return false;
  }
  kdf_salt_ = salt;
  master_envelope_ = envelope;
  for (char& c : data_key)
    c = '\0';
  locked_ = true;
  return true;
}

bool PasswordStore::UnwrapWithPassword(const std::string& password,
                                       std::string* data_key_out) {
  if (master_envelope_.empty() || kdf_salt_.empty())
    return false;
  const std::string kek = crypto::HkdfSha256(
      crypto::Pbkdf2HmacSha256(password, kdf_salt_, kdf_iterations_,
                               crypto::kKeySize),
      kdf_salt_, kKdfInfo, crypto::kKeySize);
  std::string sealed;
  if (!cipher_->Decrypt(master_envelope_, &sealed))
    return false;  // the keystore layer, before the password layer
  // No stored verifier: the wrapped key either opens or it does not. A wrong
  // password fails the AEAD tag, which is a constant-time check and reveals
  // nothing beyond "wrong".
  return crypto::Open(kek, kMasterAad, sealed, data_key_out);
}

bool PasswordStore::Unlock() {
  last_error_.clear();
  if (protection_ == MasterProtection::kMasterPassword) {
    last_error_ = "a master password is required";
    return false;
  }
  if (keystore_envelope_.empty()) {
    // First run: create the data key and let the keystore hold it.
    std::string key = random_->Bytes(crypto::kKeySize);
    if (key.size() != crypto::kKeySize) {
      last_error_ = "no random source";
      return false;
    }
    keystore_envelope_ = cipher_->Encrypt(key);
    data_key_ = std::move(key);
    locked_ = false;
    return true;
  }
  std::string key;
  if (!cipher_->Decrypt(keystore_envelope_, &key) ||
      key.size() != crypto::kKeySize) {
    last_error_ = "the platform keystore could not release the key";
    return false;
  }
  data_key_ = std::move(key);
  locked_ = false;
  return true;
}

bool PasswordStore::Unlock(const std::string& master_password) {
  last_error_.clear();
  if (protection_ != MasterProtection::kMasterPassword)
    return Unlock();
  if (master_password.empty()) {
    last_error_ = "a master password is required";
    return false;
  }
  if (master_envelope_.empty()) {
    // Never trust-on-first-use: with no envelope there is nothing to open, and
    // accepting the first password offered is how F3 let any password in.
    last_error_ = "no master password is set";
    return false;
  }
  std::string key;
  if (!UnwrapWithPassword(master_password, &key)) {
    last_error_ = "incorrect master password";
    return false;
  }
  data_key_ = std::move(key);
  locked_ = false;
  return true;
}

void PasswordStore::Lock() {
  locked_ = true;
  for (char& c : data_key_)
    c = '\0';
  data_key_.clear();  // locked means the key is gone, not just a flag flipped
}

bool PasswordStore::OnIdle(int elapsed_seconds) {
  if (auto_lock_seconds_ <= 0 || locked_)
    return false;
  if (elapsed_seconds < auto_lock_seconds_)
    return false;
  Lock();
  return true;
}

StoredCredential* PasswordStore::Find(const std::string& origin,
                                      const std::string& username) {
  for (StoredCredential& entry : entries_) {
    if (entry.origin == origin && entry.username == username)
      return &entry;
  }
  return nullptr;
}

const StoredCredential* PasswordStore::Find(const std::string& origin,
                                            const std::string& username) const {
  return const_cast<PasswordStore*>(this)->Find(origin, username);
}

bool PasswordStore::Add(const Credential& credential) {
  last_error_.clear();
  if (locked_) {
    last_error_ = "the password store is locked";
    return false;
  }
  if (credential.origin.empty() || credential.password.empty()) {
    last_error_ = "an origin and a password are required";
    return false;
  }
  if (Find(credential.origin, credential.username)) {
    last_error_ = "a credential for this site and user already exists";
    return false;
  }
  StoredCredential entry;
  entry.origin = credential.origin;
  entry.username = credential.username;
  entry.encrypted_password =
      crypto::Seal(data_key_, random_->Bytes(crypto::kNonceSize),
                  EntryAad(credential.origin, credential.username),
                  credential.password);
  if (entry.encrypted_password.empty()) {
    last_error_ = "could not encrypt the password";
    return false;
  }
  entries_.push_back(entry);
  return true;
}

bool PasswordStore::Update(const std::string& origin,
                           const std::string& username,
                           const std::string& new_password) {
  last_error_.clear();
  if (locked_) {
    last_error_ = "the password store is locked";
    return false;
  }
  StoredCredential* entry = Find(origin, username);
  if (!entry)
    return false;
  entry->encrypted_password =
      crypto::Seal(data_key_, random_->Bytes(crypto::kNonceSize),
                  EntryAad(origin, username), new_password);
  if (entry->encrypted_password.empty()) {
    last_error_ = "could not encrypt the password";
    return false;
  }
  entry->breached = false;  // a new password is not the breached one
  return true;
}

bool PasswordStore::Remove(const std::string& origin,
                           const std::string& username) {
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->origin == origin && it->username == username) {
      entries_.erase(it);
      return true;
    }
  }
  return false;
}

bool PasswordStore::Get(const std::string& origin,
                        const std::string& username,
                        std::string* password_out) {
  last_error_.clear();
  if (locked_) {
    last_error_ = "the password store is locked";
    return false;
  }
  StoredCredential* entry = Find(origin, username);
  if (!entry)
    return false;
  return crypto::Open(data_key_, EntryAad(origin, username),
                     entry->encrypted_password, password_out);
}

std::vector<StoredCredential> PasswordStore::List() const {
  // Deliberately the stored form: ciphertext. A list view needs sites and
  // usernames, not secrets.
  return entries_;
}

bool PasswordStore::RegisterPasskey(const std::string& origin,
                                    const std::string& username) {
  last_error_.clear();
  if (locked_) {
    last_error_ = "the password store is locked";
    return false;
  }
  StoredCredential entry;
  entry.origin = origin;
  entry.username = username;
  entry.is_passkey = true;
  // No key material: the platform authenticator holds it and never hands it
  // over. This record exists so the UI can show the account.
  entries_.push_back(entry);
  return true;
}

bool PasswordStore::HasPasskey(const std::string& origin) const {
  for (const StoredCredential& entry : entries_) {
    if (entry.origin == origin && entry.is_passkey)
      return true;
  }
  return false;
}

FillDecision PasswordStore::ShouldAutofill(const FillContext& context) const {
  const std::string host = HostOf(context.page_origin);

  int matches = 0;
  bool https_match_for_this_host = false;
  for (const StoredCredential& entry : entries_) {
    if (entry.is_passkey)
      continue;
    if (entry.origin == context.page_origin)
      ++matches;
    else if (HostOf(entry.origin) == host && IsHttps(entry.origin))
      https_match_for_this_host = true;
  }

  if (matches == 0) {
    // A credential saved for https must never be typed into the http version
    // of the same host: that is the downgrade attack the user cannot see.
    if (https_match_for_this_host)
      return FillDecision::kRefusedInsecure;
    return FillDecision::kNoMatch;
  }
  if (!context.is_secure_context)
    return FillDecision::kRefusedInsecure;
  if (!context.form_action_origin.empty() &&
      context.form_action_origin != context.page_origin) {
    // The form posts somewhere else. Filling it hands the password to a third
    // party before the user has read anything.
    return FillDecision::kRefusedCrossOrigin;
  }
  if (!context.field_visible) {
    // Invisible password fields are a fingerprinting and harvesting trick.
    // There is no legitimate reason to fill one, gesture or not.
    return FillDecision::kRefusedHiddenField;
  }
  if (matches > 1 && !context.user_gesture) {
    // Several accounts on this site: guessing one is worse than waiting.
    return FillDecision::kOfferOnClick;
  }
  return FillDecision::kFill;
}

BreachQuery PasswordStore::BuildBreachQuery(const std::string& password) const {
  BreachQuery query;
  if (!breach_check_enabled_) {
    // Off means off: no endpoint, no prefix, nothing for a caller to send.
    return query;
  }
  query.enabled = true;
  query.hash_prefix = PasswordHashHex(password).substr(0, 5);
  query.endpoint = breach_endpoint_;
  return query;
}

bool PasswordStore::RecordBreachResult(const std::string& origin,
                                       const std::string& username,
                                       bool found_in_breach) {
  StoredCredential* entry = Find(origin, username);
  if (!entry)
    return false;
  entry->breached = found_in_breach;
  return true;
}

}  // namespace passwords
}  // namespace bedrock
