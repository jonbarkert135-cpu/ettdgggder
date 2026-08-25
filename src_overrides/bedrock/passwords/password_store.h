// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PASSWORDS_PASSWORD_STORE_H_
#define BEDROCK_PASSWORDS_PASSWORD_STORE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "bedrock/crypto/aead.h"

// Passwords (roadmap item 34).
//
// Rules this file exists to enforce:
//
//  1. Bedrock has no password server. Nothing here syncs, uploads, or phones
//     home. Storage is local: every entry is sealed with AEAD under a random
//     256-bit data key, and that data key is itself wrapped — by the platform
//     keystore (macOS Keychain, Windows DPAPI, libsecret), or by a key derived
//     from the master password, or by both.
//  2. THE MASTER PASSWORD IS A KEY, NOT A GATE. It is stretched with a
//     password KDF into a key-encryption key that wraps the data key; without
//     it the data key does not exist in any form. Verification is the AEAD tag
//     of that wrapped blob — there is no stored verifier to compare against,
//     and therefore no way to accept a wrong password or to "set" the master
//     password by being the first to type one. This is the fix for F3 in
//     docs/security/AUDIT-2026-08-25.md, where the master password protected
//     nothing and the first password entered silently became the right one.
//  3. The store never holds plaintext OR KEY MATERIAL while locked. Lock()
//     wipes the data key; decryption happens per lookup and only while
//     unlocked.
//  4. Autofill is the dangerous part, not storage. Every fill decision goes
//     through ShouldAutofill(), which refuses cross-origin fills, refuses to
//     downgrade an https credential onto http, and refuses to fill invisible
//     fields without a user gesture. Those three refusals are most of what a
//     password manager owes its user.
//  5. Breach checking is opt-in, off by default, and k-anonymous: the first
//     five hex characters of the SHA-1 of the password are sent to a
//     third-party range API chosen by the user, never the password, never
//     the username, never the site. No Bedrock server is involved, and with
//     the feature off no request happens at all.
//
// The keystore cipher and the random source are injected; the AEAD and the KDF
// come from bedrock/crypto, which is BoringSSL inside the Chromium build. No
// cipher is implemented here.

namespace bedrock {
namespace passwords {

class Cipher {
 public:
  virtual ~Cipher() = default;
  virtual std::string Encrypt(const std::string& plaintext) = 0;
  // Returns false if the blob does not belong to this key.
  virtual bool Decrypt(const std::string& ciphertext, std::string* out) = 0;
};

enum class MasterProtection {
  kPlatformKeystoreOnly,  // unlocked with the OS session
  kMasterPassword,        // extra passphrase, required to unlock
};

// PBKDF2-HMAC-SHA256 iteration count: the only thing standing between a stolen
// profile directory and every password in it. The default follows the OWASP
// 2023 floor. Tests may lower it; production code may not. (A memory-hard KDF —
// Argon2id — is better and is what the Chromium build should switch to.)
constexpr uint32_t kDefaultKdfIterations = 600000;

struct Credential {
  std::string origin;    // https://example.com — the signon realm
  std::string username;
  std::string password;  // only ever populated on the way in or out
};

struct StoredCredential {
  std::string origin;
  std::string username;
  std::string encrypted_password;
  bool breached = false;   // set by a completed breach check
  bool is_passkey = false;
};

// Everything the autofill decision depends on.
struct FillContext {
  std::string page_origin;
  std::string form_action_origin;  // where the form posts
  bool field_visible = true;
  bool user_gesture = false;
  bool is_secure_context = true;  // https (or localhost)
};

enum class FillDecision {
  kFill,
  kOfferOnClick,        // we have a match, but it needs the user to ask
  kRefusedCrossOrigin,
  kRefusedInsecure,
  kRefusedHiddenField,
  kNoMatch,
};

struct BreachQuery {
  bool enabled = false;
  std::string hash_prefix;  // five hex characters, nothing else
  std::string endpoint;     // third-party, user-configurable
};

class PasswordStore {
 public:
  // `random` is required: a store that cannot generate a data key, a salt or a
  // nonce cannot be built safely, so there is no constructor without one.
  PasswordStore(std::unique_ptr<Cipher> cipher,
                std::unique_ptr<crypto::RandomSource> random);
  ~PasswordStore();

