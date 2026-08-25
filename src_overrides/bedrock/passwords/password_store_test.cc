// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//
// Host test, no Chromium. Run via scripts/run_host_tests.sh.

#include "bedrock/passwords/password_store.h"

#include <cstddef>
#include <iostream>
#include <memory>
#include <string>

namespace {

using namespace bedrock::passwords;  // NOLINT — test-local convenience

int failures = 0;

void Check(bool condition, const std::string& what) {
  if (!condition) {
    std::cerr << "FAIL " << what << "\n";
    ++failures;
  }
}

// Stands in for the platform keystore. Reversible and obviously not secure —
// the point is to test policy, not to invent a cipher. Note it is
// deterministic, which is exactly what the old ciphertext-comparing verifier
// silently depended on (F3): the real keystore is not.
class FakeCipher : public Cipher {
 public:
  std::string Encrypt(const std::string& plaintext) override {
    std::string out = "enc:" + plaintext;
    for (size_t i = 4; i < out.size(); ++i)
      out[i] = static_cast<char>(out[i] ^ 0x5A);
    return out;
  }
  bool Decrypt(const std::string& ciphertext, std::string* out) override {
    if (ciphertext.rfind("enc:", 0) != 0)
      return false;
    std::string plain = ciphertext.substr(4);
    for (char& c : plain)
      c = static_cast<char>(c ^ 0x5A);
    *out = plain;
    return true;
  }
};

// A deterministic stand-in for the platform CSPRNG, so that failures are
// reproducible. Production code gets the real one; there is no constructor
// without a random source at all.
class CountingRandom : public bedrock::crypto::RandomSource {
 public:
  std::string Bytes(size_t count) override {
    std::string out;
    for (size_t i = 0; i < count; ++i)
      out.push_back(static_cast<char>((counter_ * 31 + i * 7 + 11) & 0xFF));
    ++counter_;
    return out;
  }

 private:
  size_t counter_ = 1;
};

// Every store in this test uses cheap KDF parameters: the production default
// (600k PBKDF2 iterations) is the point of the design, but paying it on every
// unlock would make the suite slow. The default value itself is asserted below.
std::unique_ptr<PasswordStore> MakeStore() {
  auto store = std::make_unique<PasswordStore>(std::make_unique<FakeCipher>(),
                                              std::make_unique<CountingRandom>());
  store->SetKdfIterations(64);
  return store;
}

FillContext Context(const std::string& origin) {
  FillContext context;
  context.page_origin = origin;
  context.form_action_origin = origin;
  return context;
}

}  // namespace

