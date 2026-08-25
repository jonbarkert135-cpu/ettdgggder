// Copyright 2026 The Bedrock Authors
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BEDROCK_CRYPTO_AEAD_H_
#define BEDROCK_CRYPTO_AEAD_H_

#include <cstddef>
#include <cstdint>
#include <string>

// Authenticated encryption with associated data, behind an interface.
//
// Rules this file exists to enforce:
//
//  1. AUTHENTICATE, ALWAYS. Every Seal() produces a tag, and Open() returns
//     false on any modification. Unauthenticated encryption of a password
//     database is a bug, not a trade-off.
//  2. THE TAG IS THE VERIFIER. Checking a password by comparing ciphertexts —
//     what the store used to do (F3 in docs/security/AUDIT-2026-08-25.md) —
//     requires a deterministic cipher and therefore cannot work with real
//     AEAD. Correct: try to Open() the wrapped key; a wrong password fails the
//     tag check. That also makes verification constant-time and gives no
//     oracle beyond "wrong".
//  3. NONCE REUSE IS FATAL, so nonces come from an injected RandomSource and
//     are stored next to the ciphertext, never derived from the message.
//  4. NO BLOCK CIPHER IS IMPLEMENTED HERE. Seal/Open are HKDF split +
//     HMAC-SHA256 counter keystream + encrypt-then-MAC, composed only from
//     hash.h, so key management is testable on a bare host. Inside the
//     Chromium build these two functions become AES-256-GCM from BoringSSL;
//     the blob layout (nonce || ciphertext || tag) and the callers do not
//     change.

namespace bedrock {
namespace crypto {

constexpr size_t kKeySize = 32;    // 256-bit keys everywhere
constexpr size_t kNonceSize = 12;  // 96-bit, the AES-GCM nonce size
constexpr size_t kTagSize = 16;    // 128-bit tag, truncated from HMAC

class RandomSource {
 public:
  virtual ~RandomSource() = default;
  // Must return `count` cryptographically random bytes. The production source
  // is the platform CSPRNG; tests inject a deterministic counter so that
  // ciphertexts are reproducible.
  virtual std::string Bytes(size_t count) = 0;
};

// Returns nonce || ciphertext || tag, or an empty string if `key` or `nonce` is
// the wrong size (a caller bug must not produce a weak blob).
std::string Seal(const std::string& key,
                 const std::string& nonce,
                 const std::string& aad,
                 const std::string& plaintext);

// Returns false if the key, the aad, or any byte of the blob is wrong. `out` is
// left untouched on failure.
bool Open(const std::string& key,
          const std::string& aad,
          const std::string& sealed,
          std::string* out);

}  // namespace crypto
}  // namespace bedrock

#endif  // BEDROCK_CRYPTO_AEAD_H_
