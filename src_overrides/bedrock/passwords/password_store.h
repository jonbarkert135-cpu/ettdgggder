// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_PASSWORDS_PASSWORD_STORE_H_
#define BEDROCK_PASSWORDS_PASSWORD_STORE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

// Passwords (roadmap item 34).
//
// Rules this file exists to enforce:
//
//  1. Bedrock has no password server. Nothing here syncs, uploads, or phones
//     home. Storage is local, encrypted by the platform keystore (macOS
//     Keychain, Windows DPAPI, libsecret) and optionally by a master
//     password on top of it.
//  2. The store never holds plaintext while locked. Entries are ciphertext
//     blobs; decryption happens per lookup and only while unlocked.
//  3. Autofill is the dangerous part, not storage. Every fill decision goes
//     through ShouldAutofill(), which refuses cross-origin fills, refuses to
//     downgrade an https credential onto http, and refuses to fill invisible
//     fields without a user gesture. Those three refusals are most of what a
//     password manager owes its user.
//  4. Breach checking is opt-in, off by default, and k-anonymous: the first
//     five hex characters of the SHA-1 of the password are sent to a
//     third-party range API chosen by the user, never the password, never
//     the username, never the site. No Bedrock server is involved, and with
//     the feature off no request happens at all.
//
// The cipher is injected rather than implemented here: crypto belongs to the
// platform keystore, and a hand-rolled cipher in a browser repository is a
// liability. The host test supplies a fake so the *policy* can be tested.

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
  explicit PasswordStore(std::unique_ptr<Cipher> cipher);
  ~PasswordStore();

  // Master protection.
  void SetMasterProtection(MasterProtection protection);
  MasterProtection protection() const { return protection_; }
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

  std::unique_ptr<Cipher> cipher_;
  std::vector<StoredCredential> entries_;
  MasterProtection protection_ = MasterProtection::kPlatformKeystoreOnly;
  std::string master_password_verifier_;  // encrypted known value, not the password
  bool locked_ = true;
  int auto_lock_seconds_ = 900;
  bool breach_check_enabled_ = false;
  std::string breach_endpoint_;
  mutable std::string last_error_;
};

// SHA-1 of the password, uppercase hex — the k-anonymity range APIs in the
// wild are defined on SHA-1. Only the first five characters ever leave the
// machine, and only when the user turned the feature on.
std::string Sha1Hex(const std::string& input);

}  // namespace passwords
}  // namespace bedrock

#endif  // BEDROCK_PASSWORDS_PASSWORD_STORE_H_