  // Master protection.
  //
  // SetMasterProtection() only switches the *mode*. Turning on
  // kMasterPassword without a password would leave a store nobody can open, so
  // it is refused until SetMasterPassword() has wrapped the data key.
  bool SetMasterProtection(MasterProtection protection);
  MasterProtection protection() const { return protection_; }
  // Sets the master password for the first time and switches to it. Fails if
  // one is already set (use ChangeMasterPassword) or while locked.
  bool SetMasterPassword(const std::string& master_password);
  // Re-wraps the same data key under a new password, so stored entries stay
  // readable. Requires the current password; never a "reset".
  bool ChangeMasterPassword(const std::string& current,
                            const std::string& next);
  bool has_master_password() const { return !master_envelope_.empty(); }
  void SetKdfIterations(uint32_t iterations) { kdf_iterations_ = iterations; }
  uint32_t kdf_iterations() const { return kdf_iterations_; }

  bool Unlock();  // platform keystore path
  bool Unlock(const std::string& master_password);
  void Lock();
  bool locked() const { return locked_; }
  // Idle auto-lock, in seconds; 0 = never (the user's choice, not ours).
  void SetAutoLockSeconds(int seconds) { auto_lock_seconds_ = seconds; }
  int auto_lock_seconds() const { return auto_lock_seconds_; }
  bool OnIdle(int elapsed_seconds);  // returns true if it locked

  // Storage. Adding requires an unlocked store; so does reading.
  bool Add(const Credential& credential);
  bool Update(const std::string& origin,
              const std::string& username,
              const std::string& new_password);
  bool Remove(const std::string& origin, const std::string& username);
  // Returns false while locked, or when there is nothing stored.
  bool Get(const std::string& origin,
           const std::string& username,
           std::string* password_out);
  std::vector<StoredCredential> List() const;  // never contains plaintext
  int count() const { return static_cast<int>(entries_.size()); }

  // Passkeys are held by the platform authenticator; Bedrock records that one
  // exists and never has private key material to export.
  bool RegisterPasskey(const std::string& origin, const std::string& username);
  bool HasPasskey(const std::string& origin) const;

  // Autofill policy.
  FillDecision ShouldAutofill(const FillContext& context) const;

  // Breach checking.
  void SetBreachCheckEnabled(bool enabled) { breach_check_enabled_ = enabled; }
  bool breach_check_enabled() const { return breach_check_enabled_; }
  void SetBreachEndpoint(std::string endpoint) {
    breach_endpoint_ = std::move(endpoint);
  }
  // Builds the request that *would* be sent. Returns an empty, disabled query
  // when the feature is off — the caller then makes no request at all.
  BreachQuery BuildBreachQuery(const std::string& password) const;
  // Applies the answer from the range API: the caller matched the remaining
  // hash suffix locally, so the service never learns which one we asked for.
  bool RecordBreachResult(const std::string& origin,
                          const std::string& username,
                          bool found_in_breach);

  const std::string& last_error() const { return last_error_; }

 private:
  StoredCredential* Find(const std::string& origin,
                         const std::string& username);
  const StoredCredential* Find(const std::string& origin,
                               const std::string& username) const;

  // Wraps/unwraps the data key with a key derived from `password`. Returns
  // false when the password is wrong — indistinguishably from any other
  // failure, because the AEAD tag is the only signal.
  bool UnwrapWithPassword(const std::string& password, std::string* data_key_out);

  std::unique_ptr<Cipher> cipher_;
  std::unique_ptr<crypto::RandomSource> random_;
  std::vector<StoredCredential> entries_;
  MasterProtection protection_ = MasterProtection::kPlatformKeystoreOnly;
  uint32_t kdf_iterations_ = kDefaultKdfIterations;
  std::string kdf_salt_;         // per-store, random, not secret
  std::string master_envelope_;  // data key sealed under the password-derived key
  std::string keystore_envelope_;  // data key sealed by the platform keystore
  std::string data_key_;         // present only while unlocked, wiped by Lock()
  bool locked_ = true;
  int auto_lock_seconds_ = 900;
  bool breach_check_enabled_ = false;
  std::string breach_endpoint_;
  mutable std::string last_error_;
};

// SHA-1 of the password, uppercase hex — the k-anonymity range APIs in the
// wild are defined on SHA-1, and the digest is not protecting anything here.
// Only the first five characters ever leave the machine, and only when the user
// turned the feature on. The implementation lives in bedrock/crypto.
std::string PasswordHashHex(const std::string& input);

}  // namespace passwords
}  // namespace bedrock

#endif  // BEDROCK_PASSWORDS_PASSWORD_STORE_H_
