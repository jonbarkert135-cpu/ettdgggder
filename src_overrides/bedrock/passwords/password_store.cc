// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#include "bedrock/passwords/password_store.h"

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

uint32_t RotateLeft(uint32_t value, int bits) {
  return (value << bits) | (value >> (32 - bits));
}

}  // namespace

// Plain SHA-1, used only to build the five-character k-anonymity prefix for
// breach lookups. It is not used to protect anything — the platform keystore
// does that.
std::string Sha1Hex(const std::string& input) {
  uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u,
                   0xC3D2E1F0u};
  std::string message = input;
  const uint64_t bit_length = static_cast<uint64_t>(input.size()) * 8;
  message.push_back(static_cast<char>(0x80));
  while (message.size() % 64 != 56)
    message.push_back('\0');
  for (int i = 7; i >= 0; --i)
    message.push_back(static_cast<char>((bit_length >> (i * 8)) & 0xFF));

  for (size_t chunk = 0; chunk < message.size(); chunk += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
      const unsigned char* p =
          reinterpret_cast<const unsigned char*>(message.data()) + chunk + i * 4;
      w[i] = (static_cast<uint32_t>(p[0]) << 24) |
             (static_cast<uint32_t>(p[1]) << 16) |
             (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
    }
    for (int i = 16; i < 80; ++i)
      w[i] = RotateLeft(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
      uint32_t f, k;
      if (i < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999u;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1u;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDCu;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6u;
      }
      const uint32_t temp = RotateLeft(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = RotateLeft(b, 30);
      b = a;
      a = temp;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
  }

  char out[41];
  for (int i = 0; i < 5; ++i)
    std::snprintf(out + i * 8, 9, "%08X", h[i]);
  return std::string(out, 40);
}

PasswordStore::PasswordStore(std::unique_ptr<Cipher> cipher)
    : cipher_(std::move(cipher)) {}

PasswordStore::~PasswordStore() = default;

void PasswordStore::SetMasterProtection(MasterProtection protection) {
  protection_ = protection;
  locked_ = true;  // changing the lock relocks; anything else is a hole
}

bool PasswordStore::Unlock() {
  last_error_.clear();
  if (protection_ == MasterProtection::kMasterPassword) {
    last_error_ = "a master password is required";
    return false;
  }
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
  // The verifier is a known string encrypted under the derived key. We never
  // store the master password itself, not even encrypted.
  const std::string verifier = cipher_->Encrypt("bedrock-verifier:" +
                                                master_password);
  if (master_password_verifier_.empty())
    master_password_verifier_ = verifier;
  if (verifier != master_password_verifier_) {
    last_error_ = "incorrect master password";
    return false;
  }
  locked_ = false;
  return true;
}

void PasswordStore::Lock() {
  locked_ = true;
}

bool PasswordStore::OnIdle(int elapsed_seconds) {
  if (auto_lock_seconds_ <= 0 || locked_)
    return false;
  if (elapsed_seconds < auto_lock_seconds_)
    return false;
  locked_ = true;
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
  entry.encrypted_password = cipher_->Encrypt(credential.password);
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
  entry->encrypted_password = cipher_->Encrypt(new_password);
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
  return cipher_->Decrypt(entry->encrypted_password, password_out);
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
  query.hash_prefix = Sha1Hex(password).substr(0, 5);
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