int main() {
  Check(kDefaultKdfIterations >= 600000,
        "the default KDF cost is at least the OWASP floor");

  auto store_owner = MakeStore();
  PasswordStore& store = *store_owner;

  // Locked by default; nothing goes in or out.
  Check(store.locked(), "the store starts locked");
  Check(!store.Add({"https://example.com", "ada", "hunter2"}),
        "a locked store refuses writes");
  Check(store.Unlock(), "the platform keystore unlocks it");
  Check(store.Add({"https://example.com", "ada", "hunter2"}),
        "and then credentials can be saved");

  std::string password;
  Check(store.Get("https://example.com", "ada", &password) &&
            password == "hunter2",
        "an unlocked store returns the password");

  // Nothing readable is kept in the list view.
  for (const StoredCredential& entry : store.List()) {
    Check(entry.encrypted_password.find("hunter2") == std::string::npos,
          "the stored form contains no plaintext");
  }

  store.Lock();
  password.clear();
  Check(!store.Get("https://example.com", "ada", &password),
        "a locked store returns nothing");
  Check(password.empty(), "and leaves the output untouched");

  // --- Master password protection (audit F3) --------------------------------

  // Requiring a password that does not exist yet would lock the user out of
  // their own store, so the mode cannot be switched on before the password is
  // set.
  Check(!store.SetMasterProtection(MasterProtection::kMasterPassword),
        "requiring a master password before setting one is refused");
  Check(store.protection() == MasterProtection::kPlatformKeystoreOnly,
        "and the mode did not change");

  Check(store.Unlock(), "reopen with the keystore");
  Check(store.SetMasterPassword("correct horse"), "the master password is set");
  Check(store.has_master_password(), "and the store knows it has one");
  Check(store.locked(),
        "setting it relocks: the new password must prove itself first");
  Check(!store.SetMasterPassword("second try"),
        "a master password cannot be silently replaced");

  Check(!store.Unlock(), "the keystore alone is no longer enough");
  Check(!store.Unlock(""), "an empty password is not a password");
  Check(!store.Unlock("wrong horse"), "a wrong master password is refused");
  Check(store.locked(), "and the store stays locked");
  Check(store.Unlock("correct horse"), "the right one unlocks it");

  // The entries written before the master password existed are still readable:
  // the data key was re-wrapped, not replaced.
  password.clear();
  Check(store.Get("https://example.com", "ada", &password) &&
            password == "hunter2",
        "entries survive turning on the master password");

  // Trust-on-first-use, the third defect in F3: a fresh store must not accept
  // whatever password is offered first.
  {
    auto fresh = MakeStore();
    Check(fresh->Unlock(), "a fresh store opens with the keystore");
    Check(fresh->SetMasterProtection(MasterProtection::kPlatformKeystoreOnly),
          "and can stay in keystore mode");
    Check(!fresh->has_master_password(), "with no master password set");
    fresh->Lock();
    // Offering a password to a store that has none must not *create* one.
    Check(fresh->Unlock("anything at all"),
          "the keystore still opens it (the password is ignored, not stored)");
    Check(!fresh->has_master_password(),
          "and typing a password did not silently make it the master password");
    Check(fresh->protection() == MasterProtection::kPlatformKeystoreOnly,
          "the protection mode is unchanged");
  }

  // Changing the password re-wraps the same data key, so stored entries stay
  // readable and the old password stops working.
  Check(store.ChangeMasterPassword("correct horse", "staple battery"),
        "the master password can be changed with the current one");
  Check(!store.ChangeMasterPassword("correct horse", "third"),
        "the old password no longer authorises a change");
  Check(!store.Unlock("correct horse"), "and no longer unlocks");
  Check(store.Unlock("staple battery"), "the new one does");
  password.clear();
  Check(store.Get("https://example.com", "ada", &password) &&
            password == "hunter2",
        "entries survive a password change");

  // Locking must destroy key material, not just set a flag.
  store.Lock();
  Check(!store.Get("https://example.com", "ada", &password),
        "a locked store cannot decrypt");

  Check(store.Unlock("staple battery"), "unlock again for the rest of the test");

  // Auto-lock.
  store.SetAutoLockSeconds(300);
  Check(!store.OnIdle(299), "the store stays open inside the idle window");
  Check(store.OnIdle(300), "and locks itself when it passes");
  Check(store.locked(), "auto-lock really locks");
  store.Unlock("staple battery");
  store.SetAutoLockSeconds(0);
  Check(!store.OnIdle(100000), "0 means the user turned auto-lock off");

  // Autofill policy — the part that actually loses passwords.
  Check(store.ShouldAutofill(Context("https://example.com")) ==
            FillDecision::kFill,
        "a same-origin visible field on https is filled");

  FillContext cross = Context("https://example.com");
  cross.form_action_origin = "https://collector.example.net";
  Check(store.ShouldAutofill(cross) == FillDecision::kRefusedCrossOrigin,
        "a form posting to another origin is refused");

  FillContext insecure = Context("http://example.com");
  insecure.is_secure_context = false;
  Check(store.ShouldAutofill(insecure) == FillDecision::kRefusedInsecure,
        "an https credential is never typed into the http version of the site");

  FillContext hidden = Context("https://example.com");
  hidden.field_visible = false;
  hidden.user_gesture = true;
  Check(store.ShouldAutofill(hidden) == FillDecision::kRefusedHiddenField,
        "an invisible password field is refused even with a user gesture");

  Check(store.ShouldAutofill(Context("https://unknown.example")) ==
            FillDecision::kNoMatch,
        "a site with nothing saved gets nothing");

  store.Add({"https://example.com", "grace", "second-account"});
  Check(store.ShouldAutofill(Context("https://example.com")) ==
            FillDecision::kOfferOnClick,
        "two accounts on one site means asking, not guessing");

  // Passkeys.
  Check(store.RegisterPasskey("https://passkeys.example", "ada"),
        "a passkey can be recorded");
  Check(store.HasPasskey("https://passkeys.example"), "and is listed");
  for (const StoredCredential& entry : store.List()) {
    if (entry.is_passkey) {
      Check(entry.encrypted_password.empty(),
            "no passkey key material is held by the browser");
    }
  }
  FillContext passkey_site = Context("https://passkeys.example");
  Check(store.ShouldAutofill(passkey_site) == FillDecision::kNoMatch,
        "a passkey account is not treated as a password to type in");

  // Breach checking: off by default, k-anonymous when on.
  Check(!store.breach_check_enabled(), "breach checking is off by default");
  BreachQuery off = store.BuildBreachQuery("hunter2");
  Check(!off.enabled && off.hash_prefix.empty() && off.endpoint.empty(),
        "with the feature off there is nothing to send");

  store.SetBreachEndpoint("https://api.pwnedpasswords.com/range/");
  store.SetBreachCheckEnabled(true);
  BreachQuery on = store.BuildBreachQuery("password");
  Check(on.enabled, "the query is built when the user opted in");
  Check(on.hash_prefix == "5BAA6",
        "it is the five-character SHA-1 prefix (k-anonymity)");
  Check(on.hash_prefix.size() == 5, "and nothing longer");
  Check(on.endpoint.find("bedrock") == std::string::npos,
        "the endpoint is third-party; Bedrock runs no breach server");

  // Known-answer test for the hash used above.
  Check(PasswordHashHex("password") ==
            "5BAA61E4C9B93F3F0682250B6CF8331B7EE68FD8",
        "SHA-1 matches the published test vector");
  Check(PasswordHashHex("") == "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709",
        "SHA-1 of the empty string matches too");

  Check(store.RecordBreachResult("https://example.com", "ada", true),
        "a breach result can be recorded");
  bool marked = false;
  for (const StoredCredential& entry : store.List())
    marked |= entry.username == "ada" && entry.breached;
  Check(marked, "and shows on the entry");

  store.Update("https://example.com", "ada", "a new one");
  for (const StoredCredential& entry : store.List()) {
    if (entry.username == "ada")
      Check(!entry.breached, "changing the password clears the warning");
  }

  Check(store.Remove("https://example.com", "grace"),
        "a credential can be deleted");

  if (failures == 0)
    std::cout << "password_store_test: all assertions passed\n";
  return failures == 0 ? 0 : 1;
}
